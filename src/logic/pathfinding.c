#include "pathfinding.h"
#include "combat.h"
#include "../core/config.h"
#include "../core/battlefield.h"
#include "../entities/entities.h"
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

static bool pathfind_compute_goal(Entity *e, const Battlefield *bf,
                                  Vector2 *outGoal, float *outStopRadius,
                                  bool *outIsWaypoint);

BattleSide pathfind_presentation_side_for_position(Vector2 position, float seamY) {
    CanonicalPos pos = { .v = position };
    return bf_side_for_pos(pos, seamY);
}

static float pathfind_entity_sprite_height(const Entity *e) {
    if (!e || !e->sprite) return 0.0f;

    const SpriteSheet *sheet = sprite_sheet_get(e->sprite, e->anim.anim);
    if (!sheet || sheet->frameHeight <= 0) return 0.0f;

    return (float)sheet->frameHeight * e->spriteScale;
}

static BattleSide pathfind_resolved_presentation_side(const Entity *e, const Battlefield *bf) {
    if (!e || !bf) return SIDE_BOTTOM;

    BattleSide resolvedSide = pathfind_presentation_side_for_position(e->position, bf->seamY);
    float spriteHeight = pathfind_entity_sprite_height(e);

    if (spriteHeight > 0.0f) {
        CanonicalPos pos = { .v = e->position };
        if (bf_crosses_seam(pos, spriteHeight, bf->seamY)) {
            resolvedSide = e->presentationSide;
        }
    }

    return resolvedSide;
}

void pathfind_sync_presentation(Entity *e, const Battlefield *bf) {
    if (!e || !bf) return;
    e->spriteRotationDegrees = pathfind_sprite_rotation_for_side(e->anim.dir, e->presentationSide);
}

void pathfind_commit_presentation(Entity *e, const Battlefield *bf) {
    if (!e || !bf) return;

    e->presentationSide = pathfind_resolved_presentation_side(e, bf);
    e->spriteRotationDegrees = pathfind_sprite_rotation_for_side(e->anim.dir, e->presentationSide);
}

void pathfind_face_goal(Entity *e, const Battlefield *bf, Vector2 goal) {
    if (!e || !bf) return;
    Vector2 diff = { goal.x - e->position.x, goal.y - e->position.y };
    float dist = sqrtf(diff.x * diff.x + diff.y * diff.y);
    if (dist <= 1.0f) {
        e->spriteRotationDegrees = pathfind_sprite_rotation_for_side(e->anim.dir,
                                                                     e->presentationSide);
        return;
    }
    pathfind_apply_direction_for_side(&e->anim, diff, e->presentationSide);
    e->spriteRotationDegrees = pathfind_sprite_rotation_for_side(e->anim.dir,
                                                                 e->presentationSide);
}

void pathfind_update_walk_facing(Entity *e, const Battlefield *bf) {
    if (!e || !bf) return;
    Vector2 goal;
    float stopRadius = 0.0f;
    bool goalIsWaypoint = false;
    if (pathfind_compute_goal(e, bf, &goal, &stopRadius, &goalIsWaypoint)) {
        pathfind_face_goal(e, bf, goal);
        return;
    }

    if (e->lane < 0 || e->lane >= 3) return;
    BattleSide ownerSide = bf_side_for_player(e->ownerID);
    BattleSide enemySide = (ownerSide == SIDE_BOTTOM) ? SIDE_TOP : SIDE_BOTTOM;
    pathfind_face_goal(e, bf, bf_base_anchor(bf, enemySide).v);
}

// --- Local steering helpers ---

static Vector2 pathfind_rotate_vec(Vector2 v, float degrees) {
    float rad = degrees * PI_F / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);
    return (Vector2){ v.x * c - v.y * s, v.x * s + v.y * c };
}

static Vector2 pathfind_normalize_or(Vector2 v, Vector2 fallback) {
    float lenSq = v.x * v.x + v.y * v.y;
    if (lenSq <= 0.0001f) return fallback;

    float invLen = 1.0f / sqrtf(lenSq);
    return (Vector2){ v.x * invLen, v.y * invLen };
}

static float pathfind_point_to_segment_dist(Vector2 p, Vector2 a, Vector2 b) {
    float abx = b.x - a.x;
    float aby = b.y - a.y;
    float apx = p.x - a.x;
    float apy = p.y - a.y;
    float abLenSq = abx * abx + aby * aby;

    if (abLenSq <= 0.0001f) {
        float dx = p.x - a.x;
        float dy = p.y - a.y;
        return sqrtf(dx * dx + dy * dy);
    }

    float t = (apx * abx + apy * aby) / abLenSq;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float closestX = a.x + t * abx;
    float closestY = a.y + t * aby;
    float dx = p.x - closestX;
    float dy = p.y - closestY;
    return sqrtf(dx * dx + dy * dy);
}

static float pathfind_segment_length(Vector2 a, Vector2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return sqrtf(dx * dx + dy * dy);
}

static float pathfind_lane_total_length_for_side(const Battlefield *bf, BattleSide side, int lane) {
    if (!bf) return 0.0f;
    if (lane < 0 || lane >= 3) return 0.0f;

    float total = 0.0f;
    for (int wp = 0; wp < LANE_WAYPOINT_COUNT - 1; wp++) {
        Vector2 a = bf_waypoint(bf, side, lane, wp).v;
        Vector2 b = bf_waypoint(bf, side, lane, wp + 1).v;
        total += pathfind_segment_length(a, b);
    }
    return total;
}

static float pathfind_lane_progress_for_position_on_side(const Battlefield *bf,
                                                         BattleSide side, int lane,
                                                         Vector2 position,
                                                         Vector2 *outClosest) {
    if (!bf) return 0.0f;
    if (lane < 0 || lane >= 3) return 0.0f;

    float bestDistSq = INFINITY;
    float bestProgress = 0.0f;
    Vector2 bestPoint = bf_waypoint(bf, side, lane, 0).v;
    float accumulated = 0.0f;

    for (int wp = 0; wp < LANE_WAYPOINT_COUNT - 1; wp++) {
        Vector2 a = bf_waypoint(bf, side, lane, wp).v;
        Vector2 b = bf_waypoint(bf, side, lane, wp + 1).v;
        float abx = b.x - a.x;
        float aby = b.y - a.y;
        float abLenSq = abx * abx + aby * aby;
        float t = 0.0f;
        Vector2 closest = a;

        if (abLenSq > 0.0001f) {
            float apx = position.x - a.x;
            float apy = position.y - a.y;
            t = (apx * abx + apy * aby) / abLenSq;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            closest.x = a.x + abx * t;
            closest.y = a.y + aby * t;
        }

        float dx = position.x - closest.x;
        float dy = position.y - closest.y;
        float distSq = dx * dx + dy * dy;
        float segmentLength = pathfind_segment_length(a, b);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestProgress = accumulated + segmentLength * t;
            bestPoint = closest;
        }

        accumulated += segmentLength;
    }

    if (outClosest) *outClosest = bestPoint;
    return bestProgress;
}

static Vector2 pathfind_lane_position_at_progress_on_side(const Battlefield *bf,
                                                          BattleSide side, int lane,
                                                          float progress) {
    if (!bf || lane < 0 || lane >= 3) {
        return (Vector2){ 0.0f, 0.0f };
    }

    if (progress <= 0.0f) {
        return bf_waypoint(bf, side, lane, 0).v;
    }

    float remaining = progress;
    for (int wp = 0; wp < LANE_WAYPOINT_COUNT - 1; wp++) {
        Vector2 a = bf_waypoint(bf, side, lane, wp).v;
        Vector2 b = bf_waypoint(bf, side, lane, wp + 1).v;
        float segmentLength = pathfind_segment_length(a, b);

        if (segmentLength <= 0.0001f) continue;
        if (remaining <= segmentLength) {
            float t = remaining / segmentLength;
            return (Vector2){
                a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t
            };
        }

        remaining -= segmentLength;
    }

    return bf_waypoint(bf, side, lane, LANE_WAYPOINT_COUNT - 1).v;
}

