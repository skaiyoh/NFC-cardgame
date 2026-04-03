# Codebase Concerns

**Analysis Date:** 2026-03-28

---

## Critical Severity

### Dangling Pointer: TroopData.targetType (Use-After-Free)

- Issue: `troop_create_data_from_card()` assigns `data.targetType = tgtType->valuestring`, a pointer into the cJSON tree. `cJSON_Delete(root)` on line 80 frees the tree, making `targetType` a dangling pointer. Any subsequent read is undefined behavior.
- Files: `src/entities/troop.c` lines 72-80
- Impact: Memory corruption or crash when any code reads `data.targetType` after `troop_create_data_from_card()` returns. Currently not triggered because no targeting code is wired up, but will crash once combat targeting is implemented.
- Fix approach: Replace `data.targetType = tgtType->valuestring` with `data.targetType = strdup(tgtType->valuestring)` and free it in the caller/entity cleanup path.

### Potential Double-Free: player_remove_entity + Sweep Interaction

- Issue: `player_remove_entity()` calls `entity_destroy()` on the removed entity. If the same entity also has `markedForRemoval = true`, the sweep loop in `player_update_entities()` will call `entity_destroy()` again on the already-freed pointer.
- Files: `src/systems/player.c` lines 158-171, 100-110
- Impact: Double-free crash. Currently mitigated because `player_remove_entity()` is never called from any game-loop code, but becomes critical when it is used.
- Fix approach: In `player_remove_entity()`, set the entity pointer to NULL after destroy, and check for NULL in the sweep loop. Or, only set `markedForRemoval = true` in `player_remove_entity()` and let the sweep handle the actual destroy.

### Out-of-Bounds Array Access: Tilemap Cell Indexing

- Issue: `tilemap_draw()` indexes `tileDefs[map->cells[idx]]` without bounds checking. If a cell value is >= `TILE_COUNT` (32), this is an out-of-bounds array read, causing memory corruption.
- Files: `src/rendering/tilemap_renderer.c` lines 163-167
- Impact: Memory corruption, visual glitches, or crash. Can occur if `biome_compile_blocks()` produces indices outside [0, TILE_COUNT-1].
- Fix approach: Add `assert(map->cells[idx] < TILE_COUNT)` or clamp the value before indexing.

---

## High Severity

### Zero Test Coverage

- Issue: No test files exist anywhere in the project. No unit tests, no integration tests, no end-to-end tests. No test framework is configured.
- Files: Entire project
- Impact: Every code change is a regression risk. Bugs in entity lifecycle, card parsing, NFC protocol parsing, and cleanup paths are all undetectable until runtime.
- Fix approach: Add a test framework (e.g., Unity test framework for C, or CMocka). Start with unit tests for `troop_create_data_from_card()`, `db_query_params()`, `arduino_read_packet()`, and `entity_create()`/`entity_destroy()` lifecycle.

### Core Game Systems Completely Unimplemented

Multiple core systems exist only as empty stub files. The game currently has no win condition, no combat, no pathfinding, and no match flow:

| System | File | Status |
|--------|------|--------|
| Combat | `src/logic/combat.c` | Empty stubs (18 lines) |
| Win condition | `src/logic/win_condition.c` | Empty stubs (16 lines) |
| Pathfinding | `src/logic/pathfinding.c` | Empty stubs (15 lines) |
| Match/pregame | `src/systems/match.c` | Empty stubs (21 lines) |
| Projectiles | `src/entities/projectile.c` | Empty stubs (13 lines) |
| Building base | `src/entities/building.c` | Returns NULL (20 lines) |

- Impact: The game boots directly into a play loop where entities walk in a straight line and never interact. There is no end state, no combat, and no NFC pregame flow.
- Fix approach: Implement in dependency order: building_create_base -> combat -> win_condition -> match/pregame -> pathfinding -> projectiles.

### Player.base Is Always NULL

- Issue: `building_create_base()` always returns NULL. `player_init()` never calls it. `p->base` is always NULL, making the entire win condition system inoperative.
- Files: `src/entities/building.c` lines 7-15, `src/systems/player.c` line 65
- Impact: The game can never end. Blocks win_condition implementation.
- Fix approach: Implement `building_create_base()` using `entity_create(ENTITY_BUILDING, ...)` and call it from `player_init()`.

### Global PRNG Contamination via srand()

