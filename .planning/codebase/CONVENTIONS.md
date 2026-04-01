# Coding Conventions

**Analysis Date:** 2026-03-28

## Naming Patterns

**Files:**
- Use `snake_case.c` / `snake_case.h` for all source files
- Each module is a matched `.h`/`.c` pair with the same name (e.g., `card_effects.h` / `card_effects.c`)
- Headers use `NFC_CARDGAME_<UPPER_SNAKE>_H` include guards (e.g., `NFC_CARDGAME_TYPES_H`)
- Example: `src/rendering/sprite_renderer.h`, `src/systems/energy.h`

**Functions:**
- Public API: `module_verb_noun()` using `snake_case` with a module prefix
  - `db_init()`, `db_close()`, `db_query()`
  - `cards_load()`, `cards_find()`, `cards_free()`
  - `entity_create()`, `entity_destroy()`, `entity_update()`
  - `biome_init_all()`, `biome_compile_blocks()`, `biome_free_all()`
  - `player_init()`, `player_update()`, `player_cleanup()`
  - `nfc_init()`, `nfc_poll()`, `nfc_shutdown()`
- Static/private: `snake_case` without module prefix, or prefixed with `s_` for static globals
  - `static int s_nextEntityID` in `src/entities/entities.c`
  - `static void collect_results()` in `src/data/db.c`
  - `static void biome_define_grass()` in `src/rendering/biome.c`
  - `static int open_serial_port()` in `src/hardware/nfc_reader.c`
- Callback typedefs: `PascalCase` + `Fn` suffix: `CardPlayFn` in `src/logic/card_effects.h`

**Variables:**
- Local variables: `camelCase` (e.g., `deltaTime`, `spawnPos`, `blockIdx`, `totalWeight`)
- Struct fields: `camelCase` (e.g., `moveSpeed`, `attackRange`, `cooldownTimer`, `entityCount`)
- Pointer parameters: typically named after the struct type, lowercase (e.g., `DB *db`, `Player *p`, `Entity *e`)
- Loop counters: single letters `i`, `j`, `c`, `r`, `li` (layer index)

**Types:**
- Structs: `PascalCase` (e.g., `GameState`, `Player`, `Entity`, `CardSlot`, `TileMap`, `BiomeDef`)
- Enums: `PascalCase` type name, `UPPER_SNAKE_CASE` values with category prefix
  - `EntityType`: `ENTITY_TROOP`, `ENTITY_BUILDING`, `ENTITY_PROJECTILE`
  - `Faction`: `FACTION_PLAYER1`, `FACTION_PLAYER2`
  - `BiomeType`: `BIOME_GRASS`, `BIOME_UNDEAD`, `BIOME_SNOW`
  - `AnimationType`: `ANIM_IDLE`, `ANIM_WALK`, `ANIM_DEATH`
  - Enum count sentinel: `TYPE_COUNT` pattern (e.g., `BIOME_COUNT`, `ANIM_COUNT`, `CLR_COUNT`)
- Typedefs: always used for structs — `typedef struct { ... } Name;` or forward-declared `typedef struct Name Name;`

**Constants/Macros:**
- `UPPER_SNAKE_CASE` for all `#define` constants
- Examples: `MAX_ENTITIES`, `NUM_CARD_SLOTS`, `TILE_COUNT`, `SCREEN_WIDTH`
- Path constants: `UPPER_SNAKE_CASE` with `_PATH` suffix (e.g., `GRASS_TILESET_PATH`, `CHAR_KNIGHT_PATH`)
- Dimension constants: descriptive name (e.g., `CARD_WIDTH`, `ENERGY_BAR_HEIGHT`, `SPRITE_FRAME_SIZE`)

## Code Style

**Formatting:**
- No `.clang-format` or `.clang-tidy` configuration present
- Indentation: 4 spaces (consistent across all files)
- Opening brace: same line as control statement (K&R style)
- Single-line `if` without braces when the body is one statement:
  ```c
  if (!db || !path) return false;
  if (e) free(e);
  ```
- Multi-line conditions: aligned or wrapped naturally
- Blank line between function definitions
- No trailing whitespace observed

**Linting:**
- Compiler flags: `-Wall -Wextra -O2` (see `Makefile` line 4)
- No dedicated linter tool configured
- Use `-Wall -Wextra` as the baseline; treat warnings as errors in new code

## Import Organization

