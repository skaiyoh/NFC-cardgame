//
// Per-frame flow-field navigation cache. See nav_frame.h for the public API
// contract and the integration model. Phase 1 implements:
//   - NavFrame lifecycle (init / destroy / begin_frame)
//   - Grid coordinate helpers
//   - Reverse 8-neighbor Dijkstra kernel with a reusable binary min-heap
//   - Lane-march field builder with home-half corridor masking
//
// Target-field, free-goal-field, and ally/enemy density rasterization land in
// Phase 2 when NavFrame is wired into game_update().
//

#include "nav_frame.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#include "../core/battlefield.h"

// Compile-time safety check for the center-based edge invariant documented
// in nav_frame.h. If a future unit's body radius exceeds the clearance the
// moat provides, this line fails to compile and forces a deliberate review
// of NAV_EDGE_MOAT_CELLS.
_Static_assert(
    NAV_MAX_MOBILE_BODY_RADIUS <= (NAV_EDGE_MOAT_CELLS * NAV_CELL_SIZE + NAV_CELL_SIZE / 2),
    "NAV_EDGE_MOAT_CELLS too small for NAV_MAX_MOBILE_BODY_RADIUS; see nav_frame.h");

#define NAV_HEAP_CAPACITY ((int32_t)(sizeof(((NavFrame*)0)->heapStorage) / sizeof(NavHeapNode)))

// ---------- Coordinate helpers ----------

