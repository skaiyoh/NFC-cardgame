# Testing Patterns

**Analysis Date:** 2026-03-28

## Test Framework

**Runner:**
- None. No test framework is present in the project.
- No test files exist (no `*_test.c`, `test_*.c`, `*_spec.c`, or dedicated test directory).
- No test targets in the `Makefile`.
- No CI/CD pipeline configured.

**Current "testing":**
- Manual keyboard input in `game_test_play_knight()` at `src/core/game.c:73-80`
- Key 1 spawns a knight for Player 1, Key Q for Player 2
- This is a manual smoke test, not automated testing

## Test Coverage Status

**Coverage: 0%**

No automated tests exist. No coverage tooling is configured.

## Existing Test-Adjacent Code

**`src/core/game.c:73-80` -- Manual play test:**
```c
static void game_test_play_knight(GameState *g, int playerIndex) {
    Card *card = cards_find(&g->deck, "KNIGHT_001");
    if (!card) {
        printf("[TEST] KNIGHT_001 not found in deck\n");
        return;
    }
    card_action_play(card, g, playerIndex, 0);
}
```
- Exercises: `cards_find` -> `card_action_play` -> `spawn_troop_from_card` -> `troop_spawn`
- Only tests the happy path of one card type
- Requires a running Raylib window and database

**`src/hardware/arduino-test.ino` -- Arduino firmware test:**
- An Arduino sketch for testing NFC hardware, not a C unit test

## Testability Analysis

**Highly testable modules (no Raylib dependency):**
| Module | Files | What to test |
|--------|-------|-------------|
| Database layer | `src/data/db.c`, `src/data/db.h` | Query execution, parameterized queries, NULL handling, error paths |
| Card data | `src/data/cards.c`, `src/data/cards.h` | Load from DB, find by ID, find by UID, free without leaks |
| Energy system | `src/systems/energy.c`, `src/systems/energy.h` | Init, update regen, consume, can_afford, restore, cap at max |
| Arduino protocol | `src/hardware/arduino_protocol.c`, `src/hardware/arduino_protocol.h` | Packet parsing, checksum validation, UID-to-string conversion |
| Card effects registry | `src/logic/card_effects.c`, `src/logic/card_effects.h` | Register handlers, dispatch by type, unknown type handling |
| Troop data | `src/entities/troop.c` (troop_create_data_from_card) | JSON parsing, default values, field overrides |

**Partially testable (need Raylib mocks or stubs):**
| Module | Files | Dependency |
|--------|-------|-----------|
| Entity lifecycle | `src/entities/entities.c` | `sprite_draw()` in `entity_draw()` needs a stub |
| Player management | `src/systems/player.c` | `tilemap_create_biome()` calls `LoadTexture()` |
| Biome definitions | `src/rendering/biome.c` | `LoadTexture()`, `SetTextureFilter()` |
| Tilemap generation | `src/rendering/tilemap_renderer.c` | Cell generation is pure; draw functions need Raylib |

**Difficult to unit test (deeply coupled to Raylib):**
| Module | Files | Reason |
|--------|-------|--------|
| Sprite renderer | `src/rendering/sprite_renderer.c` | All functions call Raylib draw/texture APIs |
| Card renderer | `src/rendering/card_renderer.c` | Texture atlas, DrawTexturePro calls |
| Viewport | `src/rendering/viewport.c` | Scissor mode, Camera2D |
| UI | `src/rendering/ui.c` | DrawRectangle, DrawText |

## Recommended Test Framework

