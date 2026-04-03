# External Integrations

**Analysis Date:** 2026-03-28

## APIs & External Services

No external web APIs, cloud services, or network services are used. The game is entirely local: local database, local hardware, local rendering.

## Data Storage

### SQLite Database

**Database file:** `cardgame.db` (local file, created by `make init-db`)

**Schema:** `sqlite/schema.sql`
- `cards` table - Card definitions (card_id TEXT PK, name, cost, type, rules_text, data JSON)
- `nfc_tags` table - NFC UID to card_id mappings (uid TEXT PK, card_id FK -> cards)
- Foreign keys enabled via `PRAGMA foreign_keys = ON`

**Seed data:** `sqlite/seed.sql`
- 6 cards: knight_01, assassin_01, brute_01, farmer_01, healer_01, fireball_01
- Each card has a JSON `data` field containing both `visual` (rendering config) and gameplay stats (hp, attack, moveSpeed, etc.)
- Uses `ON CONFLICT (card_id) DO UPDATE SET ...` for idempotent re-seeding

**Client:** Custom lightweight wrapper in `src/data/db.h` / `src/data/db.c`
- `DB` struct wraps `sqlite3*` handle with connection state and error buffer
- `DBResult` struct: immutable rows x cols table of nullable strings
- `db_query()` - Execute raw SQL, returns `DBResult*`
- `db_query_params()` - Parameterized queries with positional text bindings (`?1`, `?2`, ...)
- `db_result_value()` / `db_result_isnull()` - Safe cell access
- `db_result_free()` - Manual memory management (caller must free results)
- All queries use `sqlite3_prepare_v2` + `sqlite3_bind_text` (safe from SQL injection)

**Connection lifecycle:**
1. `db_init(&db, path)` opens the database file
2. Queries are issued during `game_init()` to load all cards and NFC mappings into memory
3. `db_close(&db)` in `game_cleanup()`
4. After initial load, the database is NOT queried during the game loop

**Card data loading:** `src/data/cards.h` / `src/data/cards.c`
- `cards_load()` - Bulk-loads all cards into a `Deck` struct (heap-allocated array)
- `cards_load_nfc_map()` - Loads `nfc_tags` table into `UIDMapping` array
- `cards_find()` - O(n) linear search by card_id
- `cards_find_by_uid()` - O(n) linear search by NFC UID, then O(n) by card_id

**File Storage:**
- All assets loaded from disk via relative paths defined in `src/core/config.h`
- PNG textures loaded by Raylib's `LoadTexture()` at startup
- No asset bundling or resource packaging

**Caching:**
- None (all data loaded into memory at startup and held for the process lifetime)

## NFC Hardware Integration

### Architecture

Two Arduino boards communicate with the host PC over USB serial. Each Arduino manages 3 NFC readers via a TCA9548A I2C multiplexer (one reader per card slot per player = 6 readers total).

**Implementation files:**
- `src/hardware/nfc_reader.h` / `src/hardware/nfc_reader.c` - High-level NFC polling and event system
- `src/hardware/arduino_protocol.h` / `src/hardware/arduino_protocol.c` - Binary wire protocol parser

### Serial Port Configuration

**Baud rate:** 115200
**Mode:** Raw, non-blocking (`O_RDWR | O_NOCTTY | O_NONBLOCK`, `cfmakeraw()`)
**POSIX API:** `open()`, `read()`, `close()`, `termios` structs (Linux/macOS only, no Windows support)

**Port paths configured via environment variables:**
- `NFC_PORT` - Single Arduino for test mode (all events routed to Player 0)
- `NFC_PORT_P1` - Player 1 Arduino (dual-Arduino mode)
- `NFC_PORT_P2` - Player 2 Arduino (dual-Arduino mode)

**Typical paths:**
- macOS: `/dev/cu.usbserial-XXXXXXXX`
- Linux: `/dev/ttyACM0`, `/dev/ttyACM1`

### Binary Wire Protocol

**Format:** `| 0xAA | reader_idx | uid_len | uid_byte_0 ... uid_byte_N | checksum |`

| Field         | Size    | Description                                     |
|---------------|---------|-------------------------------------------------|
| Start byte    | 1 byte  | Always `0xAA`                                   |
| Reader index  | 1 byte  | `0`, `1`, or `2` (TCA multiplexer channel)      |
| UID length    | 1 byte  | `4` or `7` (NFC tag UID length)                 |
| UID bytes     | 4-7     | Raw NFC tag UID bytes                            |
| Checksum      | 1 byte  | XOR of all preceding bytes (start through UID)  |

