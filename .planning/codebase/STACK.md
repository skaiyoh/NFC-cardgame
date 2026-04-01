# Technology Stack

**Analysis Date:** 2026-03-28

## Languages

**Primary:**
- C (C99/C11) - All application code, ~6,700 lines of project source (excluding vendored cJSON)

**Secondary:**
- SQL - Database schema and seed data (`sqlite/schema.sql`, `sqlite/seed.sql`)

## Runtime

**Environment:**
- Native compiled binary (no VM or interpreter)
- Targets Linux and macOS (POSIX serial I/O via `termios.h`, `fcntl.h`, `unistd.h`)
- Windows is NOT supported (raw POSIX serial port calls, no Win32 serial abstraction)

**Resolution:**
- Fixed 1920x1080 window, configured in `src/core/config.h`
- 60 FPS target via `SetTargetFPS(60)` in `src/core/game.c`

## Build System

**Primary:** GNU Make (`Makefile` at project root)
- Compiler: `gcc`
- Flags: `-Wall -Wextra -O2`
- macOS cross-compile support via `MACFLAGS = -I/opt/homebrew/include -L/opt/homebrew/lib`
- No CMakeLists.txt (the `.idea/` directory suggests CLion is used as the IDE, but builds use Make)

**Build Targets:**

| Target               | Output Binary   | Links Against              |
|----------------------|-----------------|----------------------------|
| `make cardgame`      | `cardgame`      | `-lsqlite3 -lraylib -lm`  |
| `make preview`       | `card_preview`  | `-lraylib -lm`             |
| `make biome_preview` | `biome_preview` | `-lraylib -lm`             |
| `make card_enroll`   | `card_enroll`   | `-lsqlite3 -lm`            |
| `make init-db`       | (no binary)     | Runs `sqlite3` CLI tool    |

**Source file groups defined in `Makefile`:**
- `SRC_CORE` - `src/core/game.c`
- `SRC_DATA` - `src/data/db.c`, `src/data/cards.c`
- `SRC_RENDERING` - `src/rendering/*.c` (6 files)
- `SRC_ENTITIES` - `src/entities/*.c` (4 files)
- `SRC_SYSTEMS` - `src/systems/*.c` (4 files)
- `SRC_LOGIC` - `src/logic/*.c` (4 files)
- `SRC_HARDWARE` - `src/hardware/*.c` (2 files)
- `SRC_LIB` - `lib/cJSON.c`

## Frameworks

**Core:**
- Raylib (system-installed) - 2D rendering, window management, input, texture loading, Camera2D
  - Headers vendored at `lib/raylib.h`, `lib/raymath.h`, `lib/rlgl.h`
  - Linked as `-lraylib` (system shared library)
  - Used for: window lifecycle, sprite/texture rendering, split-screen viewports, scissor clipping, keyboard input, Camera2D transforms

**Database:**
- SQLite3 (system-installed) - Card data storage, NFC UID mapping
  - Linked as `-lsqlite3` (system shared library)
  - Header included via `<sqlite3.h>` (system header, not vendored)

**Testing:**
- Not detected - No test framework, no test files, no test targets in Makefile

**Build/Dev:**
- CLion IDE (`.idea/` directory present)
- No CI/CD pipeline detected

## Key Dependencies

**Critical (linked libraries):**
- `raylib` - All rendering, windowing, input handling, texture management
- `sqlite3` - All persistent data access (cards, NFC tag mappings)
- `libm` (math) - Standard math functions

**Vendored (compiled from source):**
- cJSON v1.7.x (`lib/cJSON.c`, `lib/cJSON.h`) - JSON parsing for card data fields and card visual configuration. MIT license.

**Vendored (headers only, not compiled):**
- `lib/libpq-fe.h`, `lib/postgres_ext.h` - Legacy PostgreSQL headers from a previous database migration. NOT used in any source file. The project migrated from PostgreSQL to SQLite (commit `f366cec`).
- `lib/raylib.h`, `lib/raymath.h`, `lib/rlgl.h` - Raylib header copies for IDE autocompletion. The actual library is linked from the system.

## Configuration

**Compile-time constants (`src/core/config.h`):**
- `SCREEN_WIDTH` = 1920, `SCREEN_HEIGHT` = 1080
- Asset paths: `CARD_SHEET_PATH`, `GRASS_TILESET_PATH`, `UNDEAD_TILESET_PATH`, etc.
- Character sprite paths: `CHAR_BASE_PATH`, `CHAR_KNIGHT_PATH`, etc.
- Gameplay tuning: `DEFAULT_TILE_SCALE`, `DEFAULT_TILE_SIZE`, `DEFAULT_CARD_SCALE`
- HUD layout: `ENERGY_BAR_WIDTH`, `ENERGY_BAR_HEIGHT`, `ENERGY_BAR_MARGIN`

**Runtime environment variables:**
- `DB_PATH` - SQLite database file path (default: `cardgame.db`)
- `NFC_PORT` - Single-Arduino serial port for test mode
- `NFC_PORT_P1` - Player 1 Arduino serial port
- `NFC_PORT_P2` - Player 2 Arduino serial port

**No `.env` files exist** - environment variables are passed on the command line (see `make run` target).

**Database initialization:**
- `sqlite3 cardgame.db < sqlite/schema.sql` then `sqlite3 cardgame.db < sqlite/seed.sql`
- Automated via `make init-db`

## Platform Requirements

**Development:**
- GCC (any recent version supporting C99)
- Raylib installed system-wide (`brew install raylib` on macOS, package manager on Linux)
- SQLite3 development libraries (`brew install sqlite` on macOS, `libsqlite3-dev` on Debian/Ubuntu)
- SQLite3 CLI tool (for `make init-db`)
- Make (GNU Make)

**Runtime:**
- Linux or macOS (POSIX required for serial I/O)
- Raylib shared library on library path
- SQLite3 shared library on library path
- `cardgame.db` file in working directory (or `DB_PATH` env var set)
- Arduino(s) with NFC readers connected via USB serial (optional - game works with keyboard input)

**Hardware (optional):**
- 2x Arduino boards, each with a TCA9548A I2C multiplexer and 3x PN532 NFC readers
- USB serial connection at 115200 baud
- Physical NFC cards/tags with unique UIDs

## Standalone Tools

Three utility programs share source files with the main game but build independently:

| Tool             | Source                      | Dependencies           | Purpose                        |
|------------------|-----------------------------|------------------------|--------------------------------|
| `card_preview`   | `tools/card_preview.c`      | Raylib, cJSON          | Interactive card visual editor |
| `biome_preview`  | `tools/biome_preview.c`     | Raylib                 | Interactive biome tile editor  |
| `card_enroll`    | `tools/card_enroll.c`       | SQLite3, NFC hardware  | Map NFC UIDs to card IDs      |

---

*Stack analysis: 2026-03-28*
