//
// Per-frame flow-field navigation cache.
//
// NavFrame is the shared snapshot that every moving entity consults each tick.
// `nav_begin_frame()` rebuilds the static obstacle mask, clears the per-frame
// caches, and rasterizes ally/enemy density. Lane fields, target fields, and
// free-goal fields are built lazily on first lookup within a frame and reused
// across entities that share the same cache key.
//
// Grid layout:
//   NAV_CELL_SIZE = 32
//   NAV_COLS      = (BOARD_WIDTH + 31) / 32   = 34
//   NAV_ROWS      = (BOARD_HEIGHT + 31) / 32  = 60
//   NAV_CELLS     = NAV_COLS * NAV_ROWS       = 2040
//
// All integration is reverse 8-neighbor Dijkstra with fixed movement costs
// 10 (orthogonal) and 14 (diagonal), plus dynamic per-cell shaping penalties
// from the ally/enemy density rasterization. A fixed-capacity binary min-heap
// is reused across field builds (no per-tick allocation).
//
// Scope of this header is Phase 1: types, lifecycle, lane-field builder, and
// the cache slot tables. Target-field and free-goal-field builders and
// bilinear flow sampling land in Phase 2.
//

#ifndef NFC_CARDGAME_NAV_FRAME_H
#define NFC_CARDGAME_NAV_FRAME_H

#include <stdbool.h>
#include <stdint.h>

#include "../core/config.h"

// Forward declarations to keep this header decoupled from the heavy type
// chain in src/core/types.h. Tests construct a local Battlefield stub and
// call the public API directly.
struct Battlefield;
typedef struct Battlefield Battlefield;

// ---------- Grid geometry ----------

#define NAV_CELL_SIZE  32
#define NAV_COLS       ((BOARD_WIDTH  + NAV_CELL_SIZE - 1) / NAV_CELL_SIZE)
#define NAV_ROWS       ((BOARD_HEIGHT + NAV_CELL_SIZE - 1) / NAV_CELL_SIZE)
#define NAV_CELLS      (NAV_COLS * NAV_ROWS)

// ---------- Edge moat invariant ----------
//
// Flow-field stepping is center-based: an entity's position is interpreted
// through the cell whose center it occupies. This is a conscious departure
// from the old candidate-fan pathfinder, which rejected any step whose
// center came within the unit's nav radius of a board edge.
//
// For that center-based stepping to be at least as safe as the old system,
// the nearest unblocked cell center must sit far enough from every board
// edge that the largest *mobile* unit silhouette still clears the edge.
// Today's mobile units carry body radii in [12, 18] px. With
// NAV_CELL_SIZE = 32, a one-cell outer moat puts the nearest unblocked
// center 48 px from the edge (col 1 center = 48), which leaves 30 px of
// clearance for a Brute (radius 18). That is strictly tighter than the old
// radius-test behavior, so a single-cell moat is sufficient and matches
// the old bounds check for every unit the game actually spawns.
//
// NAV_EDGE_MOAT_CELLS is the knob that encodes this decision. If any
// future unit's body radius grows past NAV_CELL_SIZE - NAV_EDGE_MOAT_CELLS
// * NAV_CELL_SIZE / 2, this constant must grow with it, and the static
// assert in nav_frame.c fires at compile time.
#define NAV_EDGE_MOAT_CELLS      1
#define NAV_MAX_MOBILE_BODY_RADIUS 18

// ---------- Integration costs ----------

// Base movement costs on the integration grid. Orthogonal steps cost 10,
// diagonal steps cost 14 (closest integer approximation of 10 * sqrt(2)).
#define NAV_COST_ORTHO     10
#define NAV_COST_DIAGONAL  14

// "Unreachable" sentinel used by the distance grid. INT32_MAX leaves plenty
// of headroom for the heap-relaxation arithmetic.
#define NAV_DIST_UNREACHABLE  INT32_MAX

// ---------- Dynamic shaping penalties ----------