static int pathfind_waypoint_index_for_progress_on_side(const Battlefield *bf,
                                                        BattleSide side, int lane,
                                                        float progress) {
    if (!bf || lane < 0 || lane >= 3) return -1;

    float accumulated = 0.0f;
    for (int wp = 1; wp < LANE_WAYPOINT_COUNT; wp++) {
        Vector2 a = bf_waypoint(bf, side, lane, wp - 1).v;
        Vector2 b = bf_waypoint(bf, side, lane, wp).v;
        accumulated += pathfind_segment_length(a, b);
        if (accumulated > progress + 0.001f) {
            return wp;
        }
    }

    return LANE_WAYPOINT_COUNT;
}

float pathfind_lane_progress_for_position(const Entity *e, const Battlefield *bf,
                                          Vector2 position) {
    if (!e || !bf) return 0.0f;
    if (e->lane < 0 || e->lane >= 3) return 0.0f;

    BattleSide ownerSide = bf_side_for_player(e->ownerID);
    return pathfind_lane_progress_for_position_on_side(bf, ownerSide, e->lane, position, NULL);
}

void pathfind_sync_lane_progress(Entity *e, const Battlefield *bf) {
    if (!e || !bf) return;
    if (e->lane < 0 || e->lane >= 3) return;

    BattleSide ownerSide = bf_side_for_player(e->ownerID);
    float projected = pathfind_lane_progress_for_position_on_side(
        bf, ownerSide, e->lane, e->position, NULL
    );
    float total = pathfind_lane_total_length_for_side(bf, ownerSide, e->lane);

    if (projected > e->laneProgress) {
        e->laneProgress = projected;
    }
    if (e->laneProgress < 0.0f) e->laneProgress = 0.0f;
    if (e->laneProgress > total) e->laneProgress = total;

    e->waypointIndex = pathfind_waypoint_index_for_progress_on_side(
        bf, ownerSide, e->lane, e->laneProgress
    );
}

typedef struct {
    bool legal;
    float goalDist;
    float progress;
    float minHardClearance;
    float softOverlap;
    float flowPreference;
    float angleDegrees;
    int lateralSign;
} PathfindCandidateEval;

typedef struct {
    float maxGoalDist;
    bool enforceLaneCorridor;
    bool allowJamRelief;
    int jamReliefTicks;
} PathfindStepParams;

// True if `other` counts as a blocker for `self` during candidate evaluation.
static bool pathfind_is_blocker(const Entity *self, const Entity *other) {
    if (!other || other == self) return false;
    if (!other->alive || other->markedForRemoval) return false;
    if (other->type == ENTITY_PROJECTILE) return false;
    return true;
}

// Effective pathfinding footprint. Entities can author a dedicated nav radius
// (bases do, to decouple nav overlap from combat contact geometry); anything
// else falls back to the body radius used by spawn placement and combat.
static float pathfind_nav_radius(const Entity *e) {
    if (!e) return 0.0f;
    return (e->navRadius > 0.0f) ? e->navRadius : e->bodyRadius;
}

static bool pathfind_is_current_static_target(const Entity *self, const Entity *other) {
    if (!self || !other) return false;
    if (self->movementTargetId != other->id) return false;
    return other->type == ENTITY_BUILDING || other->navProfile == NAV_PROFILE_STATIC;
}

static float pathfind_blocker_radius_for_self(const Entity *self, const Entity *other) {
    if (!other) return 0.0f;
    if (pathfind_is_current_static_target(self, other)) {
        return combat_target_contact_radius(other);
    }
    return pathfind_nav_radius(other);
}

static const Entity *pathfind_contact_cloud_target(const Entity *e, const Battlefield *bf) {
    if (!e || !bf || e->movementTargetId < 0) return NULL;

    const Entity *target = bf_find_entity((Battlefield *)bf, e->movementTargetId);
    if (!target || !target->alive || target->markedForRemoval) return NULL;
    if (target->type != ENTITY_BUILDING && target->navProfile != NAV_PROFILE_STATIC) {
        return NULL;
    }
    return target;
}

static float pathfind_contact_cloud_radius_for_entity(const Entity *e, const Entity *target) {
    if (!e || !target) return 0.0f;

    if (e->reservedDepositSlotKind == DEPOSIT_SLOT_PRIMARY) {
        return pathfind_nav_radius(target) + pathfind_nav_radius(e) + BASE_DEPOSIT_SLOT_GAP;
    }

    return combat_static_target_occupancy_radius(e, target);
}

static bool pathfind_same_contact_cloud_pair(const Entity *self, const Entity *other,
                                             const Entity *target) {
    if (!self || !other || !target) return false;
    if (self->ownerID != other->ownerID) return false;
    if (other->movementTargetId != target->id) return false;

    bool sameAssaultCloud = (self->navProfile == NAV_PROFILE_ASSAULT &&
                             other->navProfile == NAV_PROFILE_ASSAULT);
    bool sameDepositCloud = (self->reservedDepositSlotKind == DEPOSIT_SLOT_PRIMARY &&
                             other->reservedDepositSlotKind == DEPOSIT_SLOT_PRIMARY);

    return sameAssaultCloud || sameDepositCloud;
}

static bool pathfind_allows_contact_cloud_overlap(const Entity *self, Vector2 candidate,
                                                  const Entity *other,
                                                  const Battlefield *bf,
                                                  float *outSoftPenaltyScale) {
    if (outSoftPenaltyScale) *outSoftPenaltyScale = 0.0f;
    const Entity *target = pathfind_contact_cloud_target(self, bf);
    if (!target) return false;
    if (!pathfind_same_contact_cloud_pair(self, other, target)) return false;

    float selfCloud = pathfind_contact_cloud_radius_for_entity(self, target);
    float otherCloud = pathfind_contact_cloud_radius_for_entity(other, target);
    if (selfCloud <= 0.0f || otherCloud <= 0.0f) return false;

    float selfDx = candidate.x - target->position.x;
    float selfDy = candidate.y - target->position.y;
    float selfDist = sqrtf(selfDx * selfDx + selfDy * selfDy);
    float otherDx = other->position.x - target->position.x;
    float otherDy = other->position.y - target->position.y;
    float otherDist = sqrtf(otherDx * otherDx + otherDy * otherDy);

    bool depositCloud = (self->reservedDepositSlotKind == DEPOSIT_SLOT_PRIMARY &&
                         other->reservedDepositSlotKind == DEPOSIT_SLOT_PRIMARY);
    float selfOverlapRadius = selfCloud;
    float otherOverlapRadius = otherCloud;
    if (depositCloud) {
        selfOverlapRadius += pathfind_nav_radius(self);
        otherOverlapRadius += pathfind_nav_radius(other);
    } else {
        // Same-target base attackers need an extra ally shell beyond the core
        // occupancy radius so late arrivals can merge into the packed assault
        // blob instead of stalling just outside it.
        selfOverlapRadius += pathfind_nav_radius(other);
        otherOverlapRadius += pathfind_nav_radius(self);
    }
    bool insideCloud = selfDist <= selfOverlapRadius && otherDist <= otherOverlapRadius;
    if (!insideCloud) return false;

    if (!depositCloud && outSoftPenaltyScale) {
        *outSoftPenaltyScale = PATHFIND_ASSAULT_CLOUD_SOFT_OVERLAP_SCALE;
    }
    return true;
}

