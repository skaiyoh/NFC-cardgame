# NFC Tabletop Card Game

A two-player split-screen strategy game where physical NFC cards control the virtual board. Players place real cards on NFC readers connected via Arduinos to deploy troops and try to destroy their opponent's base.

Built in C with Raylib, SQLite, and Arduino hardware.

Primary development target: Linux. The setup and run instructions below assume a Linux workstation or Raspberry Pi OS.

## Features

- **Physical NFC card input** — each player has 3 NFC reader slots wired to an Arduino, mapped to in-game lanes
- **Split-screen two-player** — rotated viewports so players sit across from each other
- **Canonical shared battlefield** — both players view the same `1080 x 1920` world with a seam at `y=960`
- **Tile-based biome rendering** — procedural tilemaps with multiple biome themes (plains, cursed lands, undead, etc.)
- **Entity system** — troops and stationary bases with combat, pathfinding, death animations, and base-destruction win handling
- **Match result overlay** — lethal base hits freeze gameplay and render `VICTORY`, `DEFEAT`, or `DRAW` for both players
- **Energy + sustenance economy** — cards can cost regenerating energy or spendable sustenance gathered during the match
- **Card data from SQLite** — card stats, sprites, and metadata stored in a local `cardgame.db` file and loaded at runtime
- **Standalone dev tools** — card preview, biome preview, and card enrollment utilities

## Documentation

- [Linux Setup](md/LINUX_SETUP.md) — package install, Raylib setup, serial permissions, and Raspberry Pi notes
- [Card Data Guide](md/CARD_DATA_GUIDE.md) — authoring card entries in the SQLite database

## Requirements

- Linux
- A C compiler (`gcc` or `clang`)
- CMake 3.20+
- `pkg-config`
- [Raylib](https://www.raylib.com/)
- SQLite3
- Arduino(s) with NFC readers (for hardware input)

## Linux Setup

This repo expects Raylib to be installed system-wide. `cJSON` is already vendored in
`third_party/cjson`, but `raylib.h` and `libraylib` must come from your machine.

For a fuller walkthrough, see [Linux Setup](md/LINUX_SETUP.md).

On Debian / Ubuntu / Raspberry Pi OS, install the compiler toolchain, SQLite, and the libraries Raylib needs to build:

```bash
sudo apt update
sudo apt install build-essential git cmake pkg-config libsqlite3-dev sqlite3 \
  libasound2-dev libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev \
  libglu1-mesa-dev libxcursor-dev libxinerama-dev libwayland-dev libxkbcommon-dev
```

Then install Raylib. If your distro already packages it, use that package. Otherwise build and
install Raylib from source:

```bash
git clone https://github.com/raysan5/raylib.git /tmp/raylib
cmake -S /tmp/raylib -B /tmp/raylib/build
cmake --build /tmp/raylib/build -j"$(nproc)"
sudo cmake --install /tmp/raylib/build
sudo ldconfig

# Verify raylib is visible to the toolchain
pkg-config --modversion raylib
```

Official Raylib GNU/Linux guide:
https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux

## Quick Start

```bash
# Configure the build
cmake -S . -B build

# Build everything
cmake --build build -j"$(nproc)"

# Run tests
ctest --test-dir build --output-on-failure

# Initialize the database if cardgame.db is missing, or delete it first for a true reset
cmake --build build --target init-db

# Run from the project root so relative asset paths resolve correctly
./build/cardgame
```

If you prefer the `Makefile`, the equivalent local entrypoints are `make cardgame`,
`make preview`, `make biome_preview`, `make card_enroll`, and `make test`.

## Raspberry Pi Log Watching

If you need to launch the game from a terminal on the Pi but watch logs over SSH,
start the game with line-buffered output redirected to a log file:

```bash
stdbuf -oL -eL ./build/cardgame > game.log 2>&1
```

Then, from an SSH session, follow the log:

```bash
tail -f ~/NFC-cardgame/game.log
```

## Build Targets

| Command                                   | Description                                       |
|-------------------------------------------|---------------------------------------------------|
| `cmake -S . -B build`                     | Configure the project                             |
| `cmake --build build -j"$(nproc)"`        | Build all targets                                 |
| `cmake --build build --target cardgame`   | Build the main game binary                        |
| `cmake --build build --target init-db`    | Initialize and seed `cardgame.db` without clearing old rows |
| `cmake --build build --target card_preview` | Build the card preview tool                     |
| `cmake --build build --target biome_preview` | Build the biome preview tool                   |
| `cmake --build build --target card_enroll`  | Build the card enrollment tool                  |
| `ctest --test-dir build --output-on-failure` | Run the test suite                             |
| `rm -rf build`                            | Remove the CMake build directory                  |

## Tools

### Card Preview (`tools/card_preview.c`)
Standalone Raylib app for editing and previewing card visuals. No database required.
Press `E` to print the current visual JSON, `Ctrl+E` to export a layered `.psd`,
or run `./card_preview template.json --export-only --export-psd out.psd` for a
non-interactive export.

### Biome Preview (`tools/biome_preview.c`)
Interactive tool for defining biome tile blocks and previewing tilemap rendering.

### Card Enroll (`tools/card_enroll.c`)
Utility for mapping physical NFC card UIDs to game cards in the database. Requires a connected Arduino and `cardgame.db`.

## Project Structure

```
src/
  core/         Game loop, config, type definitions
  data/         Database access and card data loading
  rendering/    Card renderer, tilemap, sprites, biomes, UI, viewport
  entities/     Troops, buildings, projectiles
  systems/      Player state, energy, spawning, match logic
  logic/        Card effects, combat, pathfinding, win conditions
  hardware/     NFC reader and Arduino serial protocol
  assets/       Pixel art sprites and tilesets
tools/          Standalone dev/utility programs
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
- The top territory biome art is rotated `180°` in world space so terrain reads correctly from the opposing side without reintroducing seam clipping.

## Card Flow

```
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

The game uses a local SQLite file (`cardgame.db`) — no server required.

| File                  | Purpose                          |
|-----------------------|----------------------------------|
| `sqlite/schema.sql`   | Table definitions                |
| `sqlite/seed.sql`     | Base card data                   |
| `cardgame.db`         | Runtime database shipped in repo |

The checked-in runtime database and `sqlite/seed.sql` now both use uppercase
card IDs. The `init-db` target still does not clear existing rows first,
though, so a true reset should remove `cardgame.db` before re-seeding if you
want to drop old NFC mappings as well.

To reset the database cleanly:

```bash
rm -f cardgame.db
cmake --build build --target init-db
```

To browse the database, use [DB Browser for SQLite](https://sqlitebrowser.org/) and install `sqlitebrowser` from your distro repository.

## Environment Variables

| Variable    | Description                              | Default         |
|-------------|------------------------------------------|-----------------|
| `DB_PATH`   | Path to the SQLite database file         | `cardgame.db`   |
| `NFC_PORT`  | Single-Arduino serial port (test mode)   | —               |
| `NFC_PORT_P1` | Player 1 Arduino serial port           | —               |
| `NFC_PORT_P2` | Player 2 Arduino serial port           | —               |

On Linux, serial ports usually look like `/dev/ttyACM0` or `/dev/ttyUSB0`. Run `ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null` to find yours. If permissions block access, see [Linux Setup](md/LINUX_SETUP.md).

## AI Disclosure

This project uses AI tools as part of its development process:

Claude has been very useful in assisting with project planning, creation of documentation, and in the development of the tools throughout the project. This includes help with the structuring of the codebase and creating the preview and card enrollment tools.

All AI generated code has been reviewed and integrated by myself. The core game code and hardware integration are not written using AI.
