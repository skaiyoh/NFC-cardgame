# UVULITES - NFC Tabletop Card Game

A two-player split-screen strategy game controlled by physical NFC cards. Each player places cards on NFC reader slots wired to an Arduino to spawn troops, spend energy, and attack the opposing base.

Built in C with Raylib and SQLite. The primary target is Linux.

## Features

- Physical NFC card input through Arduino-connected readers
- Two-player split-screen presentation for across-the-table play
- Shared battlefield with animated units, bases, combat, and match results
- Energy-based card play and local SQLite-backed card data

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

If you prefer the Makefile:

```bash
make cardgame
./cardgame
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

## Notes

- The game still runs without NFC hardware if the port variables are unset, but live card input will be disabled.
- Run the binary from the repository root so relative asset paths resolve correctly.