- Issue: `tilemap_create_biome()` calls `srand(seed)`, overwriting the global random state. Called once per player during init, so any `rand()` calls from other systems after this point use the last player's seed.
- Files: `src/rendering/tilemap_renderer.c` line 78, `src/systems/player.c` lines 50-53
- Impact: Non-deterministic behavior in any system that uses `rand()` after tilemap creation. Currently only tilemap generation uses `rand()`, but this becomes a problem as systems are added.
- Fix approach: Replace `srand()`/`rand()` with a per-player LCG or xorshift struct passed through tilemap generation.

### No Energy Cost Enforcement for Card Plays

- Issue: Although `spawn_troop_from_card()` in `card_effects.c` now calls `energy_consume()` (line 72), the `play_spell()` function has the consume call inside a conditional that also checks `state` but does not gate the spell effect on the result.
- Files: `src/logic/card_effects.c` lines 56-57, 88-92
- Impact: Spells are consumed but have no actual in-game effect. The TODO at line 88 documents that spell logic only prints to console.
- Fix approach: Implement actual spell damage/targeting logic.

---

## Medium Severity

### No Texture Load Validation

- Issue: `LoadTexture()` is called in at least 5 locations without checking the return value. Raylib returns a 1x1 white fallback texture on failure (id == 0 or 1), and the code proceeds silently.
- Files:
  - `src/rendering/sprite_renderer.c` lines 12-14 (load_sheet)
  - `src/rendering/biome.c` lines 431-434, 454-455 (biome_init_all)
  - `src/rendering/card_renderer.c` lines 216-219 (card_atlas_init)
- Impact: Missing asset files cause all sprites/tiles/cards to render as white rectangles with no error message. Extremely confusing to debug.
- Fix approach: After each `LoadTexture()`, check `texture.id == 0` and log the failing path with `fprintf(stderr, ...)`. Abort or early-out for critical textures.

### Dead Code: Unused Functions and Assets

Several functions and loaded assets are never used:

| Dead Code | File | Line |
|-----------|------|------|
| `player_draw_entities()` | `src/systems/player.c` | 115-119 |
| `player_lane_pos()` | `src/systems/player.c` | 237-245 |
| `tilemap_create()` (non-biome) | `src/rendering/tilemap_renderer.c` | 32-52 |
| `card_draw()` / `card_draw_ex()` | `src/rendering/card_renderer.c` | 269-314 |
| All `ANIM_RUN` sprite sheets | `src/rendering/sprite_renderer.c` | 26-28, 32, 41, 49, 59, 69, 79 |

- Impact: ANIM_RUN textures are loaded into VRAM for every character type (6 types x 1 texture each = 6 wasted texture loads). Dead code adds maintenance burden and confusion.
- Fix approach: Remove dead functions. Remove ANIM_RUN load_sheet calls unless run-state transitions are planned.

### All Card Types Behave Identically

- Issue: `play_healer`, `play_assassin`, `play_brute`, and `play_farmer` all delegate to `spawn_troop_from_card()` with no type-specific behavior. They are functionally identical to `play_knight`.
- Files: `src/logic/card_effects.c` lines 141-163
- Impact: Card types are cosmetically different (sprite) but gameplay-identical. No tactical depth.
- Fix approach: Implement unique behavior per type as documented in the TODO comments.

### Slot Cooldown Timer Never Set

- Issue: `player_update()` decrements `cooldownTimer` correctly, but no code ever sets it to a non-zero value when a card is played. Slots are always available.
- Files: `src/systems/player.c` lines 126-127
- Impact: Cards can be played on the same slot every frame with no cooldown, enabling rapid spam-spawning.
- Fix approach: Set `slot->cooldownTimer = CARD_COOLDOWN_SECONDS` in `spawn_troop_from_card()` after a successful spawn.

### Entity Lane Never Set

- Issue: Although `spawn_troop_from_card()` now sets `e->lane = slotIndex` (line 83), the `troop_spawn()` function signature does not include lane. The lane is set after spawn, which is correct, but `troop_spawn()` internally never uses it.
- Files: `src/entities/troop.c` lines 100-102, `src/logic/card_effects.c` line 83
- Impact: Lane-based targeting logic (when implemented) will work since the lane is set in card_effects.c, but the TODO comments in troop.c are stale.
- Fix approach: Remove stale TODO comments in `troop.c` lines 100-102 since the fix is in `card_effects.c`.

### Death Animation Never Plays

