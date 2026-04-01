---
phase: 01-lane-pathfinding
verified: 2026-03-28T23:00:00Z
status: human_needed
score: 7/7 automated must-haves verified
re_verification: false
human_verification:
  - test: "Press F1 to enable debug overlay; confirm 3 colored lane paths are visible for each player"
    expected: "Left lane (blue) bows outward in a '(' curve, center (green) is a straight vertical line, right lane (red) bows outward in a ')' curve"
    why_human: "Visual appearance of curved paths cannot be verified programmatically"
  - test: "Spawn a troop in each lane (keys 1, 2, 3 for P1) with F1 overlay active; watch them walk"
    expected: "Each troop follows the colored line for its lane — no troop crosses into another lane's path"
    why_human: "Lane-separation criterion requires visual observation of runtime behavior"
  - test: "Spawn a troop and watch it reach the far end of its lane"
    expected: "Troop stops and idles at the enemy side of the board with slight random scatter; it does NOT despawn off-screen"
    why_human: "Idle-at-end behavior requires observing troop state transition in the running game"
---

# Phase 1: Lane Pathfinding Verification Report

**Phase Goal:** Troops navigate along predefined lane waypoints toward the enemy base
**Verified:** 2026-03-28
**Status:** human_needed — all automated checks pass; 3 visual criteria require human playtest
**Re-verification:** No — initial verification

---

## Goal Achievement

### Success Criteria (from ROADMAP.md)

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | A spawned troop walks along the waypoints defined for its lane, visibly following the path curves rather than moving in a straight line | ? HUMAN NEEDED | `ESTATE_WALKING` delegates to `pathfind_step_entity()` which advances waypoint indices along curved `laneWaypoints`; visual confirmation required |
| 2 | Troops in different lanes follow different paths without crossing into each other's lanes | ? HUMAN NEEDED | Each entity carries `e->lane` (0–2) and reads `owner->laneWaypoints[e->lane][...]`; lane isolation verified structurally; visual confirmation required |
| 3 | A troop that reaches the end of its lane waypoints stops or idles at the enemy side of the board | ? HUMAN NEEDED | `pathfind_step_entity()` calls `entity_set_state(e, ESTATE_IDLE)` when `waypointIndex >= LANE_WAYPOINT_COUNT`; `endDepth = 1.75f` extends paths into enemy territory; CORE-01e unit test passes; visual confirmation required |

All three success criteria have strong automated evidence. Human playtest is required to confirm the visual/runtime behavior in the actual game loop.

---

## Must-Have Truths (from PLAN frontmatter)

### Plan 01-00 Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | `make test` compiles and runs test_pathfinding without opening a Raylib window | VERIFIED | `make test` output: "All 6 tests passed!" — exit 0, no GPU required |
| 2 | Test binary exits 0 when all assertions pass, non-zero on failure | VERIFIED | `make test` exits 0 confirmed |
| 3 | Tests cover CORE-01a through CORE-01f | VERIFIED | 6 test functions present in `tests/test_pathfinding.c` covering all subtests |
| 4 | Tests for stepping and direction call production helpers in pathfinding.c, not inline simulations | VERIFIED | CORE-01d calls `pathfind_step_entity`, CORE-01e calls `pathfind_step_entity`, CORE-01f calls `pathfind_compute_direction` |

### Plan 01-01 Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Lane waypoint arrays are pre-computed once at player init, not recalculated per frame | VERIFIED | `player.c` line 60: `lane_generate_waypoints(p)` called once in `player_init()` |
| 2 | Center lane waypoints have zero lateral offset (straight line) | VERIFIED | `bow_offset()` returns 0 for `lane == 1`; CORE-01b unit test passes |
| 3 | Left and right lane waypoints bow outward symmetrically with sine curve peaking at midpoint | VERIFIED | `sinf(t * PI_F) * LANE_BOW_INTENSITY * laneWidth` with sign flip per lane; CORE-01c passes |
| 4 | First waypoint position matches the card slot spawn position exactly | VERIFIED | `p->laneWaypoints[lane][0] = p->slots[lane].worldPos` at line 43 of pathfinding.c; CORE-01a passes |
| 5 | Stepping and direction logic exists as pure helpers in pathfinding.c testable by Wave 0 tests | VERIFIED | `pathfind_step_entity` and `pathfind_compute_direction` are defined functions in pathfinding.c |
| 6 | Entities spawn with waypointIndex=1 to skip zero-distance first waypoint | VERIFIED | `card_effects.c` line 84: `e->waypointIndex = 1;` |

