# Project Status

Verified from source and local commands on 2026-04-03.

## Project Summary

This repository is a local two-player C/Raylib card game with optional Arduino NFC input, a canonical `Battlefield` world model, SQLite-backed card data, and three standalone tools (`card_preview`, `biome_preview`, `card_enroll`).

The codebase is not a complete end-to-end match game yet. The current implementation supports spawning troops, moving them through canonical lane paths, finding targets, applying hit-synced combat damage, playing death animations, and rendering both players' views over the same battlefield. Base creation, match phases, win handling, spell effects, and projectiles are still missing.

## Verified Working Now

- `make clean cardgame` rebuilds the main game successfully.
- `make preview`, `make biome_preview`, and `make card_enroll` all build successfully.
- `make test` passes all six standalone test binaries:
  - `test_pathfinding`
  - `test_combat`
  - `test_battlefield_math`
  - `test_battlefield`
  - `test_animation`
  - `test_debug_events`
- Runtime architecture is canonical:
  - one `1080 x 1920` battlefield
  - seam at `y = 960`
  - bottom and top territories rendered through separate player cameras
  - Player 2 rendered through `p2RT` and composited to the right viewport
- Card data loads from SQLite through `db.c` and `cards.c`.
- NFC UID mappings load from the `nfc_tags` table at startup.
- Troop spawn flow is wired:
  - card lookup
  - energy check
  - JSON stat parse
  - canonical spawn position lookup
  - entity creation
  - Battlefield registry insert
- Troops path through canonical waypoints from `Battlefield`.
- Troops can acquire targets, play attack clips, apply damage at the attack hit marker, die, and get swept after the death clip finishes.
- Debug rendering exists and is wired:
  - `F1` lane paths
  - `F2` attack bars
  - `F3` target lines
  - `F4` event flashes
  - `F5` attack range circles

## Partial Or Caveated Behavior

- The game starts directly in live play. There is no pregame/ready flow or game-over phase machine.
- Spell cards consume energy and print effect metadata, but they do not change game state.
- Card-type handlers for healer, assassin, brute, and farmer do not add custom behavior of their own.
  - Current gameplay differences come from per-card stats and targeting values parsed from card JSON.
  - `BRUTE_01` does already get building-priority targeting because its JSON sets `"targeting": "building"`.
- Card slots track `cooldownTimer`, but nothing currently sets a non-zero cooldown after play.
- `CardSlot.activeCard` exists on the struct but is not used by runtime gameplay code.
- `game_handle_test_input()` only plays `KNIGHT_01`.

## Not Implemented

- `src/entities/building.c`
  - `building_create_base()` returns `NULL`
- `src/logic/win_condition.c`
  - declarations only, no implementation
- `src/systems/match.c`
  - declarations only, no implementation
- `src/entities/projectile.c`
  - empty placeholder

Because of those gaps, the repo does not currently support:

- base spawning
- base damage or destruction
- match win detection
- win screen presentation
- pregame readiness flow
- projectile-driven units

## Database Reality

The checked-in runtime database and the seed SQL are not currently equivalent.

- Checked-in `cardgame.db`
  - 6 cards
  - all `card_id` values uppercase (`KNIGHT_01`, `ASSASSIN_01`, ...)
  - 2 NFC mappings in `nfc_tags`
- Fresh database from `sqlite/schema.sql` + `sqlite/seed.sql`
  - 6 cards
  - all `card_id` values lowercase (`knight_01`, `assassin_01`, ...)
  - 0 NFC mappings

That mismatch matters:

- the keyboard smoke path in `game.c` looks up `KNIGHT_01`
- it works with the checked-in `cardgame.db`
- it does not work after recreating the database from the current seed files unless the casing is aligned

## Verification Notes

- `raylib` was available locally through `pkg-config` as version `5.5.0`.
- `cmake` was not installed in this shell environment, so the CMake/CTest path was inspected from `CMakeLists.txt` but not executed here.
