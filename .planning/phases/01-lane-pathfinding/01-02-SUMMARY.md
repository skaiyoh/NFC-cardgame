---
phase: 01-lane-pathfinding
plan: 02
subsystem: pathfinding
tags: [waypoint-following, entity-movement, debug-overlay, lane-paths, c, raylib]

# Dependency graph
requires:
  - phase: 01-lane-pathfinding/01
    provides: "pathfind_step_entity(), pathfind_compute_direction(), lane waypoint data, config defines"
  - phase: 01-lane-pathfinding/00
    provides: "Unit tests for pathfinding production code (Wave 0)"
provides:
  - "ESTATE_WALKING delegates to pathfind_step_entity() (production code = tested code)"
  - "debug_draw_lane_paths() colored overlay with F1 toggle"
  - "Troops follow curved lane waypoints instead of straight-line Y movement"
affects: [combat, entity-movement, base-creation]

# Tech tracking
tech-stack:
  added: []
  patterns: [production-delegation, debug-overlay-toggle]

key-files:
  created: []
  modified:
    - src/entities/entities.c
    - src/rendering/viewport.c
    - src/rendering/viewport.h
    - src/core/game.c

key-decisions:
  - "ESTATE_WALKING is a one-line delegation to pathfind_step_entity() -- no inline logic duplication"
  - "Debug overlay uses blue/green/red for left/center/right lanes"
  - "F1 key for debug toggle (non-conflicting with gameplay keys 1/Q)"

patterns-established:
  - "Production delegation: game-loop cases delegate to tested helper functions"
  - "Debug overlay pattern: static bool toggle + conditional draw inside viewport brackets"

requirements-completed: [CORE-01]

# Metrics
duration: 2min
completed: 2026-03-28
---

# Phase 01 Plan 02: Entity Movement Integration & Debug Overlay Summary

**ESTATE_WALKING delegates to pathfind_step_entity() for waypoint-following movement, with F1-toggled colored lane debug overlay**

## Performance

- **Duration:** 2 min
- **Started:** 2026-03-28T22:39:42Z
- **Completed:** 2026-03-28T22:42:14Z
- **Tasks:** 2 (of 3; Task 3 is human-verify checkpoint)
- **Files modified:** 4

## Accomplishments
- Replaced straight-line Y movement with pathfind_step_entity() delegation in ESTATE_WALKING
- Removed all old movement code (despawnY, borderY, position.y -= moveSpeed)
- Added debug_draw_lane_paths() drawing colored waypoint dots and lines per lane
- Added F1 toggle for debug overlay, rendering inside viewport brackets (world coordinates)
- Production code path is identical to what Wave 0 unit tests verify

## Task Commits

Each task was committed atomically:

1. **Task 1: Replace straight-line movement with pathfind_step_entity() call** - `a4939ca` (feat)
2. **Task 2: Add debug lane path overlay with F1 toggle** - `b9013fe` (feat)

## Files Created/Modified
- `src/entities/entities.c` - ESTATE_WALKING now delegates to pathfind_step_entity(); old inline movement removed
- `src/rendering/viewport.h` - Added debug_draw_lane_paths() declaration
- `src/rendering/viewport.c` - Implemented debug_draw_lane_paths() with colored dots/lines per lane
- `src/core/game.c` - Added s_showLaneDebug static, F1 toggle, debug overlay calls in both viewport brackets

## Decisions Made
- ESTATE_WALKING is a single-line delegation -- keeps entity_update concise and ensures production/test code alignment
- Lane colors: BLUE (left), GREEN (center), RED (right) -- visually distinct and intuitive
- F1 chosen for debug toggle to avoid conflict with gameplay keys (1=P1 spawn, Q=P2 spawn)
- Debug overlay renders inside viewport_begin/end to use world coordinates with camera transform

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Task 3 (human-verify checkpoint) pending: visual verification of lane pathfinding behavior
- All code changes compiled and unit tests pass
- Ready for visual playtest: `make cardgame && ./cardgame`, press F1 for debug overlay, press 1/Q to spawn troops

## Self-Check: PASSED

- All 4 modified source files found
- Commits a4939ca and b9013fe verified in git log
- make cardgame compiles successfully
- make test passes all 6 pathfinding tests

---
*Phase: 01-lane-pathfinding*
*Completed: 2026-03-28*
