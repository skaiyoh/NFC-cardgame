//
// Sustenance resource node system -- battlefield-owned, tile-snapped placement.
//
// Sustenance nodes live on a fixed canonical grid (16x15 at 64px) independent of
// biome tilemap cells. Each side always owns exactly SUSTENANCE_MATCH_COUNT_PER_SIDE
// live nodes. Depleted nodes respawn immediately in the same territory.
//

#ifndef NFC_CARDGAME_SUSTENANCE_H
#define NFC_CARDGAME_SUSTENANCE_H

#include "config.h"
#include "battlefield_math.h"
#include <stdint.h>

// Forward declaration -- sustenance.c reads Battlefield for geometry queries
typedef struct Battlefield Battlefield;

typedef struct {
    int          id;               // stable for the match: side * SUSTENANCE_MATCH_COUNT_PER_SIDE + slotIndex
    BattleSide   side;
    int          slotIndex;        // 0..SUSTENANCE_MATCH_COUNT_PER_SIDE-1 within that side
    int          gridRow, gridCol; // sustenance-grid coordinates (not biome tilemap coords)
    CanonicalPos worldPos;         // center of the sustenance-grid cell
    bool         active;
    int          claimedByEntityId; // -1 if unclaimed

    // TODO: All sustenance nodes use a single default template (type=0, value=1,
    // durability=1). Add sustenance type variety and per-type tuning later.
    int          sustenanceType;       // 0 = default
    int          value;         // sustenance value awarded on pickup
    int          durability;    // work cycles remaining before depletion
    int          maxDurability; // initial durability (for UI/progress bars)
} SustenanceNode;

typedef struct {
    SustenanceNode  nodes[2][SUSTENANCE_MATCH_COUNT_PER_SIDE]; // [side][slotIndex]
    uint32_t rngState;                            // dedicated sustenance RNG (xorshift32)
} SustenanceField;

// --- Debug cell classification ---
// Priority order (first match wins): a cell that fails multiple rules
// reports the highest-priority reason.
//   1. edge blocked       (grid-margin rejection)
//   2. out of play         (cell center falls outside bf_play_bounds, i.e.
//                           inside the hand-bar zone at the player's outer edge)
//   3. lane blocked
//   4. base blocked
//   5. spawn-anchor blocked
//   6. node blocked
//   7. valid
typedef enum {
    SUSTENANCE_CELL_VALID,
    SUSTENANCE_CELL_EDGE_BLOCKED,
    SUSTENANCE_CELL_OUT_OF_PLAY,
    SUSTENANCE_CELL_LANE_BLOCKED,
    SUSTENANCE_CELL_BASE_BLOCKED,
    SUSTENANCE_CELL_SPAWN_BLOCKED,
    SUSTENANCE_CELL_NODE_BLOCKED
} SustenanceCellReason;

typedef struct {
    int row, col;
    float centerX, centerY;  // world-space cell center
    SustenanceCellReason reason;
} SustenanceCellDebugInfo;

// Classify a single sustenance-grid cell under normal placement rules.
// Returns the highest-priority reason the cell is blocked, or SUSTENANCE_CELL_VALID.
SustenanceCellDebugInfo sustenance_debug_classify_cell(const Battlefield *bf,
                                         BattleSide side, int row, int col);

// --- Lifecycle ---

// Initialize sustenance field and populate all nodes for both sides.
// seed: match-level RNG seed (deterministic in tests).
void sustenance_init(Battlefield *bf, uint32_t seed);

// Reset all sustenance nodes (e.g. rematch). Re-seeds and re-spawns.
void sustenance_reset(Battlefield *bf, uint32_t seed);

// --- Queries ---

// Get node by stable ID (0..15). Returns NULL if id is out of range.
SustenanceNode *sustenance_get_node(Battlefield *bf, int nodeId);

// Find nearest unclaimed active node for a side. Returns NULL if none available.
SustenanceNode *sustenance_find_nearest_available(Battlefield *bf, BattleSide side, Vector2 from);

// Count active nodes for a side.
int sustenance_active_count(const Battlefield *bf, BattleSide side);

// --- Claim lifecycle ---

// Claim an unclaimed active node for an entity. Returns false if already claimed,
// inactive, or either ID is invalid. entityId must be non-negative; -1 is
// reserved for "unclaimed".
bool sustenance_claim(Battlefield *bf, int nodeId, int entityId);

// Release claim on a node. entityId must match the current claimant.
void sustenance_release_claim(Battlefield *bf, int nodeId, int entityId);

// Release all claims held by an entity (e.g. farmer death/removal).
void sustenance_release_claims_for_entity(Battlefield *bf, int entityId);

// --- Depletion ---

// Deplete an active node and immediately respawn a replacement in the same
// side's territory. Replacement never reuses the exact depleted cell.
// Returns true if respawn succeeded.
bool sustenance_deplete_and_respawn(Battlefield *bf, int nodeId);

#endif //NFC_CARDGAME_SUSTENANCE_H
