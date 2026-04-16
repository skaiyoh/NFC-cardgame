//
// Ore resource node system implementation.
//

#include "battlefield.h"  // must precede ore.h: sets VECTOR2_DEFINED for battlefield_math.h
#include "ore.h"
#include <math.h>
#include <string.h>

// --- Ore grid geometry ---

// Horizontal inset to center 16*64=1024 inside 1080px territory width.
#define ORE_GRID_ORIGIN_X  ((BOARD_WIDTH - ORE_GRID_COLS * ORE_GRID_CELL_SIZE_PX) * 0.5f)

// Canonical world-space center of ore-grid cell (row, col) for a given side.
static CanonicalPos ore_cell_center(BattleSide side, int row, int col) {
    float territoryOriginY = (side == SIDE_BOTTOM) ? (float)SEAM_Y : 0.0f;
    CanonicalPos pos;
    pos.v.x = ORE_GRID_ORIGIN_X + (col + 0.5f) * ORE_GRID_CELL_SIZE_PX;
    pos.v.y = territoryOriginY   + (row + 0.5f) * ORE_GRID_CELL_SIZE_PX;
    return pos;
}

// --- Dedicated RNG (xorshift32) ---

static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

// --- Point-to-segment distance ---

static float point_to_segment_dist(Vector2 p, Vector2 a, Vector2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    float lenSq = dx * dx + dy * dy;
    if (lenSq < 1e-8f) {
        float ex = p.x - a.x;
        float ey = p.y - a.y;
        return sqrtf(ex * ex + ey * ey);
    }
    float t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lenSq;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float projX = a.x + t * dx;
    float projY = a.y + t * dy;
    float ex = p.x - projX;
    float ey = p.y - projY;
    return sqrtf(ex * ex + ey * ey);
}

// --- Candidate tile builder ---

// Temporary storage for candidate tiles during spawn.
typedef struct {
    int row, col;
    CanonicalPos pos;
} OreCandidate;

// Maximum possible candidates: ORE_GRID_ROWS * ORE_GRID_COLS = 240
#define MAX_ORE_CANDIDATES (ORE_GRID_ROWS * ORE_GRID_COLS)

// Classify a single ore-grid cell. Returns ORE_CELL_VALID if the cell passes
// all placement rules, or the highest-priority blocking reason.
// bannedRow/bannedCol: respawn-only exclusion (-1,-1 to skip).
static OreCellReason ore_classify_cell_internal(const Battlefield *bf,
                                                 const OreField *field,
                                                 BattleSide side,
                                                 int row, int col,
                                                 int bannedRow, int bannedCol) {
    // 1. Edge margin (highest priority)
    if (row < ORE_EDGE_MARGIN_CELLS || row >= ORE_GRID_ROWS - ORE_EDGE_MARGIN_CELLS ||
        col < ORE_EDGE_MARGIN_CELLS || col >= ORE_GRID_COLS - ORE_EDGE_MARGIN_CELLS) {
        return ORE_CELL_EDGE_BLOCKED;
    }

    // Banned cell (respawn-only: treat as node-blocked)
    if (row == bannedRow && col == bannedCol) return ORE_CELL_NODE_BLOCKED;

    CanonicalPos cellPos = ore_cell_center(side, row, col);
    float laneClearPx  = ORE_LANE_CLEARANCE_CELLS  * ORE_GRID_CELL_SIZE_PX;
    float baseClearPx  = ORE_BASE_CLEARANCE_CELLS   * ORE_GRID_CELL_SIZE_PX;
    float spawnClearPx = ORE_SPAWN_CLEARANCE_CELLS  * ORE_GRID_CELL_SIZE_PX;
    float nodeClearPx  = ORE_NODE_CLEARANCE_CELLS   * ORE_GRID_CELL_SIZE_PX;

    // 2. Lane clearance (check all 6 lanes: 3 per side)
    for (int s = 0; s < 2; s++) {
        for (int lane = 0; lane < 3; lane++) {
            for (int wp = 0; wp < LANE_WAYPOINT_COUNT - 1; wp++) {
                CanonicalPos wpA = bf_waypoint(bf, (BattleSide)s, lane, wp);
                CanonicalPos wpB = bf_waypoint(bf, (BattleSide)s, lane, wp + 1);
                if (point_to_segment_dist(cellPos.v, wpA.v, wpB.v) < laneClearPx) {
                    return ORE_CELL_LANE_BLOCKED;
                }
            }
        }
    }

    // 3. Base anchor clearance
    CanonicalPos baseAnchor = bf_base_anchor(bf, side);
    if (bf_distance(cellPos, baseAnchor) < baseClearPx) return ORE_CELL_BASE_BLOCKED;

    // phase 4. Spawn anchor clearance
    for (int slot = 0; slot < NUM_CARD_SLOTS; slot++) {
        CanonicalPos spawnAnchor = bf_spawn_pos(bf, side, slot);
        if (bf_distance(cellPos, spawnAnchor) < spawnClearPx) return ORE_CELL_SPAWN_BLOCKED;
    }

    // 5. Inter-node clearance (existing active nodes on this side)
    for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE; i++) {
        const OreNode *existing = &field->nodes[side][i];
        if (!existing->active) continue;
        if (bf_distance(cellPos, existing->worldPos) < nodeClearPx) return ORE_CELL_NODE_BLOCKED;
    }

    return ORE_CELL_VALID;
}

