# Architecture

**Analysis Date:** 2026-03-28

## Pattern Overview

**Overall:** Layered procedural architecture with a classic init/update/render game loop

**Key Characteristics:**
- Single-threaded game loop (init -> update -> render -> cleanup)
- Flat struct-based data model (no OOP inheritance; C structs with function-based dispatch)
- Split-screen 2-player design with rotated Camera2D viewports
- Physical NFC hardware integration via serial protocol (optional; keyboard fallback)
- SQLite database for card definitions and NFC tag enrollment
- Card type -> behavior dispatch via a function pointer registry (`CardHandler`)

## Layers

**Core (`src/core/`):**
- Purpose: Application entry point, game loop orchestration, global configuration, and central type definitions
- Location: `src/core/`
- Contains: `main()`, `game_init/update/render/cleanup`, `GameState` struct, `Player` struct, `Entity` struct, all enums
- Depends on: Every other layer (initializes and orchestrates all subsystems)
- Used by: Nothing (top-level entry point)
- Key files:
  - `src/core/game.c` -- `main()`, game loop, NFC event dispatch, entity drawing per viewport
  - `src/core/game.h` -- Public API: `game_init`, `game_update`, `game_render`, `game_cleanup`
  - `src/core/types.h` -- Central type definitions (`GameState`, `Player`, `Entity`, `CardSlot`); includes all subsystem headers
  - `src/core/config.h` -- Compile-time constants (screen size, asset paths, gameplay tuning, energy bar dimensions)

**Data (`src/data/`):**
- Purpose: Database access and card data loading
- Location: `src/data/`
- Contains: SQLite wrapper (`DB`), card loading/querying (`Deck`, `Card`), NFC UID mapping
- Depends on: `sqlite3` (system library), `lib/cJSON.h` (for card JSON parsing in consumers)
- Used by: `src/core/game.c` (init), `src/logic/card_effects.c` (card lookup), `tools/card_enroll.c`
- Key files:
  - `src/data/db.h` / `src/data/db.c` -- Generic SQLite query wrapper with parameterized queries and `DBResult` result sets
  - `src/data/cards.h` / `src/data/cards.c` -- Loads all cards into a `Deck` array; provides `cards_find()` by card_id and `cards_find_by_uid()` for NFC lookup

**Rendering (`src/rendering/`):**
- Purpose: All visual output -- tilemap generation/drawing, sprite animation, card rendering, viewport management, HUD
- Location: `src/rendering/`
- Contains: Biome system, tilemap renderer, sprite atlas/animation, card atlas, viewport split-screen, UI elements
- Depends on: Raylib (`lib/raylib.h`), `src/core/config.h`
- Used by: `src/core/game.c` (render path), `src/entities/entities.c` (entity_draw), `tools/`
- Key files:
  - `src/rendering/biome.h` / `src/rendering/biome.c` -- BiomeDef definitions (grass, undead, snow, swamp), tile block compilation, multi-layer overlay system
  - `src/rendering/tilemap_renderer.h` / `src/rendering/tilemap_renderer.c` -- TileMap creation, weighted random tile placement, base/detail/layer drawing
  - `src/rendering/sprite_renderer.h` / `src/rendering/sprite_renderer.c` -- SpriteAtlas (loads all character sprite sheets), AnimState per-entity, directional sprite drawing
  - `src/rendering/card_renderer.h` / `src/rendering/card_renderer.c` -- CardAtlas from sprite sheet, layered card compositing (11 visual layers), JSON-to-visual parsing
  - `src/rendering/viewport.h` / `src/rendering/viewport.c` -- Split-screen initialization (scissor + Camera2D), world/screen coordinate conversion, per-player tilemap draw
  - `src/rendering/ui.h` / `src/rendering/ui.c` -- Energy bar HUD drawing in screen space

**Entities (`src/entities/`):**
- Purpose: Entity lifecycle (create/destroy), per-frame update logic, state transitions
- Location: `src/entities/`
- Contains: Generic entity management, troop-specific creation from card data, building stubs, projectile stubs
- Depends on: `src/core/types.h`, `src/rendering/sprite_renderer.h`, `lib/cJSON.h`
- Used by: `src/systems/player.c` (entity management), `src/logic/card_effects.c` (spawning), `src/core/game.c` (drawing)
- Key files:
  - `src/entities/entities.h` / `src/entities/entities.c` -- `entity_create`, `entity_destroy`, `entity_update` (movement/despawn), `entity_draw`, `entity_set_state`
  - `src/entities/troop.h` / `src/entities/troop.c` -- `TroopData` struct parsed from card JSON via cJSON, `troop_spawn` creates a configured entity
  - `src/entities/building.h` / `src/entities/building.c` -- Stub: `building_create_base` returns NULL (unimplemented)
  - `src/entities/projectile.h` / `src/entities/projectile.c` -- Stub: completely empty (unimplemented)

