# Data

Verified from source and `sqlite3` queries on 2026-04-04.

## Storage Model

- Primary runtime storage is a local SQLite file.
- Default path: `cardgame.db`
- Override: `DB_PATH`

The in-memory model is:

- `DB`
  - SQLite connection wrapper with `last_error`
- `Deck`
  - heap array of `Card`
  - optional heap array of `UIDMapping`

## Schema

`sqlite/schema.sql` defines two tables:

### `cards`

| Column | Type | Notes |
|--------|------|-------|
| `card_id` | `TEXT PRIMARY KEY` | runtime lookup key |
| `name` | `TEXT NOT NULL` | display name |
| `cost` | `INTEGER NOT NULL DEFAULT 0` | cost amount |
| `cost_resource` | `TEXT NOT NULL DEFAULT 'energy'` | `energy` or `sustenance` |
| `type` | `TEXT NOT NULL` | dispatch key for `card_action_play()` |
| `rules_text` | `TEXT` | descriptive text |
| `data` | `TEXT` | JSON blob consumed by gameplay and card rendering |

### `nfc_tags`

| Column | Type | Notes |
|--------|------|-------|
| `uid` | `TEXT PRIMARY KEY` | uppercase hex UID |
| `card_id` | `TEXT NOT NULL` | references `cards(card_id)` |

## Runtime JSON Fields Actually Read

### Troop Gameplay Fields

`troop_create_data_from_card()` reads these top-level keys from `Card.data`:

- `hp`
- `maxHP`
- `attack`
- `attackSpeed`
- `attackRange`
- `moveSpeed`
- `targeting`
- `targetType`

`targetType` is currently parsed and stored, but `combat_find_target()` still
falls back to nearest-target behavior for `TARGET_SPECIFIC_TYPE`.

### Card Visual Fields

`card_visual_from_json()` expects a nested `visual` object and reads:

- `border_color`, `show_border`
- `bg_style`, `show_bg`
- `banner_color`, `show_banner`
- `corner_color`, `show_corner`
- `container_color`, `container_variant`, `show_container`
- `description_style`, `show_description`
- `innercorner_style`, `show_innercorner`
- `gem_color`, `show_gem`
- `socket_color`, `show_socket`
- `energy_top_color`, `show_energy_top`
- `energy_bot_color`, `show_energy_bot`
- optional offsets such as `off_bg`, `off_border`, `off_gem`

## Checked-In Runtime Database

The checked-in `cardgame.db` currently contains eight cards with uppercase IDs:

- `ASSASSIN_01`
- `BIRD_01`
- `BRUTE_01`
- `FARMER_01`
- `FISHFING_01`
- `HEALER_01`
- `KING_01`
- `KNIGHT_01`

It also currently contains zero rows in `nfc_tags` on fresh regeneration.

## Fresh Seeded Database

A fresh database created from `sqlite/schema.sql` and `sqlite/seed.sql`
currently contains:

- eight cards
- uppercase IDs (`ASSASSIN_01`, `BIRD_01`, ..., `KNIGHT_01`)
- zero rows in `nfc_tags`

## Re-Seeding The Checked-In Database

A seeded copy of the checked-in `cardgame.db` currently ends up with:

- six cards
- the same uppercase runtime IDs
- the original 2 NFC mappings still present

This happens because:

- `sqlite/schema.sql` uses `CREATE TABLE IF NOT EXISTS`
- `sqlite/seed.sql` now upserts using the same uppercase `card_id` values as the checked-in database
- `nfc_tags` is not seeded or cleared by the init target

## Access Patterns

- `cards_load()` loads the whole `cards` table into memory at startup.
- `cards_find()` is a linear scan by `card_id`.
- `cards_load_nfc_map()` loads `nfc_tags` into memory after the deck loads.
- `cards_find_by_uid()` first resolves `uid -> card_id`, then resolves
  `card_id -> Card`.

## Data Caveat

Card ID casing is now aligned between the checked-in database and the seed SQL.
The remaining shape difference is NFC mappings: a fresh seeded database starts
with zero `nfc_tags` rows, while re-seeding an existing runtime DB preserves
its existing mappings.
