//
// Farmer unit behavior implementation.
//
// State machine: SEEKING -> WALKING_TO_SUSTENANCE -> GATHERING -> RETURNING -> DEPOSITING -> repeat.
//

#include "farmer.h"
#include "base_geometry.h"
#include "deposit_slots.h"
#include "pathfinding.h"
#include "../core/battlefield.h"
#include "../core/sustenance.h"
#include "../core/config.h"
#include "../entities/entities.h"
#include "../systems/player.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>

static float farmer_sprite_rotation(const Entity *e) {
    return (bf_side_for_player(e->ownerID) == SIDE_TOP) ? 180.0f : 0.0f;
}

// Swap between the empty and full Cheffy sprite variants without touching
// AnimState. Callers that also need to restart on a new clip (e.g. switching
// walk duration to the full-variant spec) should call entity_set_state()
// afterward; callers that just want the pointer repoint (e.g. during an
// in-progress one-shot) leave the anim alone.
static void farmer_apply_variant(Entity *e, GameState *gs, bool full) {
    SpriteType target = full ? SPRITE_TYPE_FARMER_FULL : SPRITE_TYPE_FARMER;
    if (e->spriteType == target) return;
    e->spriteType = target;
    e->sprite = sprite_atlas_get(&gs->spriteAtlas, target);
}

static float farmer_base_contact_radius(const Entity *farmer, const Entity *base) {
    if (!farmer || !base) return 0.0f;
    float baseNavRadius = (base->navRadius > 0.0f) ? base->navRadius : base->bodyRadius;
    return baseNavRadius + farmer->bodyRadius + BASE_DEPOSIT_SLOT_GAP;
}

static float farmer_base_wait_radius(const Entity *farmer, const Entity *base) {
    return farmer_base_contact_radius(farmer, base) +
           BASE_DEPOSIT_QUEUE_RADIAL_OFFSET +
           farmer->bodyRadius +
           PATHFIND_CONTACT_GAP;
}

static void farmer_resolve_return_goal(const Entity *farmer, const Entity *base,
                                       Vector2 *outGoal, float *outRadius) {
    if (!farmer || !base || !outGoal || !outRadius) return;

    *outGoal = base_interaction_anchor(base);
    *outRadius = farmer_base_wait_radius(farmer, base);

    if (farmer->reservedDepositSlotKind == DEPOSIT_SLOT_PRIMARY) {
        *outGoal = deposit_slots_get_position(base, DEPOSIT_SLOT_PRIMARY,
                                              farmer->reservedDepositSlotIndex);
        *outRadius = FARMER_DEPOSIT_ARRIVAL_RADIUS;
        return;
    }

    if (farmer->reservedDepositSlotKind == DEPOSIT_SLOT_QUEUE) {
        *outGoal = deposit_slots_get_position(base, DEPOSIT_SLOT_QUEUE,
                                              farmer->reservedDepositSlotIndex);
        *outRadius = FARMER_QUEUE_WAIT_PROXIMITY;
    }
}

static void farmer_build_sustenance_goal_request(const Entity *farmer,
                                                 const GameState *gs,
                                                 const SustenanceNode *node,
                                                 NavFreeGoalRequest *outRequest) {
    if (!farmer || !gs || !node || !outRequest) return;
    (void)gs;

    *outRequest = (NavFreeGoalRequest){
        .goalX = node->worldPos.v.x,
        .goalY = node->worldPos.v.y,
        .stopRadius = FARMER_SUSTENANCE_INTERACT_RADIUS,
        .perspectiveSide = (int16_t)bf_side_for_player(farmer->ownerID),
        .carveTargetId = -1,
        .carveCenterX = 0.0f,
        .carveCenterY = 0.0f,
        .carveInnerRadius = 0.0f,
    };
}