// Resolve the entity's current steering goal.
// Out-params: *outGoal receives the goal position; *outIsWaypoint is true
// when the goal is the lane-following steering point, false when the goal is a
// movementTarget engagement point.
// If movementTargetId is set but the target is invalid, it is cleared here.
// Returns false when there is no remaining steering goal (lane exhausted and
// no valid pursuit target).
static bool pathfind_compute_goal(Entity *e, const Battlefield *bf,
                                  Vector2 *outGoal, float *outStopRadius,
                                  bool *outIsWaypoint) {
    if (!e || !bf || !outGoal || !outStopRadius || !outIsWaypoint) return false;

    if (e->movementTargetId != -1) {
        Entity *target = bf_find_entity((Battlefield *)bf, e->movementTargetId);
        if (target && target->alive && !target->markedForRemoval) {
            if (combat_engagement_goal(e, target, bf, outGoal, outStopRadius)) {
                *outIsWaypoint = false;
                return true;
            }
        }
        e->movementTargetId = -1;
    }

    if (e->lane < 0 || e->lane >= 3) return false;

    BattleSide ownerSide = bf_side_for_player(e->ownerID);
    pathfind_sync_lane_progress(e, bf);

    float laneTotal = pathfind_lane_total_length_for_side(bf, ownerSide, e->lane);
    if (laneTotal <= 0.0f) return false;
    if (e->laneProgress >= laneTotal - PATHFIND_WAYPOINT_REACH_GAP) return false;

    float goalProgress = e->laneProgress + PATHFIND_LANE_LOOKAHEAD_DISTANCE;
    if (goalProgress > laneTotal) goalProgress = laneTotal;
    *outGoal = pathfind_lane_position_at_progress_on_side(bf, ownerSide, e->lane, goalProgress);
    *outStopRadius = 0.0f;
    *outIsWaypoint = true;
    return true;
}

static bool pathfind_candidate_in_bounds(const Entity *e, const Battlefield *bf,
                                         Vector2 candidate) {
    if (!e || !bf) return false;

    float r = pathfind_nav_radius(e);
    return candidate.x - r >= 0.0f &&
           candidate.x + r <= bf->boardWidth &&
           candidate.y - r >= 0.0f &&
           candidate.y + r <= bf->boardHeight;
}

static bool pathfind_candidate_within_lane_corridor(const Entity *e,
                                                    const Battlefield *bf,
                                                    Vector2 candidate) {
    if (!e || !bf) return false;
    if (e->lane < 0 || e->lane >= 3) return false;

    BattleSide ownerSide = bf_side_for_player(e->ownerID);
    float laneWidth = bf->boardWidth / 3.0f;
    float maxDist = laneWidth * PATHFIND_LANE_DRIFT_MAX_RATIO;
    float bestDist = INFINITY;

    for (int wp = 0; wp < LANE_WAYPOINT_COUNT - 1; wp++) {
        Vector2 a = bf_waypoint(bf, ownerSide, e->lane, wp).v;
        Vector2 b = bf_waypoint(bf, ownerSide, e->lane, wp + 1).v;
        float dist = pathfind_point_to_segment_dist(candidate, a, b);
        if (dist < bestDist) bestDist = dist;
    }

    return bestDist <= maxDist;
}

static float pathfind_candidate_goal_dist(Vector2 goal, Vector2 candidate) {
    float dx = goal.x - candidate.x;
    float dy = goal.y - candidate.y;
    return sqrtf(dx * dx + dy * dy);
}

static float pathfind_clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static float pathfind_static_target_flow_outer_radius(const Entity *e, const Entity *target) {
    if (!e || !target) return 0.0f;
    return combat_static_target_occupancy_radius(e, target) + pathfind_nav_radius(e);
}

static bool pathfind_is_soft_blocker(const Entity *self, const Entity *other) {
    if (!pathfind_is_blocker(self, other)) return false;
    if (self->ownerID != other->ownerID) return false;
    if (self->type != ENTITY_TROOP || other->type != ENTITY_TROOP) return false;
    if (other->navProfile == NAV_PROFILE_STATIC) return false;
    bool sameTargetAttackers = (self->movementTargetId != -1 &&
                                self->movementTargetId == other->movementTargetId);
    if (!sameTargetAttackers && other->moveSpeed <= 0.0f) return false;
    return true;
}

static bool pathfind_is_same_target_attack_pair(const Entity *self, const Entity *other) {
    if (!self || !other) return false;
    if (self->movementTargetId == -1) return false;
    return self->movementTargetId == other->movementTargetId;
}

static float pathfind_soft_overlap_allowance(const Entity *self, const Entity *other) {
    float ratio = PATHFIND_ALLY_SOFT_OVERLAP_RATIO;
    float maxAllowance = PATHFIND_ALLY_SOFT_OVERLAP_MAX;
    bool sameTargetAttackers = pathfind_is_same_target_attack_pair(self, other);

    if (self && other &&
        (self->navProfile == NAV_PROFILE_ASSAULT ||
         other->navProfile == NAV_PROFILE_ASSAULT ||
         sameTargetAttackers)) {
        ratio = PATHFIND_ASSAULT_ALLY_SOFT_OVERLAP_RATIO;
        maxAllowance = PATHFIND_ASSAULT_ALLY_SOFT_OVERLAP_MAX;
    }

    float allowance = (pathfind_nav_radius(self) + pathfind_nav_radius(other)) * ratio;
    if (allowance > maxAllowance) {
        allowance = maxAllowance;
    }

    if (sameTargetAttackers) {
        allowance += PATHFIND_ASSAULT_SAME_TARGET_SOFT_OVERLAP_BONUS;
        if (allowance > PATHFIND_ASSAULT_SAME_TARGET_SOFT_OVERLAP_MAX) {
            allowance = PATHFIND_ASSAULT_SAME_TARGET_SOFT_OVERLAP_MAX;
        }
    }
    return allowance;
}

static int pathfind_candidate_lateral_sign(Vector2 forward, Vector2 stepDir) {
    float cross = forward.x * stepDir.y - forward.y * stepDir.x;
    if (cross > 0.001f) return 1;
    if (cross < -0.001f) return -1;
    return 0;
}

static bool pathfind_static_target_flow_active(const Entity *e, const Battlefield *bf) {
    if (!e || !bf || e->navProfile != NAV_PROFILE_ASSAULT) return false;

    const Entity *target = pathfind_contact_cloud_target(e, bf);
    if (!target) return false;

    float flowOuterRadius = pathfind_static_target_flow_outer_radius(e, target);
    if (flowOuterRadius <= 0.0f) return false;

    float dx = e->position.x - target->position.x;
    float dy = e->position.y - target->position.y;
    float dist = sqrtf(dx * dx + dy * dy);
    return dist <= flowOuterRadius;
}

static bool pathfind_static_target_flow_candidates(const Entity *e, const Battlefield *bf,
                                                   Vector2 forward, Vector2 *outBlendDir,
                                                   Vector2 *outFlowDir) {
    if (!outBlendDir || !outFlowDir) return false;
    if (!pathfind_static_target_flow_active(e, bf)) return false;

    const Entity *target = pathfind_contact_cloud_target(e, bf);
    if (!target) return false;

    Vector2 flowDir = combat_static_target_flow_direction(e, target);
    float flowCross = forward.x * flowDir.y - forward.y * flowDir.x;
    if (fabsf(flowCross) <= 0.001f) return false;

    float signedAngle = combat_static_target_flow_angle_degrees(e, target);
    if (flowCross < 0.0f) signedAngle = -signedAngle;

    *outFlowDir = pathfind_rotate_vec(forward, signedAngle);
    *outBlendDir = pathfind_normalize_or(
        (Vector2){ forward.x + outFlowDir->x, forward.y + outFlowDir->y },
        *outFlowDir
    );
    return true;
}

