//
// Sustenance resource node system implementation.
//

#include "battlefield.h"  // must precede sustenance.h: sets VECTOR2_DEFINED for battlefield_math.h
#include "sustenance.h"
#include <math.h>
#include <string.h>

// --- Sustenance grid geometry ---

// Horizontal inset to center 16*64=1024 inside 1080px territory width.
#define SUSTENANCE_GRID_ORIGIN_X  ((BOARD_WIDTH - SUSTENANCE_GRID_COLS * SUSTENANCE_GRID_CELL_SIZE_PX) * 0.5f)

// Canonical world-space center of sustenance-grid cell (row, col) for a given side.
static CanonicalPos sustenance_cell_center(BattleSide side, int row, int col) {
    float territoryOriginY = (side == SIDE_BOTTOM) ? (float)SEAM_Y : 0.0f;
    CanonicalPos pos;
    pos.v.x = SUSTENANCE_GRID_ORIGIN_X + (col + 0.5f) * SUSTENANCE_GRID_CELL_SIZE_PX;
    pos.v.y = territoryOriginY   + (row + 0.5f) * SUSTENANCE_GRID_CELL_SIZE_PX;
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
} SustenanceCandidate;

// Maximum possible candidates: SUSTENANCE_GRID_ROWS * SUSTENANCE_GRID_COLS = 240
#define MAX_SUSTENANCE_CANDIDATES (SUSTENANCE_GRID_ROWS * SUSTENANCE_GRID_COLS)

// Classify a single sustenance-grid cell. Returns SUSTENANCE_CELL_VALID if the cell passes
// all placement rules, or the highest-priority blocking reason.
// bannedRow/bannedCol: respawn-only exclusion (-1,-1 to skip).
static SustenanceCellReason sustenance_classify_cell_internal(const Battlefield *bf,
                                                 const SustenanceField *field,
                                                 BattleSide side,
                                                 int row, int col,
                                                 int bannedRow, int bannedCol) {
    // 1. Edge margin (highest priority)
    if (row < SUSTENANCE_EDGE_MARGIN_CELLS || row >= SUSTENANCE_GRID_ROWS - SUSTENANCE_EDGE_MARGIN_CELLS ||
        col < SUSTENANCE_EDGE_MARGIN_CELLS || col >= SUSTENANCE_GRID_COLS - SUSTENANCE_EDGE_MARGIN_CELLS) {
        return SUSTENANCE_CELL_EDGE_BLOCKED;
    }

    // Banned cell (respawn-only: treat as node-blocked)
    if (row == bannedRow && col == bannedCol) return SUSTENANCE_CELL_NODE_BLOCKED;

    CanonicalPos cellPos = sustenance_cell_center(side, row, col);
    float laneClearPx  = SUSTENANCE_LANE_CLEARANCE_CELLS  * SUSTENANCE_GRID_CELL_SIZE_PX;
    float baseClearPx  = SUSTENANCE_BASE_CLEARANCE_CELLS   * SUSTENANCE_GRID_CELL_SIZE_PX;
    float spawnClearPx = SUSTENANCE_SPAWN_CLEARANCE_CELLS  * SUSTENANCE_GRID_CELL_SIZE_PX;
    float nodeClearPx  = SUSTENANCE_NODE_CLEARANCE_CELLS   * SUSTENANCE_GRID_CELL_SIZE_PX;

    // 2. Lane clearance (check all 6 lanes: 3 per side)
    for (int s = 0; s < 2; s++) {
        for (int lane = 0; lane < 3; lane++) {
            for (int wp = 0; wp < LANE_WAYPOINT_COUNT - 1; wp++) {
                CanonicalPos wpA = bf_waypoint(bf, (BattleSide)s, lane, wp);
                CanonicalPos wpB = bf_waypoint(bf, (BattleSide)s, lane, wp + 1);
                if (point_to_segment_dist(cellPos.v, wpA.v, wpB.v) < laneClearPx) {
                    return SUSTENANCE_CELL_LANE_BLOCKED;
                }
            }
        }
    }

    // 3. Base anchor clearance
    CanonicalPos baseAnchor = bf_base_anchor(bf, side);
    if (bf_distance(cellPos, baseAnchor) < baseClearPx) return SUSTENANCE_CELL_BASE_BLOCKED;

    // 4. Spawn anchor clearance
    for (int slot = 0; slot < NUM_CARD_SLOTS; slot++) {
        CanonicalPos spawnAnchor = bf_spawn_pos(bf, side, slot);
        if (bf_distance(cellPos, spawnAnchor) < spawnClearPx) return SUSTENANCE_CELL_SPAWN_BLOCKED;
    }

    // 5. Inter-node clearance (existing active nodes on this side)
    for (int i = 0; i < SUSTENANCE_MATCH_COUNT_PER_SIDE; i++) {
        const SustenanceNode *existing = &field->nodes[side][i];
        if (!existing->active) continue;
        if (bf_distance(cellPos, existing->worldPos) < nodeClearPx) return SUSTENANCE_CELL_NODE_BLOCKED;
    }

    return SUSTENANCE_CELL_VALID;
}