**Primary: [Unity Test](https://github.com/ThrowTheSwitch/Unity)**
- Single-file C test framework (one `.c` + one `.h`)
- No external dependencies
- Fits the project's "vendored libs in `lib/`" pattern
- Assertions: `TEST_ASSERT_EQUAL`, `TEST_ASSERT_NULL`, `TEST_ASSERT_TRUE`, etc.
- Runner: generates `main()` via a Ruby script or write manually

**Alternative: [greatest](https://github.com/silentbicycle/greatest)**
- Single header file, zero dependencies
- Even simpler than Unity; good for bootstrapping
- Less widely adopted but perfectly adequate for this project size

**Mocking: [CMock](https://github.com/ThrowTheSwitch/CMock) (optional)**
- Auto-generates mock functions from header files
- Useful for stubbing Raylib calls in rendering tests
- Pairs with Unity

## Recommended Test Structure

**Directory layout:**
```
tests/
  unit/
    test_db.c            # Database query/result tests
    test_cards.c         # Card loading, lookup, NFC map tests
    test_energy.c        # Energy init, regen, consume, restore
    test_arduino.c       # Packet parsing, checksum, UID conversion
    test_card_effects.c  # Handler registration and dispatch
    test_troop.c         # TroopData from JSON, default values
    test_entities.c      # Entity create, destroy, state transitions
    test_player.c        # Add/remove entity, slot management
  integration/
    test_game_flow.c     # Full card-play pipeline with real DB
  fixtures/
    test.db              # Pre-seeded SQLite DB for integration tests
```

**Makefile targets to add:**
```makefile
TEST_SOURCES = tests/unit/test_db.c tests/unit/test_cards.c ...
TEST_LDFLAGS = -lsqlite3

test: $(TEST_SOURCES)
	$(CC) $(CFLAGS) -DTESTING $(TEST_SOURCES) $(SRC_DATA) lib/cJSON.c lib/unity.c -o run_tests $(TEST_LDFLAGS)
	./run_tests

test-coverage:
	$(CC) $(CFLAGS) --coverage $(TEST_SOURCES) $(SRC_DATA) lib/cJSON.c lib/unity.c -o run_tests $(TEST_LDFLAGS)
	./run_tests
	gcov src/data/*.c src/systems/*.c src/logic/*.c src/entities/*.c
```

## Test Patterns to Follow

**Suite organization:**
```c
#include "unity.h"
#include "../../src/data/db.h"

static DB db;

void setUp(void) {
    db_init(&db, ":memory:");
    db_query(&db, "CREATE TABLE cards (card_id TEXT PRIMARY KEY, name TEXT, cost INTEGER, type TEXT, rules_text TEXT, data TEXT)");
}

void tearDown(void) {
    db_close(&db);
}

void test_db_init_returns_true_on_success(void) {
    DB test_db;
    TEST_ASSERT_TRUE(db_init(&test_db, ":memory:"));
    TEST_ASSERT_TRUE(test_db.connected);
    db_close(&test_db);
}

void test_db_init_returns_false_on_null_path(void) {
    DB test_db;
    TEST_ASSERT_FALSE(db_init(&test_db, NULL));
}

void test_db_query_returns_null_when_not_connected(void) {
    DB disconnected = {0};
    TEST_ASSERT_NULL(db_query(&disconnected, "SELECT 1"));
}
```

**Energy system tests (pure logic, no dependencies):**
```c
#include "unity.h"
#include "../../src/core/types.h"
#include "../../src/systems/energy.h"

static Player player;

void setUp(void) {
    memset(&player, 0, sizeof(Player));
    energy_init(&player, 10.0f, 1.0f);
}

void tearDown(void) {}

void test_energy_init_sets_max(void) {
    TEST_ASSERT_EQUAL_FLOAT(10.0f, player.maxEnergy);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, player.energy);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, player.energyRegenRate);
}

void test_energy_consume_deducts(void) {
    TEST_ASSERT_TRUE(energy_consume(&player, 3));
    TEST_ASSERT_EQUAL_FLOAT(7.0f, player.energy);
}

void test_energy_consume_rejects_insufficient(void) {
    TEST_ASSERT_FALSE(energy_consume(&player, 11));
    TEST_ASSERT_EQUAL_FLOAT(10.0f, player.energy);
}

void test_energy_update_regens(void) {
    energy_consume(&player, 5);
    energy_update(&player, 2.0f);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, player.energy);
}

void test_energy_update_caps_at_max(void) {
    energy_consume(&player, 1);
    energy_update(&player, 100.0f);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, player.energy);
}
```

**Arduino protocol tests (byte-level parsing):**
```c
#include "unity.h"
#include "../../src/hardware/arduino_protocol.h"

void test_uid_to_string_4byte(void) {
    uint8_t uid[] = {0x04, 0xA1, 0xB2, 0xC3};
    char out[32];
    arduino_uid_to_string(uid, 4, out);
    TEST_ASSERT_EQUAL_STRING("04A1B2C3", out);
}

void test_uid_to_string_7byte(void) {
    uint8_t uid[] = {0x04, 0xA1, 0xB2, 0xC3, 0xDE, 0xFF, 0x01};
    char out[32];
    arduino_uid_to_string(uid, 7, out);
    TEST_ASSERT_EQUAL_STRING("04A1B2C3DEFF01", out);
}
```

## Mocking Strategy

**For Raylib-dependent code:**
- Create a `tests/mocks/raylib_stubs.c` with no-op implementations of used Raylib functions
- Define `TESTING` preprocessor flag to swap real Raylib calls for stubs
- Or use CMock to auto-generate mocks from `lib/raylib.h` (subset of used functions)

**For database tests:**
- Use SQLite `:memory:` databases for fast, isolated tests
- Create schema + seed data in `setUp()`, destroy in `tearDown()`
- No external services required

**For NFC/serial tests:**
- Test `arduino_read_packet()` by writing bytes to a pipe fd
- Create a pipe with `pipe()`, write test packets to the write end, read from the read end
- No physical hardware required

## Test Coverage Gaps (Priority Order)

**Critical -- no tests, high-impact modules:**
1. `src/data/db.c` -- All database operations untested; SQL injection risk if patterns change
2. `src/data/cards.c` -- Card loading, lookup, NFC mapping untested
3. `src/systems/energy.c` -- Pure logic, trivial to test, zero coverage
4. `src/hardware/arduino_protocol.c` -- Binary protocol parser untested; checksum bugs would be silent

**High -- complex logic, no tests:**
5. `src/logic/card_effects.c` -- Handler registry and dispatch, energy deduction
6. `src/entities/troop.c` -- JSON-to-TroopData parsing with many optional fields
7. `src/entities/entities.c` -- Entity lifecycle, state transitions

**Medium -- partially implemented, no tests:**
8. `src/systems/player.c` -- Entity add/remove, slot management, position helpers
9. `src/rendering/tilemap_renderer.c` -- Cell generation logic (pure math, testable)

**Low -- stub/unimplemented modules (test when implemented):**
10. `src/logic/combat.c` -- Empty stub
11. `src/logic/pathfinding.c` -- Empty stub
12. `src/logic/win_condition.c` -- Empty stub
13. `src/entities/building.c` -- Returns NULL always
14. `src/entities/projectile.c` -- Empty stub
15. `src/systems/match.c` -- Empty stub
16. `src/systems/spawn.c` -- Empty stub

## Getting Started Checklist

1. Download Unity framework into `lib/unity.c` and `lib/unity.h`
2. Create `tests/unit/` directory
3. Add `test` target to `Makefile`
4. Write `test_energy.c` first (pure logic, zero dependencies, fast win)
5. Write `test_arduino.c` second (standalone byte parsing, pipe-based fd testing)
6. Write `test_db.c` third (uses `:memory:` SQLite, exercises the data layer)
7. Write `test_cards.c` fourth (builds on db tests, tests card lookup)
8. Set up `gcov` coverage reporting with `--coverage` flag
9. Target 80%+ coverage on `src/data/`, `src/systems/energy.c`, `src/hardware/arduino_protocol.c`

---

*Testing analysis: 2026-03-28*