// Soft costs added to a cell's base movement cost during integration. Allies
// are never impassable; they merely make a cell expensive so the flow field
// routes around dense clusters when a cheaper alternative exists. Enemies
// apply a stronger nudge but remain passable so two crowds meeting in a lane
// do not deadlock.
#ifndef NAV_ALLY_OCCUPIED_COST
#define NAV_ALLY_OCCUPIED_COST   6
#endif
#ifndef NAV_ALLY_NEAR_COST
#define NAV_ALLY_NEAR_COST       2
#endif
#ifndef NAV_ENEMY_OCCUPIED_COST
#define NAV_ENEMY_OCCUPIED_COST  18
#endif
#ifndef NAV_ENEMY_NEAR_COST
#define NAV_ENEMY_NEAR_COST      6
#endif

// Anti-backtrack penalties used by Phase 3's per-entity neighbor picker.
// Declared here so Phase 1 tests can reference them even though they are not
// consumed by the lane-field Dijkstra itself.
#ifndef NAV_PREV_CELL_BACKTRACK_COST
#define NAV_PREV_CELL_BACKTRACK_COST  24
#endif
#ifndef NAV_ABAB_PINGPONG_COST
#define NAV_ABAB_PINGPONG_COST        64
#endif

// ---------- Lane corridor ----------

// Half-width of the home-half lane corridor used by lane-march fields. Cells
// whose center is further than this from the authored lane polyline (while
// still inside the owning side's half) are treated as hard-blocked for that
// lane's flow field. This is intentionally tighter than the old
// PATHFIND_LANE_DRIFT_MAX_RATIO envelope so lane identity holds cleanly in
// the home half; tune in src/core/config.h after smoke runs.
#ifndef NAV_HOME_LANE_CORRIDOR_RADIUS
#define NAV_HOME_LANE_CORRIDOR_RADIUS  96.0f
#endif

// Half-thickness of the front-arc ribbon used by static-target assault goal
// regions. Declared here so Phase 2's region rasterizer has a single source
// of truth.
#ifndef NAV_GOAL_RIBBON_HALF_THICKNESS
#define NAV_GOAL_RIBBON_HALF_THICKNESS  16.0f
#endif

// ---------- Cache capacities ----------
//
// Target-field cache sized for the worst case where every live entity on the
// board (MAX_ENTITIES * 2 = 128) pursues a distinct (targetId, goalKind,
// rangeQ) combination, where rangeQ is the 0.25 px integer quantization of
// the exact caller outerRadius. Free-goal cache sized for the worst case
// where every live mover has a distinct free-goal field in one frame.
#define NAV_TARGET_CACHE_CAPACITY   (MAX_ENTITIES * 2)
// Sized to the battlefield's live entity cap so every live mover can hold
// a distinct free-goal field in one frame without overflow fallback. Was
// previously 64; bumped to MAX_ENTITIES * 2 = 128 after the exact-position
// cache key rewrite made collisions rare (two free-goal callers no longer
// share a slot unless their goal points agree to within 0.25 px).
#define NAV_FREE_GOAL_CACHE_CAPACITY (MAX_ENTITIES * 2)

// Number of prebuilt lane fields: two sides x three lanes.
#define NAV_LANE_FIELD_COUNT  (2 * 3)

// Snapshot hash table capacity. Live entity count is capped at
// MAX_ENTITIES * 2 = 128, so a 256-slot power-of-two table gives a
// guaranteed <=50% load factor and keeps open-addressed linear probes
// bounded. Entity ids are NOT used as indices -- they are stored in
// entityPosId[] and probed modulo NAV_ENTITY_SNAP_CAPACITY.
#define NAV_ENTITY_SNAP_CAPACITY 256
#define NAV_ENTITY_ID_NONE       (-1)

// ---------- Cell and field types ----------

typedef struct {
    int16_t col;
    int16_t row;
} NavCellCoord;

