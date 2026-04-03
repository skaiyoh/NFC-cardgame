# Known Issues

All items below were verified from current source or current database state on 2026-04-03.

## High-Signal Gameplay Gaps

- Base gameplay is not implemented.
  - `building_create_base()` returns `NULL`
  - there are no base entities on the battlefield
- Win handling is not implemented.
  - `win_condition.c` contains declarations only
- Match/pregame flow is not implemented.
  - `match.c` contains declarations only
- Spell cards do not affect gameplay.
  - `play_spell()` only parses and prints metadata
- Projectile gameplay is not implemented.
  - `projectile.c` is empty

## Data Mismatch

- The checked-in `cardgame.db` uses uppercase `card_id` values.
- `sqlite/seed.sql` creates lowercase `card_id` values on a fresh database.
- The checked-in `cardgame.db` contains 2 NFC mappings.
- A fresh schema+seed database contains 0 NFC mappings.
- Result: `init-db` does not currently recreate the same runtime data shape that ships in the repository.

## Systems That Exist But Do Not Fully Fire

- Card slot cooldowns count down, but no gameplay path sets a cooldown after a successful play.
- `CardSlot.activeCard` exists on the struct but is not used by runtime gameplay code.
- The handler functions for healer, assassin, brute, and farmer do not add extra behavior beyond calling the common spawn helper.

## Code Risks Already Called Out In Source

- `tilemap_create_biome()` uses `srand(seed)`, which mutates global PRNG state.
- `tilemap_draw_oriented()` trusts tile indices without a bounds check before indexing `tileDefs[]`.
- `card_action_register()` allows duplicate type registration and stores raw string pointers.
- `sprite_type_from_card()` silently falls back to the knight sprite type for unknown card types.