// Build list of valid sustenance-grid cells for a side.
// Returns candidate count.
static int sustenance_build_candidates(const Battlefield *bf, const SustenanceField *field,
                                BattleSide side, int bannedRow, int bannedCol,
                                SustenanceCandidate *out) {
    int count = 0;
    for (int r = 0; r < SUSTENANCE_GRID_ROWS; r++) {
        for (int c = 0; c < SUSTENANCE_GRID_COLS; c++) {
            if (sustenance_classify_cell_internal(bf, field, side, r, c,
                                           bannedRow, bannedCol) != SUSTENANCE_CELL_VALID)
                continue;
            out[count].row = r;
            out[count].col = c;
            out[count].pos = sustenance_cell_center(side, r, c);
            count++;
        }
    }
    return count;
}

// --- Spawn helpers ---

// Populate all SUSTENANCE_MATCH_COUNT_PER_SIDE slots for one side.
static void sustenance_spawn_side(Battlefield *bf, BattleSide side) {
    SustenanceField *field = &bf->sustenanceField;

    for (int slot = 0; slot < SUSTENANCE_MATCH_COUNT_PER_SIDE; slot++) {
        SustenanceCandidate candidates[MAX_SUSTENANCE_CANDIDATES];
        int candidateCount = sustenance_build_candidates(bf, field, side, -1, -1, candidates);

        if (candidateCount == 0) break; // should not happen on this board

        int pick = (int)(xorshift32(&field->rngState) % (uint32_t)candidateCount);
        SustenanceCandidate *chosen = &candidates[pick];

        SustenanceNode *node = &field->nodes[side][slot];
        node->id              = side * SUSTENANCE_MATCH_COUNT_PER_SIDE + slot;
        node->side            = side;
        node->slotIndex       = slot;
        node->gridRow         = chosen->row;
        node->gridCol         = chosen->col;
        node->worldPos        = chosen->pos;
        node->active          = true;
        node->claimedByEntityId = -1;
        node->sustenanceType         = 0;
        node->value           = FARMER_DEFAULT_SUSTENANCE_VALUE;
        node->durability      = FARMER_DEFAULT_SUSTENANCE_DURABILITY;
        node->maxDurability   = FARMER_DEFAULT_SUSTENANCE_DURABILITY;
    }
}

// --- Public API ---

void sustenance_init(Battlefield *bf, uint32_t seed) {
    SustenanceField *field = &bf->sustenanceField;
    memset(field, 0, sizeof(SustenanceField));

    // Ensure RNG state is never zero (xorshift32 fixpoint)
    field->rngState = seed ? seed : 1;

    // Initialize all nodes to inactive/unclaimed
    for (int s = 0; s < 2; s++) {
        for (int i = 0; i < SUSTENANCE_MATCH_COUNT_PER_SIDE; i++) {
            field->nodes[s][i].id = s * SUSTENANCE_MATCH_COUNT_PER_SIDE + i;
            field->nodes[s][i].side = (BattleSide)s;
            field->nodes[s][i].slotIndex = i;
            field->nodes[s][i].active = false;
            field->nodes[s][i].claimedByEntityId = -1;
            field->nodes[s][i].sustenanceType = 0;
            field->nodes[s][i].value = FARMER_DEFAULT_SUSTENANCE_VALUE;
            field->nodes[s][i].durability = FARMER_DEFAULT_SUSTENANCE_DURABILITY;
            field->nodes[s][i].maxDurability = FARMER_DEFAULT_SUSTENANCE_DURABILITY;
        }
    }

    sustenance_spawn_side(bf, SIDE_BOTTOM);
    sustenance_spawn_side(bf, SIDE_TOP);
}

void sustenance_reset(Battlefield *bf, uint32_t seed) {
    sustenance_init(bf, seed);
}