static float pathfind_static_target_flow_preference(const Entity *e, Vector2 candidate,
                                                    Vector2 forward, Vector2 stepDir,
                                                    const Battlefield *bf) {
    if (!e || !bf || e->navProfile != NAV_PROFILE_ASSAULT) return 0.0f;

    const Entity *target = pathfind_contact_cloud_target(e, bf);
    if (!target) return 0.0f;

    float attackRadius = combat_static_target_attack_radius(e, target);
    float flowOuterRadius = pathfind_static_target_flow_outer_radius(e, target);
    float flowWidth = flowOuterRadius - attackRadius;
    if (flowWidth <= 0.001f) return 0.0f;

    float dx = candidate.x - target->position.x;
    float dy = candidate.y - target->position.y;
    float candidateDist = sqrtf(dx * dx + dy * dy);
    if (candidateDist > flowOuterRadius) return 0.0f;

    float ramp = pathfind_clamp01((flowOuterRadius - candidateDist) / flowWidth);

    Entity attackerView = *e;
    attackerView.position = candidate;
    Vector2 desiredDir = combat_static_target_flow_direction(&attackerView, target);
    float desiredCross = forward.x * desiredDir.y - forward.y * desiredDir.x;
    float candidateCross = forward.x * stepDir.y - forward.y * stepDir.x;
    if (desiredCross * candidateCross <= 0.0f) return 0.0f;

    float candidateForward = forward.x * stepDir.x + forward.y * stepDir.y;
    if (candidateForward <= 0.0f) return 0.0f;

    return fabsf(candidateCross) * fabsf(desiredCross) * candidateForward * ramp;
}

static bool pathfind_evaluate_candidate(const Entity *e, Vector2 goal,
                                        Vector2 candidate, const Battlefield *bf,
                                        const PathfindStepParams *params,
                                        PathfindCandidateEval *outEval) {
    if (!e || !bf || !params || !outEval) return false;

    outEval->legal = false;
    outEval->goalDist = INFINITY;
    outEval->progress = -INFINITY;
    outEval->minHardClearance = 1000000.0f;
    outEval->softOverlap = 0.0f;
    outEval->flowPreference = 0.0f;
    outEval->angleDegrees = 180.0f;
    outEval->lateralSign = 0;

    if (!pathfind_candidate_in_bounds(e, bf, candidate)) return false;
    if (params->enforceLaneCorridor &&
        !pathfind_candidate_within_lane_corridor(e, bf, candidate)) return false;

    float goalDist = pathfind_candidate_goal_dist(goal, candidate);
    if (goalDist > params->maxGoalDist) return false;

    float toGoalX = goal.x - e->position.x;
    float toGoalY = goal.y - e->position.y;
    float currentGoalDist = sqrtf(toGoalX * toGoalX + toGoalY * toGoalY);
    if (currentGoalDist <= 0.0001f) return false;

    Vector2 forward = { toGoalX / currentGoalDist, toGoalY / currentGoalDist };
    Vector2 stepVec = { candidate.x - e->position.x, candidate.y - e->position.y };
    float stepLen = sqrtf(stepVec.x * stepVec.x + stepVec.y * stepVec.y);
    if (stepLen <= 0.0001f) return false;
    Vector2 stepDir = { stepVec.x / stepLen, stepVec.y / stepLen };

    float selfRadius = pathfind_nav_radius(e);
    for (int i = 0; i < bf->entityCount; i++) {
        const Entity *other = bf->entities[i];
        if (!pathfind_is_blocker(e, other)) continue;

        float dx = other->position.x - candidate.x;
        float dy = other->position.y - candidate.y;
        float centerDist = sqrtf(dx * dx + dy * dy);
        float hardShell = selfRadius + pathfind_blocker_radius_for_self(e, other) +
                          PATHFIND_CONTACT_GAP;
        float contactCloudPenaltyScale = 0.0f;
        if (pathfind_allows_contact_cloud_overlap(e, candidate, other, bf,
                                                  &contactCloudPenaltyScale)) {
            float overlap = hardShell - centerDist;
            if (overlap > 0.0f) {
                outEval->softOverlap += overlap * contactCloudPenaltyScale;
            }
            continue;
        }

        bool softBlocker = pathfind_is_soft_blocker(e, other);
        float legalShell = hardShell;
        if (softBlocker) {
            legalShell -= pathfind_soft_overlap_allowance(e, other);
        }

        if (centerDist < legalShell) {
            float curDx = other->position.x - e->position.x;
            float curDy = other->position.y - e->position.y;
            float currentDist = sqrtf(curDx * curDx + curDy * curDy);
            if (currentDist >= legalShell - 0.001f) return false;
            if (centerDist < currentDist - 0.001f) return false;
        }

        if (softBlocker) {
            float overlap = hardShell - centerDist;
            if (overlap > 0.0f) outEval->softOverlap += overlap;
            continue;
        }

        float clearance = centerDist - hardShell;
        if (clearance < outEval->minHardClearance) {
            outEval->minHardClearance = clearance;
        }
    }

    float dot = forward.x * stepDir.x + forward.y * stepDir.y;
    if (dot < -1.0f) dot = -1.0f;
    if (dot > 1.0f) dot = 1.0f;

    outEval->legal = true;
    outEval->goalDist = goalDist;
    outEval->progress = currentGoalDist - goalDist;
    outEval->flowPreference = pathfind_static_target_flow_preference(e, candidate, forward,
                                                                     stepDir, bf);
    outEval->angleDegrees = acosf(dot) * 180.0f / PI_F;
    outEval->lateralSign = pathfind_candidate_lateral_sign(forward, stepDir);
    return true;
}

static float pathfind_candidate_score(const Entity *e,
                                      const PathfindCandidateEval *eval,
                                      bool jamRelief) {
    float progressWeight = 18.0f;
    float clearanceWeight = 0.7f;
    float softPenaltyWeight = 5.0f;
    float flowWeight = 0.0f;
    float angleWeight = 0.08f;

    if (e->navProfile == NAV_PROFILE_ASSAULT) {
        progressWeight = 13.0f;
        clearanceWeight = 1.2f;
        softPenaltyWeight = 1.6f;
        flowWeight = PATHFIND_ASSAULT_CLOUD_FLOW_WEIGHT;
        angleWeight = 0.05f;
    } else if (e->navProfile == NAV_PROFILE_FREE_GOAL) {
        progressWeight = 11.0f;
        clearanceWeight = 1.0f;
        softPenaltyWeight = 3.0f;
        angleWeight = 0.05f;
    }

    if (jamRelief) {
        progressWeight *= 0.55f;
        clearanceWeight *= 1.75f;
        flowWeight *= 0.75f;
        angleWeight *= 0.5f;
    }

    float continuityBonus = 0.0f;
    if (e->lastSteerSideSign != 0 &&
        eval->lateralSign != 0 &&
        eval->lateralSign == e->lastSteerSideSign) {
        continuityBonus = jamRelief ? 1.25f : 0.75f;
    }

    float cappedClearance = eval->minHardClearance;
    if (cappedClearance > 32.0f) cappedClearance = 32.0f;

    return eval->progress * progressWeight +
           cappedClearance * clearanceWeight -
           eval->softOverlap * softPenaltyWeight -
           eval->angleDegrees * angleWeight +
           eval->flowPreference * flowWeight +
           continuityBonus;
}

