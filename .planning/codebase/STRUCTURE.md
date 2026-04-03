# Codebase Structure

**Analysis Date:** 2026-03-28

## Directory Layout

```
NFC-cardgame/
├── src/                        # All game source code
│   ├── core/                   # Entry point, game loop, types, config
│   ├── data/                   # Database access and card data
│   ├── entities/               # Entity lifecycle, troops, buildings, projectiles
│   ├── hardware/               # NFC reader serial protocol
│   ├── logic/                  # Game rules (card effects, combat, win conditions)
│   ├── rendering/              # Biomes, tilemaps, sprites, cards, viewport, UI
│   ├── systems/                # Player management, energy, spawn, match phases
│   └── assets/                 # Sprite sheets, tilesets, card art (PNG files)
│       ├── cards/              # Card sprite sheet and individual card images
│       ├── characters/         # Character sprite sheets (per-type subdirectories)
│       └── environment/        # Tileset art (grass, undead, cursed land)
├── tools/                      # Standalone utility programs
├── lib/                        # Vendored third-party headers and sources
├── sqlite/                     # Database schema and seed data
├── md/                         # Project documentation
├── .planning/                  # Planning and analysis documents
├── Makefile                    # Build system (all targets)
├── cardgame                    # Main game binary (build output)
├── cardgame.db                 # SQLite database (runtime data)
├── README.md                   # Project readme
└── .gitignore                  # Git ignore rules
```

## Directory Purposes

**`src/core/`:**
- Purpose: Central hub -- game loop, global types, compile-time configuration
- Contains: 4 files (2 .c, 2 .h)
- Key files:
  - `src/core/game.c` -- `main()`, `game_init`, `game_update`, `game_render`, `game_cleanup`, NFC event handling, entity viewport drawing
  - `src/core/game.h` -- Public game loop API (4 functions)
  - `src/core/types.h` -- All core structs (`GameState`, `Player`, `Entity`, `CardSlot`), enums (`EntityType`, `Faction`, `EntityState`), constants (`MAX_ENTITIES=64`, `NUM_CARD_SLOTS=3`). Includes headers from data/, rendering/, hardware/
  - `src/core/config.h` -- `#define` constants: `SCREEN_WIDTH` (1920), `SCREEN_HEIGHT` (1080), asset paths, sprite sizes, gameplay tuning, energy bar dimensions

**`src/data/`:**
- Purpose: Data access layer -- SQLite wrapper and card loading
- Contains: 4 files (2 .c, 2 .h)
- Key files:
  - `src/data/db.h` / `src/data/db.c` -- `DB` struct, `db_init`, `db_close`, `db_query`, `db_query_params`, `DBResult` (rows/cols table of strings)
  - `src/data/cards.h` / `src/data/cards.c` -- `Card` struct, `Deck` struct, `UIDMapping` struct, `cards_load`, `cards_find`, `cards_find_by_uid`, `cards_load_nfc_map`