- Issue: `ESTATE_DEAD` immediately sets `markedForRemoval = true`. `entity_set_state()` does set `ANIM_DEATH`, but the entity is removed on the very next frame before the animation can play.
- Files: `src/entities/entities.c` lines 102-106
- Impact: Entities pop out of existence with no death animation, even though death animation sheets are loaded.
- Fix approach: Track the death animation timer. Only set `markedForRemoval = true` after the death animation frame count is exhausted.

### Hardcoded Screen Resolution

- Issue: `SCREEN_WIDTH` (1920) and `SCREEN_HEIGHT` (1080) are compile-time constants. The game cannot adapt to different displays. Additionally, `game.c` line 191 hardcodes `960` instead of using `SCREEN_WIDTH / 2`.
- Files: `src/core/config.h` lines 9-10, `src/core/game.c` line 191
- Impact: Game is locked to 1080p. Usability issue on smaller screens or projectors.
- Fix approach: Replace hardcoded literals with runtime window size queries. Use `gs->halfWidth` consistently.

### Hardcoded Seeds and Biome Assignments

- Issue: Tilemap seeds (42 and 99) and biome type (both BIOME_GRASS) are hardcoded in `viewport_init_split_screen()`.
- Files: `src/rendering/viewport.c` lines 45-51
- Impact: Every match looks identical. No biome variety.
- Fix approach: Randomize seeds from `time(NULL)` or a match ID. Allow biome selection in pregame.

### Silent Truncation in Biome Compilation

- Issue: `biome_compile_blocks()` and `biome_compile_detail_blocks()` silently drop tiles when they exceed `TILE_COUNT` or `MAX_DETAIL_DEFS` respectively. No warning is emitted.
- Files: `src/rendering/biome.c` lines 360-362, 389-390
- Impact: Biome authors lose tile definitions silently, leading to missing tiles that are hard to debug.
- Fix approach: Add `fprintf(stderr, ...)` when truncation occurs.

---

## Low Severity

### Animation Frame Counter Grows Unboundedly

- Issue: `state->frame` increments forever in `anim_state_update()` and is only wrapped via modulo in `sprite_draw()`. After ~1 year of continuous runtime at 60fps, `state->frame` overflows `int`.
- Files: `src/rendering/sprite_renderer.c` lines 158-161
- Impact: Theoretical overflow after ~2.1 billion frames (~1 year at 60fps). Extremely unlikely in practice.
- Fix approach: Wrap frame counter: `state->frame = (state->frame + 1) % MAX_FRAME_COUNT` or use modulo in `anim_state_update()`.

### Entity ID Counter Never Resets

- Issue: `s_nextEntityID` is a monotonically increasing static `int` that never resets between matches.
- Files: `src/entities/entities.c` lines 10-13
- Impact: Overflows after ~2 billion entity spawns. Practically unreachable in a single session.
- Fix approach: Reset to 1 between matches, or use `uint32_t` with explicit wrap.

### No State Transition Validation

- Issue: `entity_set_state()` allows any state transition, including `DEAD -> WALKING`. No guard logic exists.
- Files: `src/entities/entities.c` lines 59-61
- Impact: Currently harmless because nothing transitions dead entities, but becomes a bug vector when combat is added.
- Fix approach: Add a guard that rejects transitions from `ESTATE_DEAD`.

### Card Handler Registry Stores Raw String Pointers

- Issue: `handlers[i].type` stores `const char *` pointers passed during registration. Safe only if called with string literals (which it currently is). Using heap or stack strings would create dangling pointers.
- Files: `src/logic/card_effects.c` lines 19-20, 28-38
- Impact: Not currently a problem, but fragile. A future refactor that constructs type strings dynamically will break.
- Fix approach: Use `strdup()` in `card_action_register()` and free on cleanup.

### Arduino Parser Static Table Limits

- Issue: `arduino_protocol.c` uses a static `ParserCtx parsers[MAX_TRACKED_FDS]` with `MAX_TRACKED_FDS = 2`. If more than 2 file descriptors are used, `get_parser()` returns NULL and packets are silently dropped.
- Files: `src/hardware/arduino_protocol.c` lines 19, 31-44
- Impact: Cannot support more than 2 Arduinos. Consistent with the 2-player design, but the limit is hidden.
- Fix approach: Document the limit or make it configurable. Add a warning if `get_parser()` returns NULL.