static bool pathfind_select_best_candidate(const Entity *e, Vector2 goal,
                                           const Battlefield *bf,
                                           const Vector2 *directions,
                                           int directionCount,
                                           float probeStep,
                                           const PathfindStepParams *params,
                                           bool jamRelief,
                                           Vector2 *outCandidate,
                                           int *outLateralSign) {
    if (!e || !bf || !directions || directionCount <= 0 || !params ||
        !outCandidate || !outLateralSign) {
        return false;
    }

    bool found = false;
    Vector2 bestCandidate = e->position;
    float bestScore = -INFINITY;
    float bestGoalDist = INFINITY;
    int bestLateralSign = 0;

    for (int i = 0; i < directionCount; i++) {
        Vector2 candidate = {
            e->position.x + directions[i].x * probeStep,
            e->position.y + directions[i].y * probeStep
        };
        PathfindCandidateEval eval;
        if (!pathfind_evaluate_candidate(e, goal, candidate, bf, params, &eval) ||
            !eval.legal) {
            continue;
        }

        float score = pathfind_candidate_score(e, &eval, jamRelief);
        if (!found ||
            score > bestScore + 0.001f ||
            (fabsf(score - bestScore) <= 0.001f && eval.goalDist < bestGoalDist)) {
            found = true;
            bestCandidate = candidate;
            bestScore = score;
            bestGoalDist = eval.goalDist;
            bestLateralSign = eval.lateralSign;
        }
    }

    if (!found) return false;

    *outCandidate = bestCandidate;
    *outLateralSign = bestLateralSign;
    return true;
}

static bool pathfind_try_normal_march(Entity *e, Vector2 goal, const Battlefield *bf,
                                      float step, const PathfindStepParams *params,
                                      int *outLateralSign) {
    float dx = goal.x - e->position.x;
    float dy = goal.y - e->position.y;
    float goalDist = sqrtf(dx * dx + dy * dy);
    if (goalDist < 0.001f) return false;

    Vector2 forward = { dx / goalDist, dy / goalDist };
    bool preferLeft = (e->lastSteerSideSign != 0)
        ? (e->lastSteerSideSign < 0)
        : ((e->id & 1) == 0);
    float softSign = preferLeft ? -1.0f : 1.0f;
    float hardSign = preferLeft ? -1.0f : 1.0f;
    float sideSign = preferLeft ? -1.0f : 1.0f;

    Vector2 directions[9];
    int directionCount = 0;
    directions[directionCount++] = forward;
    Vector2 flowBlendDir;
    Vector2 flowDir;
    if (pathfind_static_target_flow_candidates(e, bf, forward, &flowBlendDir, &flowDir)) {
        directions[directionCount++] = flowBlendDir;
        directions[directionCount++] = flowDir;
    }
    directions[directionCount++] = pathfind_rotate_vec(forward,  softSign * PATHFIND_CANDIDATE_ANGLE_SOFT_DEG);
    directions[directionCount++] = pathfind_rotate_vec(forward, -softSign * PATHFIND_CANDIDATE_ANGLE_SOFT_DEG);
    directions[directionCount++] = pathfind_rotate_vec(forward,  hardSign * PATHFIND_CANDIDATE_ANGLE_HARD_DEG);
    directions[directionCount++] = pathfind_rotate_vec(forward, -hardSign * PATHFIND_CANDIDATE_ANGLE_HARD_DEG);
    directions[directionCount++] = pathfind_rotate_vec(forward,  sideSign * PATHFIND_CANDIDATE_ANGLE_SIDE_DEG);
    directions[directionCount++] = pathfind_rotate_vec(forward, -sideSign * PATHFIND_CANDIDATE_ANGLE_SIDE_DEG);

    Vector2 bestCandidate;
    int lateralSign = 0;
    if (!pathfind_select_best_candidate(e, goal, bf, directions, directionCount, step,
                                        params, false, &bestCandidate,
                                        &lateralSign)) {
        return false;
    }

    e->position = bestCandidate;
    if (outLateralSign) *outLateralSign = lateralSign;
    return true;
}

static bool pathfind_try_jam_relief(Entity *e, Vector2 goal, const Battlefield *bf,
                                    float step, float goalDist,
                                    const PathfindStepParams *params,
                                    int *outLateralSign) {
    if (!params->allowJamRelief) return false;
    if (e->ticksSinceProgress < params->jamReliefTicks) return false;

    float dx = goal.x - e->position.x;
    float dy = goal.y - e->position.y;
    Vector2 forward = { dx / goalDist, dy / goalDist };
    bool preferLeft = (e->lastSteerSideSign != 0)
        ? (e->lastSteerSideSign < 0)
        : ((e->id & 1) == 0);
    float sideSign = preferLeft ? -1.0f : 1.0f;
    float escapeSign = preferLeft ? -1.0f : 1.0f;

    Vector2 directions[4];
    directions[0] = pathfind_rotate_vec(forward,  sideSign * PATHFIND_CANDIDATE_ANGLE_SIDE_DEG);
    directions[1] = pathfind_rotate_vec(forward, -sideSign * PATHFIND_CANDIDATE_ANGLE_SIDE_DEG);
    directions[2] = pathfind_rotate_vec(forward,  escapeSign * PATHFIND_CANDIDATE_ANGLE_ESCAPE_DEG);
    directions[3] = pathfind_rotate_vec(forward, -escapeSign * PATHFIND_CANDIDATE_ANGLE_ESCAPE_DEG);

    float baseProbeStep = fmaxf(step, pathfind_nav_radius(e));
    static const float probeStepMultipliers[] = { 1.0f, 2.0f, 3.0f };
    for (int i = 0; i < (int)(sizeof(probeStepMultipliers) / sizeof(probeStepMultipliers[0])); i++) {
        float probeStep = baseProbeStep * probeStepMultipliers[i];
        PathfindStepParams jamParams = *params;
        jamParams.maxGoalDist = goalDist + PATHFIND_ESCAPE_BACKTRACK_STEP_RATIO * probeStep;

        Vector2 bestCandidate;
        int lateralSign = 0;
        if (pathfind_select_best_candidate(e, goal, bf, directions, 4, probeStep,
                                           &jamParams, true, &bestCandidate,
                                           &lateralSign)) {
            e->position = bestCandidate;
            if (outLateralSign) *outLateralSign = lateralSign;
            return true;
        }
    }

    return false;
}