static inline int32_t nav_clampi(int32_t v, int32_t lo, int32_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

bool nav_in_bounds(int32_t col, int32_t row) {
    return col >= 0 && col < NAV_COLS && row >= 0 && row < NAV_ROWS;
}

int32_t nav_index(int32_t col, int32_t row) {
    return row * NAV_COLS + col;
}

NavCellCoord nav_cell_coord(int32_t cellIndex) {
    NavCellCoord c;
    c.col = (int16_t)(cellIndex % NAV_COLS);
    c.row = (int16_t)(cellIndex / NAV_COLS);
    return c;
}

void nav_cell_center(int32_t cellIndex, float *outX, float *outY) {
    NavCellCoord c = nav_cell_coord(cellIndex);
    if (outX) *outX = (float)c.col * (float)NAV_CELL_SIZE + (float)NAV_CELL_SIZE * 0.5f;
    if (outY) *outY = (float)c.row * (float)NAV_CELL_SIZE + (float)NAV_CELL_SIZE * 0.5f;
}

int32_t nav_cell_index_for_world(float worldX, float worldY) {
    int32_t col = (int32_t)floorf(worldX / (float)NAV_CELL_SIZE);
    int32_t row = (int32_t)floorf(worldY / (float)NAV_CELL_SIZE);
    col = nav_clampi(col, 0, NAV_COLS - 1);
    row = nav_clampi(row, 0, NAV_ROWS - 1);
    return nav_index(col, row);
}

// ---------- Lifecycle ----------

void nav_frame_init(NavFrame *nav) {
    if (!nav) return;
    memset(nav, 0, sizeof(*nav));
}

void nav_frame_destroy(NavFrame *nav) {
    (void)nav;
}

// Rebuild the static obstacle mask for a fresh frame. Phase 1 rasterizes the
// one-cell outer border so every flow field has a guaranteed hard moat at
// the board edge -- without it, flow integration can legally path an entity
// into a cell that clips past the canonical board bounds, which the old
// candidate-fan pathfinder would reject via radius-against-edge checks.
// Phase 2 extends this with NAV_PROFILE_STATIC entity footprints from bf.
static void nav_rebuild_static_blockers(NavFrame *nav, const Battlefield *bf) {
    (void)bf;
    memset(nav->staticBlockers.blocked, 0, sizeof(nav->staticBlockers.blocked));
    for (int32_t i = 0; i < NAV_CELLS; ++i) {
        nav->staticBlockers.blockerSrc[i] = NAV_BLOCKER_SRC_NONE;
    }
    for (int32_t col = 0; col < NAV_COLS; ++col) {
        nav->staticBlockers.blocked[nav_index(col, 0)] = 1;
        nav->staticBlockers.blocked[nav_index(col, NAV_ROWS - 1)] = 1;
    }
    for (int32_t row = 0; row < NAV_ROWS; ++row) {
        nav->staticBlockers.blocked[nav_index(0, row)] = 1;
        nav->staticBlockers.blocked[nav_index(NAV_COLS - 1, row)] = 1;
    }
}

void nav_begin_frame(NavFrame *nav, const Battlefield *bf) {
    if (!nav) return;
    nav->frameCounter++;
    nav->initialized = true;

    nav_rebuild_static_blockers(nav, bf);

    memset(nav->density, 0, sizeof(nav->density));
    for (int32_t i = 0; i < NAV_ENTITY_SNAP_CAPACITY; ++i) {
        nav->entityPosId[i] = NAV_ENTITY_ID_NONE;
    }

    for (int s = 0; s < 2; ++s) {
        for (int l = 0; l < 3; ++l) {
            nav->laneFields[s][l].built = false;
        }
    }
    for (int i = 0; i < NAV_TARGET_CACHE_CAPACITY; ++i) {
        nav->targetFields[i].built = false;
    }
    for (int i = 0; i < NAV_FREE_GOAL_CACHE_CAPACITY; ++i) {
        nav->freeGoalFields[i].built = false;
    }
    nav->targetCacheSize = 0;
    nav->freeGoalCacheSize = 0;
    nav->heapSize = 0;
}

// ---------- Blocker queries ----------

bool nav_cell_is_static_blocked(const NavFrame *nav, int32_t cellIndex) {
    if (!nav) return true;
    if (cellIndex < 0 || cellIndex >= NAV_CELLS) return true;
    return nav->staticBlockers.blocked[cellIndex] != 0;
}

// ---------- Binary min-heap ----------

// Standard array-backed binary min-heap. `heap` stores NavHeapNodes indexed
// from 0. Parent = (i-1)/2; left child = 2i+1; right child = 2i+2.

static void nav_heap_reset(NavFrame *nav) {
    nav->heapSize = 0;
}

static void nav_heap_push(NavFrame *nav, int32_t cell, int32_t dist) {
    // Lazy-deletion Dijkstra: same cell can be enqueued up to 8 times on
    // the 8-neighbor grid. Capacity is 9 * NAV_CELLS; overflow is a bug,
    // not a recoverable condition.
    assert(nav->heapSize < NAV_HEAP_CAPACITY);
    int32_t i = nav->heapSize++;
    NavHeapNode *heap = nav->heapStorage;
    heap[i].cell = cell;
    heap[i].dist = dist;
    while (i > 0) {
        int32_t parent = (i - 1) / 2;
        if (heap[parent].dist <= heap[i].dist) break;
        NavHeapNode tmp = heap[parent];
        heap[parent] = heap[i];
        heap[i] = tmp;
        i = parent;
    }
}

static NavHeapNode nav_heap_pop(NavFrame *nav) {
    NavHeapNode *heap = nav->heapStorage;
    NavHeapNode top = heap[0];
    nav->heapSize--;
    if (nav->heapSize > 0) {
        heap[0] = heap[nav->heapSize];
        int32_t i = 0;
        for (;;) {
            int32_t l = 2 * i + 1;
            int32_t r = 2 * i + 2;
            int32_t smallest = i;
            if (l < nav->heapSize && heap[l].dist < heap[smallest].dist) smallest = l;
            if (r < nav->heapSize && heap[r].dist < heap[smallest].dist) smallest = r;
            if (smallest == i) break;
            NavHeapNode tmp = heap[smallest];
            heap[smallest] = heap[i];
            heap[i] = tmp;
            i = smallest;
        }
    }
    return top;
}

static bool nav_heap_empty(const NavFrame *nav) {
    return nav->heapSize == 0;
}

// ---------- Dijkstra kernel ----------

// Fixed 8-neighbor offsets with matching step cost. Order is deterministic
// (N, E, S, W, then the four diagonals) so that tie-breaks in neighbor
// selection are stable across builds.
typedef struct {
    int8_t dcol;
    int8_t drow;
    int16_t cost;
} NavNeighbor;

static const NavNeighbor NAV_NEIGHBORS[8] = {
    { 0, -1, NAV_COST_ORTHO },     // N
    { 1,  0, NAV_COST_ORTHO },     // E
    { 0,  1, NAV_COST_ORTHO },     // S
    {-1,  0, NAV_COST_ORTHO },     // W
    { 1, -1, NAV_COST_DIAGONAL },  // NE
    { 1,  1, NAV_COST_DIAGONAL },  // SE
    {-1,  1, NAV_COST_DIAGONAL },  // SW
    {-1, -1, NAV_COST_DIAGONAL },  // NW
};

// True for the diagonal neighbor indices (phase 4..7) in NAV_NEIGHBORS.
static inline bool nav_neighbor_is_diagonal(int n) {
    return n >= 4;
}

// Per-cell density cost contribution from the perspective of `allySide`.
// Ally-occupied cells add NAV_ALLY_OCCUPIED_COST; enemy-occupied cells add
// NAV_ENEMY_OCCUPIED_COST. Adjacent-cell (NEAR) contribution is added by
// scanning the phase 4 orthogonal neighbors for their occupancy, weighted with
// NAV_ALLY_NEAR_COST / NAV_ENEMY_NEAR_COST. The result is the additional
// integer penalty tacked onto a relaxation that steps INTO `cell`.
static int32_t nav_density_penalty(const NavFrame *nav, int32_t cell,
                                    int allySide) {
    int enemySide = 1 - allySide;
    int32_t penalty = 0;
    if (nav->density[allySide][cell] > 0) penalty += NAV_ALLY_OCCUPIED_COST;
    if (nav->density[enemySide][cell] > 0) penalty += NAV_ENEMY_OCCUPIED_COST;
    NavCellCoord coord = nav_cell_coord(cell);
    static const int8_t NEAR[4][2] = { {0,-1}, {1,0}, {0,1}, {-1,0} };
    for (int i = 0; i < 4; ++i) {
        int32_t nc = coord.col + NEAR[i][0];
        int32_t nr = coord.row + NEAR[i][1];
        if (!nav_in_bounds(nc, nr)) continue;
        int32_t nidx = nav_index(nc, nr);
        if (nav->density[allySide][nidx] > 0) penalty += NAV_ALLY_NEAR_COST;
        if (nav->density[enemySide][nidx] > 0) penalty += NAV_ENEMY_NEAR_COST;
    }
    return penalty;
}

// Run reverse Dijkstra on `field` starting from every cell already assigned
// a finite distance (seed cells). Density-cost shaping is read from nav's
// frozen per-side density snapshot; allies are cheap, enemies are costly,
// computed from the perspective of field->perspectiveSide.
static void nav_integrate_field(NavFrame *nav, NavField *field) {
    int allySide = field->perspectiveSide;
    if (allySide != 0 && allySide != 1) allySide = 0;

    nav_heap_reset(nav);
    for (int32_t i = 0; i < NAV_CELLS; ++i) {
        if (field->distance[i] != NAV_DIST_UNREACHABLE) {
            nav_heap_push(nav, i, field->distance[i]);
        }
    }
    while (!nav_heap_empty(nav)) {
        NavHeapNode node = nav_heap_pop(nav);
        if (node.dist != field->distance[node.cell]) {
            // Stale heap entry from a prior relaxation.
            continue;
        }
        NavCellCoord coord = nav_cell_coord(node.cell);
        for (int n = 0; n < 8; ++n) {
            int32_t ncol = coord.col + NAV_NEIGHBORS[n].dcol;
            int32_t nrow = coord.row + NAV_NEIGHBORS[n].drow;
            if (!nav_in_bounds(ncol, nrow)) continue;
            int32_t nidx = nav_index(ncol, nrow);
            if (field->hardBlocked[nidx]) continue;
            // Corner-cut prevention: a diagonal step is only legal if both
            // of the two adjacent orthogonal cells are also passable. This
            // stops flow fields from squeezing through the shared corner of
            // two hard-blocked cells (e.g. around a building footprint) in
            // ways the old radius-aware blocker test never allowed.
            if (nav_neighbor_is_diagonal(n)) {
                int32_t orthoColIdx = nav_index(coord.col + NAV_NEIGHBORS[n].dcol,
                                                coord.row);
                int32_t orthoRowIdx = nav_index(coord.col,
                                                coord.row + NAV_NEIGHBORS[n].drow);
                if (field->hardBlocked[orthoColIdx]) continue;
                if (field->hardBlocked[orthoRowIdx]) continue;
            }
            int32_t step = NAV_NEIGHBORS[n].cost
                         + nav_density_penalty(nav, nidx, allySide);
            int32_t nd = node.dist + step;
            if (nd < field->distance[nidx]) {
                field->distance[nidx] = nd;
                nav_heap_push(nav, nidx, nd);
            }
        }
    }
}

// ---------- Lane corridor masking ----------

// Squared distance from point (px,py) to segment (ax,ay)-(bx,by).
static float nav_point_segment_dist_sq(float px, float py,
                                        float ax, float ay,
                                        float bx, float by) {
    float abx = bx - ax;
    float aby = by - ay;
    float apx = px - ax;
    float apy = py - ay;
    float abLenSq = abx * abx + aby * aby;
    float t = 0.0f;
    if (abLenSq > 1e-6f) {
        t = (apx * abx + apy * aby) / abLenSq;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    }
    float closeX = ax + abx * t;
    float closeY = ay + aby * t;
    float dx = px - closeX;
    float dy = py - closeY;
    return dx * dx + dy * dy;
}

// Distance from a world point to the lane polyline [side][lane], measured as
// the minimum over all 7 segments connecting the 8 authored waypoints.
static float nav_distance_to_lane_polyline(const Battlefield *bf,
                                            int side, int lane,
                                            float px, float py) {
    float bestSq = 1e30f;
    for (int i = 0; i < LANE_WAYPOINT_COUNT - 1; ++i) {
        float ax = bf->laneWaypoints[side][lane][i].v.x;
        float ay = bf->laneWaypoints[side][lane][i].v.y;
        float bx = bf->laneWaypoints[side][lane][i + 1].v.x;
        float by = bf->laneWaypoints[side][lane][i + 1].v.y;
        float d2 = nav_point_segment_dist_sq(px, py, ax, ay, bx, by);
        if (d2 < bestSq) bestSq = d2;
    }
    return sqrtf(bestSq);
}

// True if a cell lies inside the owning side's home half. SIDE_BOTTOM owns
// y >= SEAM_Y; SIDE_TOP owns y < SEAM_Y.
static bool nav_cell_in_home_half(int32_t cellIndex, int side) {
    NavCellCoord c = nav_cell_coord(cellIndex);
    float cy = (float)c.row * (float)NAV_CELL_SIZE + (float)NAV_CELL_SIZE * 0.5f;
    if (side == 0) {
        return cy >= (float)SEAM_Y;
    }
    return cy < (float)SEAM_Y;
}

// ---------- Seed rasterization ----------

// Seed a field at every unblocked cell in the 3x3 neighborhood around
// (anchorCol, anchorRow) and, if none are found, progressively widen the
// search ring until `maxRadius` cells. Leaves field->hardBlocked untouched
// so the neighbor picker in Phase 3 cannot accidentally path a unit into a
// cell that is still authored as impassable (e.g. a base footprint). If
// the anchor itself is passable it is seeded directly.
//
// Returns the number of seed cells written. Zero means no reachable cell
// exists inside the ring, which is a build-time error and the field will
// contain only NAV_DIST_UNREACHABLE.
static int32_t nav_seed_field_at(NavField *field,
                                  int32_t anchorCol, int32_t anchorRow,
                                  int32_t maxRadius) {
    int32_t seeded = 0;
    if (nav_in_bounds(anchorCol, anchorRow)) {
        int32_t idx = nav_index(anchorCol, anchorRow);
        if (!field->hardBlocked[idx]) {
            field->distance[idx] = 0;
            return 1;
        }
    }
    for (int32_t r = 1; r <= maxRadius && seeded == 0; ++r) {
        for (int32_t dc = -r; dc <= r; ++dc) {
            for (int32_t dr = -r; dr <= r; ++dr) {
                // Only visit the outer ring of this radius; inner cells
                // were covered by prior iterations.
                if (dc != -r && dc != r && dr != -r && dr != r) continue;
                int32_t c = anchorCol + dc;
                int32_t ro = anchorRow + dr;
                if (!nav_in_bounds(c, ro)) continue;
                int32_t idx = nav_index(c, ro);
                if (field->hardBlocked[idx]) continue;
                field->distance[idx] = 0;
                seeded++;
            }
        }
    }
    return seeded;
}

// ---------- Lane field builder ----------

// Build a lane-march flow field seeded from the final authored waypoint of
// [side][lane]. Home-half cells outside the corridor are hard-blocked for
// this field only; the global staticBlockers mask is left untouched.
static void nav_build_lane_field(NavFrame *nav, const Battlefield *bf,
                                  int side, int lane, NavField *field) {
    float seedX = bf->laneWaypoints[side][lane][LANE_WAYPOINT_COUNT - 1].v.x;
    float seedY = bf->laneWaypoints[side][lane][LANE_WAYPOINT_COUNT - 1].v.y;
    field->kind = NAV_GOAL_KIND_LANE_MARCH;
    field->perspectiveSide = (int16_t)side;
    field->anchorX = seedX;
    field->anchorY = seedY;
    field->stopRadius = 0.0f;
    field->innerRadius = 0.0f;
    field->arcCenterDeg = 0.0f;
    field->arcHalfDeg = 0.0f;
    field->targetBodyRadius = 0.0f;
    field->keySide = (int16_t)side;
    field->keyLane = (int16_t)lane;
    field->keyTargetId = -1;
    field->keyGoalXQ = 0;
    field->keyGoalYQ = 0;
    field->keyRangeQ = 0;

    // Seed distance grid + per-field hard-blocked mask.
    for (int32_t i = 0; i < NAV_CELLS; ++i) {
        field->distance[i] = NAV_DIST_UNREACHABLE;
        uint8_t hard = nav->staticBlockers.blocked[i];
        if (!hard && nav_cell_in_home_half(i, side)) {
            float cx, cy;
            nav_cell_center(i, &cx, &cy);
            float d = nav_distance_to_lane_polyline(bf, side, lane, cx, cy);
            if (d > NAV_HOME_LANE_CORRIDOR_RADIUS) {
                hard = 1;
            }
        }
        field->hardBlocked[i] = hard;
    }

    // Seed the goal region at the final authored waypoint on this lane.
    // When that waypoint sits inside a hard-blocked cell (typical for a
    // waypoint anchored near the enemy base footprint), nav_seed_field_at
    // expands outward until it finds unblocked neighbors. It never clears
    // the hardBlocked mask, so the Phase 3 neighbor picker cannot step an
    // entity into a cell that is authored as impassable.
    int32_t seedCell = nav_cell_index_for_world(seedX, seedY);
    NavCellCoord anchor = nav_cell_coord(seedCell);
    int32_t seeded = nav_seed_field_at(field, anchor.col, anchor.row, 4);
    assert(seeded > 0 && "lane field seed region is fully blocked");
    (void)seeded;

    nav_integrate_field(nav, field);
    field->built = true;
}

const NavField *nav_get_or_build_lane_field(NavFrame *nav, const Battlefield *bf,
                                             int side, int lane) {
    if (!nav || !nav->initialized || !bf) return NULL;
    if (side < 0 || side >= 2) return NULL;
    if (lane < 0 || lane >= 3) return NULL;
    NavField *field = &nav->laneFields[side][lane];
    if (!field->built) {
        nav_build_lane_field(nav, bf, side, lane, field);
    }
    return field;
}

const NavField *nav_find_lane_field(const NavFrame *nav, int side, int lane) {
    if (!nav || !nav->initialized) return NULL;
    if (side < 0 || side >= 2) return NULL;
    if (lane < 0 || lane >= 3) return NULL;
    const NavField *field = &nav->laneFields[side][lane];
    return field->built ? field : NULL;
}

// ---------- Target / free-goal field builders ----------

// Populate field->hardBlocked from the global staticBlockers mask and zero
// the distance grid. Used as the common prelude of every target / free-goal
// field build.
static void nav_field_reset_for_build(const NavFrame *nav, NavField *field) {
    for (int32_t i = 0; i < NAV_CELLS; ++i) {
        field->distance[i] = NAV_DIST_UNREACHABLE;
        field->hardBlocked[i] = nav->staticBlockers.blocked[i];
    }
    field->innerRadius = 0.0f;
    field->arcCenterDeg = 0.0f;
    field->arcHalfDeg = 0.0f;
    field->targetBodyRadius = 0.0f;
}

// Normalize an angle to [-180, 180] degrees.
static float nav_wrap_deg(float deg) {
    while (deg > 180.0f) deg -= 360.0f;
    while (deg < -180.0f) deg += 360.0f;
    return deg;
}

// Seed the target front-arc ribbon. Walks the bounding box of the outer
// radius and marks every cell whose center sits within the ribbon AND
// whose bearing from the target lies inside the front-arc angle. The
// outer edge is clamped to exactly `outerRadius` (i.e.
// combat_static_target_attack_radius), so every seeded cell satisfies
// combat_in_range for the attacker; the inner edge is one ribbon
// thickness closer to the target, giving a 2*half-thickness-wide band
// entirely inside combat range.
static int32_t nav_seed_static_arc(NavField *field, const NavTargetGoal *goal) {
    int32_t seeded = 0;
    float maxR = goal->outerRadius;
    int32_t c0 = (int32_t)floorf((goal->targetX - maxR) / (float)NAV_CELL_SIZE);
    int32_t c1 = (int32_t)floorf((goal->targetX + maxR) / (float)NAV_CELL_SIZE);
    int32_t r0 = (int32_t)floorf((goal->targetY - maxR) / (float)NAV_CELL_SIZE);
    int32_t r1 = (int32_t)floorf((goal->targetY + maxR) / (float)NAV_CELL_SIZE);
    if (c0 < 0) c0 = 0;
    if (r0 < 0) r0 = 0;
    if (c1 > NAV_COLS - 1) c1 = NAV_COLS - 1;
    if (r1 > NAV_ROWS - 1) r1 = NAV_ROWS - 1;
    float innerR = goal->outerRadius - 2.0f * NAV_GOAL_RIBBON_HALF_THICKNESS;
    if (innerR < goal->innerRadiusMin) innerR = goal->innerRadiusMin;
    if (innerR < 0.0f) innerR = 0.0f;
    for (int32_t row = r0; row <= r1; ++row) {
        float cellY = (float)row * (float)NAV_CELL_SIZE + (float)NAV_CELL_SIZE * 0.5f;
        for (int32_t col = c0; col <= c1; ++col) {
            float cellX = (float)col * (float)NAV_CELL_SIZE + (float)NAV_CELL_SIZE * 0.5f;
            float dx = cellX - goal->targetX;
            float dy = cellY - goal->targetY;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < innerR || d > maxR) continue;
            float bearingDeg = atan2f(dy, dx) * 180.0f / PI_F;
            float delta = nav_wrap_deg(bearingDeg - goal->arcCenterDeg);
            if (fabsf(delta) > goal->arcHalfDeg) continue;
            int32_t idx = nav_index(col, row);
            if (field->hardBlocked[idx]) continue;
            field->distance[idx] = 0;
            seeded++;
        }
    }
    return seeded;
}

