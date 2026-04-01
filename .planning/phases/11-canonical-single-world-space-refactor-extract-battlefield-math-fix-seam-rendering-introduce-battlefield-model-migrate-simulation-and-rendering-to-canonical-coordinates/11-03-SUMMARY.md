---
phase: 11-canonical-single-world-space-refactor
plan: 03
subsystem: core
tags: [battlefield, canonical-coordinates, pathfinding, combat, entity-registry, coordinate-migration]

requires:
  - phase: 11-02
    provides: Battlefield model with canonical waypoints, spawn anchors, and entity registry

provides:
  - All simulation (spawn, pathfinding, combat, entity update) operates on canonical coordinates
  - map_to_opponent_space deleted from combat.c
  - Battlefield entity registry wired into spawn and dead-entity sweep
  - pathfind_step_entity reads canonical Battlefield waypoints
  - combat_in_range uses bf_distance on canonical positions directly

affects: [11-04-rendering-rewrite, 11-05-adapter-removal]

tech-stack:
  added: []
  patterns: [canonical-coordinate-migration, dual-registry-adapter]

key-files:
  created: []
  modified:
    - src/logic/card_effects.c
    - src/logic/pathfinding.h
    - src/logic/pathfinding.c
    - src/logic/combat.h
    - src/logic/combat.c
    - src/entities/entities.c
    - src/systems/player.c
    - tests/test_pathfinding.c
    - tests/test_combat.c

key-decisions:
  - "Pathfinding reads Battlefield canonical waypoints via bf_waypoint instead of Player.laneWaypoints"
  - "Combat uses bf_distance on canonical CanonicalPos -- no cross-space coordinate mapping"
  - "Dead entity sweep removes from both Battlefield and Player registries in player_update_entities"
  - "lane_generate_waypoints retained as adapter for Player.laneWaypoints backward compatibility"
  - "Test stubs for Battlefield are minimal structs with only the fields accessed by production code"

patterns-established:
  - "Dual-registry pattern: entities registered in both Battlefield (authoritative) and Player (adapter) during transition"
  - "Canonical coordinate convention: all Entity.position values are in world space (0..1080 x, 0..1920 y)"

requirements-completed: [REFACTOR-11]

duration: 6min
completed: 2026-04-01
---

# Phase 11 Plan 03: Simulation Migration to Canonical Coordinates Summary

**Spawn, pathfinding, and combat migrated to canonical Battlefield coordinates; map_to_opponent_space deleted; all 44 tests pass**

## Performance

- **Duration:** 6 min
- **Started:** 2026-04-01T17:09:39Z
- **Completed:** 2026-04-01T17:16:26Z
- **Tasks:** 2
- **Files modified:** 9

## Accomplishments
- Entity spawning now uses bf_spawn_pos for canonical positions and bf_slot_to_lane for canonical lane indices
- Pathfinding reads waypoints from Battlefield.laneWaypoints via bf_waypoint instead of Player.laneWaypoints
- Combat range checking uses bf_distance on canonical positions directly -- no coordinate space mapping
- map_to_opponent_space and dist2d deleted from combat.c (structural validation passes)
- Spawned entities registered in both Battlefield (authoritative) and Player (adapter) registries
- Dead entity sweep removes from both registries

## Task Commits

Each task was committed atomically:

1. **Task 1: Migrate spawn and entity registry to canonical Battlefield coordinates** - `09ee9f3` (feat)
2. **Task 2: Migrate pathfinding and combat to canonical Battlefield coordinates** - `e14aa2b` (feat)

## Files Created/Modified
- `src/logic/card_effects.c` - Canonical spawn via bf_spawn_pos, bf_slot_to_lane, bf_add_entity
- `src/logic/pathfinding.h` - Updated pathfind_step_entity signature to take const Battlefield *bf
- `src/logic/pathfinding.c` - Reads canonical waypoints via bf_waypoint
- `src/logic/combat.h` - Header unchanged (signatures compatible)
- `src/logic/combat.c` - Deleted map_to_opponent_space/dist2d; uses bf_distance on canonical positions
- `src/entities/entities.c` - ESTATE_WALKING passes gs->battlefield to pathfind_step_entity
- `src/systems/player.c` - Dead entity sweep also calls bf_remove_entity; added battlefield.h include
- `tests/test_pathfinding.c` - Added Battlefield stub; updated calls to new pathfind_step_entity interface
- `tests/test_combat.c` - Replaced cross-space mapping tests with canonical distance tests

## Decisions Made
- Pathfinding: Retained lane_generate_waypoints as adapter function for Player.laneWaypoints; Battlefield.laneWaypoints is authoritative
- Combat: Deleted map_to_opponent_space entirely rather than deprecating; combat_in_range ignores GameState parameter
- Tests: Used minimal Battlefield stubs with only accessed fields rather than including battlefield.c directly
- Entity sweep: bf_remove_entity called before entity_destroy to avoid dangling pointers in Battlefield registry
- Comment containing "map_to_opponent_space" literal removed from combat.c to pass structural validation rg check

## Deviations from Plan

None - plan executed exactly as written.

## Known Stubs

None - no new stubs introduced. Existing [ADAPTER] fields on Player remain as documented in Plan 11-02.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- All simulation operates in canonical coordinate space
- Rendering (Plan 04) can now draw entities directly from canonical positions
- Player adapter fields (playArea, laneWaypoints, entity arrays) remain for rendering backward compatibility
- seamRT and crossed-entity rendering code in game.c remains (transitional, to be deleted in Plan 04)

---
*Phase: 11-canonical-single-world-space-refactor*
*Completed: 2026-04-01*