static bool farmer_try_score_sustenance_node(const Entity *farmer,
                                             GameState *gs,
                                             const SustenanceNode *node,
                                             int32_t *outCost) {
    if (!farmer || !gs || !node || !outCost) return false;
    if (!gs->nav.initialized) return false;

    NavFreeGoalRequest request;
    farmer_build_sustenance_goal_request(farmer, gs, node, &request);
    const NavField *field = nav_get_or_build_free_goal_field(&gs->nav,
                                                             &gs->battlefield,
                                                             &request);
    if (!field) return false;

    int32_t cell = nav_cell_index_for_world(farmer->position.x, farmer->position.y);
    int32_t cost = field->distance[cell];
    if (cost == NAV_DIST_UNREACHABLE) return false;

    *outCost = cost;
    return true;
}

static SustenanceNode *farmer_find_best_sustenance_node(Entity *farmer, GameState *gs) {
    if (!farmer || !gs) return NULL;

    Battlefield *bf = &gs->battlefield;
    BattleSide side = bf_side_for_player(farmer->ownerID);
    SustenanceNode *bestReachable = NULL;
    int32_t bestCost = INT_MAX;
    float bestReachableDistSq = INFINITY;
    SustenanceNode *bestNearest = NULL;
    float bestNearestDistSq = INFINITY;

    for (int i = 0; i < SUSTENANCE_MATCH_COUNT_PER_SIDE; ++i) {
        SustenanceNode *node = &bf->sustenanceField.nodes[side][i];
        if (!node->active || node->claimedByEntityId != -1) continue;

        float dx = node->worldPos.v.x - farmer->position.x;
        float dy = node->worldPos.v.y - farmer->position.y;
        float distSq = dx * dx + dy * dy;
        if (!bestNearest || distSq < bestNearestDistSq) {
            bestNearest = node;
            bestNearestDistSq = distSq;
        }

        int32_t navCost = NAV_DIST_UNREACHABLE;
        if (!farmer_try_score_sustenance_node(farmer, gs, node, &navCost)) {
            continue;
        }

        if (!bestReachable ||
            navCost < bestCost ||
            (navCost == bestCost && distSq < bestReachableDistSq)) {
            bestReachable = node;
            bestCost = navCost;
            bestReachableDistSq = distSq;
        }
    }

    return bestReachable ? bestReachable : bestNearest;
}

// Move a farmer toward `target`, stopping when within `radius`. Delegates
// to pathfind_move_toward_goal so farmers participate in the same
// obstacle-aware local steering as combat troops (queueing behind each
// other and sidestepping around bases) while keeping the farmer-specific
// 180-degree top-side rotation hook.
static bool farmer_move_with_steering(Entity *e, GameState *gs, Vector2 target,
                                      float radius, float deltaTime) {
    bool arrived = pathfind_move_toward_goal(e, target, radius,
                                             &gs->nav, &gs->battlefield,
                                             deltaTime);
    e->spriteRotationDegrees = farmer_sprite_rotation(e);
    return arrived;
}

// --- State handlers ---

static void farmer_seek(Entity *e, GameState *gs) {
    Battlefield *bf = &gs->battlefield;
    e->movementTargetId = -1;

    // Seeking Cheffies are always empty. Safety net in case a prior state
    // left the wrong variant attached (e.g. future code paths).
    farmer_apply_variant(e, gs, false);

    SustenanceNode *node = farmer_find_best_sustenance_node(e, gs);
    if (!node) {
        // No sustenance available — idle until one frees up
        if (e->state != ESTATE_IDLE) entity_set_state(e, ESTATE_IDLE);
        return;
    }

    if (!sustenance_claim(bf, node->id, e->id)) return;

    e->claimedSustenanceNodeId = node->id;
    e->farmerState = FARMER_WALKING_TO_SUSTENANCE;
    entity_set_state(e, ESTATE_WALKING);

    printf("[FARMER] Entity %d claimed sustenance node %d\n", e->id, node->id);
}

