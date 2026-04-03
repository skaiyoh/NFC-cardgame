# Integrations

## SQLite Runtime Integration

- Startup opens a SQLite file through `db_init()`.
- Default database path is `cardgame.db`.
- Override with `DB_PATH`.
- `cards_load()` reads the full deck into memory.
- `cards_load_nfc_map()` reads `nfc_tags` into memory.
- `card_enroll` writes `uid -> card_id` mappings back into `nfc_tags`.

## NFC / Arduino Integration

### Environment Selection

- `NFC_PORT`
  - single-Arduino test mode
  - all reader events map to Player 0
- `NFC_PORT_P1` + `NFC_PORT_P2`
  - dual-player mode

If neither form is set, the game prints an NFC-disabled warning and continues.

### Packet Format

`arduino_protocol.c` expects:

```text
0xAA | reader_index | uid_len | uid bytes... | xor_checksum
```

Rules enforced by the parser:

- `reader_index` must be `0`, `1`, or `2`
- `uid_len` must be `1..7`
- checksum is XOR of every preceding packet byte

### NFC Polling Behavior

- `nfc_poll()` is non-blocking.
- Events are emitted on rising edge only.
- The same UID on the same reader does not re-fire until removal is inferred.
- Removal is inferred after `30` consecutive frames without packets for that reader.

## Asset Integration

- The game loads art from relative paths under `src/assets/`.
- The renderer assumes the process is launched from the project root so those relative paths resolve.
- `sprite_frame_atlas.h` is generated code consumed by `sprite_renderer.c`.
- `make sprite-frame-atlas` regenerates that header through `tools/generate_sprite_frame_atlas.py`.

## Tool Integrations

### `card_preview`

- depends on Raylib and cJSON
- optionally reads a JSON file path from `argv[1]`
- can print the current visual JSON to stdout

### `biome_preview`

- depends on Raylib
- imports built-in biome definitions
- can save/load preview sessions and export biome definition code

### `card_enroll`

- depends on SQLite plus one serial NFC port
- scans a UID
- prompts for a card choice
- writes the mapping with `INSERT ... ON CONFLICT (uid) DO UPDATE`

## Logging Integration

- Logging is plain stdout/stderr.
- Typical prefixes include:
  - `[NFC]`
  - `[TEST]`
  - `[PLAY]`
  - `[SPELL]`
  - `[TROOP]`
  - `[COMBAT]`