**Order (observed consistently):**
1. Own header (e.g., `#include "game.h"` at top of `game.c`)
2. Project headers from other modules (e.g., `#include "../logic/card_effects.h"`)
3. Library headers (e.g., `#include "../../lib/cJSON.h"`, `#include "../../lib/raylib.h"`)
4. Standard library headers (e.g., `<stdlib.h>`, `<stdio.h>`, `<string.h>`)

**Path Style:**
- All includes use relative paths from the current file
- Library headers: `../../lib/raylib.h`
- Sibling module headers: `../module/file.h`
- Same-directory headers: `"file.h"`
- No include path aliases or `-I` flags beyond system defaults and Homebrew paths

## Error Handling

**Patterns:**

**1. Bool return for init/load functions:**
```c
bool db_init(DB *db, const char *path);     // src/data/db.h
bool cards_load(Deck *deck, DB *db);        // src/data/cards.h
bool nfc_init(NFCReader *r, const char *port0, const char *port1);  // src/hardware/nfc_reader.h
```
- Return `true` on success, `false` on failure
- Caller checks return value and handles cascading cleanup

**2. NULL return for allocation/lookup failures:**
```c
Entity *entity_create(EntityType type, Faction faction, Vector2 pos);  // returns NULL on malloc fail
Card *cards_find(Deck *deck, const char *card_id);                     // returns NULL if not found
DBResult *db_query(DB *db, const char *sql);                           // returns NULL on error
```

**3. NULL-guard at function entry:**
```c
if (!db || !path) return false;   // src/data/db.c:10
if (!deck || !db) return false;   // src/data/cards.c:11
if (!e || !e->alive) return;      // src/entities/entities.c:67
```
- Every public function validates pointer parameters before use
- Use this pattern in all new code

**4. Error reporting:**
- `fprintf(stderr, ...)` for errors that indicate bugs or misconfiguration
- `printf("[TAG] ...")` with bracketed tags for informational/debug output
  - Tags: `[NFC]`, `[TROOP]`, `[PLAY]`, `[SPELL]`, `[TEST]`
- `perror(...)` for POSIX system call failures (see `src/hardware/nfc_reader.c:18`)
- `DB.last_error[256]` buffer for database error context (see `src/data/db.h:15`)

**5. Void suppression for unused parameters:**
```c
(void)owner;      // src/entities/building.c:12
(void)position;   // src/entities/building.c:13
(void)slotIndex;  // src/logic/card_effects.c:91
(void)oldState;   // src/entities/entities.c:61
```
- Always cast unused parameters to `(void)` rather than omitting names

## Memory Management

**Allocation patterns:**

**1. malloc + memset(0) for struct initialization:**
```c
Entity *e = malloc(sizeof(Entity));
if (!e) return NULL;
memset(e, 0, sizeof(Entity));       // src/entities/entities.c:16-18
```

**2. calloc for arrays:**
```c
deck->cards = calloc(rows, sizeof(Card));        // src/data/cards.c:29
deck->uid_map = calloc(rows, sizeof(UIDMapping));  // src/data/cards.c:82
```

**3. strdup for string ownership:**
```c
c->card_id = strdup(db_result_value(res, i, 0));  // src/data/cards.c:40
```
- Strings from the database are always strdup'd into the owning struct
- Every strdup'd string has a corresponding `free()` in the cleanup function

**4. Paired init/cleanup lifecycle:**
- Every `*_init()` has a matching `*_cleanup()`, `*_free()`, or `*_close()`
- Cleanup functions are NULL-safe and idempotent:
  ```c
  void db_close(DB *db) { if (!db || !db->connected) return; ... }
  void entity_destroy(Entity *e) { if (e) free(e); }
  void tilemap_free(TileMap *map) { free(map->cells); map->cells = NULL; ... }
  ```
- Cleanup order in `game_cleanup()` (`src/core/game.c:201-215`):
  1. Hardware shutdown (NFC)
  2. Player cleanup (frees tilemaps and entities)
  3. Atlas/texture cleanup
  4. Window close
  5. Data free (cards, deck, database)

**5. Swap-with-last for array removal:**
```c
// src/systems/player.c:100-109
p->entities[i] = p->entities[p->entityCount - 1];
p->entityCount--;
entity_destroy(dead);
```
- O(1) removal from unordered arrays; iterate backward for safe removal