**Systems (`src/systems/`):**
- Purpose: Player state management, energy system, spawn coordination, match lifecycle
- Location: `src/systems/`
- Contains: Player initialization/update/cleanup, energy regen/consume, card slot management, position helpers
- Depends on: `src/core/types.h`, `src/entities/`, `src/rendering/biome.h`
- Used by: `src/core/game.c` (player updates), `src/logic/card_effects.c` (spawn/energy), `src/rendering/viewport.c` (init)
- Key files:
  - `src/systems/player.h` / `src/systems/player.c` -- `player_init` (camera, tilemap, energy, card slots), `player_update` (energy + cooldowns), `player_update_entities` (update + sweep dead), entity add/remove/find, position helpers (lane, base, front, tile-to-world)
  - `src/systems/energy.h` / `src/systems/energy.c` -- `energy_init`, `energy_update` (time-based regen), `energy_can_afford`, `energy_consume`, `energy_restore`
  - `src/systems/spawn.h` / `src/systems/spawn.c` -- Stub: spawn logic currently lives in `src/logic/card_effects.c`
  - `src/systems/match.h` / `src/systems/match.c` -- Stub: pregame/match phase system declared but unimplemented

**Logic (`src/logic/`):**
- Purpose: Game rules -- card effect dispatch, combat resolution, pathfinding, win conditions
- Location: `src/logic/`
- Contains: Card action registry (function pointer table), troop/spell handlers, combat/pathfinding/win stubs
- Depends on: `src/entities/`, `src/systems/`, `src/data/cards.h`, `lib/cJSON.h`
- Used by: `src/core/game.c` (card_action_play on NFC/keyboard events)
- Key files:
  - `src/logic/card_effects.h` / `src/logic/card_effects.c` -- `CardPlayFn` typedef, `card_action_register`, `card_action_play` dispatcher, per-type handlers (knight, healer, assassin, brute, farmer, spell), `spawn_troop_from_card` helper
  - `src/logic/combat.h` / `src/logic/combat.c` -- Stub: `combat_resolve`, `combat_in_range`, `combat_find_target` declared but not implemented
  - `src/logic/pathfinding.h` / `src/logic/pathfinding.c` -- Stub: `pathfind_next_step` declared but not implemented
  - `src/logic/win_condition.h` / `src/logic/win_condition.c` -- Stub: `win_check`, `win_trigger` declared but not implemented

**Hardware (`src/hardware/`):**
- Purpose: NFC reader communication via serial port to Arduino microcontrollers
- Location: `src/hardware/`
- Contains: Serial port management, binary packet parser (state machine), UID debouncing, card removal detection
- Depends on: POSIX APIs (`termios.h`, `fcntl.h`, `unistd.h`)
- Used by: `src/core/game.c` (nfc_poll each frame), `tools/card_enroll.c`
- Key files:
  - `src/hardware/nfc_reader.h` / `src/hardware/nfc_reader.c` -- `NFCReader` struct (2 serial fds, debounce tables), `nfc_init`/`nfc_init_single`/`nfc_poll`/`nfc_shutdown`, rising-edge detection with removal timeout
  - `src/hardware/arduino_protocol.h` / `src/hardware/arduino_protocol.c` -- Binary wire protocol parser (5-state FSM: start byte 0xAA -> reader_idx -> uid_len -> uid bytes -> XOR checksum), `arduino_uid_to_string`

## Data Flow

**Card Play (NFC physical card -> game entity):**

1. Arduino reads NFC tag via I2C multiplexer, sends binary packet over USB serial
2. `arduino_read_packet()` in `src/hardware/arduino_protocol.c` parses the packet (state machine FSM)
3. `nfc_poll()` in `src/hardware/nfc_reader.c` debounces UIDs (only emits on rising edge), produces `NFCEvent`
4. `game_handle_nfc_events()` in `src/core/game.c` calls `cards_find_by_uid()` to look up the `Card` from the `Deck`
5. `card_action_play()` in `src/logic/card_effects.c` dispatches to the registered `CardPlayFn` by card type
6. Handler (e.g. `play_knight`) calls `spawn_troop_from_card()` which:
   - Checks `energy_consume()` for cost deduction
   - Parses card JSON via cJSON into `TroopData`
   - Calls `troop_spawn()` which calls `entity_create()` and configures stats/sprite
   - Calls `player_add_entity()` to insert entity into the player's entity array
7. Entity walks upward each frame in `entity_update()`, eventually crossing into opponent viewport

**Keyboard Test Input (debug fallback):**

1. `game_handle_test_input()` checks `IsKeyPressed(KEY_ONE)` / `IsKeyPressed(KEY_Q)`
2. Calls `game_test_play_knight()` which does `cards_find("KNIGHT_001")` -> `card_action_play()`
3. Same flow as NFC from step 5 onward

**Rendering Pipeline (per frame):**

1. `game_render()` calls `BeginDrawing()` + `ClearBackground()`
2. For each player (P1, P2):
   a. `viewport_begin()` -- `BeginScissorMode()` (clips to half-screen) + `BeginMode2D()` (rotated camera)
   b. `viewport_draw_tilemap()` -- draws base tiles, detail overlay, biome layers
   c. `game_draw_entities_for_viewport()` -- draws owner's entities normally, mirrors crossed-border entities into opponent viewport
   d. `DrawText()` -- player label
   e. `viewport_end()` -- `EndMode2D()` + `EndScissorMode()`