// Seed a one-cell-thick melee ring. outer edge is clamped to exactly
// `outerRadius` (so every seeded cell satisfies combat_in_range); inner
// edge is one cell closer.
static int32_t nav_seed_melee_ring(NavField *field, const NavTargetGoal *goal) {
    int32_t seeded = 0;
    float outerR = goal->outerRadius;
    int32_t c0 = (int32_t)floorf((goal->targetX - outerR) / (float)NAV_CELL_SIZE);
    int32_t c1 = (int32_t)floorf((goal->targetX + outerR) / (float)NAV_CELL_SIZE);
    int32_t r0 = (int32_t)floorf((goal->targetY - outerR) / (float)NAV_CELL_SIZE);
    int32_t r1 = (int32_t)floorf((goal->targetY + outerR) / (float)NAV_CELL_SIZE);
    if (c0 < 0) c0 = 0;
    if (r0 < 0) r0 = 0;
    if (c1 > NAV_COLS - 1) c1 = NAV_COLS - 1;
    if (r1 > NAV_ROWS - 1) r1 = NAV_ROWS - 1;
    float innerR = goal->outerRadius - (float)NAV_CELL_SIZE;
    if (innerR < goal->innerRadiusMin) innerR = goal->innerRadiusMin;
    if (innerR < 0.0f) innerR = 0.0f;
    for (int32_t row = r0; row <= r1; ++row) {
        float cellY = (float)row * (float)NAV_CELL_SIZE + (float)NAV_CELL_SIZE * 0.5f;
        for (int32_t col = c0; col <= c1; ++col) {
            float cellX = (float)col * (float)NAV_CELL_SIZE + (float)NAV_CELL_SIZE * 0.5f;
            float dx = cellX - goal->targetX;
            float dy = cellY - goal->targetY;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < innerR || d > outerR) continue;
            int32_t idx = nav_index(col, row);
            if (field->hardBlocked[idx]) continue;
            field->distance[idx] = 0;
            seeded++;
        }
    }
    return seeded;
}

