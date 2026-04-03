---
phase: 11
slug: canonical-single-world-space-refactor
status: active
nyquist_compliant: true
wave_0_complete: true
created: 2026-04-01
---

# Phase 11 — Validation Strategy

> Per-phase validation contract for the full canonical Battlefield rewrite.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Custom C assert-based tests |
| **Quick run command** | `make test` |
| **Full suite command** | `make test && make` |
| **Estimated runtime** | ~2-5 seconds |

---

## Sampling Rate

- After every task: run `make test`
- After every plan: run `make test && make`
- Before final sign-off: run structural grep checks plus manual visual verification

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | Status |
|---------|------|------|-------------|-----------|-------------------|--------|
| 11-01-01 | 01 | 1 | battlefield_math foundation | unit | `make test_battlefield_math && ./test_battlefield_math` | ✅ green |
| 11-02-01 | 02 | 2 | Battlefield model + adapter boundary | unit/build | `make test` | ⬜ pending |
| 11-03-01 | 03 | 3 | canonical simulation migration | unit/build | `make test` | ⬜ pending |
| 11-04-01 | 04 | 4 | canonical rendering rewrite | unit/build | `make test && make` | ⬜ pending |
| 11-05-01 | 05 | 5 | cleanup, invariants, final hardening | unit/build/search | `make test && make` | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Structural Checks Required In Later Plans

- `rg -n "game_map_crossed_world_point|game_apply_crossed_direction" src`
  - must return no live code hits by end of Plan 11-04
- `rg -n "map_to_opponent_space" src`
  - must return no live code hits by end of Plan 11-03
- `rg -n "seamRT|spriteOverlapsSeam" src/core src/rendering`
  - must return no live code hits by end of Plan 11-05

---

## Automated Coverage Expectations

- `tests/test_battlefield_math.c` remains green throughout Phase 11
- `tests/test_pathfinding.c` is updated to Battlefield-owned waypoint access
- `tests/test_combat.c` is updated to canonical direct-distance combat
- Any test stubs mirroring `types.h` are updated in the same task that changes struct layout

---

## Manual-Only Verifications

| Behavior | Why Manual | Test Instructions |
|----------|------------|-------------------|
| Troops do not disappear when crossing from one territory to the other | Final symptom is visual and camera-dependent | Run the game, spawn troops from both sides, watch them traverse the seam area and continue into the opponent territory |
| P1 and P2 viewport orientation remains correct | Camera rotation correctness is visual | Verify P1 bottom-side view and P2 top-side view still face the intended direction |
| Battlefield-owned terrain renders correctly | Biome/tilemap ownership is visual | Verify both halves render their assigned biomes from Battlefield territory data |

**Important:** Manual validation does NOT require pixel-identical continuity at the literal screen center seam. It requires that canonical visibility is correct and no entity disappears because of cross-space handoff logic.

---

## Validation Sign-Off

- [x] Wave 0 / math foundation exists
- [x] Every remaining plan has an automated verification step
- [x] Structural grep checks defined for deletion of remap code
- [ ] Final manual validation complete after Plans 11-04 and 11-05

**Approval:** pending until canonical rewrite is complete