// Build list of valid ore-grid cells for a side.
// Returns candidate count.
static int ore_build_candidates(const Battlefield *bf, const OreField *field,
                                BattleSide side, int bannedRow, int bannedCol,
                                OreCandidate *out) {
    int count = 0;
    for (int r = 0; r < ORE_GRID_ROWS; r++) {
        for (int c = 0; c < ORE_GRID_COLS; c++) {
            if (ore_classify_cell_internal(bf, field, side, r, c,
                                           bannedRow, bannedCol) != ORE_CELL_VALID)
                continue;
            out[count].row = r;
            out[count].col = c;
            out[count].pos = ore_cell_center(side, r, c);
            count++;
        }
    }
    return count;
}

// --- Spawn helpers ---

// Populate all ORE_MATCH_COUNT_PER_SIDE slots for one side.
static void ore_spawn_side(Battlefield *bf, BattleSide side) {
    OreField *field = &bf->oreField;

    for (int slot = 0; slot < ORE_MATCH_COUNT_PER_SIDE; slot++) {
        OreCandidate candidates[MAX_ORE_CANDIDATES];
        int candidateCount = ore_build_candidates(bf, field, side, -1, -1, candidates);

        if (candidateCount == 0) break; // should not happen on this board

        int pick = (int)(xorshift32(&field->rngState) % (uint32_t)candidateCount);
        OreCandidate *chosen = &candidates[pick];

        OreNode *node = &field->nodes[side][slot];
        node->id              = side * ORE_MATCH_COUNT_PER_SIDE + slot;
        node->side            = side;
        node->slotIndex       = slot;
        node->gridRow         = chosen->row;
        node->gridCol         = chosen->col;
        node->worldPos        = chosen->pos;
        node->active          = true;
        node->claimedByEntityId = -1;
        node->oreType         = 0;
        node->value           = FARMER_DEFAULT_ORE_VALUE;
        node->durability      = FARMER_DEFAULT_ORE_DURABILITY;
        node->maxDurability   = FARMER_DEFAULT_ORE_DURABILITY;
    }
}

// --- Public API ---

void ore_init(Battlefield *bf, uint32_t seed) {
    OreField *field = &bf->oreField;
    memset(field, 0, sizeof(OreField));

    // Ensure RNG state is never zero (xorshift32 fixpoint)
    field->rngState = seed ? seed : 1;

    // Initialize all nodes to inactive/unclaimed
    for (int s = 0; s < 2; s++) {
        for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE; i++) {
            field->nodes[s][i].id = s * ORE_MATCH_COUNT_PER_SIDE + i;
            field->nodes[s][i].side = (BattleSide)s;
            field->nodes[s][i].slotIndex = i;
            field->nodes[s][i].active = false;
            field->nodes[s][i].claimedByEntityId = -1;
            field->nodes[s][i].oreType = 0;
            field->nodes[s][i].value = FARMER_DEFAULT_ORE_VALUE;
            field->nodes[s][i].durability = FARMER_DEFAULT_ORE_DURABILITY;
            field->nodes[s][i].maxDurability = FARMER_DEFAULT_ORE_DURABILITY;
        }
    }

    ore_spawn_side(bf, SIDE_BOTTOM);
    ore_spawn_side(bf, SIDE_TOP);
}

