---
phase: 11-canonical-single-world-space-refactor
plan: 05
subsystem: core
tags: [c, refactor, architecture, player-struct, battlefield, canonical-coordinates, assertions]

# Dependency graph
requires:
  - phase: 11-canonical-single-world-space-refactor (plans 01-04)
    provides: Battlefield model, canonical coordinates, entity registry, rendering migration
provides:
  - Lean Player struct (seat/view/input/resource only)
  - All adapter fields removed from Player
  - BF_ASSERT_IN_BOUNDS debug assertions at spawn and pathfinding
  - All remap/seam transitional code deleted from src/
  - Phase 11 canonical architecture locked
affects: [future-phases, combat-system, base-creation, win-condition]

# Tech tracking
tech-stack:
  added: []
  patterns: [lean-player-struct, battlefield-entity-ownership, canonical-coordinate-assertions]

key-files:
  created: []
  modified:
    - src/core/types.h
    - src/systems/player.h
    - src/systems/player.c
    - src/rendering/viewport.c
    - src/rendering/viewport.h
    - src/core/game.c
    - src/logic/card_effects.c
    - src/logic/combat.c
    - src/logic/pathfinding.c
    - src/logic/pathfinding.h
    - tests/test_pathfinding.c
    - tests/test_combat.c

key-decisions:
  - "Player struct reduced to 9 fields: id, side, screenArea, camera, cameraRotation, slots[], energy, maxEnergy, energyRegenRate"
  - "combat_find_target iterates Battlefield entities with ownerID filter instead of Player entity arrays"
  - "Base fallback in combat_find_target removed (deferred to future base-in-Battlefield implementation)"
  - "lane_generate_waypoints fully deleted from pathfinding (canonical waypoints owned by bf_init)"

patterns-established:
  - "Player is seat/view/input/resource only: no geometry, no entities, no tilemap"
  - "All entity iteration goes through Battlefield.entities[], not Player.entities[]"
  - "BF_ASSERT_IN_BOUNDS guards coordinate invariants at mutation entry points"

requirements-completed: [REFACTOR-11]

# Metrics
duration: 10min
completed: 2026-04-01
---

# Phase 11 Plan 05: Remove Adapter Fields and Lock Canonical Architecture

**Lean Player struct with 9 fields, BF_ASSERT_IN_BOUNDS at spawn/pathfinding, all remap/seam code deleted, 45 tests passing**

## Performance

- **Duration:** 10 min
- **Started:** 2026-04-01T17:27:00Z
- **Completed:** 2026-04-01T17:37:00Z
- **Tasks:** 1 of 2 (Task 2 is checkpoint:human-verify)
- **Files modified:** 12

## Accomplishments
- Removed 13 adapter fields from Player struct, reducing it to 9 essential fields (id, side, screenArea, camera, cameraRotation, slots, energy, maxEnergy, energyRegenRate)
- Deleted 14 dead Player functions (player_add_entity, player_remove_entity, player_find_entity, player_update_entities, player_draw_entities, player_init_card_slots, player_tile_to_world, player_world_to_tile, player_center, player_base_pos, player_front_pos, player_lane_pos, player_slot_spawn_pos, lane_generate_waypoints)
- Added BF_ASSERT_IN_BOUNDS debug assertions at entity spawn (card_effects.c) and pathfinding entry (pathfinding.c)
- Migrated game_update entity loop from Player to Battlefield registry
- Migrated combat_find_target from Player entity arrays to Battlefield entity registry with ownerID filtering
- All 45 tests pass across 4 test suites (pathfinding, combat, battlefield_math, battlefield)
- All VALIDATION.md structural grep checks pass in their documented source scopes: zero hits for seamRT, spriteOverlapsSeam, game_map_crossed_world_point, game_apply_crossed_direction, map_to_opponent_space

## Task Commits

Each task was committed atomically:

1. **Task 1: Remove Player adapter fields, add BattleSide, update all consumers** - `2a68481` (feat)

**Task 2:** checkpoint:human-verify -- awaiting visual verification

## Files Created/Modified
- `src/core/types.h` - Player struct reduced to 9 lean fields with BattleSide side
- `src/systems/player.h` - Cleaned declaration: only init, update, cleanup, slot access
- `src/systems/player.c` - Rewrote player_init to take BattleSide + Battlefield, deleted 14 functions
- `src/rendering/viewport.c` - Simplified viewport_init_split_screen with new player_init signature
- `src/rendering/viewport.h` - Removed viewport_draw_tilemap adapter declaration
- `src/core/game.c` - game_update iterates Battlefield entities, deleted crossed remap functions
- `src/logic/card_effects.c` - Removed player_add_entity, added BF_ASSERT_IN_BOUNDS at spawn
- `src/logic/combat.c` - combat_find_target iterates Battlefield entities with ownerID filter
- `src/logic/pathfinding.c` - Deleted lane_generate_waypoints, added BF_ASSERT_IN_BOUNDS at entry
- `src/logic/pathfinding.h` - Removed lane_generate_waypoints declaration
- `tests/test_pathfinding.c` - Rewrote for Battlefield-only stubs, 6 tests
- `tests/test_combat.c` - Updated stubs for lean Player, Battlefield entity targeting, 18 tests

## Decisions Made
- Player struct reduced to identity (id, side), viewport (screenArea, camera, cameraRotation), input (slots[]), and resource (energy) fields only
- combat_find_target now iterates all Battlefield entities with ownerID != attacker->ownerID filter, replacing the old pattern of iterating enemy Player's entity array
- Base fallback in combat_find_target removed (was: `if (!bestTarget && enemy->base)`) since bases will be registered in Battlefield entity registry in a future phase
- lane_generate_waypoints fully deleted -- canonical waypoints are generated once by bf_init and read via bf_waypoint()

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Updated combat test stubs for Battlefield entity targeting**
- **Found during:** Task 1 (test compilation)
- **Issue:** test_combat.c stubs had old Player struct with entities[]/entityCount/base fields; combat_find_target now iterates Battlefield entities
- **Fix:** Updated Player stub to lean struct, added Battlefield stub with entities array, created bf_test_add_entity helper, replaced test_find_target_falls_back_to_base with test_find_target_skips_friendly
- **Files modified:** tests/test_combat.c
- **Verification:** All 18 combat tests pass
- **Committed in:** 2a68481

---

**Total deviations:** 1 auto-fixed (1 bug fix)
**Impact on plan:** Test stub updates were necessary due to the Player struct change. No scope creep.

## Issues Encountered
None

## Known Stubs
None -- all changes are structural cleanup; no new stubs introduced.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Phase 11 canonical architecture is locked: one world, two cameras, no remap
- Future phases can build on Battlefield as the single source of truth
- Base creation (building_create_base) still returns NULL -- must be addressed in a future phase
- Combat base fallback removed -- bases need to be registered in Battlefield entity registry

---
*Phase: 11-canonical-single-world-space-refactor*
*Completed: 2026-04-01*