SustenanceNode *sustenance_get_node(Battlefield *bf, int nodeId) {
    if (nodeId < 0 || nodeId >= SUSTENANCE_MATCH_COUNT_PER_SIDE * 2) return NULL;
    int side = nodeId / SUSTENANCE_MATCH_COUNT_PER_SIDE;
    int slot = nodeId % SUSTENANCE_MATCH_COUNT_PER_SIDE;
    return &bf->sustenanceField.nodes[side][slot];
}

SustenanceNode *sustenance_find_nearest_available(Battlefield *bf, BattleSide side, Vector2 from) {
    SustenanceField *field = &bf->sustenanceField;
    SustenanceNode *best = NULL;
    float bestDist = 1e18f;
    CanonicalPos fromPos = { .v = from };

    for (int i = 0; i < SUSTENANCE_MATCH_COUNT_PER_SIDE; i++) {
        SustenanceNode *node = &field->nodes[side][i];
        if (!node->active || node->claimedByEntityId != -1) continue;
        float dist = bf_distance(fromPos, node->worldPos);
        if (dist < bestDist) {
            bestDist = dist;
            best = node;
        }
    }
    return best;
}

int sustenance_active_count(const Battlefield *bf, BattleSide side) {
    const SustenanceField *field = &bf->sustenanceField;
    int count = 0;
    for (int i = 0; i < SUSTENANCE_MATCH_COUNT_PER_SIDE; i++) {
        if (field->nodes[side][i].active) count++;
    }
    return count;
}

bool sustenance_claim(Battlefield *bf, int nodeId, int entityId) {
    if (entityId < 0) return false;
    SustenanceNode *node = sustenance_get_node(bf, nodeId);
    if (!node || !node->active || node->claimedByEntityId != -1) return false;
    node->claimedByEntityId = entityId;
    return true;
}

void sustenance_release_claim(Battlefield *bf, int nodeId, int entityId) {
    if (entityId < 0) return;
    SustenanceNode *node = sustenance_get_node(bf, nodeId);
    if (!node) return;
    if (node->claimedByEntityId == entityId) {
        node->claimedByEntityId = -1;
    }
}

void sustenance_release_claims_for_entity(Battlefield *bf, int entityId) {
    if (entityId < 0) return;
    SustenanceField *field = &bf->sustenanceField;
    for (int s = 0; s < 2; s++) {
        for (int i = 0; i < SUSTENANCE_MATCH_COUNT_PER_SIDE; i++) {
            if (field->nodes[s][i].claimedByEntityId == entityId) {
                field->nodes[s][i].claimedByEntityId = -1;
            }
        }
    }
}

SustenanceCellDebugInfo sustenance_debug_classify_cell(const Battlefield *bf,
                                         BattleSide side, int row, int col) {
    CanonicalPos center = sustenance_cell_center(side, row, col);
    SustenanceCellDebugInfo info;
    info.row = row;
    info.col = col;
    info.centerX = center.v.x;
    info.centerY = center.v.y;
    info.reason = sustenance_classify_cell_internal(bf, &bf->sustenanceField, side,
                                              row, col, -1, -1);
    return info;
}

bool sustenance_deplete_and_respawn(Battlefield *bf, int nodeId) {
    SustenanceNode *node = sustenance_get_node(bf, nodeId);
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
    SustenanceCandidate candidates[MAX_SUSTENANCE_CANDIDATES];
    int candidateCount = sustenance_build_candidates(bf, &bf->sustenanceField, side, oldRow, oldCol, candidates);

    if (candidateCount == 0) {
        // Restore the original node rather than silently reducing live sustenance count.
        node->active = true;
        node->gridRow = oldRow;
        node->gridCol = oldCol;
        node->worldPos = oldPos;
        return false;
    }

    int pick = (int)(xorshift32(&bf->sustenanceField.rngState) % (uint32_t)candidateCount);
    SustenanceCandidate *chosen = &candidates[pick];

    // Reuse the same slot
    node = &bf->sustenanceField.nodes[side][slot];
    node->gridRow  = chosen->row;
    node->gridCol  = chosen->col;
    node->worldPos = chosen->pos;
    node->active          = true;
    node->claimedByEntityId = -1;
    node->sustenanceType         = 0;
    node->value           = FARMER_DEFAULT_SUSTENANCE_VALUE;
    node->durability      = FARMER_DEFAULT_SUSTENANCE_DURABILITY;
    node->maxDurability   = FARMER_DEFAULT_SUSTENANCE_DURABILITY;

    return true;
}
