//
// Battlefield model implementation -- authoritative world geometry (per D-11)
//

#include "battlefield.h"
#include "types.h"  // Full Entity definition needed for entity registry operations
#include <string.h>
#include <math.h>
#include <stdio.h>

// --- Internal helpers ---

// Compute lateral bow offset for outer lanes using sinf.
// Same formula as pathfinding.c bow_offset (per D-18: centralized math).
// t: normalized 0.0 = spawn end, 1.0 = enemy end.
// Returns signed offset in world units (negative = left, positive = right).
// Center lane (lane 1) always returns 0.
static float bow_offset(int lane, float t, float laneWidth) {
    if (lane == 1) return 0.0f;

    // sin(t * PI) peaks at t=0.5 (midpoint of full path), zero at both ends
    float bow = sinf(t * PI_F) * LANE_BOW_INTENSITY * laneWidth;

    // Lane 0 (left) bows left (negative X), lane 2 (right) bows right (positive X)
    return (lane == 0) ? -bow : bow;
}

// Pull the outer lanes slightly toward the center so they sit less tightly
// against the board edges at both spawn and seam ends.
static float outer_lane_inset(int lane, float laneWidth) {
    if (lane == 1) return 0.0f;

    float inset = laneWidth * LANE_OUTER_INSET_RATIO;
    return (lane == 0) ? inset : -inset;
}

static float smoothstep01(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return t * t * (3.0f - 2.0f * t);
}

// Default lane spawn Y lives inside the shortened playable rect (territory
// minus the player-edge hand-bar inset), so spawn/base/waypoint geometry never
// projects under the hand bar.
static float default_lane_spawn_y_from_play(Rectangle play, BattleSide side) {
    if (side == SIDE_BOTTOM) {
        return play.y + play.height * 0.8f;
    }
    return play.y + play.height * 0.2f;
}

static float base_anchor_y_from_play(Rectangle play, BattleSide side) {
    float spawnY = default_lane_spawn_y_from_play(play, side);
    return spawnY + ((side == SIDE_TOP)
        ? -BASE_HOME_OFFSET_FROM_SPAWN
        : BASE_HOME_OFFSET_FROM_SPAWN);
}

// Center-lane troops need to clear the owning base blocker even if the base
// itself is moved without retuning the spawn row. Keep the base anchored at
// its authored position and push only the middle lane's spawn out in front of
// the base toward the battlefield interior.
static float center_lane_spawn_y_from_play(Rectangle play, BattleSide side) {
    float baseY = base_anchor_y_from_play(play, side);
    float blockerCenterY = baseY + ((side == SIDE_TOP)
        ? -BASE_NAV_BLOCKER_BACK_OFFSET_TOP
        : BASE_NAV_BLOCKER_BACK_OFFSET_BOTTOM);
    float battlefieldSign = (side == SIDE_BOTTOM) ? -1.0f : 1.0f;
    float clearance = BASE_NAV_RADIUS + DEFAULT_MELEE_BODY_RADIUS +
                      PATHFIND_CONTACT_GAP;
    return blockerCenterY + battlefieldSign * clearance;
}

static float lane_spawn_y_from_play(Rectangle play, BattleSide side, int lane) {
    if (lane == 1) {
        return center_lane_spawn_y_from_play(play, side);
    }
    return default_lane_spawn_y_from_play(play, side);
}

static float outer_lane_base_approach(int lane, float t) {
    if (lane == 1) return 0.0f;

    float localT = (t - LANE_BASE_APPROACH_START) / (1.0f - LANE_BASE_APPROACH_START);
    return smoothstep01(localT);
}

