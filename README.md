# UVULITES - NFC Tabletop Card Game

A two-player split-screen strategy game controlled by physical NFC cards. Each player places cards on NFC reader slots wired to an Arduino to spawn troops, spend energy, and attack the opposing base.

Built in C with Raylib and SQLite. The primary target is Linux.

## Features

- Physical NFC card input through Arduino-connected readers
- Two-player split-screen presentation for across-the-table play
- Shared battlefield with animated units, bases, combat, and match results
- Energy and sustenance-based card play backed by local SQLite data

## Requirements

- Linux
- A C compiler such as `gcc` or `clang`
- CMake 3.20+
- `make`
- `pkg-config`
- [Raylib](https://www.raylib.com/)
- SQLite3 development headers and the `sqlite3` CLI
- Arduino(s) with NFC readers if you want live hardware input

## Setup

This project expects Raylib to be installed system-wide. `cJSON` is already vendored in `third_party/cjson`.

For Linux package installation, Raylib setup, serial permissions, and Raspberry Pi notes, see [md/LINUX_SETUP.md](md/LINUX_SETUP.md).

## Quick Start

Build and run from the repository root so relative asset paths resolve correctly:

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"

# Initialize the database if needed
cmake --build build --target init-db

# Run the game
./build/cardgame
```

If you are using connected NFC hardware and want CMake to launch the game with the ports baked into the build directory:

```bash
cmake -S . -B build \
  -DNFC_PORT_P1=/dev/ttyACM0 \
  -DNFC_PORT_P2=/dev/ttyACM1
cmake --build build --target run-cardgame
```

## Makefile Workflow

The repo also ships a simple `Makefile`:

```bash
make cardgame
./cardgame
```

To use the `make run` shortcut, you must provide both player ports:

```bash
NFC_PORT_P1=/dev/ttyACM0 NFC_PORT_P2=/dev/ttyACM1 make run
```

`make run` will fail if either `NFC_PORT_P1` or `NFC_PORT_P2` is missing.

## Common Commands

| Command | Purpose |
| --- | --- |
| `cmake -S . -B build` | Configure the project |
| `cmake --build build -j"$(nproc)"` | Build the project |
| `cmake --build build --target init-db` | Create or reseed `cardgame.db` using `sqlite/schema.sql` and `sqlite/seed.sql` |
| `cmake --build build --target run-cardgame` | Run the game with `DB_PATH` and `NFC_PORT*` from the CMake cache |
| `./build/cardgame` | Run the compiled game directly |
| `make cardgame` | Build the game with the Makefile |
| `NFC_PORT_P1=... NFC_PORT_P2=... make run` | Build and run through the Makefile using dual-Arduino mode |
| `make clean` | Remove local build outputs created by the Makefile |

## Database

The game uses a local SQLite file, `cardgame.db`.

- `sqlite/schema.sql`: database schema
- `sqlite/seed.sql`: seed card data
- `cardgame.db`: runtime database used by the game

`init-db` reads the schema and seed scripts into the configured database path. If you want a true reset, remove `cardgame.db` first:

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

Notes:

- The standalone binary can run without NFC hardware; live card input will simply be disabled.
- The CMake configure step warns when a provided `NFC_PORT`, `NFC_PORT_P1`, or `NFC_PORT_P2` path does not exist on the current machine.
- The current `Makefile` run shortcut is dual-Arduino only and uses `NFC_PORT_P1` plus `NFC_PORT_P2`.
