# Structure

## Top-Level Layout

```text
NFC-cardgame/
├── docs/               replacement documentation for the removed .planning tree
├── md/                 older hand-written docs kept outside docs/
├── sqlite/             schema and seed SQL
├── src/
│   ├── core/           game loop, config, canonical battlefield, shared types
│   ├── data/           SQLite wrapper and card loading
│   ├── entities/       entity lifecycle, troop parsing, building/projectile stubs
│   ├── hardware/       Arduino serial protocol and NFC polling
│   ├── logic/          card effects, combat, pathfinding, win-condition stub
│   ├── rendering/      tilemaps, biomes, sprites, cards, UI, debug overlays
│   ├── systems/        player state, energy, spawn stub, match stub
│   └── assets/         character, card, and environment art
├── tests/              standalone assert-based test executables
├── third_party/        vendored cJSON
├── tools/              card preview, biome preview, card enroll
├── CMakeLists.txt      CMake build and CTest registration
├── Makefile            actively usable local build/test entrypoint
├── cardgame.db         checked-in runtime SQLite database
└── README.md           project readme
```

## Source Areas

| Area | Purpose | Key Files |
|------|---------|-----------|
| `src/core/` | Root orchestration and canonical battlefield model | `game.c`, `types.h`, `battlefield.c`, `battlefield_math.c` |
| `src/data/` | SQLite access and in-memory deck loading | `db.c`, `cards.c` |
| `src/entities/` | Entity allocation, state changes, troop parsing | `entities.c`, `troop.c` |
| `src/hardware/` | Serial NFC ingest from Arduino boards | `nfc_reader.c`, `arduino_protocol.c` |
| `src/logic/` | Game rules | `card_effects.c`, `combat.c`, `pathfinding.c` |
| `src/rendering/` | Cameras, tilemaps, sprites, UI, debug overlay | `viewport.c`, `tilemap_renderer.c`, `sprite_renderer.c`, `ui.c` |
| `src/systems/` | Player and energy state | `player.c`, `energy.c` |
| `tools/` | Standalone utilities | `card_preview.c`, `biome_preview.c`, `card_enroll.c` |
| `tests/` | Headless executable tests | `test_pathfinding.c`, `test_combat.c`, `test_battlefield_math.c`, `test_battlefield.c`, `test_animation.c`, `test_debug_events.c` |

## Build Surfaces

- `Makefile`
  - builds `cardgame`, the three tools, and all six test executables
- `CMakeLists.txt`
  - defines the same main binary and tools
  - registers the six tests with CTest

## Runtime Assets And Data

- Assets are loaded from relative paths under `src/assets/`.
- The default SQLite runtime file is `cardgame.db` in the repo root.
- Because asset paths are relative, the game should be launched from the project root.