// Seed a solid disk: every unblocked cell within outerRadius of the goal.
static int32_t nav_seed_disk(NavField *field, const NavTargetGoal *goal) {
    int32_t seeded = 0;
    float r = goal->outerRadius;
    if (r < (float)NAV_CELL_SIZE * 0.5f) r = (float)NAV_CELL_SIZE * 0.5f;
    int32_t c0 = (int32_t)floorf((goal->targetX - r) / (float)NAV_CELL_SIZE);
    int32_t c1 = (int32_t)floorf((goal->targetX + r) / (float)NAV_CELL_SIZE);
    int32_t r0 = (int32_t)floorf((goal->targetY - r) / (float)NAV_CELL_SIZE);
    int32_t r1 = (int32_t)floorf((goal->targetY + r) / (float)NAV_CELL_SIZE);
    if (c0 < 0) c0 = 0;
    if (r0 < 0) r0 = 0;
    if (c1 > NAV_COLS - 1) c1 = NAV_COLS - 1;
    if (r1 > NAV_ROWS - 1) r1 = NAV_ROWS - 1;
    float r2 = r * r;
    for (int32_t row = r0; row <= r1; ++row) {
        float cellY = (float)row * (float)NAV_CELL_SIZE + (float)NAV_CELL_SIZE * 0.5f;
        for (int32_t col = c0; col <= c1; ++col) {
            float cellX = (float)col * (float)NAV_CELL_SIZE + (float)NAV_CELL_SIZE * 0.5f;
            float dx = cellX - goal->targetX;
            float dy = cellY - goal->targetY;
            if (dx * dx + dy * dy > r2) continue;
            int32_t idx = nav_index(col, row);
            if (field->hardBlocked[idx]) continue;
            field->distance[idx] = 0;
            seeded++;
        }
    }
    // Fallback: if the goal disk falls entirely on hard-blocked cells, widen
    // outward until we find at least one seed cell.
    if (seeded == 0) {
        int32_t anchorCell = nav_cell_index_for_world(goal->targetX, goal->targetY);
        NavCellCoord anchor = nav_cell_coord(anchorCell);
        seeded = nav_seed_field_at(field, anchor.col, anchor.row, 4);
    }
    return seeded;
}

