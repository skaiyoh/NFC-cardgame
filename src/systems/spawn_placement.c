//
// Non-overlapping spawn placement implementation.
//

#include "spawn_placement.h"
#include "../core/battlefield.h"
#include "../core/battlefield_math.h"
#include "../core/config.h"
#include "../logic/base_geometry.h"

// Check if placing a circle of `bodyRadius` at `pos` would overlap any
// living, unmarked entity in the registry. Mirrors pathfinding's blocker
// overlap check so spawned troops land in the same non-overlapping
// geometry that local steering enforces per tick.
static Vector2 spawn_blocker_center(const Entity *other) {
    if (!other) return (Vector2){ 0.0f, 0.0f };
    if (other->type == ENTITY_BUILDING) {
        return base_nav_blocker_center(other);
    }
    return other->position;
}

static float spawn_blocker_radius(const Entity *other) {
    if (!other) return 0.0f;
    if (other->type == ENTITY_BUILDING) {
        return (other->navRadius > 0.0f) ? other->navRadius : other->bodyRadius;
    }
    return other->bodyRadius;
}

static bool spawn_pos_overlaps(const Battlefield *bf, Vector2 pos,
                               float bodyRadius) {
    for (int i = 0; i < bf->entityCount; i++) {
        const Entity *other = bf->entities[i];
        if (!other || !other->alive || other->markedForRemoval) continue;
        if (other->type == ENTITY_PROJECTILE) continue;
        Vector2 blockerCenter = spawn_blocker_center(other);
        float blockerRadius = spawn_blocker_radius(other);
        float dx = blockerCenter.x - pos.x;
        float dy = blockerCenter.y - pos.y;
        float minDist = bodyRadius + blockerRadius + PATHFIND_CONTACT_GAP;
        if (dx * dx + dy * dy < minDist * minDist) return true;
    }
    return false;
}

static bool spawn_pos_in_bounds(const Battlefield *bf, Vector2 pos,
                                float bodyRadius) {
    if (pos.x - bodyRadius < 0.0f || pos.x + bodyRadius > bf->boardWidth)
        return false;
    if (pos.y - bodyRadius < 0.0f || pos.y + bodyRadius > bf->boardHeight)
        return false;
    return true;
}

bool spawn_find_free_anchor(GameState *gs, BattleSide side, int slotIndex,
                            float bodyRadius, Vector2 *outPos) {
    if (!gs || !outPos || bodyRadius <= 0.0f) return false;

    Battlefield *bf = &gs->battlefield;
    CanonicalPos anchorCanon = bf_spawn_pos(bf, side, slotIndex);
    Vector2 anchor = anchorCanon.v;

    // Lateral offsets along the lane centerline axis (x). Order: center,
    // then alternating left/right at soft, medium, wide distances.
    static const float lateralMults[] = {
        0.0f, -1.25f, 1.25f, -2.5f, 2.5f, -3.75f, 3.75f, -5.0f, 5.0f
    };
    const int lateralCount = (int)(sizeof(lateralMults) / sizeof(lateralMults[0]));

    // Behind the anchor = toward the owner's home edge.
    // SIDE_BOTTOM home is +y, SIDE_TOP home is -y.
    float behindSign = (side == SIDE_BOTTOM) ? 1.0f : -1.0f;

    for (int row = 0; row < 4; row++) {
        float yOffset = behindSign * bodyRadius * (float)row;
        for (int j = 0; j < lateralCount; j++) {
            Vector2 candidate = {
                anchor.x + lateralMults[j] * bodyRadius,
                anchor.y + yOffset
            };
            if (!spawn_pos_in_bounds(bf, candidate, bodyRadius)) continue;
            if (spawn_pos_overlaps(bf, candidate, bodyRadius)) continue;
            *outPos = candidate;
            return true;
        }
    }
    return false;
}
