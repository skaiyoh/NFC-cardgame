# Testing

Verified on 2026-04-03.

## What Exists

The repo uses standalone assert-based test executables, not an external C test framework.

Current test binaries:

- `test_pathfinding`
- `test_combat`
- `test_battlefield_math`
- `test_battlefield`
- `test_animation`
- `test_debug_events`

## Local Result

`make test` compiled and passed all six test executables successfully in this environment.

## Coverage Areas

| Test Binary | Covers |
|------------|--------|
| `test_pathfinding` | canonical waypoint stepping, lane progression, movement direction, invalid lane handling |
| `test_combat` | canonical range checks, target selection, damage application, kill/clamp behavior |
| `test_battlefield_math` | coordinate transforms, seam helpers, bounds, slot-to-lane mapping |
| `test_battlefield` | territory setup, spawn anchors, waypoint generation, entity registry, side mapping |
| `test_animation` | clip playback, hit-marker behavior, animation policy, cycle calculations |
| `test_debug_events` | debug flash ring buffer behavior |

## Testing Pattern

The tests intentionally compile production `.c` files directly with local stubs.

Common pattern:

- predefine header guards to block heavy include chains
- define lightweight local stubs for `Vector2`, `Player`, `Battlefield`, Raylib types, or other dependencies
- include the production `.c` file directly
- call the real implementation under test

This pattern is used heavily in:

- `tests/test_pathfinding.c`
- `tests/test_combat.c`
- `tests/test_battlefield_math.c`
- `tests/test_battlefield.c`
- `tests/test_animation.c`

## Build-System Exposure

- `Makefile`
  - builds and runs all six tests with `make test`
- `CMakeLists.txt`
  - registers the same six tests with CTest

## What Is Not Covered By Automated Tests

- the full Raylib game loop in a real window
- hardware serial I/O against live Arduino devices
- SQLite data-layer behavior
- card renderer output correctness
- biome preview and card preview runtime behavior
- win-condition flow
- pregame/match-state flow
- spell effects
- base creation and projectile behavior

## CI Status

There is no CI workflow checked into the repository at the time of writing.

## Verification Caveat

`cmake` was not installed in this shell environment, so the CTest path was inspected from `CMakeLists.txt` but not executed locally here. The `Makefile` path was executed and passed.