3. HUD layer (screen space, no camera): `ui_draw_energy_bar()` for each player
4. `EndDrawing()`

**State Management:**
- All game state is owned by a single `GameState` struct allocated on the stack in `main()`
- `GameState` owns: `DB`, `Deck`, `CardAtlas`, `Player[2]`, `BiomeDef[BIOME_COUNT]`, `SpriteAtlas`, `NFCReader`
- Each `Player` owns: `TileMap`, local `TileDef[]` copies, `CardSlot[3]`, `Entity*[MAX_ENTITIES]`, energy state, `Camera2D`
- Entities are heap-allocated (`malloc`), owned by their player's entity array, freed via swap-with-last sweep

## Key Abstractions

**GameState:**
- Purpose: Root container for all game data; passed by pointer to all subsystems
- Definition: `src/core/types.h` (lines 104-124)
- Pattern: God-object passed everywhere (no dependency injection)

**Entity:**
- Purpose: Represents any game object (troop, building, projectile) with shared fields
- Definition: `src/core/types.h` (lines 29-57)
- Pattern: Tagged union via `EntityType` enum; behavior dispatch in `entity_update()` switch statements

**BiomeDef:**
- Purpose: Complete definition of a tileset's visual mapping, weights, and overlay layers
- Definition: `src/rendering/biome.h` (lines 55-101)
- Pattern: Data-driven tile generation; `TileBlock` structs compiled into flat `TileDef[]` arrays; `BiomeLayer` supports RANDOM (density-based) and PAINT (explicit cell placement) modes

**CardHandler (function pointer registry):**
- Purpose: Maps card type strings to play functions for extensible card behavior
- Definition: `src/logic/card_effects.c` (lines 18-22)
- Pattern: Static array of `{type, CardPlayFn}` pairs; O(n) linear scan dispatch

**TileMap:**
- Purpose: Per-player generated grid of tile indices with optional detail and biome layer overlays
- Definition: `src/rendering/tilemap_renderer.h` (lines 52-62)
- Pattern: 1D flat arrays (`cells`, `detailCells`, `biomeLayerCells[]`) indexed by `row * cols + col`

**SpriteAtlas / AnimState:**
- Purpose: Shared sprite textures (atlas) separated from per-entity animation state
- Definition: `src/rendering/sprite_renderer.h`
- Pattern: Flyweight -- `CharacterSprite` (shared, heavy) referenced by pointer; `AnimState` (per-entity, lightweight) stored inline

## Entry Points

**Main game (`cardgame`):**
- Location: `src/core/game.c` line 217 (`int main(void)`)
- Triggers: Direct execution
- Responsibilities: Initialize all systems, run game loop, cleanup on exit

**Card preview tool (`card_preview`):**
- Location: `tools/card_preview.c` line 41 (`int main(...)`)
- Triggers: `make preview-run`
- Responsibilities: Interactive card visual editor using Raylib; no DB needed; reads optional JSON file

**Biome preview tool (`biome_preview`):**
- Location: `tools/biome_preview.c`
- Triggers: `make biome-preview-run`
- Responsibilities: Interactive biome tile block editor with live tilemap preview; no DB needed

**Card enroll tool (`card_enroll`):**
- Location: `tools/card_enroll.c` line 27 (`int main(void)`)
- Triggers: `make card-enroll-run` (requires NFC_PORT env var)
- Responsibilities: CLI tool to map physical NFC card UIDs to game card IDs in the database

## Error Handling

**Strategy:** Defensive null checks with early returns; errors printed to stderr/stdout; non-fatal failures degrade gracefully (NFC disabled, textures render white, etc.)

**Patterns:**
- Database operations return `NULL` on failure; callers check and log
- Entity/player functions guard against NULL pointers with `if (!ptr) return`
- NFC hardware failure is non-fatal: game continues with keyboard-only input
- Texture load failures are silently accepted by Raylib (1x1 white fallback); no runtime error detection
- Memory allocation failures (malloc/calloc) return NULL and propagate up; no abort/crash recovery
- No structured error codes or error propagation mechanism -- each function handles errors locally

## Cross-Cutting Concerns

**Logging:** printf/fprintf to stdout/stderr with `[TAG]` prefixes (e.g. `[NFC]`, `[TROOP]`, `[PLAY]`, `[SPELL]`). No log levels, no file output, no structured logging.

**Validation:** Minimal -- null pointer checks at function entry. No schema validation on card JSON beyond field presence. No bounds checking on tilemap cell indices.

**Authentication:** Not applicable (local 2-player game). NFC UID matching serves as card identity.

**Configuration:** Compile-time `#define` constants in `src/core/config.h`. Runtime configuration via environment variables: `DB_PATH`, `NFC_PORT`, `NFC_PORT_P1`, `NFC_PORT_P2`.

**Memory Management:** Manual malloc/free with explicit cleanup functions (`game_cleanup`, `player_cleanup`, `cards_free`, `tilemap_free`, etc.). No garbage collection. Ownership is hierarchical: GameState -> Player -> Entity.

---

*Architecture analysis: 2026-03-28*
