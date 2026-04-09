//
// Battlefield model -- authoritative world geometry (per D-11)
//
// Owns canonical board geometry, territories, lane waypoints,
// slot spawn anchors, and entity registry. Player retains adapter
// fields during the transition (Plans 02-04); Battlefield is
// the single source of truth for geometry.
//

#ifndef NFC_CARDGAME_BATTLEFIELD_H
#define NFC_CARDGAME_BATTLEFIELD_H

#include "../rendering/tilemap_renderer.h"
#include "../rendering/biome.h"
#include "config.h"

// Raylib's Vector2 is already defined via tilemap_renderer.h -> raylib.h.
// Suppress battlefield_math.h's fallback Vector2 definition.
#ifndef VECTOR2_DEFINED
#define VECTOR2_DEFINED
#endif
#include "battlefield_math.h"
#include "sustenance.h"

// Forward declarations
typedef struct Entity Entity;

// Territory: one half of the canonical board (per D-02, D-13, D-14)
typedef struct {
    BattleSide side;
    Rectangle bounds;           // canonical rect: top={0,0,1080,960}, bottom={0,960,1080,960}
    BiomeType biome;
    const BiomeDef *biomeDef;
    TileMap tilemap;
    TileDef tileDefs[TILE_COUNT];
    int tileDefCount;
    TileDef detailDefs[MAX_DETAIL_DEFS];
    int detailDefCount;
} Territory;

// Battlefield: authoritative world model (per D-11)
typedef struct Battlefield {
    float boardWidth;           // 1080 (per D-01)
    float boardHeight;          // 1920 (per D-01)
    float seamY;               // 960 (per D-02)

    Territory territories[2];   // [SIDE_BOTTOM]=P1, [SIDE_TOP]=P2

    // Canonical lane waypoints: laneWaypoints[side][lane][waypointIdx]
    // All positions are in canonical world space (per D-05)
    CanonicalPos laneWaypoints[2][3][LANE_WAYPOINT_COUNT];

    // Canonical slot spawn anchors: slotSpawnAnchors[side][slot]
    CanonicalPos slotSpawnAnchors[2][NUM_CARD_SLOTS];

    // Sustenance resource nodes (battlefield-owned, not Entity instances)
    SustenanceField sustenanceField;

    // Authoritative entity registry (per D-11)
    Entity *entities[MAX_ENTITIES * 2];  // room for both sides
    int entityCount;
} Battlefield;

// --- Lifecycle ---
// Initialize battlefield with canonical geometry.
// biomeDefs: array of BiomeDef[BIOME_COUNT] from GameState.
// bottomBiome/topBiome: biome assignment for each territory (per D-14).
// tileSize: rendered tile size (DEFAULT_TILE_SIZE * DEFAULT_TILE_SCALE).
// seeds: per-territory random seeds for tilemap generation.
void bf_init(Battlefield *bf, const BiomeDef biomeDefs[],
             BiomeType bottomBiome, BiomeType topBiome,
             float tileSize, unsigned int seedBottom, unsigned int seedTop);

// Free battlefield resources (tilemaps)
void bf_cleanup(Battlefield *bf);

// --- Entity registry ---
void bf_add_entity(Battlefield *bf, Entity *e);
void bf_remove_entity(Battlefield *bf, int entityID);
Entity *bf_find_entity(Battlefield *bf, int entityID);

// --- World queries ---
// Get territory for a given canonical position
Territory *bf_territory_at(Battlefield *bf, CanonicalPos pos);

// Get territory for a given side
Territory *bf_territory_for_side(Battlefield *bf, BattleSide side);

// Get canonical spawn position for a given side and slot
CanonicalPos bf_spawn_pos(const Battlefield *bf, BattleSide side, int slotIndex);

// Get canonical waypoint for a given side, lane, and waypoint index
CanonicalPos bf_waypoint(const Battlefield *bf, BattleSide side, int lane, int waypointIdx);

// Get canonical home-base anchor for a given side.
// Positioned behind the center-lane spawn point by BASE_SPAWN_GAP.
CanonicalPos bf_base_anchor(const Battlefield *bf, BattleSide side);

// Map playerID (0 or 1) to BattleSide (per D-03, D-04: player 0 = SIDE_BOTTOM, player 1 = SIDE_TOP)
BattleSide bf_side_for_player(int playerID);

#endif //NFC_CARDGAME_BATTLEFIELD_H