// Try one obstacle-aware step toward `goal`. Updates position, facing, and
// ticksSinceProgress. Returns true if a candidate was accepted and the entity
// moved; false if nothing was valid.
//
// Dispatches on e->navProfile:
//  LANE       -- strict monotone progress; jam relief only if allowJamRelief
//                and the lane-corridor drift check keeps candidates on the
//                authored path. Byte-identical to the pre-Phase-2 behavior.
//  FREE_GOAL  -- tangential side-steps are legal up to a per-step lateral
//                slack so the entity can orbit blockers on the way to its
//                goal. Jam relief is always enabled and uses no corridor
//                drift check (free movers aren't bound to a lane polyline).
//  STATIC     -- no motion; returns false and keeps ticksSinceProgress ticking.
//
// The `allowJamRelief` and `enforceLaneCorridor` parameters only apply to
// the LANE branch; FREE_GOAL overrides them with its own authoritative
// values so farmer callers (which pass both as false) still get jam relief.
static bool pathfind_try_step_toward(Entity *e, Vector2 goal, float stopRadius,
                                     const Battlefield *bf, float deltaTime,
                                     bool allowJamRelief,
                                     bool enforceLaneCorridor) {
    const float arriveEpsilon = 0.01f;
    if (!e || e->navProfile == NAV_PROFILE_STATIC) {
        if (e) e->ticksSinceProgress++;
        return false;
    }

    float dx = goal.x - e->position.x;
    float dy = goal.y - e->position.y;
    float goalDist = sqrtf(dx * dx + dy * dy);
    if (goalDist <= stopRadius + arriveEpsilon) {
        e->ticksSinceProgress = 0;
        pathfind_sync_presentation(e, bf);
        pathfind_face_goal(e, bf, goal);
        return false;
    }

    bool moved = false;
    int lateralSign = 0;

    if (goalDist > 0.001f) {
        float step = e->moveSpeed * deltaTime;
        // Cap step so the entity doesn't hop through the goal in a single frame.
        float stepBudget = goalDist - stopRadius;
        if (stepBudget < 0.0f) stepBudget = 0.0f;
        if (step > stepBudget) step = stepBudget;
        if (step > 0.0f) {
            PathfindStepParams params = {
                .maxGoalDist = goalDist - 0.001f,
                .enforceLaneCorridor = enforceLaneCorridor,
                .allowJamRelief = allowJamRelief,
                .jamReliefTicks = PATHFIND_JAM_RELIEF_TICKS
            };

            if (e->navProfile == NAV_PROFILE_ASSAULT) {
                float lateralSlack = step * PATHFIND_ASSAULT_LATERAL_TOLERANCE_RATIO;
                params.maxGoalDist = goalDist + lateralSlack;
                params.enforceLaneCorridor = false;
                params.allowJamRelief = true;
                params.jamReliefTicks = PATHFIND_ASSAULT_JAM_RELIEF_TICKS;
                moved = pathfind_try_normal_march(e, goal, bf, step, &params, &lateralSign);
                if (!moved) {
                    moved = pathfind_try_jam_relief(e, goal, bf, step, goalDist,
                                                    &params, &lateralSign);
                }
            } else if (e->navProfile == NAV_PROFILE_FREE_GOAL) {
                float lateralSlack = step * PATHFIND_FREE_MOVER_LATERAL_TOLERANCE_RATIO;
                params.maxGoalDist = goalDist + lateralSlack;
                params.enforceLaneCorridor = false;
                params.allowJamRelief = true;
                moved = pathfind_try_normal_march(e, goal, bf, step, &params, &lateralSign);
                if (!moved) {
                    moved = pathfind_try_jam_relief(e, goal, bf, step, goalDist,
                                                    &params, &lateralSign);
                }
            } else {
                moved = pathfind_try_normal_march(e, goal, bf, step, &params, &lateralSign);
                if (!moved && params.allowJamRelief) {
                    moved = pathfind_try_jam_relief(e, goal, bf, step, goalDist,
                                                    &params, &lateralSign);
                }
            }
        }
    }

    if (moved) {
        e->ticksSinceProgress = 0;
        if (lateralSign != 0) {
            e->lastSteerSideSign = lateralSign;
        }
        pathfind_sync_presentation(e, bf);
        pathfind_face_goal(e, bf, goal);
    } else {
        e->ticksSinceProgress++;
    }
    return moved;
}

// Phase 3d: free-goal flow stepper for farmers and any other free-mover.
//
// Builds (or fetches) a free-goal flow field centered on `goal` with
// stopRadius-sized seed disk from the caller's own side perspective, then
// sub-steps the entity along the bilinear-sampled flow direction. Same
// sub-step hard-blocker semantics as the lane/target flow steppers.
// Returns true when the stepper either consumed the tick cleanly or
// determined the entity has arrived; false if nav is unavailable and the
// caller should fall back to the old candidate fan.
static bool pathfind_step_free_goal_flow(Entity *e, Vector2 goal,
                                           float stopRadius, NavFrame *nav,
                                           const Battlefield *bf,
                                           float deltaTime) {
    if (!nav) return false;
    int side = (e->ownerID == 0) ? 0 : 1;
    const NavField *field =
        nav_get_or_build_free_goal_field(nav, bf, goal.x, goal.y, stopRadius, side);
    if (!field) return false;

    float fx = 0.0f, fy = 0.0f;
    nav_sample_flow(field, e->position.x, e->position.y, &fx, &fy);
    if (fx == 0.0f && fy == 0.0f) {
        // No flow: either already inside the seed disk (arrived) or
        // standing in a fully-blocked neighborhood. The arrival check
        // in the outer move_toward_goal covers the first case; for the
        // second, stall and let the outer caller re-tick next frame.
        e->ticksSinceProgress++;
        pathfind_face_goal(e, bf, goal);
        return true;
    }

    float totalStep = e->moveSpeed * deltaTime;
    if (totalStep < 0.0f) totalStep = 0.0f;
    float maxSub = (float)NAV_CELL_SIZE * 0.5f;
    int32_t subCount = 1;
    if (totalStep > maxSub) {
        subCount = (int32_t)ceilf(totalStep / maxSub);
    }
    float subLen = (subCount > 0) ? totalStep / (float)subCount : 0.0f;

    float posX = e->position.x;
    float posY = e->position.y;
    bool moved = false;
    for (int32_t s = 0; s < subCount; ++s) {
        float tryX = posX + fx * subLen;
        float tryY = posY + fy * subLen;
        int32_t tryCell = nav_cell_index_for_world(tryX, tryY);
        if (field->hardBlocked[tryCell]) break;
        // Arrival: if we would cross into the stopRadius, clamp and
        // stop. Saves a frame of wiggling at the ring boundary.
        float dxGoal = tryX - goal.x;
        float dyGoal = tryY - goal.y;
        if (dxGoal * dxGoal + dyGoal * dyGoal <= stopRadius * stopRadius) {
            posX = tryX;
            posY = tryY;
            moved = true;
            break;
        }
        posX = tryX;
        posY = tryY;
        moved = true;
    }
    if (!moved) {
        e->ticksSinceProgress++;
        pathfind_face_goal(e, bf, goal);
        return true;
    }

    if (posX < 0.0f) posX = 0.0f;
    if (posY < 0.0f) posY = 0.0f;
    if (posX > (float)BOARD_WIDTH)  posX = (float)BOARD_WIDTH;
    if (posY > (float)BOARD_HEIGHT) posY = (float)BOARD_HEIGHT;

    e->position.x = posX;
    e->position.y = posY;
    e->ticksSinceProgress = 0;
    pathfind_face_goal(e, bf, goal);
    return true;
}

bool pathfind_move_toward_goal(Entity *e, Vector2 goal, float stopRadius,
                               NavFrame *nav, const Battlefield *bf,
                               float deltaTime) {
    const float arriveEpsilon = 0.01f;
    if (!e || !bf) return true;

    float dx = goal.x - e->position.x;
    float dy = goal.y - e->position.y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist <= stopRadius + arriveEpsilon) {
        e->ticksSinceProgress = 0;
        return true;
    }

    // Phase 3d: route through the free-goal flow field when a live
    // nav snapshot is available. Falls through to the old candidate
    // fan otherwise (tests that ride the Phase 3a NULL macros).
    if (pathfind_step_free_goal_flow(e, goal, stopRadius, nav, bf, deltaTime)) {
        // Re-check arrival after the flow step.
        float ddx = goal.x - e->position.x;
        float ddy = goal.y - e->position.y;
        if (ddx * ddx + ddy * ddy <= (stopRadius + arriveEpsilon) *
                                      (stopRadius + arriveEpsilon)) {
            return true;
        }
        return false;
    }

    pathfind_try_step_toward(e, goal, stopRadius, bf, deltaTime, false, false);
    return false;
}