// Kind of goal region a flow field is seeded from. `LANE_MARCH` is the only
// kind used by Phase 1; Phase 2 introduces the remaining kinds and a full
// NavGoalRegion struct.
typedef enum {
    NAV_GOAL_KIND_LANE_MARCH = 0,
    NAV_GOAL_KIND_STATIC_ATTACK,
    NAV_GOAL_KIND_MELEE_RING,
    NAV_GOAL_KIND_DIRECT_RANGE,
    NAV_GOAL_KIND_FREE_GOAL
} NavGoalKind;

// Hard-blocker mask: bit set means the cell is impassable (board edge, static
// building footprint, or a lane field's corridor mask). This is a separate
// layer from the soft density penalties so the integration kernel can skip
// hard-blocked neighbors without consulting density costs.
//
// `blockerSrc[i]` records the entity id of the most recent stamp that
// blocked cell `i` via nav_stamp_static_entity(). Cells blocked by the
// board-edge moat or by nav_stamp_static_blocker_disk() (the raw stamp
// used by tests) hold NAV_BLOCKER_SRC_NONE. Target-field builders use
// this to carve a combat corridor only through cells the target
// entity itself owns, so other nearby blockers are preserved.
#define NAV_BLOCKER_SRC_NONE (-1)
typedef struct {
    uint8_t blocked[NAV_CELLS];
    int32_t blockerSrc[NAV_CELLS];
} NavBlockerMask;

// A completed flow field. `distance[i]` is the integrated cost from cell `i`
// to the nearest seed cell along the cheapest 8-neighbor path, including
// any dynamic density penalties contributed by NavFrame.density[] from the
// perspective of `perspectiveSide` (allies cheap, enemies expensive).
// Unreachable cells hold NAV_DIST_UNREACHABLE.
typedef struct {
    int32_t  distance[NAV_CELLS];
    uint8_t  hardBlocked[NAV_CELLS]; // local copy of the per-field blocker mask
    bool     built;
    NavGoalKind kind;
    // perspectiveSide selects which side's density costs the integration
    // kernel treats as "ally" (cheap) vs "enemy" (expensive). Must be 0 or 1.
    int16_t  perspectiveSide;
    // World-space anchor of the goal region. Lane fields store their final
    // authored waypoint; target fields store the target pivot (from the
    // frame snapshot); free-goal fields store the goal point. Exposed for
    // debug draw and for the Phase 3 combat_engagement_goal compatibility
    // shim via nav_goal_region_anchor().
    float    anchorX;
    float    anchorY;
    // Authored outer radius of the goal region. Lane fields leave this
    // at 0; target fields store the exact caller outerRadius; free-goal
    // fields store the caller stopRadius. Exposed via
    // nav_goal_region_stop_radius() so debug_overlay.c and the
    // combat_engagement_goal shim can satisfy their radius contract
    // without re-deriving it from goal geometry.
    float    stopRadius;
    // Exact authored geometry metadata for debug draw. STATIC_ATTACK uses
    // innerRadius plus arcCenterDeg/arcHalfDeg to describe the ribbon;
    // MELEE_RING uses innerRadius for the inner ring edge; disk-style goals
    // leave innerRadius at 0 and ignore the arc values.
    float    innerRadius;
    float    arcCenterDeg;
    float    arcHalfDeg;
    float    targetBodyRadius;
    // Cache key fields. Interpretation depends on `kind`:
    //   LANE_MARCH   -> keySide, keyLane
    //   STATIC_ATTACK, MELEE_RING, DIRECT_RANGE -> keyTargetId, keyRangeQ
    //   FREE_GOAL    -> keyGoalXQ, keyGoalYQ, keyRangeQ, keyTargetId
    //
    // keyRangeQ and keyGoalXQ/keyGoalYQ are all 0.25 px integer
    // quantizations (multiply by phase 4, round to int) of the exact caller
    // values. Two free-goal callers do not alias unless their goal
    // positions agree to within 0.25 px -- tighter than any physical
    // gameplay difference. For FREE_GOAL fields, keyTargetId stores the
    // optional carveTargetId (-1 when no carve is requested) so carved and
    // uncarved requests at the same goal point do not alias. No snapping is
    // applied to the seeded geometry: fields rasterize with the exact caller
    // coordinates and radius, so the seed set always matches the caller's own
    // threshold.
    int16_t  keySide;
    int16_t  keyLane;
    int32_t  keyTargetId;
    int32_t  keyGoalXQ;
    int32_t  keyGoalYQ;
    int32_t  keyRangeQ;
} NavField;

