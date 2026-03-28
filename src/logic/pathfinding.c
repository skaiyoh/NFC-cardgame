#include "pathfinding.h"
#include "../systems/player.h"
#include "../core/config.h"
#include "../entities/entities.h"
#include <math.h>
#include <stdlib.h>

// Forward declaration
void pathfind_compute_direction(Entity *e, Vector2 diff);

// Compute lateral bow offset for outer lanes using sinf.
// t: normalized 0.0 = spawn end, 1.0 = enemy end (along full path, NOT raw depth).
// Returns signed offset in world units (negative = left, positive = right).
// Center lane (lane 1) always returns 0.
static float bow_offset(int lane, float t, float laneWidth) {
    if (lane == 1) return 0.0f;

    // sin(t * PI) peaks at t=0.5 (midpoint of full path), zero at both ends
    float bow = sinf(t * PI_F) * LANE_BOW_INTENSITY * laneWidth;

    // Lane 0 (left) bows left (negative X), lane 2 (right) bows right (positive X)
    return (lane == 0) ? -bow : bow;
}

void lane_generate_waypoints(Player *p) {
    float laneWidth = p->playArea.width / 3.0f;

    // Compute spawn depth from slot Y position.
    // player_lane_pos formula: y = playArea.y + playArea.height * (0.9 - depth * 0.8)
    // Inverse: depth = (0.9 - (slotY - playArea.y) / playArea.height) / 0.8
    // For spawnY = playArea.y + playArea.height * 0.8:
    //   depth = (0.9 - 0.8) / 0.8 = 0.125
    float spawnDepth = 0.125f;
    float endDepth = 1.75f;  // Extend into opponent territory (depth > 1.0 crosses border)

    for (int lane = 0; lane < 3; lane++) {
        for (int i = 0; i < LANE_WAYPOINT_COUNT; i++) {
            if (i == 0) {
                // First waypoint matches slot spawn position exactly to avoid
                // backward snap on spawn (see RESEARCH.md Pitfall 3).
                // Entities spawn with waypointIndex=1 to skip this zero-distance
                // waypoint (review fix: zero-distance first waypoint concern).
                p->laneWaypoints[lane][i] = p->slots[lane].worldPos;
                continue;
            }

            // Normalized parameter t: 0.0 at spawn, 1.0 at enemy base
            float t = (float)i / (float)(LANE_WAYPOINT_COUNT - 1);

            // Map t to depth: linearly interpolate from spawnDepth to endDepth
            float depth = spawnDepth + (endDepth - spawnDepth) * t;

            // Base position from existing lane calculation.
            // NOTE: player_lane_pos uses p->playArea, so P2 gets correct coords
            // for P2's coordinate space. Camera rotation handles visual flip.
            Vector2 pos = player_lane_pos(p, lane, depth);

            // Add bow offset using normalized t (peaks at t=0.5 = path midpoint)
            pos.x += bow_offset(lane, t, laneWidth);

            p->laneWaypoints[lane][i] = pos;
        }
    }
}

bool pathfind_step_entity(Entity *e, const Player *owner, float deltaTime) {
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

    Vector2 target = owner->laneWaypoints[e->lane][e->waypointIndex];
    float dx = target.x - e->position.x;
    float dy = target.y - e->position.y;
    float dist = sqrtf(dx * dx + dy * dy);
    float step = e->moveSpeed * deltaTime;

    if (dist <= step || dist < 1.0f) {
        // Reached waypoint -- snap and advance
        e->position = target;
        e->waypointIndex++;

        if (e->waypointIndex >= LANE_WAYPOINT_COUNT) {
            // End of path: apply random jitter (per D-11)
            float jx = ((float)(rand() % ((int)(LANE_JITTER_RADIUS * 2) + 1)) - LANE_JITTER_RADIUS);
            float jy = ((float)(rand() % ((int)(LANE_JITTER_RADIUS * 2) + 1)) - LANE_JITTER_RADIUS);
            e->position.x += jx;
            e->position.y += jy;
            entity_set_state(e, ESTATE_IDLE);
            return false;
        }
    } else {
        // Move toward waypoint
        float inv = 1.0f / dist;
        e->position.x += dx * inv * step;
        e->position.y += dy * inv * step;
    }

    // Compute direction from movement vector
    Vector2 diff = { target.x - e->position.x, target.y - e->position.y };
    float ddist = sqrtf(diff.x * diff.x + diff.y * diff.y);
    if (ddist > 1.0f) {
        pathfind_compute_direction(e, diff);
    }

    return true;
}

void pathfind_compute_direction(Entity *e, Vector2 diff) {
    float ax = fabsf(diff.x);
    float ay = fabsf(diff.y);

    // Hysteresis: only change direction if the dominant axis is clearly dominant.
    // When near 45 degrees (ax ~= ay), keep the current direction to prevent flicker.
    // The threshold is relative: |ax - ay| / max(ax, ay) must exceed DIRECTION_HYSTERESIS.
    float maxComp = (ax > ay) ? ax : ay;
    if (maxComp < 0.001f) return;  // Zero-length vector, keep current direction

    float ratio = fabsf(ax - ay) / maxComp;
    if (ratio < DIRECTION_HYSTERESIS) return;  // Near 45 degrees, keep current direction

    if (ax > ay) {
        // Dominant horizontal movement
        e->anim.dir = DIR_SIDE;
        e->anim.flipH = (diff.x < 0);  // true = facing left
    } else {
        // Dominant vertical movement
        e->anim.dir = (diff.y < 0) ? DIR_UP : DIR_DOWN;
        e->anim.flipH = false;
    }
}