**6. Stack allocation with `= {0}` zero-init:**
```c
GameState game = {0};         // src/core/game.c:218
TroopData data = {0};         // src/entities/troop.c:12
NFCEvent events[6];           // src/core/game.c:83 (stack array, not zero-init'd)
```
- Zero-initialize structs on the stack before filling fields

## Header File Conventions

**Include guard format:**
```c
#ifndef NFC_CARDGAME_<UPPER_SNAKE_CASE>_H
#define NFC_CARDGAME_<UPPER_SNAKE_CASE>_H
// ...
#endif //NFC_CARDGAME_<UPPER_SNAKE_CASE>_H
```
- Comment after `#endif` echoes the guard name

**Forward declarations to break circular includes:**
```c
typedef struct Entity Entity;      // src/core/types.h:19
typedef struct Player Player;      // src/core/types.h:20, also src/systems/energy.h:8
typedef struct GameState GameState; // src/core/types.h:21, also src/logic/card_effects.h:11
typedef struct BiomeDef BiomeDef;   // src/rendering/tilemap_renderer.h:65
```

**Header content rules:**
- Headers contain only: include guards, includes, type definitions, function declarations, macros
- No function bodies in headers
- No `static` declarations in headers
- All public functions declared in the header, all `static` functions kept in `.c` files only

**File header comment:**
```c
//
// Created by Nathan Davis on 2/16/26.
//
```
- Present in every `.h` and `.c` file as the first lines

## Macro Usage Patterns

**Convenience macros (locally scoped):**
```c
#define R(x, y, w, h)  (Rectangle){ (x), (y), (w), (h) }   // src/rendering/biome.c:10
#define EMPTY           (Rectangle){ 0, 0, 0, 0 }            // src/rendering/card_renderer.c:43
```
- `R()` macro is defined, used, then `#undef R` within biome layer blocks
- Keep macro scope minimal; `#undef` after use

**Constants as macros:**
- All compile-time constants use `#define` (no `static const` pattern observed)
- Grouped by category in header files (screen, paths, gameplay tuning, UI dimensions)
- Location: `src/core/config.h` for global constants, individual headers for module-specific constants

## Type Definitions and Struct Patterns

**Anonymous structs with typedef (primary pattern):**
```c
typedef struct {
    int rows;
    int cols;
    int *cells;
    float tileSize;
} TileMap;
```

**Named structs with forward declaration (when self-referential or cross-module):**
```c
// Forward declare
typedef struct GameState GameState;

// Define later
struct GameState {
    DB db;
    Deck deck;
    Player players[2];
    // ...
};
```

**Enum + struct pairing:**
- Enums define the type taxonomy, structs hold the data
- Sentinel `_COUNT` value in every enum enables array sizing:
  ```c
  typedef enum { BIOME_GRASS, BIOME_UNDEAD, BIOME_SNOW, BIOME_SWAMP, BIOME_COUNT } BiomeType;
  BiomeDef biomeDefs[BIOME_COUNT];
  ```

**Designated initializers (C99):**
```c
p->camera = (Camera2D){0};
b->blocks[0] = (TileBlock){ .srcX = 0, .srcY = 0, .cols = 4, .rows = 4, .tileW = 32, .tileH = 32 };
```
- Use designated initializers for clarity when setting struct literals

## TODO Comment Convention

**Format:** Multi-line `// TODO:` blocks explaining what, why, and how to fix:
```c
// TODO: cards_find() is an O(n) linear scan with strcmp. For a small deck (< 100 cards) this is
// TODO: fine, but consider a hash map keyed on card_id if the deck grows or lookups become frequent.
```
- Each continuation line starts with `// TODO:` for grep-ability
- 198 TODO comments across 19 source files
- TODOs explain: the current behavior, why it matters, and the recommended fix

## Function Pointer / Callback Pattern

**Registry pattern for extensible dispatch:**
```c
// src/logic/card_effects.h
typedef void (*CardPlayFn)(const Card *card, GameState *state, int playerIndex, int slotIndex);
void card_action_register(const char *type, CardPlayFn fn);
bool card_action_play(const Card *card, GameState *state, int playerIndex, int slotIndex);
```
- Static array of handlers with string-matched type dispatch
- Registration happens once in `card_action_init()` at startup
- Use this pattern for new extensible systems (e.g., building effects, spell types)

---

*Convention analysis: 2026-03-28*