// Binary min-heap node used by the Dijkstra kernel. `cell` is a flat index
// into the NAV_CELLS arrays; `dist` is the current tentative distance.
typedef struct {
    int32_t dist;
    int32_t cell;
} NavHeapNode;

// ---------- NavFrame ----------

typedef struct NavFrame {
    // Static obstacle mask rebuilt at nav_begin_frame from board bounds and
    // NAV_PROFILE_STATIC entities.
    NavBlockerMask staticBlockers;

    // Per-side entity density. density[side][cell] counts the live troops
    // owned by `side` that occupy that cell, as stamped by
    // nav_stamp_density(). Consumers treat their own side as "ally" density
    // and the other side as "enemy" density at query time, so Phase 2 only
    // needs one 2 x NAV_CELLS array instead of mirrored ally/enemy layers.
    int16_t density[2][NAV_CELLS];

    // Frozen per-entity position snapshot. Populated by
    // nav_snapshot_entity_position() from game.c's entity iteration loop
    // at nav_begin_frame time. Target-field builders read from this table
    // so that the first caller to request a field for a mobile target
    // does not lock the field against whatever live position that target
    // happened to be at mid-update. Every caller in a given frame reads
    // the same frozen position, which removes the id-sorted update order
    // dependence.
    //
    // entityPosId[] is keyed on entity id, not indexed by it: ids grow
    // monotonically across the match and routinely exceed any fixed
    // MAX_ENTITIES cap, so raw indexing would silently alias or drop
    // snapshots in long matches. The live entity count is bounded by
    // MAX_ENTITIES * 2 = 128, so an open-addressed hash table of that
    // many slots is always sufficient. Sentinel value for an empty slot
    // is NAV_ENTITY_ID_NONE (-1); lookups linear-probe within
    // NAV_ENTITY_SNAP_CAPACITY.
    int32_t  entityPosId[NAV_ENTITY_SNAP_CAPACITY];
    float    entityPosX[NAV_ENTITY_SNAP_CAPACITY];
    float    entityPosY[NAV_ENTITY_SNAP_CAPACITY];

    // Prebuilt lane fields. Indexed by [side][lane]. Built lazily on first
    // lookup via nav_get_or_build_lane_field().
    NavField laneFields[2][3];

    // Lazy target-field cache with linear-probe lookup by key.
    NavField targetFields[NAV_TARGET_CACHE_CAPACITY];
    int32_t  targetCacheSize;

    // Lazy free-goal field cache (farmers, free-mover helpers).
    NavField freeGoalFields[NAV_FREE_GOAL_CACHE_CAPACITY];
    int32_t  freeGoalCacheSize;

    // Reusable heap storage for the Dijkstra kernel.
    //
    // This kernel uses lazy deletion (stale entries are discarded at pop
    // time) rather than a decrease-key operation, so the same cell may be
    // enqueued once per incoming relaxation. On the 8-neighbor grid that
    // is bounded above by 8 * NAV_CELLS inserts plus up to NAV_CELLS seed
    // inserts from nav_integrate_field's initial scan. Capacity is sized
    // to 9 * NAV_CELLS to cover both with headroom; nav_heap_push asserts
    // on overflow so a future regression cannot silently out-of-bounds.
    NavHeapNode heapStorage[NAV_CELLS * 9];
    int32_t     heapSize;

    // Frame sequence number, incremented by nav_begin_frame(). Exposed so
    // debug overlays and assertions can detect stale field reads.
    uint32_t frameCounter;

    // Scratch flag: true if nav_begin_frame has ever been called on this
    // frame. Makes the "used before begin" case detectable in tests.
    bool initialized;
} NavFrame;

