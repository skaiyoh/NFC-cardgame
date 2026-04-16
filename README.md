# UVULITES - NFC Tabletop Card Game

A two-player split-screen strategy game where physical NFC cards control the virtual board. Players place real cards on NFC readers connected via Arduinos to deploy troops and try to destroy their opponent's base.

Built in C with Raylib and SQLite. The primary target is Linux.

## Features

- Physical NFC card input with Arduino-connected readers mapped to in-game lanes
- Split-screen two-player presentation with rotated viewports for opposite-seat play
- Canonical shared battlefield sized `1080 x 1920` with a visible center seam
- Tile-based biome rendering with multiple battlefield themes
- Troops, bases, combat, pathfinding, death animation, and win-condition handling
- Energy and sustenance economy tied to SQLite-backed card data

## Documentation

- [Linux Setup](md/LINUX_SETUP.md) - package install, Raylib setup, serial permissions, and Raspberry Pi notes
- [Card Data Guide](md/CARD_DATA_GUIDE.md) - authoring card entries in the SQLite database

## Requirements

- Linux
- A C compiler such as `gcc` or `clang`
- CMake 3.20+
- `pkg-config`
- [Raylib](https://www.raylib.com/)
- SQLite3
- Arduino(s) with NFC readers if you want live hardware input

## Setup

This project expects Raylib to be installed system-wide. `cJSON` is already vendored in `third_party/cjson`.

For Linux package installation, Raylib setup, serial permissions, and Raspberry Pi notes, see [md/LINUX_SETUP.md](md/LINUX_SETUP.md).

## Quick Start

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"

# Initialize the database if needed
cmake --build build --target init-db

# Run from the project root so asset paths resolve correctly
./build/cardgame
```

To run through CMake with explicit NFC ports, pass them at configure time:

```bash
cmake -S . -B build \
  -DNFC_PORT_P1=/dev/ttyACM0 \
  -DNFC_PORT_P2=/dev/ttyACM1
cmake --build build --target run-cardgame
```

If you prefer the Makefile, the equivalent local entrypoints are `make cardgame` and `make run`:

```bash
NFC_PORT_P1=/dev/ttyACM0 NFC_PORT_P2=/dev/ttyACM1 make run
```

## Raspberry Pi Log Watching

If you need to launch the game from a terminal on the Pi but watch logs over SSH, start the game with line-buffered output redirected to a log file:

```bash
stdbuf -oL -eL ./build/cardgame > game.log 2>&1
```

Then, from an SSH session, follow the log:

```bash
tail -f ~/NFC-cardgame/game.log
```

## Build Targets

| Command | Description |
|---------|-------------|
| `cmake -S . -B build` | Configure the project |
| `cmake --build build -j"$(nproc)"` | Build all targets |
| `cmake --build build --target cardgame` | Build the main game binary |
| `cmake --build build --target init-db` | Initialize and seed `cardgame.db` without clearing old rows |
| `cmake --build build --target run-cardgame` | Run the game with `DB_PATH` and `NFC_PORT*` from the CMake cache |
| `rm -rf build` | Remove the CMake build directory |

## Project Structure

```text
src/
  core/         Game loop, config, type definitions
  data/         Database access and card data loading
  rendering/    Card renderer, tilemap, sprites, biomes, UI, viewport
  entities/     Troops, buildings, projectiles
  systems/      Player state, energy, spawning, match logic
  logic/        Card effects, combat, pathfinding, win conditions
  hardware/     NFC reader and Arduino serial protocol
  assets/       Pixel art sprites and tilesets
third_party/    Vendored dependencies (currently cJSON)
sqlite/         Database schema and seed data
```

## Rendering Model

- The match runs in one canonical battlefield owned by `Battlefield`, not separate per-player world spaces.
- The board is `1080 x 1920` world units with two territories:
  - top: `{0, 0, 1080, 960}`
  - bottom: `{0, 960, 1080, 960}`
- Player 1 and Player 2 each render that same world through their own rotated viewport.
- Player 2 is rendered through a `RenderTexture2D` and composited into the right half of the screen to preserve seam visibility and opposite-seat perspective.
- The top territory biome art is rotated `180 degrees` in world space so terrain reads correctly from the opposing side without reintroducing seam clipping.

## Card Flow

```text
NFC read  ->  cards_find_by_uid(uid)
          ->  card_action_play(card, g, playerIndex, slotIndex)
                ->  play_knight(card, state)
                      ->  spawn_troop_from_card(card, state)
                            ->  troop_create_data_from_card(card)   <- reads JSON data
                            ->  bf_spawn_pos(battlefield, side, slot)
                            ->  troop_spawn(player, data, canonical_pos, atlas)
                            ->  bf_add_entity(battlefield, e)
```

## Database

The game uses a local SQLite file, `cardgame.db`.

- `sqlite/schema.sql`: database schema
- `sqlite/seed.sql`: seed card data
- `cardgame.db`: runtime database used by the game

To recreate the database from scratch:

```bash
rm -f cardgame.db
cmake --build build --target init-db
```

For card authoring details, see [md/CARD_DATA_GUIDE.md](md/CARD_DATA_GUIDE.md).

## Runtime Configuration

Environment variables:

- `DB_PATH`: path to the SQLite database file. Default: `cardgame.db`
- `NFC_PORT`: single-Arduino serial port for test mode
- `NFC_PORT_P1`: Player 1 Arduino serial port
- `NFC_PORT_P2`: Player 2 Arduino serial port

Typical Linux serial devices look like `/dev/ttyACM0` or `/dev/ttyUSB0`.

If you use the CMake run targets, the configure step will warn when a specified `NFC_PORT`, `NFC_PORT_P1`, or `NFC_PORT_P2` path does not exist on the current machine.

## Notes

- The game still runs without NFC hardware if the port variables are unset, but live card input will be disabled.
- Run the binary from the repository root so relative asset paths resolve correctly.