// Initialize a single territory
static void territory_init(Territory *t, BattleSide side, Rectangle bounds,
                           BiomeType biome, const BiomeDef *biomeDef,
                           float tileSize, unsigned int seed) {
    t->side = side;
    t->bounds = bounds;
    t->biome = biome;
    t->biomeDef = biomeDef;

    // Copy biome tile definitions into territory-local arrays
    biome_copy_tiledefs(biomeDef, t->tileDefs);
    t->tileDefCount = biome_tile_count(biomeDef);
    biome_copy_detail_defs(biomeDef, t->detailDefs);
    t->detailDefCount = biomeDef->detailDefCount;

    // Create tilemap for this territory
    t->tilemap = tilemap_create_biome(bounds, tileSize, seed, biomeDef);
}

// Generate canonical lane waypoints for one side.
// SIDE_BOTTOM (P1): forward = toward decreasing y (per D-03)
// SIDE_TOP (P2): forward = toward increasing y (per D-04)
// Waypoints are generated in side-local space, then converted to canonical
// via bf_to_canonical (per D-06, D-18).
//
// Y math lives inside bf_play_bounds(side), the shortened playable rect
// that excludes the hand-bar region on each player's outer edge. X math
// still uses the full territory width (1080 / 3 lanes) because the hand
// bar only shortens the vertical axis.
static void generate_canonical_waypoints(Battlefield *bf, BattleSide side) {
    Territory *t = &bf->territories[side];
    BattleSide enemySide = (side == SIDE_BOTTOM) ? SIDE_TOP : SIDE_BOTTOM;
    Rectangle play = bf_play_bounds(bf, side);
    Rectangle enemyPlay = bf_play_bounds(bf, enemySide);
    float laneWidth = t->bounds.width / 3.0f;
    float enemyBaseX = bf->boardWidth * 0.5f;
    float enemyBaseY = base_anchor_y_from_play(enemyPlay, enemySide);
    float enemyApproachY = enemyBaseY + ((enemySide == SIDE_TOP) ? LANE_BASE_APPROACH_GAP
                                                                 : -LANE_BASE_APPROACH_GAP);

    // Depth parameters matching pathfinding.c
    float spawnDepth = 0.125f;
    float endDepth = 2.125f; // last waypoint at opponent's spawn

    for (int lane = 0; lane < 3; lane++) {
        // Map lane index to the slot index for this side (for spawn position)
        // For SIDE_BOTTOM: slot = lane (identity)
        // For SIDE_TOP: slot = 2 - lane (mirror per D-08)
        int slot = (side == SIDE_BOTTOM) ? lane : (2 - lane);

        for (int wp = 0; wp < LANE_WAYPOINT_COUNT; wp++) {
            float t_param;
            float depth;

            if (wp == 0) {
                // First waypoint matches slot spawn position exactly
                // (same convention as pathfinding.c: waypointIndex=1 at spawn)
                float localX = t->bounds.x + (slot + 0.5f) * laneWidth;
                float localY = lane_spawn_y_from_play(play, side, lane);

                SideLocalPos local = { .v = { localX, localY }, .side = side };
                CanonicalPos canonical = bf_to_canonical(local, bf->boardWidth);
                canonical.v.x += outer_lane_inset(lane, laneWidth);
                bf->laneWaypoints[side][lane][wp] = canonical;
                continue;
            }

            // Normalized parameter: 0.0 at spawn, 1.0 at enemy base
            t_param = (float)wp / (float)(LANE_WAYPOINT_COUNT - 1);
            depth = spawnDepth + (endDepth - spawnDepth) * t_param;

            // Compute position in side-local space
            float localX = t->bounds.x + (slot + 0.5f) * laneWidth;

            float localY;
            if (side == SIDE_BOTTOM) {
                // P1: y decreases as depth increases (toward enemy at top).
                // Formula from player_lane_pos: y = play.y + play.height * (0.9 - depth*0.8)
                localY = play.y + play.height * (0.9f - depth * 0.8f);
            } else {
                // P2: y increases as depth increases (toward enemy at bottom,
                // in canonical terms). Mirrored formula inside the shortened
                // play rect: y = play.y + play.height * (0.1 + depth*0.8)
                localY = play.y + play.height * (0.1f + depth * 0.8f);
            }

            // Convert to canonical coordinates FIRST, then apply bow in canonical space.
            // Applying bow before bf_to_canonical would flip the bow direction for SIDE_TOP
            // because bf_to_canonical mirrors X (per D-06).
            SideLocalPos local = { .v = { localX, localY }, .side = side };
            CanonicalPos canonical = bf_to_canonical(local, bf->boardWidth);

            canonical.v.x += outer_lane_inset(lane, laneWidth);

            // Apply bow offset in canonical space (sign is consistent for both sides)
            canonical.v.x += bow_offset(lane, t_param, laneWidth);

            // Outer lanes taper toward the enemy base so units that finish there
            // can immediately find and attack it instead of idling off to the side.
            float baseApproach = outer_lane_base_approach(lane, t_param);
            if (baseApproach > 0.0f) {
                canonical.v.x += (enemyBaseX - canonical.v.x) * baseApproach;
                canonical.v.y += (enemyApproachY - canonical.v.y) * baseApproach;
            }

            bf->laneWaypoints[side][lane][wp] = canonical;
        }
    }

    // Store slot spawn anchors: first waypoint of each lane mapped to slot index
    for (int slot = 0; slot < NUM_CARD_SLOTS; slot++) {
        int lane = bf_slot_to_lane(side, slot);
        bf->slotSpawnAnchors[side][slot] = bf->laneWaypoints[side][lane][0];
    }
}

