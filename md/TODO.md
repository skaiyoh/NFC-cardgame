# NFC Card Game - TODO

Last verified: 2026-04-01

## Verification Snapshot

- `make test` passes:
  - pathfinding: 6 tests
  - combat: 18 tests
  - battlefield_math: 12 tests
  - battlefield: 9 tests
- `make cardgame` builds successfully
- `Makefile` and `CMakeLists.txt` list the same main source groups by inspection
- CMake was not build-verified in this environment because `cmake` is not installed

---

## Implemented and Verified

| Feature | Files | Notes |
|---------|-------|-------|
| Database connection and card loading | `src/data/db.c`, `src/data/cards.c` | Loads cards and NFC UID mappings from SQLite |
| Card action registry and troop card dispatch | `src/logic/card_effects.c` | All current troop card types route through the play registry |
| Troop spawn pipeline | `src/logic/card_effects.c`, `src/entities/troop.c`, `src/systems/energy.c` | Enforces energy, reads JSON stats, spawns into canonical battlefield lanes |
| Entity lifecycle and state machine | `src/entities/entities.c` | Create, update, draw, destroy, idle/walk/attack/dead states |
| Canonical battlefield model | `src/core/battlefield.c`, `src/core/battlefield_math.c` | Shared world space, territory layout, spawn anchors, lane waypoints |
| Pathfinding and lane walking | `src/logic/pathfinding.c` | Troops walk canonical waypoint paths and idle at the end |
| Combat targeting and damage | `src/logic/combat.c` | Nearest/building targeting, range checks, cooldown-based damage |
| Split-screen viewport model | `src/rendering/viewport.c`, `src/systems/player.c` | Shared battlefield rendered from both player perspectives |
| Sprite animation loading/rendering | `src/rendering/sprite_renderer.c` | Base plus current troop sprite sets and all animation slots are loaded |
| Card visual rendering | `src/rendering/card_renderer.c` | Atlas-driven card frame/background/icon composition |
| Biome-aware tilemap rendering | `src/rendering/tilemap_renderer.c`, `src/rendering/biome.c` | Procedural territory tilemaps with biome definitions |
| Core game loop and NFC event handling | `src/core/game.c`, `src/hardware/nfc_reader.c`, `src/hardware/arduino_protocol.c` | Init, update, render, cleanup, keyboard test input, NFC polling |
| HUD basics | `src/rendering/ui.c` | Energy bars and viewport labels are implemented |
| Build/test maintenance | `Makefile`, `CMakeLists.txt`, `tests/*.c` | Main source lists are aligned; unit tests cover battlefield, math, pathfinding, and combat |

---

## Stubbed or Partial

| Feature | Files | Current state |
|---------|-------|---------------|
| Building/base system | `src/entities/building.c`, `src/entities/building.h` | API exists, but `building_create_base()` returns `NULL` and `building_take_damage()` is empty |
| Win conditions | `src/logic/win_condition.c`, `src/logic/win_condition.h` | Header is empty; source contains declarations/comments only; `GameState` has no game-over fields yet |
| Pregame / match flow | `src/systems/match.c`, `src/systems/match.h` | Header is empty; source contains declarations/comments only |
| Projectile system | `src/entities/projectile.c`, `src/entities/projectile.h` | Header is empty; source is comment-only |
| Spell gameplay effects | `src/logic/card_effects.c` | `play_spell()` spends energy and prints parsed data, but applies no gameplay effect |
| Death animation lifecycle | `src/entities/entities.c` | `ESTATE_DEAD` marks entities for removal immediately, so `ANIM_DEATH` does not finish |
| Idle combat scanning | `src/entities/entities.c` | Idle entities do not search for targets until another state transition happens |
| Snow and swamp biome identity | `src/rendering/biome.c` | Both are still placeholders built from the grass tileset |
| Spawn system ownership | `src/systems/spawn.c` | File exists, but actual spawn logic still lives in `card_effects.c` and `troop.c` |

---

## Highest Priority

1. Implement player bases as real building entities.
   - Finish `building_create_base()` and `building_take_damage()`.
   - Decide where base references live (`Player`, `GameState`, or battlefield lookup).
   - Spawn/register both bases during startup so the match has actual objectives.

2. Implement win-condition and match-end state.
   - Add end-state fields to `GameState` before wiring `win_check()` / `win_trigger()`.
   - Call `win_check()` from `game_update()` after entity updates.
   - Add a visible game-over path instead of letting matches run forever.

3. Add base-health UI.
   - Draw HP bars for both bases in `src/rendering/ui.c`.
   - Add a simple winner overlay once match-end state exists.

4. Fix death handling so death animations can complete.
   - Keep dead entities alive long enough to play `ANIM_DEATH`.
   - Remove them only after the animation finishes.

5. Make spell cards do real gameplay work.
   - Replace the current debug-print path with actual targeting/effect resolution.

6. Build the projectile system if ranged attacks are part of the near-term plan.
   - Define the header API first.
   - Decide whether projectiles live in the battlefield entity list or in a dedicated pool.

7. Add a real pregame/match phase.
   - Replace the declarations in `match.c` with real state and transitions.
   - Gate gameplay start on player readiness / deck confirmation.

---

## Secondary Cleanup

- Fix the macro redefinition warning in `src/rendering/biome.c` (`R` is defined twice).
- Move shared spawn orchestration out of `card_effects.c` if `src/systems/spawn.c` is meant to own it.
- Fill out the empty public headers for `match`, `projectile`, and `win_condition`.
- Revisit idle combat scanning after bases exist so lane-end troops do not stall forever.