// Phase 3b: flow-field lane-march stepper.
//
// Entry conditions (checked by the caller in pathfind_step_entity):
//   - nav is a live per-frame NavFrame snapshot
//   - e->navProfile == NAV_PROFILE_LANE
//   - goalIsWaypoint (pure lane march, no pursuit target)
//
// This stepper bypasses the old candidate fan entirely. Movement is
// continuous position integration along the bilinear-sampled flow vector
// from the lane field. Hard blockers (board-edge moat, NAV_PROFILE_STATIC
// footprints) are consulted via the global staticBlockers mask; allied
// troops are NOT blockers here -- their influence is already baked into
// the flow field as density shaping cost. The 96 px home-half lane
// corridor is enforced by the lane-field builder's corridor mask, so a
// lane troop steered off the corridor simply loses its flow gradient and
// stalls instead of being hard-rejected by a candidate check.
//
// The stepper still updates facing, lane progress, and stagnation ticks
// the same way the old stepper did, so downstream combat/target logic
// remains unchanged.
static void pathfind_step_lane_flow(Entity *e, NavFrame *nav,
                                      const Battlefield *bf,
                                      BattleSide ownerSide,
                                      Vector2 facingGoal,
                                      float deltaTime) {
    const NavField *field = nav_get_or_build_lane_field(nav, bf, ownerSide, e->lane);
    if (!field) {
        e->ticksSinceProgress++;
        return;
    }

    float fx = 0.0f, fy = 0.0f;
    nav_sample_flow(field, e->position.x, e->position.y, &fx, &fy);
    if (fx == 0.0f && fy == 0.0f) {
        // No flow neighborhood: units just outside the 96 px corridor can
        // land here. Stand in place, keep facing, and let the enclosing
        // tick loop decide whether to switch state.
        e->ticksSinceProgress++;
        return;
    }

    // Walk the step length in sub-steps of at most half a cell so large
    // moveSpeed * deltaTime products do not tunnel past static blockers
    // (bases, board-edge moat). The sampled flow direction is held
    // constant for the whole tick -- within a single frame at realistic
    // moveSpeeds this is indistinguishable from re-sampling per sub-step.
    float totalStep = e->moveSpeed * deltaTime;
    if (totalStep < 0.0f) totalStep = 0.0f;
    float maxSub = (float)NAV_CELL_SIZE * 0.5f;
    int32_t subCount = 1;
    if (totalStep > maxSub) {
        subCount = (int32_t)ceilf(totalStep / maxSub);
    }
    float subLen = (subCount > 0) ? totalStep / (float)subCount : 0.0f;

    float posX = e->position.x;
    float posY = e->position.y;
    bool moved = false;
    for (int32_t s = 0; s < subCount; ++s) {
        float tryX = posX + fx * subLen;
        float tryY = posY + fy * subLen;
        int32_t tryCell = nav_cell_index_for_world(tryX, tryY);
        // Per-field hard-blocked check. The lane field's hardBlocked
        // mask is the single source of truth for "where this lane's
        // troops are allowed to stand": it contains the static
        // obstacle mask that was live when the field was built PLUS
        // the 96 px home-half corridor mask. Checking it here
        // (instead of nav->staticBlockers) is what actually enforces
        // the corridor at step time -- a bilinearly-blended flow
        // vector that points across a corridor edge no longer leaks.
        if (field->hardBlocked[tryCell]) {
            break;
        }
        posX = tryX;
        posY = tryY;
        moved = true;
    }
    if (!moved) {
        // Fully blocked in front. Stall.
        e->ticksSinceProgress++;
        return;
    }

    // Clamp to canonical board bounds as a last safety net. The moat
    // should already cover this, but keep the assertion friendly.
    if (posX < 0.0f) posX = 0.0f;
    if (posY < 0.0f) posY = 0.0f;
    if (posX > (float)BOARD_WIDTH)  posX = (float)BOARD_WIDTH;
    if (posY > (float)BOARD_HEIGHT) posY = (float)BOARD_HEIGHT;

    e->position.x = posX;
    e->position.y = posY;
    e->ticksSinceProgress = 0;

    // Facing uses the outer waypoint-derived goal, not the bilinear-
    // sampled flow direction. Flow sampling on the 8-way integer cell
    // gradient picks up a small x component whenever the entity is
    // offset from the lane center (e.g. after a crowd-shaped step),
    // and feeding that directly to pathfind_face_goal would flip the
    // sprite flipH on near-pure-vertical movement. The waypoint goal
    // stays stable along the straight lane centerline, which matches
    // the old stepper's facing behavior.
    pathfind_face_goal(e, bf, facingGoal);
}

// Phase 3c: flow-field target-pursuit stepper.
//
// Entry conditions (checked by the caller in pathfind_step_entity):
//   - nav is a live per-frame NavFrame snapshot
//   - e->movementTargetId >= 0 and points at a live non-self entity
//   - goalIsWaypoint == false (outer path computed a pursuit point)
//
// Builds a NavTargetGoal from the target's frozen snapshot position and
// the attacker's effective engagement radius (taken from the caller's
// already-resolved stopRadius so combat_in_range stays in sync), fetches
// the per-(target, kind, radius, side) field via
// nav_get_or_build_target_field, and sub-steps the entity along the
// sampled flow. STATIC_ATTACK goals seed a 160-deg front-arc ribbon
// oriented toward the attacker's own side (SIDE_BOTTOM faces up,
// SIDE_TOP faces down). Mobile melee attackers seed a one-cell-thick
// ring; ranged/support attackers seed a full disk so they can settle
// anywhere inside their attack range.
//
// When the field cannot be built (overflow, target mid-snapshot gone,
// goal kind unsupported) the stepper returns false and the caller falls
// back to the old candidate-fan path.
static bool pathfind_step_target_flow(Entity *e, NavFrame *nav,
                                        const Battlefield *bf,
                                        BattleSide ownerSide,
                                        Vector2 facingGoal,
                                        float stopRadius,
                                        float deltaTime) {
    if (e->movementTargetId < 0) return false;
    Entity *target = bf_find_entity((Battlefield *)bf, e->movementTargetId);
    if (!target || !target->alive) return false;
    if (stopRadius <= 0.0f) return false;

    bool isStatic = (target->navProfile == NAV_PROFILE_STATIC) ||
                    (target->type == ENTITY_BUILDING);
    bool isRanged = e->attackRange > e->bodyRadius + 48.0f;

    NavTargetGoal goal = { 0 };
    goal.targetX = target->position.x;
    goal.targetY = target->position.y;
    goal.outerRadius = stopRadius;
    goal.targetBodyRadius = target->bodyRadius;
    // Inner floor matches the stepper's target-body contact shell so
    // the ribbon never seeds cells the stepper will refuse to enter.
    goal.innerRadiusMin = target->bodyRadius + e->bodyRadius +
                          (float)PATHFIND_CONTACT_GAP;
    goal.targetId = target->id;
    goal.perspectiveSide = (int16_t)ownerSide;

    if (isStatic) {
        goal.kind = NAV_GOAL_KIND_STATIC_ATTACK;
        // Attackers approach from the enemy half, so the target's front
        // arc faces away from its own spawn. Bottom-side targets face
        // upward (-y, -90 deg), top-side targets face downward (+y, +90
        // deg). 160-deg total arc -> half = 80.
        BattleSide targetSide = bf_side_for_player(target->ownerID);
        goal.arcCenterDeg = (targetSide == SIDE_BOTTOM) ? -90.0f : 90.0f;
        goal.arcHalfDeg = 80.0f;
    } else if (isRanged) {
        goal.kind = NAV_GOAL_KIND_DIRECT_RANGE;
    } else {
        goal.kind = NAV_GOAL_KIND_MELEE_RING;
    }

    const NavField *field = nav_get_or_build_target_field(nav, bf, &goal);
    if (!field) return false;

    float fx = 0.0f, fy = 0.0f;
    nav_sample_flow(field, e->position.x, e->position.y, &fx, &fy);
    if (fx == 0.0f && fy == 0.0f) {
        // Attacker is already on a seed cell (at engagement range) or
        // no reachable flow. Stand still so combat_in_range can latch
        // in the caller.
        e->ticksSinceProgress++;
        pathfind_face_goal(e, bf, facingGoal);
        return true;
    }

    float totalStep = e->moveSpeed * deltaTime;
    if (totalStep < 0.0f) totalStep = 0.0f;
    float maxSub = (float)NAV_CELL_SIZE * 0.5f;
    int32_t subCount = 1;
    if (totalStep > maxSub) {
        subCount = (int32_t)ceilf(totalStep / maxSub);
    }
    float subLen = (subCount > 0) ? totalStep / (float)subCount : 0.0f;

    // Minimum separation from the target center: the attacker's body
    // plus the target's body plus the contact gap. DIRECT_RANGE and
    // MELEE_RING fields seed cells all the way to the target center
    // (for mobile targets the target has no static footprint), so the
    // stepper must enforce this here. STATIC targets already get the
    // nav_stamp_static_entity mover-clearance shell, so this is a
    // no-op for them -- but the check is cheap and catches all kinds.
    float minSepTargetSq = target->bodyRadius + e->bodyRadius +
                            (float)PATHFIND_CONTACT_GAP;
    minSepTargetSq *= minSepTargetSq;

    float posX = e->position.x;
    float posY = e->position.y;
    bool moved = false;
    for (int32_t s = 0; s < subCount; ++s) {
        float tryX = posX + fx * subLen;
        float tryY = posY + fy * subLen;
        int32_t tryCell = nav_cell_index_for_world(tryX, tryY);
        // Target-field hardBlocked mirrors nav->staticBlockers plus any
        // per-field mask; checking it keeps the attacker out of base
        // footprints and the board-edge moat.
        if (field->hardBlocked[tryCell]) break;
        // Target-body contact shell. Refuse sub-steps that would pull
        // the attacker center inside (attacker.body + target.body +
        // gap). For STATIC targets this is already enforced by the
        // static-entity stamp shell; for mobile targets it is the
        // only thing keeping the attacker from walking into the
        // target sprite.
        float dxT = tryX - target->position.x;
        float dyT = tryY - target->position.y;
        if (dxT * dxT + dyT * dyT < minSepTargetSq) break;
        posX = tryX;
        posY = tryY;
        moved = true;
    }
    if (!moved) {
        e->ticksSinceProgress++;
        return true;
    }

    if (posX < 0.0f) posX = 0.0f;
    if (posY < 0.0f) posY = 0.0f;
    if (posX > (float)BOARD_WIDTH)  posX = (float)BOARD_WIDTH;
    if (posY > (float)BOARD_HEIGHT) posY = (float)BOARD_HEIGHT;

    e->position.x = posX;
    e->position.y = posY;
    e->ticksSinceProgress = 0;
    pathfind_face_goal(e, bf, facingGoal);
    return true;
}

