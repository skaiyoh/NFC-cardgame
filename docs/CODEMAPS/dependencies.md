# Dependencies

## Required System Libraries

| Dependency | Used By | Notes |
|-----------|---------|-------|
| `raylib` | `cardgame`, `card_preview`, `biome_preview`, rendering modules | required for the windowed game and both preview tools |
| `sqlite3` | `cardgame`, `card_enroll`, data layer | required both as a library and, for init scripts, as a CLI |
| `libm` | main game, tests, rendering, logic | used for `sqrtf`, `sinf`, `fabsf`, and related math |

## Vendored Code

| Dependency | Location | Notes |
|-----------|----------|-------|
| `cJSON` | `third_party/cjson/` | compiled directly into targets that parse card JSON |

## Build Tooling Declared By The Repo

| Tool | Status In Repo | Notes |
|------|----------------|-------|
| `gcc` or `clang` | supported | `Makefile` uses `gcc`; CMake is compiler-agnostic |
| `make` | active | local verification in this environment used the `Makefile` |
| `cmake >= 3.20` | supported | required if you use `CMakeLists.txt` and `ctest` |
| `python3` | optional | only needed for `make sprite-frame-atlas` |
| `sqlite3` CLI | optional but practical | used by `init-db` targets/scripts |

## Hardware Dependencies

Optional for gameplay debugging, required for real NFC play:

- 1 Arduino in single-port test mode, or 2 Arduinos in dual-player mode
- 3 readers per Arduino
- serial transport at `115200` baud
- NFC tags with UIDs enrolled in `nfc_tags`

## POSIX APIs Used

The hardware layer depends on standard POSIX serial/file APIs:

- `open`
- `read`
- `close`
- `termios`
- `fcntl`
- `unistd`

## Environment Variables

| Variable | Used By | Meaning |
|----------|---------|---------|
| `DB_PATH` | game and `card_enroll` | alternate SQLite file path |
| `NFC_PORT` | game and `card_enroll` | single-Arduino test mode |
| `NFC_PORT_P1` | game | Player 1 serial port |
| `NFC_PORT_P2` | game | Player 2 serial port |

## Verification Note

The repo does not pin a `raylib` version in source. Local verification on 2026-04-03 used system `raylib` `5.5.0`.