// --- Public API ---

BattleSide bf_side_for_player(int playerID) {
    return (playerID == 0) ? SIDE_BOTTOM : SIDE_TOP;
}

void bf_init(Battlefield *bf, const BiomeDef biomeDefs[],
             BiomeType bottomBiome, BiomeType topBiome,
             float tileSize, unsigned int seedBottom, unsigned int seedTop) {
    memset(bf, 0, sizeof(Battlefield));

    bf->boardWidth = BOARD_WIDTH;
    bf->boardHeight = BOARD_HEIGHT;
    bf->seamY = SEAM_Y;

    // Initialize bottom territory (P1): {0, 960, 1080, 960} (per D-02)
    Rectangle bottomBounds = { 0, SEAM_Y, BOARD_WIDTH, BOARD_HEIGHT - SEAM_Y };
    territory_init(&bf->territories[SIDE_BOTTOM], SIDE_BOTTOM, bottomBounds,
                   bottomBiome, &biomeDefs[bottomBiome], tileSize, seedBottom);

    // Initialize top territory (P2): {0, 0, 1080, 960} (per D-02)
    Rectangle topBounds = { 0, 0, BOARD_WIDTH, SEAM_Y };
    territory_init(&bf->territories[SIDE_TOP], SIDE_TOP, topBounds,
                   topBiome, &biomeDefs[topBiome], tileSize, seedTop);

    // Generate canonical lane waypoints for both sides
    generate_canonical_waypoints(bf, SIDE_BOTTOM);
    generate_canonical_waypoints(bf, SIDE_TOP);

    // Initialize entity registry
    bf->entityCount = 0;
}

void bf_cleanup(Battlefield *bf) {
    tilemap_free(&bf->territories[SIDE_BOTTOM].tilemap);
    tilemap_free(&bf->territories[SIDE_TOP].tilemap);

    // Do NOT destroy entities here -- they are owned by the caller's lifecycle,
    // matching the current Player pattern.
    bf->entityCount = 0;
}

void bf_add_entity(Battlefield *bf, Entity *e) {
    if (bf->entityCount >= MAX_ENTITIES * 2) {
        fprintf(stderr, "[Battlefield] Entity limit reached (%d)\n", MAX_ENTITIES * 2);
        return;
    }
    bf->entities[bf->entityCount++] = e;
}