**`src/entities/`:**
- Purpose: Entity types and their lifecycle/behavior
- Contains: 8 files (4 .c, 4 .h)
- Key files:
  - `src/entities/entities.c` -- Generic entity create/destroy/update/draw/set_state. Movement logic (walk upward, despawn past opponent's area)
  - `src/entities/troop.c` -- `TroopData` parsed from card JSON via cJSON. `troop_spawn()` creates a configured entity with stats and sprite
  - `src/entities/building.c` -- Stub: `building_create_base()` returns NULL
  - `src/entities/projectile.c` -- Stub: empty file with TODO comments

**`src/hardware/`:**
- Purpose: Physical NFC reader integration via Arduino serial protocol
- Contains: 4 files (2 .c, 2 .h)
- Key files:
  - `src/hardware/nfc_reader.c` -- Opens serial ports (115200 baud, raw, non-blocking), polls for NFC events, debounces UID changes with removal timeout (30 frames)
  - `src/hardware/arduino_protocol.c` -- 5-state FSM packet parser for binary wire format `[0xAA | reader_idx | uid_len | uid_bytes... | XOR_checksum]`

**`src/logic/`:**
- Purpose: Game rules and card behavior
- Contains: 8 files (4 .c, 4 .h)
- Key files:
  - `src/logic/card_effects.c` -- Card action registry (function pointer table), `card_action_init` registers 6 handlers (spell, knight, healer, assassin, brute, farmer), `spawn_troop_from_card` helper with energy check
  - `src/logic/combat.c` -- Stub: forward declarations only (combat_resolve, combat_in_range, combat_find_target)
  - `src/logic/pathfinding.c` -- Stub: forward declaration only (pathfind_next_step)
  - `src/logic/win_condition.c` -- Stub: forward declarations only (win_check, win_trigger)

**`src/rendering/`:**
- Purpose: All visual rendering subsystems
- Contains: 12 files (6 .c, 6 .h)
- Key files:
  - `src/rendering/biome.c` -- 545 lines. Defines 4 biomes (grass, undead, snow, swamp). Undead has 5 paint layers with static cell data. Compiles TileBlocks into flat TileDef arrays
  - `src/rendering/tilemap_renderer.c` -- Tilemap creation (weighted random tile placement), drawing for base/detail/biome layers
  - `src/rendering/sprite_renderer.c` -- Loads sprite sheets for 6 character types (base + 5 types), AnimState update/draw with directional frames
  - `src/rendering/card_renderer.c` -- 523 lines. Parses card sprite sheet into atlas regions, draws 11 composited card layers, JSON-to-CardVisual parser
  - `src/rendering/viewport.c` -- Split-screen init (2 players, rotated cameras), scissor+camera begin/end, tilemap drawing
  - `src/rendering/ui.c` -- Energy bar HUD (screen space)

**`src/systems/`:**
- Purpose: Player state management and game system coordination
- Contains: 8 files (4 .c, 4 .h)
- Key files:
  - `src/systems/player.c` -- 252 lines. `player_init` (camera, tilemap, energy, card slots), `player_update` (energy + cooldowns), entity management (add/remove/find/update+sweep), position helpers (tile_to_world, lane_pos, base_pos, front_pos, slot_spawn_pos)
  - `src/systems/energy.c` -- Energy init/update/can_afford/consume/restore/set_regen_rate
  - `src/systems/spawn.c` -- Stub: redirect comment (spawn logic in card_effects.c)
  - `src/systems/match.c` -- Stub: pregame phase forward declarations (pregame_init, pregame_update, pregame_render, etc.)

**`src/assets/`:**
- Purpose: Game art assets (PNG sprite sheets and tilesets)
- Contains: PNG files only (no code)
- Subdirectories:
  - `src/assets/cards/` -- Card sprite sheet (`ModularCardsRPG/modularCardsRPGSheet.png`), individual card images
  - `src/assets/characters/` -- Per-type subdirectories: `Base/`, `Knight/`, `Healer/`, `Assassin/`, `Brute/`, `Farmer/`. Each contains animation PNGs: idle.png, walk.png, run.png, hurt.png, death.png, attack.png (naming varies)
  - `src/assets/environment/` -- Tileset packs: `Pixel Art Top Down - Basic v1.2.3/` (grass), `Undead-Tileset-Top-Down-Pixel-Art/` (undead), `Cursed-Land-Top-Down-Pixel-Art-Tileset/` (unused currently)

**`tools/`:**
- Purpose: Standalone utility programs (separate binaries, not part of main game)
- Contains: 3 .c files
- Key files:
  - `tools/card_preview.c` -- 301 lines. Raylib GUI for editing card visuals. No DB needed. Links: card_renderer.c, cJSON.c
  - `tools/biome_preview.c` -- 2010 lines. Raylib GUI for defining biome tile blocks interactively. No DB needed. Links: tilemap_renderer.c, biome.c
  - `tools/card_enroll.c` -- 131 lines. CLI tool for mapping NFC UIDs to card_ids. Needs DB + serial port

**`lib/`:**
- Purpose: Vendored third-party headers and source files
- Contains: 7 files
- Key files:
  - `lib/raylib.h` -- Raylib graphics library header (system library linked at build time)
  - `lib/raymath.h` -- Raylib math utilities
  - `lib/rlgl.h` -- Raylib OpenGL abstraction
  - `lib/cJSON.h` / `lib/cJSON.c` -- JSON parser (compiled into all targets that parse card data)
  - `lib/libpq-fe.h` / `lib/postgres_ext.h` -- Legacy PostgreSQL headers (no longer used; DB migrated to SQLite)

**`sqlite/`:**
- Purpose: Database schema and seed data
- Contains: 2 .sql files
- Key files:
  - `sqlite/schema.sql` -- Creates `cards` table (card_id PK, name, cost, type, rules_text, data JSON) and `nfc_tags` table (uid PK, card_id FK)
  - `sqlite/seed.sql` -- Inserts 6 cards: knight_01, assassin_01, brute_01, farmer_01, healer_01, fireball_01 (with full JSON data including visual config and combat stats)

**`md/`:**
- Purpose: Project documentation
- Contains: Markdown files
- Key files:
  - `md/CARD_DATA_GUIDE.md` -- Card data JSON format documentation
  - `md/TODO.md` -- Project task list

## Key File Locations

**Entry Points:**
- `src/core/game.c`: Main game entry point (`main()` at line 217)
- `tools/card_preview.c`: Card preview tool entry point
- `tools/biome_preview.c`: Biome preview tool entry point
- `tools/card_enroll.c`: Card enrollment tool entry point

**Configuration:**
- `src/core/config.h`: All compile-time constants (screen size, paths, tuning)
- `Makefile`: Build targets and source file lists
- `sqlite/schema.sql`: Database schema
- `sqlite/seed.sql`: Initial card data

**Core Logic:**
- `src/logic/card_effects.c`: Card type -> behavior dispatch (the bridge between NFC input and game entities)
- `src/entities/entities.c`: Entity update loop (movement, state transitions)
- `src/systems/player.c`: Player state management and entity lifecycle

**Database:**
- `cardgame.db`: SQLite database file (runtime, in project root)
- `src/data/db.c`: Database access layer
- `src/data/cards.c`: Card data loading

**Testing:**
- No test files exist. No test framework is configured.

## Naming Conventions

**Files:**
- `snake_case.c` / `snake_case.h`: All source and header files use lowercase snake_case
- Header/source pairs share the same base name: `biome.h` / `biome.c`
- Tool files named descriptively: `card_preview.c`, `biome_preview.c`, `card_enroll.c`

**Directories:**
- `snake_case/`: All directories use lowercase (e.g., `card_renderer`, `sprite_renderer`)
- Organized by architectural layer, not by feature

**Functions:**
- `module_action()` pattern: `entity_create()`, `player_init()`, `biome_compile_blocks()`, `energy_consume()`
- Static helpers prefixed with module name or are purely local: `open_serial_port()`, `rect_center()`
- Public functions declared in matching `.h` file

**Structs/Types:**
- `PascalCase` for struct names: `GameState`, `Player`, `Entity`, `TroopData`, `BiomeDef`
- `PascalCase` for enum type names: `EntityType`, `BiomeType`, `AnimationType`
- `UPPER_SNAKE_CASE` for enum values: `ENTITY_TROOP`, `BIOME_GRASS`, `ANIM_IDLE`
- `UPPER_SNAKE_CASE` for `#define` constants: `MAX_ENTITIES`, `TILE_COUNT`, `SCREEN_WIDTH`

**Variables:**
- `camelCase` for local variables and struct fields: `deltaTime`, `entityCount`, `moveSpeed`, `tileScale`
- `s_` prefix for file-static globals: `s_nextEntityID`

## Where to Add New Code

**New Card Type:**
1. Add handler function in `src/logic/card_effects.c` (follow `play_knight` pattern)
2. Register it in `card_action_init()` with `card_action_register("typename", play_typename)`
3. Add sprite mapping in `src/rendering/sprite_renderer.c`: add `SPRITE_TYPE_TYPENAME` to `SpriteType` enum, load sheets in `sprite_atlas_init()`, add case in `sprite_type_from_card()`
4. Add character sprite PNGs in `src/assets/characters/TypeName/`
5. Add seed data in `sqlite/seed.sql`

**New Entity Type (e.g., tower, projectile):**
1. Implement in `src/entities/` (new .c/.h pair or fill in existing stubs like `projectile.c`)
2. Add to `EntityType` enum in `src/core/types.h`
3. Add case in `entity_update()` switch in `src/entities/entities.c`
4. Add source files to `SRC_ENTITIES` in `Makefile`

**New Biome:**
1. Add enum value to `BiomeType` in `src/rendering/biome.h`
2. Add `biome_define_newbiome()` static function in `src/rendering/biome.c`
3. Call it from `biome_init_all()`
4. Add tileset PNGs to `src/assets/environment/`

**New Game System:**
1. Create `src/systems/newsystem.h` and `src/systems/newsystem.c`
2. Add to `SRC_SYSTEMS` in `Makefile`
3. Call init from `game_init()`, update from `game_update()`, cleanup from `game_cleanup()`

**New Rendering Component:**
1. Create `src/rendering/newcomponent.h` and `src/rendering/newcomponent.c`
2. Add to `SRC_RENDERING` in `Makefile`
3. Call from `game_render()` in the appropriate viewport or HUD phase

**New Tool Binary:**
1. Create `tools/newtool.c` with its own `main()`
2. Add Makefile target: specify only the source files it needs (tools do not link the full game)
3. Tools should NOT link `libpq`; use `-lraylib -lm` (visual tools) or `-lsqlite3 -lm` (DB tools)
4. Add binary name to `clean:` target in `Makefile`

**New UI Element:**
1. Add function to `src/rendering/ui.h` / `src/rendering/ui.c`
2. Call from `game_render()` after `viewport_end()` (screen space) or inside `viewport_begin()`/`viewport_end()` block (world space)

## Special Directories

**`lib/`:**
- Purpose: Vendored third-party code (headers + cJSON source)
- Generated: No (manually vendored)
- Committed: Yes
- Note: `libpq-fe.h` and `postgres_ext.h` are legacy from a PostgreSQL era; no longer used at build time but still present

**`src/assets/`:**
- Purpose: Binary art assets (PNG sprite sheets, tilesets)
- Generated: No (artist-created)
- Committed: Yes
- Note: Paths are hardcoded in `src/core/config.h`; moving assets requires updating those defines

**`.planning/`:**
- Purpose: Planning and analysis documents for development workflow
- Generated: By development tools
- Committed: Yes

**Build outputs (project root):**
- `cardgame` -- main game binary
- `card_preview` -- card preview tool binary
- `biome_preview` -- biome preview tool binary
- `card_enroll` -- card enrollment tool binary
- `cardgame.db` -- SQLite database (initialized via `make init-db`)
- All removed by `make clean` (except `cardgame.db`)

## Build System

**Makefile targets:**

| Target | Output Binary | Dependencies | Description |
|--------|--------------|--------------|-------------|
| `cardgame` | `cardgame` | All SRC_* + cJSON | Main game (`-lsqlite3 -lraylib -lm`) |
| `preview` | `card_preview` | card_preview.c, card_renderer.c, cJSON.c | Card visual editor (`-lraylib -lm`) |
| `biome_preview` | `biome_preview` | biome_preview.c, tilemap_renderer.c, biome.c | Biome tile editor (`-lraylib -lm`) |
| `card_enroll` | `card_enroll` | card_enroll.c, db.c, cards.c, nfc_reader.c, arduino_protocol.c, cJSON.c | NFC enrollment CLI (`-lsqlite3 -lm`) |
| `init-db` | cardgame.db | schema.sql, seed.sql | Initialize SQLite database |
| `run` | -- | cardgame | Build + run with NFC_PORT env vars |
| `clean` | -- | -- | Remove all binaries |

**Source file groups (must be updated when adding files):**
- `SRC_CORE` -- `src/core/` source files
- `SRC_DATA` -- `src/data/` source files
- `SRC_RENDERING` -- `src/rendering/` source files
- `SRC_ENTITIES` -- `src/entities/` source files
- `SRC_SYSTEMS` -- `src/systems/` source files
- `SRC_LOGIC` -- `src/logic/` source files
- `SRC_HARDWARE` -- `src/hardware/` source files
- `SRC_LIB` -- `lib/cJSON.c`

---

*Structure analysis: 2026-03-28*
