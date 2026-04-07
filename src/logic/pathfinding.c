#include "pathfinding.h"
#include "../core/config.h"
#include "../core/battlefield.h"
#include "../entities/entities.h"
#include <math.h>
#include <stdlib.h>

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

void pathfind_update_walk_facing(Entity *e, const Battlefield *bf) {
    if (!e || !bf) return;
    if (e->lane < 0 || e->lane >= 3) return;
    if (e->waypointIndex < 0 || e->waypointIndex >= LANE_WAYPOINT_COUNT) return;

    BattleSide ownerSide = bf_side_for_player(e->ownerID);
    Vector2 target = bf_waypoint(bf, ownerSide, e->lane, e->waypointIndex).v;
    Vector2 diff = { target.x - e->position.x, target.y - e->position.y };
    float dist = sqrtf(diff.x * diff.x + diff.y * diff.y);

    if (dist <= 1.0f) {
        e->spriteRotationDegrees = pathfind_sprite_rotation_for_side(e->anim.dir, e->presentationSide);
        return;
    }

    pathfind_apply_direction_for_side(&e->anim, diff, e->presentationSide);
    e->spriteRotationDegrees = pathfind_sprite_rotation_for_side(e->anim.dir, e->presentationSide);
}

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
    BattleSide ownerSide = bf_side_for_player(e->ownerID);
    CanonicalPos targetWP = bf_waypoint(bf, ownerSide, e->lane, e->waypointIndex);
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
        pathfind_sync_presentation(e, bf);

        if (e->waypointIndex >= LANE_WAYPOINT_COUNT) {
            // End of path: apply random jitter (per D-11)
            float jx = ((float) (rand() % ((int) (LANE_JITTER_RADIUS * 2) + 1)) - LANE_JITTER_RADIUS);
            float jy = ((float) (rand() % ((int) (LANE_JITTER_RADIUS * 2) + 1)) - LANE_JITTER_RADIUS);
            e->position.x += jx;
            e->position.y += jy;
            pathfind_commit_presentation(e, bf);
            BattleSide enemySide = (ownerSide == SIDE_BOTTOM) ? SIDE_TOP : SIDE_BOTTOM;
            CanonicalPos enemyBase = bf_base_anchor(bf, enemySide);
            Vector2 baseDiff = {
                enemyBase.v.x - e->position.x,
                enemyBase.v.y - e->position.y
            };
            pathfind_apply_direction_for_side(&e->anim, baseDiff, e->presentationSide);
            e->spriteRotationDegrees = pathfind_sprite_rotation_for_side(e->anim.dir,
                                                                         e->presentationSide);
            entity_set_state(e, ESTATE_IDLE);
            return false;
        }
    } else {
        // Move toward waypoint
        float inv = 1.0f / dist;
        e->position.x += dx * inv * step;
        e->position.y += dy * inv * step;
        pathfind_sync_presentation(e, bf);
    }

    // Only update facing during actual movement -- not on the snap frame
    // where the entity just arrived at a waypoint. This preserves the
    // walking direction so entities don't snap to a new facing on arrival.
    if (!advancedWaypoint) {
        pathfind_update_walk_facing(e, bf);
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

void pathfind_apply_direction_for_side(AnimState *anim, Vector2 diff, BattleSide side) {
    const float eps = 0.001f;

    if (fabsf(diff.x) < eps && fabsf(diff.y) < eps) return;

    if (fabsf(diff.x) >= eps) {
        anim->dir = DIR_SIDE;
        // Top-side units are rendered with a 180-degree sprite rotation, so
        // their horizontal flip must be inverted to preserve left/right facing.
        anim->flipH = (side == SIDE_TOP) ? (diff.x > 0) : (diff.x < 0);
        return;
    }

    // Vertical facing is side-perspective aware:
    // SIDE_BOTTOM: moving up the canonical board means away from camera.
    // SIDE_TOP: moving down the canonical board means away from camera.
    bool movingAway = (side == SIDE_BOTTOM) ? (diff.y < 0.0f) : (diff.y > 0.0f);
    anim->dir = movingAway ? DIR_UP : DIR_DOWN;
    anim->flipH = false;
}

float pathfind_sprite_rotation_for_side(SpriteDirection dir, BattleSide side) {
    (void)dir;
    if (side == SIDE_TOP) {
        return 180.0f;
    }
    return 0.0f;
}