// ---------- Lifecycle ----------

// Zero-initialize the NavFrame. Safe to call on a fresh stack-local or on a
// heap allocation that has not been memset.
void nav_frame_init(NavFrame *nav);

// No-op today; exists so production code can pair init/destroy symmetrically
// and so a future heap-backed NavFrame can release resources here without
// touching call sites.
void nav_frame_destroy(NavFrame *nav);

// Begin a new frame: clear all per-frame caches, rebuild the static obstacle
// mask from `bf` (board bounds only in Phase 1; NAV_PROFILE_STATIC entity
// footprints land in Phase 2 when NavFrame is wired into game_update).
// Ally/enemy density is zeroed; Phase 2 fills it from live entities.
void nav_begin_frame(NavFrame *nav, const Battlefield *bf);

// ---------- Coordinate helpers ----------

// Clamp a world position to the canonical board and convert to a flat cell
// index. Positions outside the board are clamped to the nearest edge cell.
int32_t nav_cell_index_for_world(float worldX, float worldY);

// Unpack a flat cell index into (col, row).
NavCellCoord nav_cell_coord(int32_t cellIndex);

// Center of a cell in world coordinates.
void nav_cell_center(int32_t cellIndex, float *outX, float *outY);

// True if (col, row) is inside the grid.
bool nav_in_bounds(int32_t col, int32_t row);

// Flat index from (col, row). Caller must guarantee in-bounds.
int32_t nav_index(int32_t col, int32_t row);

// ---------- Blocker queries ----------

// True if the cell is hard-blocked by the static obstacle mask on this frame.
bool nav_cell_is_static_blocked(const NavFrame *nav, int32_t cellIndex);

// ---------- Lane field access ----------

// Return the lane-march flow field for (side, lane), building it lazily on
// first access within the current frame. Returns NULL if side/lane are out
// of range or nav has not been initialized this frame.
const NavField *nav_get_or_build_lane_field(NavFrame *nav, const Battlefield *bf,
                                             int side, int lane);

// Return the already-built lane field for (side, lane), or NULL if that
// field has not been built in the current frame.
const NavField *nav_find_lane_field(const NavFrame *nav, int side, int lane);

// ---------- Target / free-goal field access ----------