static void farmer_walk_to_sustenance(Entity *e, GameState *gs, float deltaTime) {
    Battlefield *bf = &gs->battlefield;
    e->movementTargetId = -1;
    SustenanceNode *node = sustenance_get_node(bf, e->claimedSustenanceNodeId);

    // Claimed node became invalid — release and re-seek
    if (!node || !node->active || node->claimedByEntityId != e->id) {
        if (node && node->claimedByEntityId == e->id) {
            sustenance_release_claim(bf, e->claimedSustenanceNodeId, e->id);
        }
        e->claimedSustenanceNodeId = -1;
        e->farmerState = FARMER_SEEKING;
        return;
    }

    bool arrived = farmer_move_with_steering(e, gs, node->worldPos.v,
                                              FARMER_SUSTENANCE_INTERACT_RADIUS,
                                              deltaTime);
    if (arrived) {
        SpriteDirection dir = e->anim.dir;
        bool flipH = e->anim.flipH;
        e->farmerState = FARMER_GATHERING;
        // Reuse the authored Cheffy idle sheet as a one-shot gather animation
        // via the empty variant's ATTACK spec row.
        entity_set_state(e, ESTATE_ATTACKING);
        // Preserve the approach-facing when switching into the one-shot clip.
        e->anim.dir = dir;
        e->anim.flipH = flipH;
        printf("[FARMER] Entity %d arrived at sustenance node %d, gathering\n",
               e->id, node->id);
    }
}

static void farmer_gather(Entity *e, GameState *gs, float deltaTime) {
    (void)deltaTime;
    Battlefield *bf = &gs->battlefield;
    e->movementTargetId = -1;
    SustenanceNode *node = sustenance_get_node(bf, e->claimedSustenanceNodeId);

    // Node invalidated while gathering
    if (!node || !node->active) {
        e->claimedSustenanceNodeId = -1;
        e->farmerState = FARMER_SEEKING;
        entity_set_state(e, ESTATE_WALKING);
        return;
    }

    // Wait for one-shot attack clip to finish before applying one work cycle.
    if (!e->anim.finished) return;

    // Work cycle complete — decrement durability
    node->durability--;
    if (node->durability <= 0) {
        // Sustenance depleted: pick up value, respawn node, head home
        e->carriedSustenanceValue = node->value;
        sustenance_deplete_and_respawn(bf, node->id);
        e->claimedSustenanceNodeId = -1;
        e->farmerState = FARMER_RETURNING;
        // Swap to the full Cheffy variant BEFORE entity_set_state so the new
        // walk clip is driven by the full variant's spec row.
        farmer_apply_variant(e, gs, true);
        entity_set_state(e, ESTATE_WALKING);
        printf("[FARMER] Entity %d gathered sustenance (value=%d), returning to base\n",
               e->id, e->carriedSustenanceValue);
    } else {
        // More cycles needed — restart animation
        entity_restart_clip(e);
    }
}