void ore_reset(Battlefield *bf, uint32_t seed) {
    ore_init(bf, seed);
}

OreNode *ore_get_node(Battlefield *bf, int nodeId) {
    if (nodeId < 0 || nodeId >= ORE_MATCH_COUNT_PER_SIDE * 2) return NULL;
    int side = nodeId / ORE_MATCH_COUNT_PER_SIDE;
    int slot = nodeId % ORE_MATCH_COUNT_PER_SIDE;
    return &bf->oreField.nodes[side][slot];
}

OreNode *ore_find_nearest_available(Battlefield *bf, BattleSide side, Vector2 from) {
    OreField *field = &bf->oreField;
    OreNode *best = NULL;
    float bestDist = 1e18f;
    CanonicalPos fromPos = { .v = from };

    for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE; i++) {
        OreNode *node = &field->nodes[side][i];
        if (!node->active || node->claimedByEntityId != -1) continue;
        float dist = bf_distance(fromPos, node->worldPos);
        if (dist < bestDist) {
            bestDist = dist;
            best = node;
        }
    }
    return best;
}

int ore_active_count(const Battlefield *bf, BattleSide side) {
    const OreField *field = &bf->oreField;
    int count = 0;
    for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE; i++) {
        if (field->nodes[side][i].active) count++;
    }
    return count;
}

bool ore_claim(Battlefield *bf, int nodeId, int entityId) {
    if (entityId < 0) return false;
    OreNode *node = ore_get_node(bf, nodeId);
    if (!node || !node->active || node->claimedByEntityId != -1) return false;
    node->claimedByEntityId = entityId;
    return true;
}

void ore_release_claim(Battlefield *bf, int nodeId, int entityId) {
    if (entityId < 0) return;
    OreNode *node = ore_get_node(bf, nodeId);
    if (!node) return;
    if (node->claimedByEntityId == entityId) {
        node->claimedByEntityId = -1;
    }
}

void ore_release_claims_for_entity(Battlefield *bf, int entityId) {
    if (entityId < 0) return;
    OreField *field = &bf->oreField;
    for (int s = 0; s < 2; s++) {
        for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE; i++) {
            if (field->nodes[s][i].claimedByEntityId == entityId) {
                field->nodes[s][i].claimedByEntityId = -1;
            }
        }
    }
}

OreCellDebugInfo ore_debug_classify_cell(const Battlefield *bf,
                                         BattleSide side, int row, int col) {
    CanonicalPos center = ore_cell_center(side, row, col);
    OreCellDebugInfo info;
    info.row = row;
    info.col = col;
    info.centerX = center.v.x;
    info.centerY = center.v.y;
    info.reason = ore_classify_cell_internal(bf, &bf->oreField, side,
                                              row, col, -1, -1);
    return info;
}

bool ore_deplete_and_respawn(Battlefield *bf, int nodeId) {
    OreNode *node = ore_get_node(bf, nodeId);
    if (!node || !node->active) return false;

    BattleSide side = node->side;
    int slot = node->slotIndex;
    int oldRow = node->gridRow;
    int oldCol = node->gridCol;
    CanonicalPos oldPos = node->worldPos;

    // Mark depleted
    node->active = false;
    node->claimedByEntityId = -1;

    // Find a new valid position
    OreCandidate candidates[MAX_ORE_CANDIDATES];
    int candidateCount = ore_build_candidates(bf, &bf->oreField, side, oldRow, oldCol, candidates);

    if (candidateCount == 0) {
        // Restore the original node rather than silently reducing live ore count.
        node->active = true;
        node->gridRow = oldRow;
        node->gridCol = oldCol;
        node->worldPos = oldPos;
        return false;
    }

    int pick = (int)(xorshift32(&bf->oreField.rngState) % (uint32_t)candidateCount);
    OreCandidate *chosen = &candidates[pick];

    // Reuse the same slot
    node = &bf->oreField.nodes[side][slot];
    node->gridRow  = chosen->row;
    node->gridCol  = chosen->col;
    node->worldPos = chosen->pos;
    node->active          = true;
    node->claimedByEntityId = -1;
    node->oreType         = 0;
    node->value           = FARMER_DEFAULT_ORE_VALUE;
    node->durability      = FARMER_DEFAULT_ORE_DURABILITY;
    node->maxDurability   = FARMER_DEFAULT_ORE_DURABILITY;

    return true;
}
