---
phase: 01-lane-pathfinding
plan: 01
subsystem: pathfinding
tags: [waypoints, sine-bow, pathfinding, entity-movement, direction-hysteresis, c, raylib]

# Dependency graph
requires:
  - phase: 01-lane-pathfinding/00
    provides: "Test infrastructure for pathfinding (test_pathfinding.c, make test target)"
provides:
  - "LANE_WAYPOINT_COUNT, LANE_BOW_INTENSITY, LANE_JITTER_RADIUS, PI_F, DIRECTION_HYSTERESIS config defines"
  - "Entity.waypointIndex field for tracking path progress"
  - "Player.laneWaypoints[3][8] pre-computed waypoint arrays"
  - "lane_generate_waypoints() function with sine bow for outer lanes"
  - "pathfind_step_entity() pure helper for waypoint stepping"
  - "pathfind_compute_direction() pure helper with hysteresis"
  - "player_init lane waypoint generation integration"
  - "spawn waypointIndex=1 to skip zero-distance first waypoint"
affects: [01-lane-pathfinding/02, combat, entity-movement]

# Tech tracking
tech-stack:
  added: []
  patterns: [sine-bow-curve, waypoint-stepping, direction-hysteresis, pre-computed-paths]

key-files:
  created: []
  modified:
    - src/core/config.h
    - src/core/types.h
    - src/logic/pathfinding.h
    - src/logic/pathfinding.c
    - src/systems/player.c
    - src/logic/card_effects.c

key-decisions:
  - "Sine curve (sinf(depth * PI)) for lane bow instead of quadratic -- smooth taper at endpoints"
  - "endDepth=0.85 leaves room for future enemy base at depth ~1.0"
  - "Hysteresis threshold (15%) prevents sprite direction flicker near 45-degree angles"
  - "Waypoint[0] matches slot spawn position exactly to avoid backward snap on spawn"
  - "waypointIndex=1 at spawn skips zero-distance first waypoint"

patterns-established:
  - "Pre-computed paths: waypoints generated once at player_init, stored on Player struct"
  - "Pure helpers: pathfind_step_entity and pathfind_compute_direction are testable without game state"
  - "Direction hysteresis: dominant-axis check with configurable threshold prevents flicker"

requirements-completed: [CORE-01]

# Metrics
duration: 3min
completed: 2026-03-28
---

# Phase 01 Plan 01: Lane Waypoint Data Layer Summary

**Lane waypoint generation with sine bow curves, stepping/direction pure helpers with hysteresis, and player init + spawn integration**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-28T22:30:36Z
- **Completed:** 2026-03-28T22:33:56Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments
- Config defines for lane tuning (waypoint count, bow intensity, jitter radius, hysteresis threshold)
- Entity.waypointIndex and Player.laneWaypoints struct fields added
- lane_generate_waypoints() produces sine-bowed paths for outer lanes and straight center lane
- pathfind_step_entity() and pathfind_compute_direction() as pure testable helpers
- player_init integrates waypoint generation after card slot initialization
- spawn_troop_from_card sets waypointIndex=1 to skip zero-distance first waypoint

## Task Commits

Each task was committed atomically:

1. **Task 1: Add config defines and struct fields for lane waypoints** - `e99a130` (feat)
2. **Task 2: Implement waypoint generation, stepping/direction helpers, and integrate** - `13460fc` (feat)

## Files Created/Modified
- `src/core/config.h` - Added LANE_WAYPOINT_COUNT, LANE_BOW_INTENSITY, LANE_JITTER_RADIUS, PI_F, DIRECTION_HYSTERESIS defines
- `src/core/types.h` - Added waypointIndex to Entity, laneWaypoints[3][8] to Player
- `src/logic/pathfinding.h` - Replaced empty stub with lane_generate_waypoints, pathfind_step_entity, pathfind_compute_direction declarations
- `src/logic/pathfinding.c` - Full implementation: bow_offset, waypoint generation, stepping, direction with hysteresis
- `src/systems/player.c` - Added pathfinding.h include and lane_generate_waypoints call in player_init
- `src/logic/card_effects.c` - Added waypointIndex=1 after lane assignment in spawn_troop_from_card

## Decisions Made
- Sine curve for bow offset (sinf(depth * PI_F)) -- smooth taper at both endpoints, zero offset at spawn and end
- endDepth = 0.85 leaves room for future enemy base targeting
- Direction hysteresis at 15% relative threshold prevents sprite direction flicker near 45-degree movement
- Invalid lane (out of 0-2 range) transitions entity to IDLE instead of silently breaking
- pathfind_step_entity includes end-of-path jitter (LANE_JITTER_RADIUS) for visual variety at path terminus

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered
None

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Waypoint data layer complete; Plan 02 (entity movement integration) can consume laneWaypoints and call pathfind_step_entity from ESTATE_WALKING case
- Plan 01-00 (Wave 0 tests) validates this production code via pathfind_step_entity and pathfind_compute_direction calls
- All 6 files compile cleanly with make cardgame

## Self-Check: PASSED

- All 6 source files found
- Commits e99a130 and 13460fc verified in git log
- make cardgame compiles successfully

---
*Phase: 01-lane-pathfinding*
*Completed: 2026-03-28*