static void farmer_return(Entity *e, GameState *gs, float deltaTime) {
    Entity *base = gs->players[e->ownerID].base;
    if (!base) {
        // Base destroyed — drop any stale reservation fields and idle.
        // The ring itself died with the base entity, no release to make.
        e->reservedDepositSlotIndex = -1;
        e->reservedDepositSlotKind = DEPOSIT_SLOT_NONE;
        e->movementTargetId = -1;
        entity_set_state(e, ESTATE_IDLE);
        return;
    }

    // Treat the home base as the farmer's current static target while
    // returning so local steering can use the smaller contact shell instead
    // of the full authored nav footprint.
    e->movementTargetId = base->id;

    // Promote a queue reservation the moment a primary slot opens up. This is
    // the only hand-off path; id-sorted update order ensures concurrent queue
    // farmers compete deterministically.
    if (e->reservedDepositSlotKind == DEPOSIT_SLOT_QUEUE) {
        int newIdx = -1;
        if (deposit_slots_try_promote(base, e->id,
                                      e->reservedDepositSlotIndex, &newIdx)) {
            e->reservedDepositSlotIndex = newIdx;
            e->reservedDepositSlotKind = DEPOSIT_SLOT_PRIMARY;
            // Restore the walk clip if this farmer was parked in queue idle.
            if (e->state == ESTATE_IDLE) {
                SpriteDirection dir = e->anim.dir;
                bool flipH = e->anim.flipH;
                entity_set_state(e, ESTATE_WALKING);
                e->anim.dir = dir;
                e->anim.flipH = flipH;
            }
        }
    }

    // Parked queue farmer waiting for promotion: stay in Idle Full, skip
    // steering entirely until try_promote above flips us back to WALKING.
    if (e->state == ESTATE_IDLE &&
        e->reservedDepositSlotKind == DEPOSIT_SLOT_QUEUE) {
        return;
    }

    // First-time reservation attempt from the farmer's current position.
    if (e->reservedDepositSlotKind == DEPOSIT_SLOT_NONE) {
        int idx = -1;
        DepositSlotKind kind = deposit_slots_reserve_for(base, e->id, e->position, &idx);
        if (kind == DEPOSIT_SLOT_NONE) {
            // All deposit tickets are taken. Stage outside the queue ring and
            // retry next tick instead of walking aimlessly at the base pivot.
            farmer_move_with_steering(e, gs, base_interaction_anchor(base),
                                      farmer_base_wait_radius(e, base), deltaTime);
            return;
        }
        e->reservedDepositSlotIndex = idx;
        e->reservedDepositSlotKind = kind;
    }

    Vector2 target = base_interaction_anchor(base);
    float arriveRadius = 0.0f;
    farmer_resolve_return_goal(e, base, &target, &arriveRadius);

    bool arrived = farmer_move_with_steering(e, gs, target, arriveRadius, deltaTime);

    // Only a primary-slot arrival promotes to DEPOSITING. Queue-slot arrivals
    // park the farmer in Idle Full and keep retrying try_promote each tick.
    if (arrived && e->reservedDepositSlotKind == DEPOSIT_SLOT_PRIMARY) {
        SpriteDirection dir = e->anim.dir;
        bool flipH = e->anim.flipH;
        e->farmerState = FARMER_DEPOSITING;
        // Reuse the full-variant authored idle sheet as a one-shot deposit
        // animation via the full variant's ATTACK spec row.
        entity_set_state(e, ESTATE_ATTACKING);
        // Preserve the return-facing when switching into the one-shot clip.
        e->anim.dir = dir;
        e->anim.flipH = flipH;
        printf("[FARMER] Entity %d reached deposit slot %d, depositing\n",
               e->id, e->reservedDepositSlotIndex);
    } else if (arrived && e->reservedDepositSlotKind == DEPOSIT_SLOT_QUEUE &&
               e->state != ESTATE_IDLE) {
        // First arrival at the queue wait slot: park in Idle Full.
        SpriteDirection dir = e->anim.dir;
        bool flipH = e->anim.flipH;
        entity_set_state(e, ESTATE_IDLE);
        e->anim.dir = dir;
        e->anim.flipH = flipH;
    }
}

