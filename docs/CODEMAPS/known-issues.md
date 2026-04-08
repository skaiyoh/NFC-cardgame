# Known Issues

All items below were verified from current source or current database state on
2026-04-08.

## High-Signal Gameplay Gaps

- Match/pregame flow is not implemented.
  - `match.c` contains declarations only
- Spell cards do not affect gameplay.
  - `play_spell()` only parses and prints metadata
- Projectile gameplay is not implemented.
  - `projectile.c` is empty
- There is no base HP UI and no in-game card/hand display.
- Match end freezes gameplay, but there is no restart/rematch flow.

## Data Caveat

- The checked-in `cardgame.db` and `sqlite/seed.sql` now both use uppercase
  `card_id` values.
- The checked-in `cardgame.db` contains 2 NFC mappings.
- A fresh schema+seed database contains 0 NFC mappings.
- Re-seeding an existing database preserves existing `nfc_tags` rows because
  the init target does not clear the DB first.
- Result: `init-db` still does not recreate a fully clean runtime DB unless
  the file is removed first.

## Systems That Exist But Do Not Fully Fire

- Card slot cooldowns count down, but no gameplay path sets a cooldown after a
  successful play.
- `CardSlot.activeCard` exists on the struct but is not used by runtime
  gameplay code.
- The handler functions for healer, assassin, brute, and farmer do not add
  extra behavior beyond calling the common spawn helper.
- `TARGET_SPECIFIC_TYPE` is parsed from card JSON, but combat targeting still
  falls back to nearest-target behavior.

## Verified Runtime Bugs

- Base idle timing changes do not apply on first spawn.
  - `entity_create()` initializes every entity with a generic `0.5s` idle clip.
  - `building_create_base()` assigns `SPRITE_TYPE_BASE` but does not
    reinitialize the animation state from `anim_spec_get()`.
  - Result: edits to base idle timing in `entity_animation.c` are ignored until
    the base later transitions through another animation state.

## Code Risks Already Called Out In Source

- `tilemap_create_biome()` uses `srand(seed)`, which mutates global PRNG state.
- `tilemap_draw_oriented()` trusts tile indices without a bounds check before
  indexing `tileDefs[]`.
- `card_action_register()` allows duplicate type registration and stores raw
  string pointers.
- `sprite_type_from_card()` silently falls back to the knight sprite type for
  unknown card types.
