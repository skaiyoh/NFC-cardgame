//
// Ore resource node system -- battlefield-owned, tile-snapped placement.
//
// Ore nodes live on a fixed canonical grid (16x15 at 64px) independent of
// biome tilemap cells. Each side always owns exactly ORE_MATCH_COUNT_PER_SIDE
// live nodes. Depleted nodes respawn immediately in the same territory.
//

#ifndef NFC_CARDGAME_ORE_H
#define NFC_CARDGAME_ORE_H

#include "config.h"
#include "battlefield_math.h"
#include <stdint.h>

// Forward declaration -- ore.c reads Battlefield for geometry queries
typedef struct Battlefield Battlefield;

typedef struct {
    int          id;               // stable for the match: side * ORE_MATCH_COUNT_PER_SIDE + slotIndex
    BattleSide   side;
    int          slotIndex;        // 0..ORE_MATCH_COUNT_PER_SIDE-1 within that side
    int          gridRow, gridCol; // ore-grid coordinates (not biome tilemap coords)
    CanonicalPos worldPos;         // center of the ore-grid cell
    bool         active;
    int          claimedByEntityId; // -1 if unclaimed

    // TODO: All ore nodes use a single default template (type=0, value=1,
    // durability=1). Add ore type variety and per-type tuning later.
    int          oreType;       // 0 = default
    int          value;         // ore value awarded on pickup
    int          durability;    // work cycles remaining before depletion
    int          maxDurability; // initial durability (for UI/progress bars)
} OreNode;

typedef struct {
    OreNode  nodes[2][ORE_MATCH_COUNT_PER_SIDE]; // [side][slotIndex]
    uint32_t rngState;                            // dedicated ore RNG (xorshift32)
} OreField;

// --- Debug cell classification ---
// Priority order (first match wins): a cell that fails multiple rules
// reports the highest-priority reason.
//   1. edge blocked
//   2. lane blocked
//   3. base blocked
//   phase 4. spawn-anchor blocked
//   5. node blocked
//   6. valid
typedef enum {
    ORE_CELL_VALID,
    ORE_CELL_EDGE_BLOCKED,
    ORE_CELL_LANE_BLOCKED,
    ORE_CELL_BASE_BLOCKED,
    ORE_CELL_SPAWN_BLOCKED,
    ORE_CELL_NODE_BLOCKED
} OreCellReason;

typedef struct {
    int row, col;
    float centerX, centerY;  // world-space cell center
    OreCellReason reason;
} OreCellDebugInfo;

// Classify a single ore-grid cell under normal placement rules.
// Returns the highest-priority reason the cell is blocked, or ORE_CELL_VALID.
OreCellDebugInfo ore_debug_classify_cell(const Battlefield *bf,
                                         BattleSide side, int row, int col);

// --- Lifecycle ---

// Initialize ore field and populate all nodes for both sides.
// seed: match-level RNG seed (deterministic in tests).
void ore_init(Battlefield *bf, uint32_t seed);

// Reset all ore nodes (e.g. rematch). Re-seeds and re-spawns.
void ore_reset(Battlefield *bf, uint32_t seed);

// --- Queries ---

// Get node by stable ID (0..15). Returns NULL if id is out of range.
OreNode *ore_get_node(Battlefield *bf, int nodeId);

// Find nearest unclaimed active node for a side. Returns NULL if none available.
OreNode *ore_find_nearest_available(Battlefield *bf, BattleSide side, Vector2 from);

// Count active nodes for a side.
int ore_active_count(const Battlefield *bf, BattleSide side);

// --- Claim lifecycle ---

// Claim an unclaimed active node for an entity. Returns false if already claimed,
// inactive, or either ID is invalid. entityId must be non-negative; -1 is
// reserved for "unclaimed".
bool ore_claim(Battlefield *bf, int nodeId, int entityId);

// Release claim on a node. entityId must match the current claimant.
void ore_release_claim(Battlefield *bf, int nodeId, int entityId);

// Release all claims held by an entity (e.g. farmer death/removal).
void ore_release_claims_for_entity(Battlefield *bf, int entityId);

// --- Depletion ---

// Deplete an active node and immediately respawn a replacement in the same
// side's territory. Replacement never reuses the exact depleted cell.
// Returns true if respawn succeeded.
bool ore_deplete_and_respawn(Battlefield *bf, int nodeId);

#endif //NFC_CARDGAME_ORE_H