// Exact-radius cache key. `keyRangeQ` is the outer radius quantized to a
// 0.25 px grid; that is tight enough that two distinct gameplay radii
// essentially never collide, while keeping the key a small integer. No
// snapping is applied to the seeded geometry, so target fields rasterize
// with the exact caller outerRadius and always agree with combat_in_range.
static int32_t nav_range_q(float radius) {
    if (radius < 0.0f) radius = 0.0f;
    return (int32_t)(radius * 4.0f + 0.5f);
}

static float nav_free_goal_seed_radius(float stopRadius) {
    float seedRadius = stopRadius;
    float minSeedRadius = sqrtf(2.0f) * (float)NAV_CELL_SIZE * 0.5f;
    if (seedRadius < minSeedRadius) {
        seedRadius = minSeedRadius;
    }
    return seedRadius;
}

// ---------- Entity position snapshot (open-addressed hash table) ----------
//
// Entity ids grow monotonically and can exceed MAX_ENTITIES*2 in long
// matches, so the snapshot cannot be a raw-indexed array. We use a small
// open-addressed table of NAV_ENTITY_SNAP_CAPACITY slots (power of two,
// guaranteed <=50% load factor at the MAX_ENTITIES*2 live cap) and linear
// probe from a cheap multiplicative hash.

static inline int32_t nav_entity_probe_slot(int32_t entityId) {
    uint32_t h = (uint32_t)entityId * 2654435761u;  // Knuth multiplicative
    return (int32_t)(h & (NAV_ENTITY_SNAP_CAPACITY - 1));
}

static int32_t nav_entity_snap_find(const NavFrame *nav, int32_t entityId) {
    if (entityId < 0) return -1;
    int32_t slot = nav_entity_probe_slot(entityId);
    for (int32_t probes = 0; probes < NAV_ENTITY_SNAP_CAPACITY; ++probes) {
        int32_t idHere = nav->entityPosId[slot];
        if (idHere == entityId) return slot;
        if (idHere == NAV_ENTITY_ID_NONE) return -1;
        slot = (slot + 1) & (NAV_ENTITY_SNAP_CAPACITY - 1);
    }
    return -1;
}

static void nav_entity_snap_insert(NavFrame *nav, int32_t entityId,
                                     float x, float y) {
    if (entityId < 0) return;
    int32_t slot = nav_entity_probe_slot(entityId);
    for (int32_t probes = 0; probes < NAV_ENTITY_SNAP_CAPACITY; ++probes) {
        int32_t idHere = nav->entityPosId[slot];
        if (idHere == NAV_ENTITY_ID_NONE || idHere == entityId) {
            nav->entityPosId[slot] = entityId;
            nav->entityPosX[slot] = x;
            nav->entityPosY[slot] = y;
            return;
        }
        slot = (slot + 1) & (NAV_ENTITY_SNAP_CAPACITY - 1);
    }
}

// Resolve the target's authoritative world position for the frame. When a
// caller passes a non-negative targetId that matches a live snapshot slot,
// the snapshot wins. Otherwise the caller-supplied coordinates are used as
// a fallback (static buildings, free-goal points, tests with no entity id).
static void nav_resolve_entity_position(const NavFrame *nav,
                                        int32_t entityId,
                                        float fallbackX, float fallbackY,
                                        float *outX, float *outY) {
    float x = fallbackX;
    float y = fallbackY;
    int32_t slot = nav_entity_snap_find(nav, entityId);
    if (slot >= 0) {
        x = nav->entityPosX[slot];
        y = nav->entityPosY[slot];
    }
    if (outX) *outX = x;
    if (outY) *outY = y;
}

static void nav_resolve_target_position(const NavFrame *nav,
                                        const NavTargetGoal *goal,
                                        float *outX, float *outY) {
    nav_resolve_entity_position(nav, goal->targetId,
                                goal->targetX, goal->targetY,
                                outX, outY);
}

static void nav_target_field_stamp_key(NavField *field, const NavTargetGoal *goal,
                                         int32_t rangeQ) {
    field->kind = goal->kind;
    field->perspectiveSide = goal->perspectiveSide;
    field->anchorX = goal->targetX;
    field->anchorY = goal->targetY;
    field->stopRadius = goal->outerRadius;
    field->innerRadius = goal->innerRadiusMin;
    field->arcCenterDeg = goal->arcCenterDeg;
    field->arcHalfDeg = goal->arcHalfDeg;
    field->targetBodyRadius = goal->targetBodyRadius;
    field->keyTargetId = goal->targetId;
    field->keyRangeQ = rangeQ;
    field->keySide = -1;
    field->keyLane = -1;
    field->keyGoalXQ = (int32_t)(goal->targetX * 4.0f + 0.5f);
    field->keyGoalYQ = (int32_t)(goal->targetY * 4.0f + 0.5f);
}

static void nav_free_goal_field_stamp_key(NavField *field,
                                          const NavFreeGoalRequest *request,
                                          int32_t rangeQ) {
    field->kind = NAV_GOAL_KIND_FREE_GOAL;
    field->perspectiveSide = request->perspectiveSide;
    field->anchorX = request->goalX;
    field->anchorY = request->goalY;
    field->stopRadius = request->stopRadius;
    field->innerRadius = 0.0f;
    field->arcCenterDeg = 0.0f;
    field->arcHalfDeg = 0.0f;
    field->targetBodyRadius = 0.0f;
    field->keyTargetId = request->carveTargetId;
    field->keyRangeQ = rangeQ;
    field->keySide = -1;
    field->keyLane = -1;
    field->keyGoalXQ = (int32_t)(request->goalX * 4.0f + 0.5f);
    field->keyGoalYQ = (int32_t)(request->goalY * 4.0f + 0.5f);
}