**Parser:** State machine in `src/hardware/arduino_protocol.c` with 5 states:
`PS_WAIT_START` -> `PS_READ_READER` -> `PS_READ_UID_LEN` -> `PS_READ_UID` -> `PS_READ_CHECKSUM`

- Maintains per-fd parser state via a static table (max 2 tracked file descriptors)
- Invalid reader index or UID length causes resync (drops to `PS_WAIT_START`)
- Bad checksum logs a warning and resyncs

### Event System

**NFCEvent struct** (`src/hardware/nfc_reader.h`):
```c
typedef struct {
    char uid[32];       // Uppercase hex UID string (e.g. "04A1B2C3")
    int  readerIndex;   // Which reader slot fired (0-2, maps to card slot / lane)
    int  playerIndex;   // Which player this reader belongs to (0 or 1)
} NFCEvent;
```

**Polling model:**
- `nfc_poll()` called once per frame in `game_handle_nfc_events()` (`src/core/game.c`)
- Non-blocking reads from both serial fds
- Debounce: only emits events on rising edge (new card placed, different UID than last seen)
- Card removal detection: after `NFC_REMOVAL_TIMEOUT_FRAMES` (30 frames, ~500ms) of silence on a reader channel, `lastUID` is cleared so the same card can be re-detected

**Card resolution flow:**
1. `nfc_poll()` produces `NFCEvent` with raw UID string
2. `cards_find_by_uid()` looks up UID in the `nfc_tags` table (loaded into memory)
3. `card_action_play()` dispatches to the card type handler
4. Handler spawns entity or applies effect

### Dual vs Single Arduino Modes

**Dual mode** (`NFC_PORT_P1` + `NFC_PORT_P2` set):
- `nfc_init()` opens both serial ports
- Events from `fds[0]` tagged as `playerIndex=0`, `fds[1]` as `playerIndex=1`

**Single test mode** (`NFC_PORT` set):
- `nfc_init_single()` opens one port
- All reader events routed to Player 0
- `fds[1]` stays at `-1` (skipped in poll loop)

**No NFC mode** (no env vars set):
- Game prints "[NFC] No NFC port env vars set -- NFC disabled"
- NFC fds stay at `-1`, `nfc_poll()` returns 0 events every frame
- Game is fully playable via keyboard (KEY_ONE = Player 1, KEY_Q = Player 2 spawn knight)

## JSON Data Parsing

**Library:** cJSON (vendored at `lib/cJSON.c`, `lib/cJSON.h`)

**Used for:**
- Parsing card `data` JSON field from SQLite into gameplay stats (`src/entities/troop.c` - `troop_create_data_from_card()`)
- Parsing card `data` JSON field into visual configuration (`src/rendering/card_renderer.c` - `card_visual_from_json()`)
- Parsing spell effect data (`src/logic/card_effects.c` - `play_spell()`)

**Pattern:**
```c
cJSON *root = cJSON_Parse(card->data);
if (!root) return defaults;
cJSON *field = cJSON_GetObjectItem(root, "fieldName");
if (field && cJSON_IsNumber(field)) value = field->valueint;
cJSON_Delete(root);
```

## Authentication & Identity

No authentication system. The game is a local two-player application. Player identity is determined by which Arduino/serial port the NFC event originates from.

## Monitoring & Observability

**Error Tracking:** None (no external service)

**Logs:**
- `printf()` / `fprintf(stderr, ...)` to stdout/stderr
- Prefixed with module tags: `[NFC]`, `[TROOP]`, `[PLAY]`, `[SPELL]`, `[arduino_protocol]`
- No log levels, no log files, no structured logging

## CI/CD & Deployment

**Hosting:** Local desktop application (not deployed to any server)
**CI Pipeline:** None detected (no `.github/workflows/`, no `.gitlab-ci.yml`, no `Jenkinsfile`)

## Webhooks & Callbacks

**Incoming:** None
**Outgoing:** None

## Network

No network communication whatsoever. The game is entirely local. The only external I/O is:
1. SQLite file reads at startup
2. USB serial communication with Arduino hardware
3. Raylib window system (display output, keyboard/mouse input)
4. PNG texture file reads at startup

---

*Integration audit: 2026-03-28*