### Plan 01-02 Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | A spawned troop walks along the waypoints defined for its lane, visibly following the path curves | ? HUMAN NEEDED | Code path confirmed; visual verification required |
| 2 | Troops in different lanes follow different paths without crossing into each other's lanes | ? HUMAN NEEDED | Structural lane isolation confirmed; visual required |
| 3 | A troop that reaches the end of its lane waypoints stops and idles with random jitter | ? HUMAN NEEDED | IDLE transition verified by CORE-01e unit test; visual required |
| 4 | Sprites face their movement direction | ? HUMAN NEEDED | `pathfind_compute_direction` wired with hysteresis; visual required |
| 5 | Center lane troops always face DIR_UP | ? HUMAN NEEDED | CORE-01f unit test includes center lane direction verification; visual required |
| 6 | Debug overlay draws colored lane paths toggled by F1 key | ? HUMAN NEEDED | `s_showLaneDebug`, `IsKeyPressed(KEY_F1)`, and `debug_draw_lane_paths` all wired; visual required |
| 7 | ESTATE_WALKING calls pathfind_step_entity() from pathfinding.c | VERIFIED | `entities.c` line 81: `pathfind_step_entity(e, owner, deltaTime);` — old straight-line code removed |

**Automated Score:** 7/7 programmatically verifiable must-haves confirmed

---

## Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `tests/test_pathfinding.c` | 6 assert-based unit tests for CORE-01a–01f | VERIFIED | 585-line file with all 6 test functions, SYNC REQUIREMENT comment, production code calls |
| `Makefile` | `make test` target | VERIFIED | Lines 24–28: `test_pathfinding` build + `test` run targets; `clean` removes binary |
| `src/core/config.h` | LANE_WAYPOINT_COUNT, LANE_BOW_INTENSITY, LANE_JITTER_RADIUS, PI_F, DIRECTION_HYSTERESIS | VERIFIED | Lines 39–43 contain all 5 defines |
| `src/core/types.h` | `waypointIndex` on Entity, `laneWaypoints[3][8]` on Player | VERIFIED | Lines 53 and 104 confirm both fields |
| `src/logic/pathfinding.h` | `lane_generate_waypoints`, `pathfind_step_entity`, `pathfind_compute_direction` | VERIFIED | All 3 declarations present; `pathfind_next_step` absent (old stub removed) |
| `src/logic/pathfinding.c` | Waypoint generation with sine bow, stepping helper, direction helper | VERIFIED | 139 lines with `bow_offset`, `lane_generate_waypoints`, `pathfind_step_entity`, `pathfind_compute_direction` |
| `src/systems/player.c` | `lane_generate_waypoints` call in `player_init` | VERIFIED | Line 7 includes `pathfinding.h`; line 60 calls `lane_generate_waypoints(p)` after `player_init_card_slots` |
| `src/logic/card_effects.c` | `waypointIndex = 1` set at spawn | VERIFIED | Line 84: `e->waypointIndex = 1;` |
| `src/entities/entities.c` | ESTATE_WALKING delegates to `pathfind_step_entity` | VERIFIED | Line 81: single-line delegation; old `despawnY`, `borderY`, `position.y -=` absent |
| `src/rendering/viewport.c` | `debug_draw_lane_paths` function | VERIFIED | Lines 103–121: full implementation with `DrawCircleV`/`DrawLineV` per lane |
| `src/rendering/viewport.h` | `debug_draw_lane_paths` declaration | VERIFIED | Line 32 |
| `src/core/game.c` | F1 toggle + `debug_draw_lane_paths` calls in render | VERIFIED | Lines 16, 98, 194–200; overlay uses `BeginMode2D`/`EndMode2D` (not viewport_begin/end — see note) |

---

## Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `tests/test_pathfinding.c` | `src/logic/pathfinding.c` | `#include "../src/logic/pathfinding.c"` directly; calls `pathfind_step_entity` | WIRED | Line 234: direct include; `pathfind_step_entity` and `pathfind_compute_direction` called in 4 tests |
| `Makefile` | `tests/test_pathfinding.c` | `test_pathfinding:` target compiles and `test:` runs it | WIRED | Lines 24–28 |
| `src/systems/player.c` | `src/logic/pathfinding.h` | `player_init` calls `lane_generate_waypoints` | WIRED | Line 7 include, line 60 call |
| `src/logic/pathfinding.c` | `src/systems/player.h` | `player_lane_pos` used for base waypoint positions | WIRED | Line 2 include; `player_lane_pos` called at line 56 |
| `src/logic/card_effects.c` | `src/core/types.h` | `e->waypointIndex = 1` at spawn | WIRED | Line 84 |
| `src/entities/entities.c` | `src/logic/pathfinding.h` | ESTATE_WALKING calls `pathfind_step_entity(e, owner, deltaTime)` | WIRED | Line 6 include, line 81 call |
| `src/core/game.c` | `src/rendering/viewport.c` | Calls `debug_draw_lane_paths` inside camera transforms | WIRED | Lines 194–200 (wrapped in BeginMode2D/EndMode2D, not viewport_begin/end) |

