# NFC Tabletop Card Game

A two-player split-screen strategy game where physical NFC cards control the virtual board. Players place real cards on NFC readers connected via Arduino's to spawn troops, cast spells, and try to destroy their opponents base.

Built in C with Raylib, SQLite, and Arduino hardware.

## Features

- **Physical NFC card input** — each player has 3 NFC reader slots wired to an Arduino, mapped to in-game lanes
- **Split-screen two-player** — rotated viewports so players sit across from each other
- **Canonical shared battlefield** — both players view the same `1080 x 1920` world with a seam at `y=960`
- **Tile-based biome rendering** — procedural tilemaps with multiple biome themes (plains, cursed lands, undead, etc.)
- **Entity system** — troops, buildings, and projectiles with combat, pathfinding, and win conditions
- **Energy system** — card plays cost energy that regenerates over time
- **Card data from SQLite** — card stats, sprites, and metadata stored in a local `cardgame.db` file and loaded at runtime
- **Standalone dev tools** — card preview, biome preview, and card enrollment utilities

## Requirements

- GCC
- [Raylib](https://www.raylib.com/)
- SQLite3 (`brew install sqlite`)
- Arduino(s) with NFC readers (for hardware input)

## Quick Start

```bash
# Initialize the database (first time only)
make init-db

# Build and run the game
make run
```

## Build Targets

| Command                  | Description                                       |
|--------------------------|---------------------------------------------------|
| `make init-db`           | Create and seed `cardgame.db` (first time setup)  |
| `make cardgame`          | Build the main game binary                        |
| `make run`               | Clean, build, and run the game                    |
| `make preview`           | Build the card preview tool                       |
| `make preview-run`       | Build and run card preview                        |
| `make biome_preview`     | Build the biome preview tool                      |
| `make biome-preview-run` | Build and run biome preview                       |
| `make card_enroll`       | Build the card enrollment tool                    |
| `make card-enroll-run`   | Build and run card enrollment                     |
| `make clean`             | Remove all built binaries                         |

## Tools

### Card Preview (`tools/card_preview.c`)
Standalone Raylib app for editing and previewing card visuals. No database required.

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
lib/            Third-party libs (cJSON, Raylib headers)
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
| `cardgame.db`         | Runtime database (git-ignored)   |

To reset the database: `rm cardgame.db && make init-db`

To browse the database, use [DB Browser for SQLite](https://sqlitebrowser.org/) (`brew install --cask db-browser-for-sqlite`).

## Environment Variables

| Variable    | Description                              | Default         |
|-------------|------------------------------------------|-----------------|
| `DB_PATH`   | Path to the SQLite database file         | `cardgame.db`   |
| `NFC_PORT`  | Single-Arduino serial port (test mode)   | —               |
| `NFC_PORT_P1` | Player 1 Arduino serial port           | —               |
| `NFC_PORT_P2` | Player 2 Arduino serial port           | —               |

On macOS, serial ports look like `/dev/cu.usbserial-XXXXXXXX`. Run `ls /dev/cu.*` to find yours.

## AI Disclosure

This project uses AI tools as part of its development process:

Claude has been very useful in assisting with project planning, creation of documentation, and in the development of the tools throughout the project. This includes help with the structuring of the codebase and creating the preview and card enrollment tools.

All AI generated code has been reviewed and integrated by myself. The core game code and hardware integration are not written using AI.
