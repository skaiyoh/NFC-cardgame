---
phase: 1
slug: lane-pathfinding
status: draft
nyquist_compliant: true
wave_0_complete: false
created: 2026-03-28
---

# Phase 1 -- Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Assert-based C tests (no external framework) |
| **Config file** | Makefile `test` and `test_pathfinding` targets |
| **Quick run command** | `make test` |
| **Full suite command** | `make test && make cardgame` |
| **Estimated runtime** | ~3 seconds (compile test + run + compile game) |

---

## Sampling Rate

- **After every task commit:** Run `make cardgame` (build succeeds) + `make test` (unit tests pass)
- **After every plan wave:** Run `make test && make cardgame && ./cardgame` (unit tests + visual verification)
- **Before `/gsd:verify-work`:** Full build + all unit tests green + visual verification of all 3 lanes
- **Max feedback latency:** 3 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 1-00-01 | 00 | 0 | CORE-01 | unit | `make test` | tests/test_pathfinding.c | pending |
| 1-00-02 | 00 | 0 | CORE-01 | build | `grep "test_pathfinding" Makefile` | Makefile | pending |
| 1-01-01 | 01 | 1 | CORE-01 | build | `make cardgame` | src/core/config.h, src/core/types.h | pending |
| 1-01-02 | 01 | 1 | CORE-01 | unit+build | `make test && make cardgame` | src/logic/pathfinding.c | pending |
| 1-02-01 | 02 | 2 | CORE-01 | unit+build | `make test && make cardgame` | src/entities/entities.c | pending |
| 1-02-02 | 02 | 2 | CORE-01 | build | `make cardgame` | src/rendering/viewport.c, src/core/game.c | pending |
| 1-02-03 | 02 | 2 | CORE-01 | visual | `make cardgame && ./cardgame` (manual) | N/A | pending |

*Status: pending / green / red / flaky*

---

## Wave 0 Requirements

- [x] Test file: `tests/test_pathfinding.c` -- assert-based unit tests for CORE-01a through CORE-01f
- [x] Makefile target: `make test` compiles and runs `test_pathfinding` without Raylib window
- [x] Test harness: minimal `main()` that runs test functions and reports pass/fail via assert + printf

---

## Test Coverage Map (CORE-01 subtests)

| Subtest | Description | Test Function | Automated |
|---------|-------------|---------------|-----------|
| CORE-01a | Waypoint generation produces correct positions | `test_waypoint_positions` | `make test` |
| CORE-01b | Center lane waypoints have no lateral offset | `test_center_lane_zero_offset` | `make test` |
| CORE-01c | Outer lanes bow outward symmetrically | `test_outer_lane_bow_magnitude` | `make test` |
| CORE-01d | Entity advances through waypoints at moveSpeed | `test_movement_step_advances_waypoint` | `make test` |
| CORE-01e | Entity transitions to ESTATE_IDLE at last waypoint | `test_idle_at_last_waypoint` | `make test` |
| CORE-01f | Sprite direction matches movement vector dominant axis | `test_sprite_direction_from_movement` | `make test` |
| CORE-01g | Troop visibly follows curved path | N/A | manual (F1 overlay) |
| CORE-01h | Troops in different lanes follow different paths | N/A | manual (spawn all 3) |

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Troops follow curved lane paths | CORE-01g | Visual/gameplay behavior -- no headless rendering | Launch game, spawn troops in all 3 lanes, verify center lane straight, outer lanes bow outward at midpoint |
| Lanes don't cross | CORE-01h | Spatial relationship -- visual only | Spawn troops in left and right lanes simultaneously, verify paths never intersect |
| Troops idle at end | CORE-01e | End-state (also covered by unit test) | Watch a troop reach the end of its lane, verify it stops and idles with random jitter |
| Sprite direction follows curve | CORE-01f | Animation direction (also covered by unit test) | Watch left lane troop: faces left at spawn, up at midpoint, right near enemy end |
| Debug overlay toggle | CORE-01 | Visual overlay | Press F1, verify colored lane lines appear |

---

## Validation Sign-Off

- [x] All tasks have `<automated>` verify or Wave 0 dependencies
- [x] Sampling continuity: no 3 consecutive tasks without automated verify
- [x] Wave 0 covers all MISSING references
- [x] No watch-mode flags
- [x] Feedback latency < 5s
- [x] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
