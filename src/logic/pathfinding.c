#include "pathfinding.h"
#include "../core/config.h"
#include "../core/battlefield.h"
#include "../entities/entities.h"
#include <math.h>
#include <stdlib.h>

bool pathfind_step_entity(Entity *e, const Battlefield *bf, float deltaTime) {
    // Debug assertion: entity position must be within canonical board bounds
    CanonicalPos posCheck = { e->position };
    BF_ASSERT_IN_BOUNDS(posCheck, BOARD_WIDTH, BOARD_HEIGHT);

    // Validate lane bounds -- invalid lane means entity cannot path
    if (e->lane < 0 || e->lane >= 3) {
        entity_set_state(e, ESTATE_IDLE);
        return false;
    }

    // Check if already past all waypoints
    if (e->waypointIndex >= LANE_WAYPOINT_COUNT) {
        entity_set_state(e, ESTATE_IDLE);
        return false;
    }

    // Read canonical waypoint from Battlefield (per D-05, D-18)
    BattleSide side = bf_side_for_player(e->ownerID);
    CanonicalPos targetWP = bf_waypoint(bf, side, e->lane, e->waypointIndex);
    Vector2 target = targetWP.v;
    float dx = target.x - e->position.x;
    float dy = target.y - e->position.y;
    float dist = sqrtf(dx * dx + dy * dy);
    float step = e->moveSpeed * deltaTime;
    bool advancedWaypoint = false;

    if (dist <= step || dist < 1.0f) {
        // Reached waypoint -- snap and advance
        e->position = target;
        e->waypointIndex++;
        advancedWaypoint = true;

        if (e->waypointIndex >= LANE_WAYPOINT_COUNT) {
            // End of path: apply random jitter (per D-11)
            float jx = ((float) (rand() % ((int) (LANE_JITTER_RADIUS * 2) + 1)) - LANE_JITTER_RADIUS);
            float jy = ((float) (rand() % ((int) (LANE_JITTER_RADIUS * 2) + 1)) - LANE_JITTER_RADIUS);
            e->position.x += jx;
            e->position.y += jy;
            // Face toward the enemy when idling at the end of the path.
            // SIDE_BOTTOM walks toward decreasing y (enemy above) → DIR_UP
            // SIDE_TOP walks toward increasing y (enemy below) → DIR_DOWN
            e->anim.dir = (side == SIDE_BOTTOM) ? DIR_UP : DIR_DOWN;
            e->anim.flipH = false;
            entity_set_state(e, ESTATE_IDLE);
            return false;
        }
    } else {
        // Move toward waypoint
        float inv = 1.0f / dist;
        e->position.x += dx * inv * step;
        e->position.y += dy * inv * step;
    }

    // Only update facing during actual movement -- not on the snap frame
    // where the entity just arrived at a waypoint. This preserves the
    // walking direction so entities don't snap to a new facing on arrival.
    if (!advancedWaypoint) {
        Vector2 diff = {target.x - e->position.x, target.y - e->position.y};
        float ddist = sqrtf(diff.x * diff.x + diff.y * diff.y);
        if (ddist > 1.0f) {
            pathfind_apply_direction(&e->anim, diff);
        }
    }

    return true;
}

void pathfind_apply_direction(AnimState *anim, Vector2 diff) {
    const float eps = 0.001f;

    if (fabsf(diff.x) < eps && fabsf(diff.y) < eps) return;

    if (fabsf(diff.x) >= eps) {
        anim->dir = DIR_SIDE;
        anim->flipH = (diff.x < 0);
    } else if (diff.y < 0) {
        anim->dir = DIR_UP;
        anim->flipH = false;
    } else {
        anim->dir = DIR_DOWN;
        anim->flipH = false;
    }
}