void bf_remove_entity(Battlefield *bf, int entityID) {
    for (int i = 0; i < bf->entityCount; i++) {
        if (bf->entities[i]->id == entityID) {
            // Swap with last element
            bf->entities[i] = bf->entities[bf->entityCount - 1];
            bf->entityCount--;
            // Do NOT call entity_destroy (caller's responsibility)
            return;
        }
    }
}

Entity *bf_find_entity(Battlefield *bf, int entityID) {
    for (int i = 0; i < bf->entityCount; i++) {
        if (bf->entities[i]->id == entityID) {
            return bf->entities[i];
        }
    }
    return NULL;
}

int bf_build_update_order(const Battlefield *bf, int *outIndices) {
    if (!bf || !outIndices) return 0;
    int count = bf->entityCount;
    for (int i = 0; i < count; i++) outIndices[i] = i;

    // Insertion sort by entity.id ascending. n <= MAX_ENTITIES * 2 = 128 so
    // O(n^2) worst case is trivially cheap, and stable ordering beats
    // quicksort for nearly-sorted per-frame state.
    for (int i = 1; i < count; i++) {
        int key = outIndices[i];
        int keyId = bf->entities[key]->id;
        int j = i - 1;
        while (j >= 0 && bf->entities[outIndices[j]]->id > keyId) {
            outIndices[j + 1] = outIndices[j];
            j--;
        }
        outIndices[j + 1] = key;
    }
    return count;
}

Territory *bf_territory_at(Battlefield *bf, CanonicalPos pos) {
    BattleSide side = bf_side_for_pos(pos, bf->seamY);
    return &bf->territories[side];
}

Territory *bf_territory_for_side(Battlefield *bf, BattleSide side) {
    return &bf->territories[side];
}

CanonicalPos bf_spawn_pos(const Battlefield *bf, BattleSide side, int slotIndex) {
    if (slotIndex < 0 || slotIndex >= NUM_CARD_SLOTS) {
        // Return board center as fallback
        CanonicalPos fallback = { .v = { bf->boardWidth / 2.0f, bf->boardHeight / 2.0f } };
        return fallback;
    }
    return bf->slotSpawnAnchors[side][slotIndex];
}

CanonicalPos bf_waypoint(const Battlefield *bf, BattleSide side, int lane, int waypointIdx) {
    if (lane < 0 || lane >= 3 || waypointIdx < 0 || waypointIdx >= LANE_WAYPOINT_COUNT) {
        // Return board center as fallback
        CanonicalPos fallback = { .v = { bf->boardWidth / 2.0f, bf->boardHeight / 2.0f } };
        return fallback;
    }
    return bf->laneWaypoints[side][lane][waypointIdx];
}

CanonicalPos bf_base_anchor(const Battlefield *bf, BattleSide side) {
    Rectangle play = bf_play_bounds(bf, side);
    CanonicalPos base = {
        .v = {
            bf->boardWidth * 0.5f,
            base_anchor_y_from_play(play, side)
        }
    };
    return base;
}

Rectangle bf_play_bounds(const Battlefield *bf, BattleSide side) {
    // Shortened per-side playable rect: territory minus HAND_UI_DEPTH_PX
    // on the outer (player-facing) edge. X dimensions are preserved so
    // lane-width and lateral math remain unchanged.
    Rectangle play;
    play.x = 0.0f;
    play.width = bf->boardWidth;
    if (side == SIDE_BOTTOM) {
        // Player-edge is the bottom of the board (y = boardHeight).
        // Inset inward from there, keeping the seam edge fixed.
        play.y = bf->seamY;
        play.height = (bf->boardHeight - bf->seamY) - (float)HAND_UI_DEPTH_PX;
    } else {
        // Player-edge is the top of the board (y = 0).
        // Inset downward from there, keeping the seam edge fixed.
        play.y = (float)HAND_UI_DEPTH_PX;
        play.height = bf->seamY - (float)HAND_UI_DEPTH_PX;
    }
    return play;
}
