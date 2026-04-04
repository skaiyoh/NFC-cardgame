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
static void generate_canonical_waypoints(Battlefield *bf, BattleSide side) {
    Territory *t = &bf->territories[side];
    float laneWidth = t->bounds.width / 3.0f;

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
                float localY;

                if (side == SIDE_BOTTOM) {
                    // P1 spawn at 80% depth from territory top
                    localY = t->bounds.y + t->bounds.height * 0.8f;
                } else {
                    // P2 spawn in local space: also at 80% from top of territory
                    // In canonical terms P2 territory is {0,0,1080,960}
                    // P2 base is at y=0 (own edge), forward is toward y=960
                    // Spawn at 20% from base = 80% from front
                    localY = t->bounds.y + t->bounds.height * 0.2f;
                }

                SideLocalPos local = { .v = { localX, localY }, .side = side };
                bf->laneWaypoints[side][lane][wp] = bf_to_canonical(local, bf->boardWidth);
                continue;
            }

            // Normalized parameter: 0.0 at spawn, 1.0 at enemy base
            t_param = (float)wp / (float)(LANE_WAYPOINT_COUNT - 1);
            depth = spawnDepth + (endDepth - spawnDepth) * t_param;

            // Compute position in side-local space
            float localX = t->bounds.x + (slot + 0.5f) * laneWidth;

            float localY;
            if (side == SIDE_BOTTOM) {
                // P1: y decreases as depth increases (toward enemy at top)
                // Formula from player_lane_pos: y = area.y + area.height * (0.9 - depth * 0.8)
                localY = t->bounds.y + t->bounds.height * (0.9f - depth * 0.8f);
            } else {
                // P2: y increases as depth increases (toward enemy at bottom, in canonical terms)
                // In P2's local territory {0,0,1080,960}:
                //   depth=0 => near own base (y small)
                //   depth increases => toward seam and beyond
                // Use mirrored formula: y = area.y + area.height * (0.1 + depth * 0.8)
                localY = t->bounds.y + t->bounds.height * (0.1f + depth * 0.8f);
            }

            // Convert to canonical coordinates FIRST, then apply bow in canonical space.
            // Applying bow before bf_to_canonical would flip the bow direction for SIDE_TOP
            // because bf_to_canonical mirrors X (per D-06).
            SideLocalPos local = { .v = { localX, localY }, .side = side };
            CanonicalPos canonical = bf_to_canonical(local, bf->boardWidth);

            // Apply bow offset in canonical space (sign is consistent for both sides)
            canonical.v.x += bow_offset(lane, t_param, laneWidth);
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
    // Center lane (lane 1), first waypoint = spawn position
    CanonicalPos spawn = bf->laneWaypoints[side][1][0];
    CanonicalPos base = spawn;
    if (side == SIDE_TOP) {
        base.v.y = spawn.v.y - BASE_SPAWN_GAP;
    } else {
        base.v.y = spawn.v.y + BASE_SPAWN_GAP;
    }
    return base;
}