static void nav_field_carve_target_owned_blockers(const NavFrame *nav,
                                                  NavField *field,
                                                  int32_t targetId,
                                                  float centerX,
                                                  float centerY,
                                                  float innerRadius) {
    if (!nav || !field) return;
    if (targetId < 0) return;

    if (innerRadius < 0.0f) innerRadius = 0.0f;
    float innerSq = innerRadius * innerRadius;

    for (int32_t idx = 0; idx < NAV_CELLS; ++idx) {
        if (nav->staticBlockers.blockerSrc[idx] != targetId) continue;
        if (!field->hardBlocked[idx]) continue;

        NavCellCoord c = nav_cell_coord(idx);
        float cellX = (float)c.col * (float)NAV_CELL_SIZE +
                      (float)NAV_CELL_SIZE * 0.5f;
        float cellY = (float)c.row * (float)NAV_CELL_SIZE +
                      (float)NAV_CELL_SIZE * 0.5f;
        float dx = cellX - centerX;
        float dy = cellY - centerY;
        if (dx * dx + dy * dy >= innerSq) {
            field->hardBlocked[idx] = 0;
        }
    }
}

static void nav_build_target_field(NavFrame *nav, NavField *field,
                                     const NavTargetGoal *goalIn) {
    // Resolve the target position from the frame snapshot when possible,
    // so every attacker pursuing the same target in the same frame seeds
    // its field against the same frozen pivot regardless of update order.
    NavTargetGoal goal = *goalIn;
    nav_resolve_target_position(nav, &goal, &goal.targetX, &goal.targetY);

    // No bucket snapping on outerRadius: the seed set must match the
    // caller's exact combat_in_range threshold. Cache keying uses the
    // quantized radius (0.25 px) so near-identical callers still share
    // the same slot.
    int32_t rangeQ = nav_range_q(goal.outerRadius);

    nav_field_reset_for_build(nav, field);
    nav_target_field_stamp_key(field, &goal, rangeQ);

    // Combat-corridor carve for STATIC_ATTACK. The global staticBlockers
    // mask contains the target's owned hard core. Attackers approaching
    // from outside that core still need approach cells near the combat
    // ring, so we clear every blocker cell this target owns EXCEPT the
    // innermost cells inside the stepper's target-body contact shell.
    // Cells attributed to other static entities are preserved.
    //
    // Iterating the full grid is O(NAV_CELLS) per built target field,
    // which is cheap at 2040 cells and robust against not knowing the
    // target's stamp radius from the goal struct.
    if (goal.kind == NAV_GOAL_KIND_STATIC_ATTACK && goal.targetId >= 0) {
        float innerFloor = goal.targetBodyRadius;
        if (goal.innerRadiusMin > innerFloor) innerFloor = goal.innerRadiusMin;
        field->innerRadius = innerFloor;
        nav_field_carve_target_owned_blockers(nav, field, goal.targetId,
                                              goal.targetX, goal.targetY,
                                              innerFloor);
    }

    int32_t seeded = 0;
    switch (goal.kind) {
        case NAV_GOAL_KIND_STATIC_ATTACK:
            seeded = nav_seed_static_arc(field, &goal);
            break;
        case NAV_GOAL_KIND_MELEE_RING:
            seeded = nav_seed_melee_ring(field, &goal);
            break;
        case NAV_GOAL_KIND_DIRECT_RANGE:
        case NAV_GOAL_KIND_FREE_GOAL:
            seeded = nav_seed_disk(field, &goal);
            break;
        default:
            break;
    }
    // Blocked-goal fallback: if the authored region produced zero seeds
    // (e.g. the target sits pressed against the edge moat or inside another
    // building's footprint), widen outward from the target pivot until we
    // find at least one unblocked cell. Without this, a pursuer stalls on
    // a field of NAV_DIST_UNREACHABLE even though a perfectly legal
    // approach cell exists one step over.
    if (seeded == 0) {
        int32_t anchorCell = nav_cell_index_for_world(goal.targetX, goal.targetY);
        NavCellCoord anchor = nav_cell_coord(anchorCell);
        seeded = nav_seed_field_at(field, anchor.col, anchor.row, 6);
    }
    if (seeded == 0) {
        // Still nothing: leave the field all-unreachable. Callers observe
        // a zero flow and stand in place instead of crashing.
        field->built = true;
        return;
    }
    nav_integrate_field(nav, field);
    field->built = true;
}

const NavField *nav_get_or_build_target_field(NavFrame *nav, const Battlefield *bf,
                                                const NavTargetGoal *goal) {
    if (!nav || !nav->initialized || !bf || !goal) return NULL;
    int perspective = goal->perspectiveSide;
    if (perspective < 0 || perspective > 1) return NULL;
    // Cache key: (targetId, kind, rangeQ (0.25 px), side). rangeQ is
    // derived from the exact caller radius so the seed set always
    // matches combat_in_range at query time.
    int32_t rangeQ = nav_range_q(goal->outerRadius);
    for (int32_t i = 0; i < nav->targetCacheSize; ++i) {
        NavField *f = &nav->targetFields[i];
        if (!f->built) continue;
        if (f->kind != goal->kind) continue;
        if (f->keyTargetId != goal->targetId) continue;
        if (f->keyRangeQ != rangeQ) continue;
        if (f->perspectiveSide != (int16_t)perspective) continue;
        return f;
    }
    if (nav->targetCacheSize >= NAV_TARGET_CACHE_CAPACITY) {
        // Overflow: Phase 3 will log once; Phase 2 fails hard in debug
        // to catch any case where a test inadvertently creates > 128
        // distinct (target, kind, rangeClass, side) keys per frame.
        assert(0 && "NavFrame target field cache overflow");
        return NULL;
    }
    NavField *field = &nav->targetFields[nav->targetCacheSize++];
    nav_build_target_field(nav, field, goal);
    return field;
}

const NavField *nav_find_target_field(const NavFrame *nav,
                                      const NavTargetGoal *goal) {
    if (!nav || !nav->initialized || !goal) return NULL;
    int perspective = goal->perspectiveSide;
    if (perspective < 0 || perspective > 1) return NULL;
    int32_t rangeQ = nav_range_q(goal->outerRadius);
    for (int32_t i = 0; i < nav->targetCacheSize; ++i) {
        const NavField *f = &nav->targetFields[i];
        if (!f->built) continue;
        if (f->kind != goal->kind) continue;
        if (f->keyTargetId != goal->targetId) continue;
        if (f->keyRangeQ != rangeQ) continue;
        if (f->perspectiveSide != (int16_t)perspective) continue;
        return f;
    }
    return NULL;
}