// Parameters describing the seed region of a target or free-goal field. The
// builder seeds every cell matching the region, then runs reverse Dijkstra
// outward. nav_frame.c stays entity-agnostic: combat.c / pathfinding.c
// populate this struct from live Entity + combat helpers in Phase 3.
//
// Field meaning per `kind`:
//
//   NAV_GOAL_KIND_STATIC_ATTACK
//     Seed a front-arc ribbon around (targetX, targetY). `outerRadius` is
//     the intended attack radius; the ribbon seeds cells whose center is
//     within NAV_GOAL_RIBBON_HALF_THICKNESS of outerRadius AND whose
//     bearing from the target lies inside [arcCenterDeg - arcHalfDeg,
//     arcCenterDeg + arcHalfDeg]. The target's blocker disk is assumed to
//     already be stamped in staticBlockers.
//
//   NAV_GOAL_KIND_MELEE_RING
//     Seed a one-cell-thick ring around (targetX, targetY) at
//     outerRadius (no arc constraint, full 360 degrees).
//
//   NAV_GOAL_KIND_DIRECT_RANGE
//     Seed every unblocked cell whose center is within outerRadius of
//     (targetX, targetY). Used by ranged / healer attackers.
//
//   NAV_GOAL_KIND_FREE_GOAL
//     Seed every unblocked cell whose center is within outerRadius (stop
//     radius) of (targetX, targetY). No arc, no ring thickness. Used by
//     farmers and any free-goal helper.
typedef struct {
    NavGoalKind kind;
    float  targetX;
    float  targetY;
    float  outerRadius;    // exact authored outer radius (cache key is quantized)
    float  arcCenterDeg;   // only read for STATIC_ATTACK
    float  arcHalfDeg;     // only read for STATIC_ATTACK
    // Authored body radius of the target itself. For STATIC_ATTACK fields,
    // the builder uses this to preserve the target's inner no-entry core
    // while carving any owned outer blocker cells that would otherwise
    // prevent the combat ribbon from seeding approach cells.
    float  targetBodyRadius;
    // Minimum distance from the target center at which the stepper will
    // accept the attacker's position. For STATIC_ATTACK and MELEE_RING
    // fields this is `targetBodyRadius + attackerBodyRadius +
    // PATHFIND_CONTACT_GAP`. Seeds below this radius are unreachable
    // (the stepper's target-body contact shell rejects them), so the
    // builder uses it as a hard floor on both the carve and the
    // ribbon inner edge. Leave 0 for non-static / direct-range goals
    // where the disk seeding handles overlap on its own.
    float  innerRadiusMin;
    int32_t targetId;      // cache key; -1 for free-goal fields
    int16_t perspectiveSide; // 0 or 1; determines ally/enemy density cost
} NavTargetGoal;

// Parameters describing a free-goal field for farmers / helpers. The field is
// anchored at the exact goal disk centered at (goalX, goalY) with gameplay
// stop radius stopRadius. The builder may widen the seeded disk just enough to
// guarantee at least one nearby cell center on the 32 px nav grid, while the
// actual arrival test still uses stopRadius. Optionally, callers can carve the
// owned outer blocker cells of one static target entity (identified by
// carveTargetId) while preserving an inner no-entry radius.
typedef struct {
    float   goalX;
    float   goalY;
    float   stopRadius;
    int16_t perspectiveSide; // 0 or 1; determines ally/enemy density cost
    int32_t carveTargetId;   // -1 when no static-target carve is needed
    float   carveCenterX;
    float   carveCenterY;
    float   carveInnerRadius;
} NavFreeGoalRequest;

// Return the target flow field matching `goal`, building it lazily on the
// first lookup within the current frame. Cache key is
// (targetId, kind, rangeClass, perspectiveSide). Returns NULL if the cache
// has no free slot (assertion in debug builds).
const NavField *nav_get_or_build_target_field(NavFrame *nav, const Battlefield *bf,
                                                const NavTargetGoal *goal);

// Return the already-built target field matching `goal`, or NULL if the
// cache does not contain it in the current frame.
const NavField *nav_find_target_field(const NavFrame *nav,
                                      const NavTargetGoal *goal);

// Return the free-goal flow field described by `request`, building it lazily on
// first lookup within the current frame. Cache key is
// (goalXQ, goalYQ, rangeQ, perspectiveSide, carveTargetId), where goalXQ /
// goalYQ are the 0.25 px integer quantization of the exact caller coordinates
// and rangeQ is the 0.25 px quantization of the exact caller stopRadius. Two
// free-goal callers do not alias unless their goal points, radii, perspective,
// and carve target all agree. Returns NULL only if the cache overflows
// (capacity is MAX_ENTITIES * 2 = 128, which matches the battlefield's live
// entity cap, so this is unreachable in practice).
const NavField *nav_get_or_build_free_goal_field(NavFrame *nav,
                                                 const Battlefield *bf,
                                                 const NavFreeGoalRequest *request);

// Return the already-built free-goal field matching `request`'s exact cache
// key, or NULL if it has not been built in the current frame.
const NavField *nav_find_free_goal_field(const NavFrame *nav,
                                         const NavFreeGoalRequest *request);