### No Duplicate Handler Check in Card Registry

- Issue: Registering the same card type twice silently adds a second entry. Only the first match fires, making the second dead.
- Files: `src/logic/card_effects.c` lines 33-34
- Impact: Wasted registry slot. Currently not triggered since `card_action_init()` registers each type once.
- Fix approach: Check for duplicate type strings before inserting.

### Viewport Play Area Overlap

- Issue: P1 playArea (x=0, w=1080) and P2 playArea (x=960, w=1080) overlap by 120px in world X space.
- Files: `src/rendering/viewport.c` lines 13-41
- Impact: Entities in the 120px overlap zone may appear in both viewports before scissoring. Visually confusing.
- Fix approach: Document if intentional, or adjust geometry so play areas are non-overlapping.

---

## Build System Concerns

### Dual Build Systems (Makefile + CMakeLists.txt)

- Issue: Both a `Makefile` and likely CLion project references exist, but no `CMakeLists.txt` file is present in the root. The `.idea` directory suggests CLion usage, but the build is Makefile-only.
- Files: `Makefile`
- Impact: IDE integration may be incomplete. No CMake build available.
- Fix approach: Either add a `CMakeLists.txt` for CLion integration or remove CLion project files.

### Mac-Specific Flags in Makefile

- Issue: `MACFLAGS = -I/opt/homebrew/include -L/opt/homebrew/lib` is always passed to the compiler, even on Linux.
- Files: `Makefile` line 7
- Impact: Harmless on Linux if the paths do not exist, but untidy. Could cause issues if `/opt/homebrew` exists with incompatible libraries on a Linux system.
- Fix approach: Conditionally set MACFLAGS based on `uname -s` or use `pkg-config`.

### No Compiler Sanitizer Flags in Debug Mode

- Issue: The Makefile only defines `-Wall -Wextra -O2`. No debug target with `-g -fsanitize=address,undefined`.
- Files: `Makefile` line 4
- Impact: Memory bugs (the dangling pointer, potential double-free) are not caught during development.
- Fix approach: Add a `debug` target: `CFLAGS_DEBUG = -Wall -Wextra -g -O0 -fsanitize=address,undefined`.

---

## Dependency Risks

### Vendored Raylib and cJSON (No Version Tracking)

- Issue: Raylib headers (`lib/raylib.h`, `lib/rlgl.h`, `lib/raymath.h`) and cJSON (`lib/cJSON.c`, `lib/cJSON.h`) are vendored in `lib/` with no version file or update mechanism.
- Files: `lib/raylib.h`, `lib/cJSON.c`
- Impact: No way to track which version is vendored or when it was last updated. Security patches to cJSON (known overflow at line 1902) remain unpatched.
- Fix approach: Add a `lib/VERSIONS.md` file recording vendored library versions and dates.

### SQLite Linked as System Library

- Issue: SQLite is linked via `-lsqlite3` (system library), not vendored. Version depends on the host system.
- Files: `Makefile` line 5
- Impact: Different SQLite versions on different machines could cause subtle behavior differences.
- Fix approach: Acceptable for now; document the minimum required SQLite version.

### Platform Portability: POSIX-Only Serial I/O

- Issue: `nfc_reader.c` and `arduino_protocol.c` use POSIX APIs (`open()`, `read()`, `termios.h`, `fcntl.h`). No Windows support.
- Files: `src/hardware/nfc_reader.c`, `src/hardware/arduino_protocol.c`
- Impact: NFC hardware support is Linux/macOS only. Matches the Makefile's MACFLAGS suggesting cross-platform intent (Mac + Linux).
- Fix approach: Acceptable for current scope. If Windows support is needed, abstract serial I/O behind a platform layer.

---

## Test Coverage Gaps

### No Tests Exist

- What's not tested: Everything. The entire codebase has zero test files.
- Files: All source files under `src/`
- Risk: Any of the Critical/High issues above (dangling pointer, double-free, out-of-bounds access) could be caught by even basic unit tests.
- Priority: High. Start with:
  1. `troop_create_data_from_card()` - validates the dangling pointer bug
  2. `arduino_read_packet()` - validates protocol parsing with crafted byte sequences
  3. `entity_create()` / `entity_destroy()` lifecycle
  4. `db_query_params()` - validates parameterized query safety
  5. `cards_load()` / `cards_free()` - validates memory lifecycle

---

*Concerns audit: 2026-03-28*