bool pathfind_step_entity(Entity *e, NavFrame *nav, const Battlefield *bf,
                           float deltaTime) {
    // Debug assertion: entity position must be within canonical board bounds
    CanonicalPos posCheck = { e->position };
    BF_ASSERT_IN_BOUNDS(posCheck, BOARD_WIDTH, BOARD_HEIGHT);

    // Validate lane bounds -- invalid lane means entity cannot path
    if (e->lane < 0 || e->lane >= 3) {
        entity_set_state(e, ESTATE_IDLE);
        return false;
    }

    BattleSide ownerSide = bf_side_for_player(e->ownerID);

    // --- Resolve steering goal ---
    Vector2 goal;
    float stopRadius = 0.0f;
    bool goalIsWaypoint = false;
    if (!pathfind_compute_goal(e, bf, &goal, &stopRadius, &goalIsWaypoint)) {
        BattleSide enemySide = (ownerSide == SIDE_BOTTOM) ? SIDE_TOP : SIDE_BOTTOM;
        pathfind_commit_presentation(e, bf);
        pathfind_face_goal(e, bf, bf_base_anchor(bf, enemySide).v);
        entity_set_state(e, ESTATE_IDLE);
        return false;
    }

    // Phase 3b: for pure lane-march stepping (no pursuit target), route
    // movement through the flow-field stepper. This bypasses the old
    // candidate fan's hard lane corridor and ally-as-blocker rules in
    // favor of the NavFrame snapshot: the 96 px home corridor is enforced
    // by the lane field's hardBlocked mask, crowd shaping is density cost
    // rolled into the integration, and the enemy half is free to route
    // cross-lane to the seed. When nav is NULL (legacy tests that ride
    // the Phase 3a compatibility macros) the old candidate-fan path runs
    // unchanged.
    bool handledByFlow = false;
    if (nav && goalIsWaypoint && e->navProfile == NAV_PROFILE_LANE) {
        // Phase 3b: lane-march flow stepper.
        pathfind_step_lane_flow(e, nav, bf, ownerSide, goal, deltaTime);
        handledByFlow = true;
    } else if (nav && !goalIsWaypoint && e->movementTargetId >= 0) {
        // Phase 3c: target-pursuit flow stepper (mobile melee, mobile
        // ranged/support, static assault). Falls through to the old
        // candidate fan if the target field cannot be built.
        handledByFlow = pathfind_step_target_flow(e, nav, bf, ownerSide,
                                                  goal, stopRadius, deltaTime);
    }
    if (!handledByFlow) {
        pathfind_try_step_toward(e, goal, stopRadius, bf, deltaTime, true, true);
    }
    pathfind_sync_lane_progress(e, bf);

    // --- Lane bookkeeping (only meaningful when lane-following) ---
    if (goalIsWaypoint) {
        float laneTotal = pathfind_lane_total_length_for_side(bf, ownerSide, e->lane);
        if (e->laneProgress >= laneTotal - PATHFIND_WAYPOINT_REACH_GAP) {
            // End of path: idle in place, face enemy base. No random jitter.
            BattleSide enemySide = (ownerSide == SIDE_BOTTOM) ? SIDE_TOP : SIDE_BOTTOM;
            CanonicalPos enemyBase = bf_base_anchor(bf, enemySide);
            pathfind_commit_presentation(e, bf);
            pathfind_face_goal(e, bf, enemyBase.v);
            entity_set_state(e, ESTATE_IDLE);
            return false;
        }
    }

    return true;
}

void pathfind_apply_direction(AnimState *anim, Vector2 diff) {
    const float eps = 0.001f;

    if (fabsf(diff.x) < eps && fabsf(diff.y) < eps) return;

    anim->dir = DIR_SIDE;
    // Troops now use only left/right-facing rows. Pure vertical motion keeps a
    // stable right-facing bias instead of selecting front/back rows.
    anim->flipH = (fabsf(diff.x) >= eps) ? (diff.x < 0.0f) : false;
}

void pathfind_apply_direction_for_side(AnimState *anim, Vector2 diff, BattleSide side) {
    const float eps = 0.001f;

    if (fabsf(diff.x) < eps && fabsf(diff.y) < eps) return;

    anim->dir = DIR_SIDE;
    // Top-side units are rendered with a 180-degree sprite rotation, so their
    // horizontal flip must be inverted to preserve left/right-facing. Pure
    // vertical motion keeps a stable right-facing bias.
    if (fabsf(diff.x) >= eps) {
        anim->flipH = (side == SIDE_TOP) ? (diff.x > 0.0f) : (diff.x < 0.0f);
        return;
    }

    anim->flipH = (side == SIDE_TOP);
}

float pathfind_sprite_rotation_for_side(SpriteDirection dir, BattleSide side) {
    (void)dir;
    if (side == SIDE_TOP) {
        return 180.0f;
    }
    return 0.0f;
}
