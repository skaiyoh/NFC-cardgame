//
// Farmer unit behavior implementation.
//
// State machine: SEEKING -> WALKING_TO_SUSTENANCE -> GATHERING -> RETURNING -> DEPOSITING -> repeat.
//

#include "farmer.h"
#include "pathfinding.h"
#include "../core/battlefield.h"
#include "../core/sustenance.h"
#include "../core/config.h"
#include "../entities/entities.h"
#include <math.h>
#include <stdio.h>

static float farmer_sprite_rotation(const Entity *e) {
    return (bf_side_for_player(e->ownerID) == SIDE_TOP) ? 180.0f : 0.0f;
}

// TODO: farmer_move_direct uses naive line-of-sight movement.
// Add collision avoidance / A* pathfinding when obstacles are implemented.
// TODO: FARMER_SUSTENANCE_INTERACT_RADIUS and FARMER_BASE_DEPOSIT_RADIUS are fixed
// constants. Make data-driven per unit or scale with sustenance node size later.
static bool farmer_move_direct(Entity *e, Vector2 target, float radius,
                               float deltaTime) {
    float dx = target.x - e->position.x;
    float dy = target.y - e->position.y;
    float dist = sqrtf(dx * dx + dy * dy);

    if (dist <= radius) return true;

    float step = e->moveSpeed * deltaTime;
    if (step >= dist) {
        e->position = target;
    } else {
        float inv = 1.0f / dist;
        e->position.x += dx * inv * step;
        e->position.y += dy * inv * step;
    }

    // Update facing using the owner's perspective, not raw world-Y.
    Vector2 diff = { target.x - e->position.x, target.y - e->position.y };
    BattleSide side = bf_side_for_player(e->ownerID);
    pathfind_apply_direction_for_side(&e->anim, diff, side);
    e->spriteRotationDegrees = farmer_sprite_rotation(e);

    return false;
}

// --- State handlers ---

static void farmer_seek(Entity *e, GameState *gs) {
    Battlefield *bf = &gs->battlefield;
    BattleSide side = bf_side_for_player(e->ownerID);

    SustenanceNode *node = sustenance_find_nearest_available(bf, side, e->position);
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

    bool arrived = farmer_move_direct(e, node->worldPos.v,
                                      FARMER_SUSTENANCE_INTERACT_RADIUS, deltaTime);
    if (arrived) {
        SpriteDirection dir = e->anim.dir;
        bool flipH = e->anim.flipH;
        e->farmerState = FARMER_GATHERING;
        // TODO: Farmer reuses ESTATE_ATTACKING (current farmer attack clip) as work animation.
        // Add ESTATE_WORKING when farmer-specific animations are available.
        entity_set_state(e, ESTATE_ATTACKING);
        // Preserve the approach-facing when switching into the placeholder work clip.
        e->anim.dir = dir;
        e->anim.flipH = flipH;
        printf("[FARMER] Entity %d arrived at sustenance node %d, gathering\n",
               e->id, node->id);
    }
}

static void farmer_gather(Entity *e, GameState *gs, float deltaTime) {
    (void)deltaTime;
    Battlefield *bf = &gs->battlefield;
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
        // Base destroyed — idle
        entity_set_state(e, ESTATE_IDLE);
        return;
    }

    bool arrived = farmer_move_direct(e, base->position,
                                      FARMER_BASE_DEPOSIT_RADIUS, deltaTime);
    if (arrived) {
        SpriteDirection dir = e->anim.dir;
        bool flipH = e->anim.flipH;
        e->farmerState = FARMER_DEPOSITING;
        // Reuse attack clip for deposit animation
        entity_set_state(e, ESTATE_ATTACKING);
        // Preserve the return-facing when switching into the placeholder work clip.
        e->anim.dir = dir;
        e->anim.flipH = flipH;
        printf("[FARMER] Entity %d reached base, depositing\n", e->id);
    }
}

static void farmer_deposit(Entity *e, GameState *gs, float deltaTime) {
    (void)deltaTime;
    // Wait for one-shot attack clip to finish
    if (!e->anim.finished) return;

    // Deposit complete
    gs->players[e->ownerID].sustenanceCollected += e->carriedSustenanceValue;
    printf("[FARMER] Entity %d deposited %d sustenance (player %d total: %d)\n",
           e->id, e->carriedSustenanceValue, e->ownerID,
           gs->players[e->ownerID].sustenanceCollected);
    e->carriedSustenanceValue = 0;
    e->farmerState = FARMER_SEEKING;
    entity_set_state(e, ESTATE_IDLE);
}

// --- Public API ---

void farmer_update(Entity *e, GameState *gs, float deltaTime) {
    if (!e || e->markedForRemoval) return;

    // Dead farmer: play death animation, then mark for removal.
    // This mirrors the generic ESTATE_DEAD branch in entity_update.
    if (!e->alive) {
        e->spriteRotationDegrees = farmer_sprite_rotation(e);
        AnimPlaybackEvent evt = anim_state_update(&e->anim, deltaTime);
        if (evt.finishedThisTick) {
            e->markedForRemoval = true;
        }
        return;
    }

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

    // Award carried sustenance to the opposing player
    if (farmer->carriedSustenanceValue > 0) {
        int opponent = 1 - farmer->ownerID;
        gs->players[opponent].sustenanceCollected += farmer->carriedSustenanceValue;
        printf("[FARMER] Entity %d died carrying %d sustenance — awarded to player %d\n",
               farmer->id, farmer->carriedSustenanceValue, opponent);
        farmer->carriedSustenanceValue = 0;
    }

    // Release any sustenance claim
    sustenance_release_claims_for_entity(bf, farmer->id);
    farmer->claimedSustenanceNodeId = -1;
}