static void nav_build_free_goal_field(NavFrame *nav, NavField *field,
                                      const NavFreeGoalRequest *request) {
    float seedRadius = nav_free_goal_seed_radius(request->stopRadius);
    NavTargetGoal goal = {
        .kind = NAV_GOAL_KIND_FREE_GOAL,
        .targetX = request->goalX,
        .targetY = request->goalY,
        .outerRadius = seedRadius,
        .arcCenterDeg = 0.0f,
        .arcHalfDeg = 0.0f,
        .targetId = -1,
        .perspectiveSide = request->perspectiveSide,
    };
    int32_t rangeQ = nav_range_q(request->stopRadius);

    nav_field_reset_for_build(nav, field);
    nav_free_goal_field_stamp_key(field, request, rangeQ);

    if (request->carveTargetId >= 0) {
        float carveX = request->carveCenterX;
        float carveY = request->carveCenterY;
        nav_resolve_entity_position(nav, request->carveTargetId,
                                    carveX, carveY,
                                    &carveX, &carveY);
        nav_field_carve_target_owned_blockers(nav, field,
                                              request->carveTargetId,
                                              carveX, carveY,
                                              request->carveInnerRadius);
    }

    int32_t seeded = nav_seed_disk(field, &goal);
    if (seeded == 0) {
        int32_t anchorCell = nav_cell_index_for_world(goal.targetX, goal.targetY);
        NavCellCoord anchor = nav_cell_coord(anchorCell);
        seeded = nav_seed_field_at(field, anchor.col, anchor.row, 6);
    }
    if (seeded == 0) {
        field->built = true;
        return;
    }
    nav_integrate_field(nav, field);
    field->built = true;
}

const NavField *nav_get_or_build_free_goal_field(NavFrame *nav,
                                                 const Battlefield *bf,
                                                 const NavFreeGoalRequest *request) {
    if (!nav || !nav->initialized || !bf || !request) return NULL;
    if (request->perspectiveSide < 0 || request->perspectiveSide > 1) return NULL;
    // Key on the exact goal coordinates at 0.25 px granularity, not on
    // the containing cell. Two goals inside the same 32 px cell that are
    // more than 0.25 px apart build distinct fields; closer than 0.25 px
    // they alias, which is tighter than any gameplay-visible difference.
    int32_t goalXQ = (int32_t)(request->goalX * 4.0f + 0.5f);
    int32_t goalYQ = (int32_t)(request->goalY * 4.0f + 0.5f);
    int32_t rangeQ = nav_range_q(request->stopRadius);
    for (int32_t i = 0; i < nav->freeGoalCacheSize; ++i) {
        NavField *f = &nav->freeGoalFields[i];
        if (!f->built) continue;
        if (f->kind != NAV_GOAL_KIND_FREE_GOAL) continue;
        if (f->keyGoalXQ != goalXQ) continue;
        if (f->keyGoalYQ != goalYQ) continue;
        if (f->keyRangeQ != rangeQ) continue;
        if (f->perspectiveSide != request->perspectiveSide) continue;
        if (f->keyTargetId != request->carveTargetId) continue;
        return f;
    }
    if (nav->freeGoalCacheSize >= NAV_FREE_GOAL_CACHE_CAPACITY) {
        assert(0 && "NavFrame free-goal field cache overflow");
        return NULL;
    }
    NavField *field = &nav->freeGoalFields[nav->freeGoalCacheSize++];
    nav_build_free_goal_field(nav, field, request);
    return field;
}

const NavField *nav_find_free_goal_field(const NavFrame *nav,
                                         const NavFreeGoalRequest *request) {
    if (!nav || !nav->initialized || !request) return NULL;
    if (request->perspectiveSide < 0 || request->perspectiveSide > 1) return NULL;
    int32_t goalXQ = (int32_t)(request->goalX * 4.0f + 0.5f);
    int32_t goalYQ = (int32_t)(request->goalY * 4.0f + 0.5f);
    int32_t rangeQ = nav_range_q(request->stopRadius);
    for (int32_t i = 0; i < nav->freeGoalCacheSize; ++i) {
        const NavField *f = &nav->freeGoalFields[i];
        if (!f->built) continue;
        if (f->kind != NAV_GOAL_KIND_FREE_GOAL) continue;
        if (f->keyGoalXQ != goalXQ) continue;
        if (f->keyGoalYQ != goalYQ) continue;
        if (f->keyRangeQ != rangeQ) continue;
        if (f->perspectiveSide != request->perspectiveSide) continue;
        if (f->keyTargetId != request->carveTargetId) continue;
        return f;
    }
    return NULL;
}

void nav_goal_region_anchor(const NavField *field, float *outX, float *outY) {
    float x = 0.0f, y = 0.0f;
    if (field) {
        x = field->anchorX;
        y = field->anchorY;
    }
    if (outX) *outX = x;
    if (outY) *outY = y;
}

float nav_goal_region_stop_radius(const NavField *field) {
    return field ? field->stopRadius : 0.0f;
}

float nav_goal_region_inner_radius(const NavField *field) {
    return field ? field->innerRadius : 0.0f;
}

float nav_goal_region_arc_center_deg(const NavField *field) {
    return field ? field->arcCenterDeg : 0.0f;
}

float nav_goal_region_arc_half_deg(const NavField *field) {
    return field ? field->arcHalfDeg : 0.0f;
}

float nav_goal_region_target_body_radius(const NavField *field) {
    return field ? field->targetBodyRadius : 0.0f;
}

void nav_snapshot_entity_position(NavFrame *nav, int entityId,
                                    float x, float y) {
    if (!nav) return;
    if (entityId < 0) return;
    nav_entity_snap_insert(nav, entityId, x, y);
}

// ---------- Raw mutators (Phase 2) ----------

static void nav_stamp_blocker_disk_internal(NavFrame *nav,
                                              int32_t entityId,
                                              float centerX, float centerY,
                                              float radius) {
    if (!nav || radius <= 0.0f) return;
    float minX = centerX - radius;
    float maxX = centerX + radius;
    float minY = centerY - radius;
    float maxY = centerY + radius;
    int32_t c0 = (int32_t)floorf(minX / (float)NAV_CELL_SIZE);
    int32_t c1 = (int32_t)floorf(maxX / (float)NAV_CELL_SIZE);
    int32_t r0 = (int32_t)floorf(minY / (float)NAV_CELL_SIZE);
    int32_t r1 = (int32_t)floorf(maxY / (float)NAV_CELL_SIZE);
    if (c0 < 0) c0 = 0;
    if (r0 < 0) r0 = 0;
    if (c1 > NAV_COLS - 1) c1 = NAV_COLS - 1;
    if (r1 > NAV_ROWS - 1) r1 = NAV_ROWS - 1;
    float r2 = radius * radius;
    for (int32_t row = r0; row <= r1; ++row) {
        float cellY = (float)row * (float)NAV_CELL_SIZE + (float)NAV_CELL_SIZE * 0.5f;
        float dy = cellY - centerY;
        for (int32_t col = c0; col <= c1; ++col) {
            float cellX = (float)col * (float)NAV_CELL_SIZE + (float)NAV_CELL_SIZE * 0.5f;
            float dx = cellX - centerX;
            if (dx * dx + dy * dy <= r2) {
                int32_t idx = nav_index(col, row);
                nav->staticBlockers.blocked[idx] = 1;
                nav->staticBlockers.blockerSrc[idx] = entityId;
            }
        }
    }
}

