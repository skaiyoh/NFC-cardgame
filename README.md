# NFC Tabletop Card Game

A two-player split-screen strategy game where physical NFC cards control the virtual board. Players place real cards on NFC readers connected via Arduino's to spawn troops, cast spells, and try to destroy their opponents base.

Built in C with Raylib, PostgreSQL, and Arduino hardware.

## Features

- **Physical NFC card input** — each player has 3 NFC reader slots wired to an Arduino, mapped to in-game lanes
- **Split-screen two-player** — rotated viewports so players sit across from each other
- **Tile-based biome rendering** — procedural tilemaps with multiple biome themes (plains, cursed lands, undead, etc.)
- **Entity system** — troops, buildings, and projectiles with combat, pathfinding, and win conditions
- **Energy system** — card plays cost energy that regenerates over time
- **Card data from PostgreSQL** — card stats, sprites, and metadata stored in a database and loaded at runtime
- **Standalone dev tools** — card preview, biome preview, and card enrollment utilities

## Requirements

- GCC
- [Raylib](https://www.raylib.com/)
- PostgreSQL client library (`libpq`)
- Docker & Docker Compose (for the database)
- Arduino(s) with NFC readers (for hardware input)

## Quick Start

```bash
# Start the database
docker compose up -d

# Build and run the game
make run
```

## Build Targets

| Command                | Description                                  |
|------------------------|----------------------------------------------|
| `make cardgame`        | Build the main game binary                   |
| `make run`             | Clean, build, and run with default DB config |
| `make preview`         | Build the card preview tool                  |
| `make preview-run`     | Build and run card preview                   |
| `make biome_preview`   | Build the biome preview tool                 |
| `make biome-preview-run` | Build and run biome preview                |
| `make card_enroll`     | Build the card enrollment tool               |
| `make card-enroll-run` | Build and run card enrollment                |
| `make clean`           | Remove all built binaries                    |

## Tools

### Card Preview (`tools/card_preview.c`)
Standalone Raylib app for editing and previewing card visuals. No database required.

### Biome Preview (`tools/biome_preview.c`)
Interactive tool for defining biome tile blocks and previewing tilemap rendering.

### Card Enroll (`tools/card_enroll.c`)
Utility for writing card data to NFC tags and registering them in the database. Requires a connected Arduino and the PostgreSQL database.

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
postgres/       Database setup scripts
```

## Card Flow

```
NFC read  ->  cards_find(card_id)  ->  g->currentPlayerIndex = playerIndex
          ->  card_action_play(card, g)
                ->  play_knight(card, state)
                      ->  spawn_troop_from_card(card, state)
                            ->  troop_create_data_from_card(card)   <- reads JSON data
                            ->  troop_spawn(player, data, pos, atlas)
                            ->  player_add_entity(player, e)
```

## Docker Services

| Service  | Port  | Description          |
|----------|-------|----------------------|
| Postgres | 5432  | Game database        |
| pgAdmin  | 5050  | Database admin UI    |

Default credentials are in `docker-compose.yml`.

## Environment Variables

| Variable         | Description                        | Example                                                          |
|------------------|------------------------------------|------------------------------------------------------------------|
| `DB_CONNECTION`  | PostgreSQL connection string       | `host=localhost port=5432 dbname=appdb user=postgres password=postgres` |
| `NFC_PORT`       | Single-Arduino serial port         | `/dev/ttyUSB0`                                                   |
| `NFC_PORT_P1`    | Player 1 Arduino serial port       | `/dev/ttyACM0`                                                   |
| `NFC_PORT_P2`    | Player 2 Arduino serial port       | `/dev/ttyACM1`                                                   |

## AI Disclosure

This project uses AI tools as part of its development process:

Claude has been very useful in assisting with project planning, creation of documentation, and in the development of the tools throughout the project. This includes help with the structuring of the codebase and creating the preview and card enrollment tools.

All AI generated code has been reviewed and integrated by myself. The core game code and hardware integration are not written using AI.