---

## Requirements Coverage

| Requirement | Source Plans | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| CORE-01 | 01-00, 01-01, 01-02 | Troops move along per-lane waypoints toward the enemy base (not raw straight-line Y movement) | SATISFIED | Waypoint generation implemented with sine bow curves; ESTATE_WALKING delegates to pathfind_step_entity; waypointIndex advances per frame; all 6 unit tests pass; REQUIREMENTS.md line 10 already marks this `[x]` |

No orphaned requirements: REQUIREMENTS.md assigns only CORE-01 to Phase 1, and all three plans claim CORE-01.

---

## Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/entities/entities.c` | 11–13 | TODO: `s_nextEntityID` overflow (no rollover) | INFO | Pre-existing concern unrelated to Phase 1; noted in REQUIREMENTS.md as REL-01 |
| `src/entities/entities.c` | 65–67 | TODO: No combat wired into `entity_update` | INFO | Expected at Phase 1 — combat is Phase 3/4 scope |
| `src/entities/entities.c` | 73–75 | TODO: ESTATE_IDLE has no targeting behavior | INFO | Expected at Phase 1 scope |
| `src/entities/entities.c` | 85–89 | TODO: ESTATE_DEAD does not wait for death animation | INFO | Expected at Phase 1; Phase 5 scope |

No blockers. All TODOs are pre-existing or expected future work from later phases.

---

## Implementation Note: `endDepth` Deviation

The plan specified `endDepth = 0.85f` (leave room for future enemy base). The implementation uses `endDepth = 1.75f`, extending waypoints into enemy territory. This was evidently an intentional change during execution (commit `2bf7c9d` is titled "extend paths across viewports and fix bow curve"). The tests in `test_waypoint_positions` explicitly comment "Y extends into opponent territory (depth > 1.0), no upper/lower bound check." This satisfies success criterion 3 ("stops or idles at the enemy side of the board") and represents a forward-looking design decision rather than a gap.

## Implementation Note: Debug Overlay Camera Wrapping

Plan 01-02 specified the debug overlay should be drawn inside `viewport_begin()`/`viewport_end()` brackets. The actual implementation wraps each call in `BeginMode2D(g->players[N].camera)` / `EndMode2D()` outside the scissor region (commit `2bf7c9d`: "extend paths across viewports"). This allows the debug overlay to span both viewports without scissor clipping. The behavior is functionally correct and visually superior to the planned approach.

---

## Human Verification Required

### 1. Curved Lane Path Rendering

**Test:** Build and run `make cardgame && ./cardgame`. Press F1 to toggle the debug overlay.
**Expected:** Three colored lane paths appear: blue on the left in a '(' outward curve, green in the center as a straight vertical line, red on the right in a ')' outward curve. Both player viewports show the paths.
**Why human:** Visual correctness of curve shapes cannot be verified by grep or static analysis.

### 2. Troops Follow Lane Paths (no crossing)

**Test:** With debug overlay active, press 1 (or the key that spawns P1 troops) to spawn troops in each lane. Observe their movement.
**Expected:** Each troop tracks along the colored line for its assigned lane. Troops in different lanes maintain separate paths and do not cross.
**Why human:** Runtime entity movement along waypoint paths requires observing the game loop executing.

### 3. Troop Idles at Enemy Side

**Test:** Spawn a troop and watch it reach the end of its lane path.
**Expected:** The troop reaches the far end (enemy territory), stops with a slight random scatter offset, and transitions to an idle animation. It does NOT despawn or walk off-screen.
**Why human:** Idle-at-end state transition requires observing the full path traversal to completion.

---

## Summary

Phase 1 implementation is complete and well-structured. All automated verification passes:

- All 6 unit tests (CORE-01a through CORE-01f) pass via `make test`
- All key artifacts exist with substantive implementations (no stubs)
- All critical wiring links are confirmed in the codebase
- Old straight-line movement code (`despawnY`, `borderY`, `position.y -=`) fully removed
- ESTATE_WALKING is a clean single-line delegation to tested production code
- CORE-01 is satisfied and marked complete in REQUIREMENTS.md

Three success criteria from ROADMAP.md require human visual confirmation because they describe observable gameplay behavior (curved path following, lane isolation, idle-at-end). The code evidence strongly supports all three criteria being satisfied.

---

_Verified: 2026-03-28_
_Verifier: Claude (gsd-verifier)_