void nav_stamp_static_entity(NavFrame *nav, int32_t entityId,
                              float centerX, float centerY,
                              float entityRadius) {
    nav_stamp_blocker_disk_internal(nav, entityId, centerX, centerY,
                                     entityRadius);
}

void nav_stamp_static_entity_cell(NavFrame *nav, int32_t entityId,
                                  float worldX, float worldY) {
    if (!nav || entityId < 0) return;

    int32_t idx = nav_cell_index_for_world(worldX, worldY);
    if (idx < 0 || idx >= NAV_CELLS) return;

    nav->staticBlockers.blocked[idx] = 1;
    nav->staticBlockers.blockerSrc[idx] = entityId;
}

void nav_stamp_static_blocker_disk(NavFrame *nav, float centerX, float centerY,
                                    float radius) {
    // Raw stamp with no source attribution. Target-field carves cannot
    // reclaim cells stamped this way -- that is intentional so tests
    // using this low-level stamp maintain blocker semantics.
    nav_stamp_blocker_disk_internal(nav, NAV_BLOCKER_SRC_NONE,
                                     centerX, centerY, radius);
}

void nav_stamp_density(NavFrame *nav, int side, float x, float y) {
    if (!nav) return;
    if (side < 0 || side >= 2) return;
    int32_t cell = nav_cell_index_for_world(x, y);
    // Saturate at INT16_MAX defensively; in practice density[] only counts
    // live troops and MAX_ENTITIES * 2 = 128 fits comfortably.
    if (nav->density[side][cell] < INT16_MAX) {
        nav->density[side][cell]++;
    }
}

// ---------- Flow sampling ----------

void nav_cell_flow_direction(const NavField *field, int32_t cell,
                              int *outDcol, int *outDrow) {
    int dc = 0;
    int dr = 0;
    if (field && cell >= 0 && cell < NAV_CELLS &&
        field->distance[cell] != NAV_DIST_UNREACHABLE) {
        NavCellCoord coord = nav_cell_coord(cell);
        int32_t bestDist = field->distance[cell];
        for (int n = 0; n < 8; ++n) {
            int32_t nc = coord.col + NAV_NEIGHBORS[n].dcol;
            int32_t nr = coord.row + NAV_NEIGHBORS[n].drow;
            if (!nav_in_bounds(nc, nr)) continue;
            int32_t nidx = nav_index(nc, nr);
            if (field->hardBlocked[nidx]) continue;
            // Corner-cut prevention applies to flow sampling too, otherwise
            // the per-cell direction could point through the shared corner
            // of two blockers even though the Dijkstra integration refused
            // to propagate that way.
            if (nav_neighbor_is_diagonal(n)) {
                int32_t orthoColIdx = nav_index(coord.col + NAV_NEIGHBORS[n].dcol,
                                                coord.row);
                int32_t orthoRowIdx = nav_index(coord.col,
                                                coord.row + NAV_NEIGHBORS[n].drow);
                if (field->hardBlocked[orthoColIdx]) continue;
                if (field->hardBlocked[orthoRowIdx]) continue;
            }
            int32_t d = field->distance[nidx];
            if (d < bestDist) {
                bestDist = d;
                dc = NAV_NEIGHBORS[n].dcol;
                dr = NAV_NEIGHBORS[n].drow;
            }
        }
    }
    if (outDcol) *outDcol = dc;
    if (outDrow) *outDrow = dr;
}

// Bilinear-blended continuous flow sampling. Walks the four cell centers
// nearest (x, y), fetches each cell's per-cell flow direction, and blends
// the vectors by bilinear weights. Unreachable cells contribute a zero
// vector, so the blend naturally degrades near walls and blockers without
// pulling the sampling unit into an invalid cell.
void nav_sample_flow(const NavField *field, float x, float y,
                      float *outDx, float *outDy) {
    float dx = 0.0f;
    float dy = 0.0f;
    if (!field) {
        if (outDx) *outDx = dx;
        if (outDy) *outDy = dy;
        return;
    }
    // Convert (x, y) into grid space relative to cell centers. Cell (col,
    // row) has its center at (col * S + S/2, row * S + S/2); so in "center"
    // units, cell (col, row) sits at integer (col, row).
    float gx = x / (float)NAV_CELL_SIZE - 0.5f;
    float gy = y / (float)NAV_CELL_SIZE - 0.5f;
    int32_t c0 = (int32_t)floorf(gx);
    int32_t r0 = (int32_t)floorf(gy);
    float tx = gx - (float)c0;
    float ty = gy - (float)r0;
    // Fetch the four corner cells' flow vectors. Out-of-bounds corners
    // contribute (0,0), which keeps the blend finite at the board edges.
    struct { int32_t col; int32_t row; float w; } corners[4] = {
        { c0,     r0,     (1.0f - tx) * (1.0f - ty) },
        { c0 + 1, r0,     tx          * (1.0f - ty) },
        { c0,     r0 + 1, (1.0f - tx) * ty          },
        { c0 + 1, r0 + 1, tx          * ty          },
    };
    for (int i = 0; i < 4; ++i) {
        int32_t c = corners[i].col;
        int32_t r = corners[i].row;
        if (!nav_in_bounds(c, r)) continue;
        int cellIdx = nav_index(c, r);
        int fdc, fdr;
        nav_cell_flow_direction(field, cellIdx, &fdc, &fdr);
        if (fdc == 0 && fdr == 0) continue;
        float len = sqrtf((float)(fdc * fdc + fdr * fdr));
        if (len <= 0.0f) continue;
        float invLen = 1.0f / len;
        dx += corners[i].w * (float)fdc * invLen;
        dy += corners[i].w * (float)fdr * invLen;
    }
    // Renormalize the blended vector so callers scaling by moveSpeed * dt
    // get uniform step length regardless of sample position or the number
    // of unreachable corners. Zero stays zero (no valid flow neighborhood).
    float blendLen = sqrtf(dx * dx + dy * dy);
    if (blendLen > 1e-6f) {
        dx /= blendLen;
        dy /= blendLen;
    } else {
        dx = 0.0f;
        dy = 0.0f;
    }
    if (outDx) *outDx = dx;
    if (outDy) *outDy = dy;
}
