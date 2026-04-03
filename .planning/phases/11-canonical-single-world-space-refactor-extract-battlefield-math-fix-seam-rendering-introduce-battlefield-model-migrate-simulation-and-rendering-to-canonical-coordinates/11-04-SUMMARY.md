---
phase: 11-canonical-single-world-space-refactor
plan: 04
subsystem: rendering
tags: [raylib, split-screen, camera2d, canonical-coordinates, rendering-pipeline]

# Dependency graph
requires:
  - phase: 11-03
    provides: "Simulation (spawn, pathfinding, combat) migrated to canonical Battlefield coordinates"
provides:
  - "Canonical rendering pipeline: both viewports draw from one Battlefield entity list"
  - "viewport_draw_battlefield_tilemap for territory-based tilemap rendering"
  - "All seam remap code deleted (game_map_crossed_world_point, game_apply_crossed_direction, game_render_seam_rt, seamRT)"
  - "Viewport camera targets derived from Battlefield territory bounds"
affects: [11-05, rendering, viewport, game-loop]

# Tech tracking
tech-stack:
  added: []
  patterns: ["canonical-viewport: two rotated Camera2D views onto one world, no remap"]

key-files:
  created: []
  modified:
    - src/core/game.c
    - src/core/types.h
    - src/rendering/viewport.h
    - src/rendering/viewport.c
    - src/systems/player.c

key-decisions:
  - "Both viewports draw both territories' tilemaps (full board visible in each camera)"
  - "game_draw_canonical_entities is intentionally simple -- no visibility culling at 128 max entities"
  - "Removed pathfinding.h include from game.c since pathfind_apply_direction was only used in deleted seam code"

patterns-established:
  - "Canonical rendering: iterate Battlefield.entities, let Camera2D + scissor handle viewport clipping"
  - "Territory tilemap drawing via viewport_draw_battlefield_tilemap instead of per-Player tilemap"

requirements-completed: [REFACTOR-11]

# Metrics
duration: 4min
completed: 2026-04-01
---

# Phase 11 Plan 04: Rendering Migration to Canonical World Space Summary

**Rewrote rendering pipeline to draw canonical Battlefield entities directly, deleted all dual-space seam remap code and seamRT RenderTexture**

## Performance

- **Duration:** 4 min
- **Started:** 2026-04-01T17:19:53Z
- **Completed:** 2026-04-01T17:24:07Z
- **Tasks:** 2
- **Files modified:** 5

## Accomplishments
- Viewport initialization now derives camera targets from Battlefield territory bounds instead of hardcoded overlapping playAreas
- game_render draws all entities from Battlefield.entities in each viewport -- no ownership branching, no cross-space remap
- Deleted all transitional seam code: game_map_crossed_world_point, game_apply_crossed_direction, game_draw_entities_for_viewport, game_render_seam_rt, seamRT
- debug_draw_lane_paths_screen reads Battlefield canonical waypoints instead of Player.laneWaypoints

## Task Commits

Each task was committed atomically:

1. **Task 1: Rewrite viewport initialization to use Battlefield territory bounds** - `56ec939` (feat)
2. **Task 2: Rewrite game_render to canonical entity drawing, delete all remap/seam code** - `662be62` (feat)

## Files Created/Modified
- `src/core/game.c` - Simplified canonical rendering pipeline; deleted 4 seam functions and seamRT init/cleanup
- `src/core/types.h` - Removed RenderTexture2D seamRT from GameState
- `src/rendering/viewport.h` - Added viewport_draw_battlefield_tilemap, updated debug_draw_lane_paths_screen signature
- `src/rendering/viewport.c` - Rewrote viewport_init_split_screen for canonical territories, added battlefield tilemap draw, updated debug overlay
- `src/systems/player.c` - Updated stale TODO comment referencing deleted function

## Decisions Made
- Both viewports draw BOTH territories' tilemaps so the full canonical board is visible through each rotated camera
- game_draw_canonical_entities iterates all entities without visibility culling (128 max entities is well within budget for brute-force draw)
- Removed pathfinding.h and sprite_renderer.h includes from game.c since they were only needed by deleted seam functions

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Updated stale TODO comment in player.c**
- **Found during:** Task 2 (structural validation grep)
- **Issue:** player.c contained a TODO referencing the now-deleted game_draw_entities_for_viewport function
- **Fix:** Updated comment to reference game_draw_canonical_entities and note removal in Plan 05
- **Files modified:** src/systems/player.c
- **Verification:** rg confirms no references to old function name remain
- **Committed in:** 662be62 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 bug - stale comment)
**Impact on plan:** Trivial cleanup, no scope creep.

## Issues Encountered
None

## Known Stubs
None -- all rendering paths are fully wired to Battlefield data.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Rendering pipeline is fully canonical: two rotated cameras viewing one Battlefield world
- Plan 05 can now remove remaining Player adapter fields (playArea, tilemap, laneWaypoints, entities)
- Visual verification of entity rendering may be done in Plan 05 or manually

---
*Phase: 11-canonical-single-world-space-refactor*
*Completed: 2026-04-01*