// Representative world-space anchor of the field's goal region. Lane fields
// return the final authored lane waypoint; target fields return the target
// pivot; free-goal fields return the goal point. Used by the Phase 3
// combat_engagement_goal compatibility shim to satisfy its Vector2 contract.
void nav_goal_region_anchor(const NavField *field, float *outX, float *outY);

// Authored outer radius of the field's goal region. Returns 0 for lane
// fields; returns the exact caller-supplied outerRadius for target fields
// and stopRadius for free-goal fields. Consumed by the Phase 3
// combat_engagement_goal shim (which must still return a stopRadius to
// callers like debug_overlay.c:draw_assault_geometry).
float nav_goal_region_stop_radius(const NavField *field);

// Inner edge of the authored goal region. Returns 0 for lane fields and
// disk-style goals that do not have an inner floor.
float nav_goal_region_inner_radius(const NavField *field);

// Authored front-arc center bearing in degrees. Only meaningful for
// STATIC_ATTACK fields.
float nav_goal_region_arc_center_deg(const NavField *field);

// Authored front-arc half-angle in degrees. Only meaningful for
// STATIC_ATTACK fields.
float nav_goal_region_arc_half_deg(const NavField *field);

// Authored target body radius carried through from the goal description.
float nav_goal_region_target_body_radius(const NavField *field);

// ---------- Raw mutators (called by game.c from the entity iteration loop) ----------
//
// nav_frame.c is intentionally Entity-agnostic: game.c iterates the live
// entity registry and calls these raw mutators with world coordinates, so
// the nav module never needs to #include the heavy types.h chain.

// Stamp a hard-blocked disk of the given radius into staticBlockers. Every
// cell whose center lies within `radius` world units of (centerX, centerY)
// is marked blocked. This raw stamp carries no blocker ownership metadata.
void nav_stamp_static_blocker_disk(NavFrame *nav, float centerX, float centerY,
                                    float radius);

// Stamp an owned hard-blocked disk for a NAV_PROFILE_STATIC entity. Unlike
// nav_stamp_static_blocker_disk(), this records `entityId` in blockerSrc so
// target-field carves can distinguish this entity's cells from overlapping
// blockers.
void nav_stamp_static_entity(NavFrame *nav, int32_t entityId,
                              float centerX, float centerY,
                              float entityRadius);

// Stamp the single blocker cell containing (worldX, worldY) and attribute it
// to `entityId`. Used for authored static-entity cell masks.
void nav_stamp_static_entity_cell(NavFrame *nav, int32_t entityId,
                                  float worldX, float worldY);

// Add one unit of density for `side` at the cell containing (x, y). Called
// once per live mobile troop during the per-frame density pass.
void nav_stamp_density(NavFrame *nav, int side, float x, float y);

// Snapshot a live entity's world position into the per-frame position
// array. Every target-field builder reads position from here instead of
// trusting caller-supplied coordinates, so two attackers pursuing the
// same mobile target in the same frame always build the same field.
// Must be called between nav_begin_frame() and the entity update loop.
void nav_snapshot_entity_position(NavFrame *nav, int entityId,
                                    float x, float y);

// ---------- Flow sampling ----------

// Compute the integer per-cell flow direction for `cell` on `field`. Returns
// the signed (dcol, drow) pair pointing to the 8-neighbor with the lowest
// distance, or (0, 0) if the cell is unreachable or no neighbor improves on
// it. The returned delta is in {-1, 0, 1} for each component.
void nav_cell_flow_direction(const NavField *field, int32_t cell,
                              int *outDcol, int *outDrow);

// Bilinear-blended flow vector at continuous world position (x, y). The
// output is the normalized direction a unit at (x, y) should step to follow
// the flow field downhill toward the seed; if no nearby cells are reachable
// the output is (0, 0). Writes through `outDx`/`outDy`.
void nav_sample_flow(const NavField *field, float x, float y,
                      float *outDx, float *outDy);

#endif // NFC_CARDGAME_NAV_FRAME_H