static void farmer_deposit(Entity *e, GameState *gs, float deltaTime) {
    (void)deltaTime;
    Entity *base = gs->players[e->ownerID].base;
    if (base) {
        e->movementTargetId = base->id;
    }

    // Wait for one-shot attack clip to finish
    if (!e->anim.finished) return;

    // Deposit complete — funnel through the helper so progression stays in
    // sync on the same frame the counter increases.
    player_award_sustenance(gs, e->ownerID, e->carriedSustenanceValue);
    printf("[FARMER] Entity %d deposited %d sustenance (player %d bank: %d, lifetime: %d)\n",
           e->id, e->carriedSustenanceValue, e->ownerID,
           gs->players[e->ownerID].sustenanceBank,
           gs->players[e->ownerID].sustenanceCollected);

    // Release the primary deposit slot so the next waiting farmer can promote.
    base = gs->players[e->ownerID].base;
    if (base && e->reservedDepositSlotKind == DEPOSIT_SLOT_PRIMARY) {
        deposit_slots_release(base, DEPOSIT_SLOT_PRIMARY,
                              e->reservedDepositSlotIndex, e->id);
    }
    e->reservedDepositSlotIndex = -1;
    e->reservedDepositSlotKind = DEPOSIT_SLOT_NONE;

    e->carriedSustenanceValue = 0;
    e->movementTargetId = -1;
    e->farmerState = FARMER_SEEKING;
    // Swap back to the empty variant before transitioning so the idle clip
    // uses the empty sheet.
    farmer_apply_variant(e, gs, false);
    entity_set_state(e, ESTATE_IDLE);
}

bool farmer_debug_nav_goal(const Entity *e, const GameState *gs,
                           Vector2 *outGoal, float *outStopRadius) {
    if (!e || !gs || !outGoal || !outStopRadius) return false;

    switch (e->farmerState) {
        case FARMER_WALKING_TO_SUSTENANCE: {
            const SustenanceNode *node =
                sustenance_get_node((Battlefield *)&gs->battlefield,
                                    e->claimedSustenanceNodeId);
            if (!node || !node->active || node->claimedByEntityId != e->id) {
                return false;
            }
            *outGoal = node->worldPos.v;
            *outStopRadius = FARMER_SUSTENANCE_INTERACT_RADIUS;
            return true;
        }

        case FARMER_RETURNING: {
            const Entity *base = gs->players[e->ownerID].base;
            if (!base) return false;

            farmer_resolve_return_goal(e, base, outGoal, outStopRadius);
            return true;
        }

        default:
            return false;
    }
}

// --- Public API ---

void farmer_update(Entity *e, GameState *gs, float deltaTime) {
    if (!e || e->markedForRemoval || !e->alive) return;

    e->spriteRotationDegrees = farmer_sprite_rotation(e);

    // Tick animation for all living states
    anim_state_update(&e->anim, deltaTime);

    switch (e->farmerState) {
        case FARMER_SEEKING:        farmer_seek(e, gs);                break;
        case FARMER_WALKING_TO_SUSTENANCE: farmer_walk_to_sustenance(e, gs, deltaTime); break;
        case FARMER_GATHERING:      farmer_gather(e, gs, deltaTime);   break;
        case FARMER_RETURNING:      farmer_return(e, gs, deltaTime);   break;
        case FARMER_DEPOSITING:     farmer_deposit(e, gs, deltaTime);  break;
    }
}

void farmer_on_death(Entity *farmer, GameState *gs) {
    if (!farmer || !gs) return;

    Battlefield *bf = &gs->battlefield;

    // Award carried sustenance to the opposing player via the progression
    // helper so their base level/regen updates immediately.
    if (farmer->carriedSustenanceValue > 0) {
        int opponent = 1 - farmer->ownerID;
        player_award_sustenance(gs, opponent, farmer->carriedSustenanceValue);
        printf("[FARMER] Entity %d died carrying %d sustenance — awarded to player %d\n",
               farmer->id, farmer->carriedSustenanceValue, opponent);
        farmer->carriedSustenanceValue = 0;
    }

    // Release any sustenance claim
    sustenance_release_claims_for_entity(bf, farmer->id);
    farmer->claimedSustenanceNodeId = -1;

    // Release any deposit slot reservation held by this farmer. The owning
    // base may be NULL if it was destroyed on the same frame -- the ring died
    // with the base entity, so skipping the call is safe.
    Entity *base = gs->players[farmer->ownerID].base;
    if (base) {
        deposit_slots_release_for_entity(base, farmer->id);
    }
    farmer->reservedDepositSlotIndex = -1;
    farmer->reservedDepositSlotKind = DEPOSIT_SLOT_NONE;
}
