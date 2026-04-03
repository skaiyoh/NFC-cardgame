---
phase: 11-canonical-single-world-space-refactor
plan: 01
subsystem: core
tags: [coordinate-system, math, tdd, typed-wrappers]

# Dependency graph
requires: []
provides:
  - "BattleSide enum (SIDE_BOTTOM, SIDE_TOP) for spatial player identification"
  - "CanonicalPos / SideLocalPos typed coordinate wrappers"
  - "bf_to_canonical / bf_to_local transform functions"
  - "bf_distance, bf_in_bounds, bf_slot_to_lane, bf_crosses_seam, bf_side_for_pos helpers"
  - "BF_ASSERT_IN_BOUNDS debug macro"
  - "BOARD_WIDTH=1080, BOARD_HEIGHT=1920, SEAM_Y=960 constants in config.h"
affects: [11-02, 11-03, 11-04, 11-05]

# Tech tracking
tech-stack:
  added: []
  patterns: [typed-coordinate-wrappers, header-guard-override-tests, self-inverse-mirror-transform]

key-files:
  created:
    - src/core/battlefield_math.h
    - src/core/battlefield_math.c
    - tests/test_battlefield_math.c
  modified:
    - src/core/config.h
    - Makefile
    - CMakeLists.txt
    - .gitignore

key-decisions:
  - "Vector2 guarded with VECTOR2_DEFINED to coexist with Raylib and test stubs"
  - "Mirror transform is self-inverse: bf_to_local reuses same formula as bf_to_canonical for SIDE_TOP"
  - "SEAM_Y boundary: y >= seamY is SIDE_BOTTOM (on-seam = bottom territory)"

patterns-established:
  - "Typed coordinate wrappers: CanonicalPos/SideLocalPos prevent accidental mixing of coordinate spaces"
  - "bf_ prefix namespace for all battlefield math functions"
  - "TDD with header-guard-override pattern for new modules"

requirements-completed: [REFACTOR-11]

# Metrics
duration: 3min
completed: 2026-04-01
---

# Phase 11 Plan 01: Extract Battlefield Math Summary

**TDD-built battlefield_math module with typed coordinate wrappers (CanonicalPos/SideLocalPos), 7 transform/geometry functions, and 12 comprehensive tests**

## Performance

- **Duration:** 3 min
- **Started:** 2026-04-01T06:46:28Z
- **Completed:** 2026-04-01T06:49:19Z
- **Tasks:** 2
- **Files modified:** 7

## Accomplishments
- Created battlefield_math.h/.c as the single source of truth for all coordinate transform math
- Typed wrappers (CanonicalPos, SideLocalPos) provide compile-time friction against coordinate mixing
- 12 test functions cover transforms, distance, bounds, seam crossing, and slot-to-lane mapping
- All 19 tests pass (7 pathfinding + 12 battlefield_math)

## Task Commits

Each task was committed atomically:

1. **Task 1: Create test suite and config constants (RED)** - `979a9fb` (test)
2. **Task 2: Implement battlefield_math module (GREEN)** - `53db6a0` (feat)
3. **Housekeeping: Add test binaries to .gitignore** - `4db65a7` (chore)

## Files Created/Modified
- `src/core/battlefield_math.h` - Type definitions (BattleSide, CanonicalPos, SideLocalPos) and function declarations
- `src/core/battlefield_math.c` - All 7 coordinate transform and geometry function implementations
- `tests/test_battlefield_math.c` - 12 unit tests covering all functions and edge cases
- `src/core/config.h` - Added BOARD_WIDTH, BOARD_HEIGHT, SEAM_Y constants
- `Makefile` - Added test_battlefield_math target and integrated into `make test`
- `CMakeLists.txt` - Added test_battlefield_math target with CTest integration
- `.gitignore` - Added test binary exclusions

## Decisions Made
- Vector2 guarded with VECTOR2_DEFINED preprocessor check to coexist with Raylib in production and stubs in tests
- Mirror transform is self-inverse (same formula for bf_to_canonical SIDE_TOP and bf_to_local SIDE_TOP)
- On-seam boundary (y == SEAM_Y) assigned to SIDE_BOTTOM territory

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Added test binaries to .gitignore**
- **Found during:** Post-task cleanup
- **Issue:** Compiled test binaries (test_pathfinding, test_battlefield_math) were untracked
- **Fix:** Added both to .gitignore
- **Files modified:** .gitignore
- **Committed in:** 4db65a7

**2. [Rule 3 - Blocking] Skipped test_combat CMakeLists.txt entry**
- **Found during:** Task 1 (CMakeLists.txt update)
- **Issue:** Plan requested adding test_combat target but no test_combat.c file exists in the repository. combat.c only contains TODO stubs and forward declarations.
- **Fix:** Did not add the non-existent test_combat target. Will be added when combat tests are created.
- **Committed in:** N/A (skip, not addition)

---

**Total deviations:** 2 (1 auto-fixed, 1 skipped non-existent target)
**Impact on plan:** Minimal. Gitignore fix is housekeeping. Skipping test_combat avoids build errors from non-existent file.

## Issues Encountered
- Worktree was based on main branch which lacked tests/ directory and dev branch code; merged origin/dev to get current codebase before starting work.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness
- battlefield_math module ready for use by Plans 02-05
- CanonicalPos/SideLocalPos types ready for Battlefield model (Plan 02)
- Transform functions ready for migration of game.c and combat.c (Plans 03-05)

---
*Phase: 11-canonical-single-world-space-refactor*
*Completed: 2026-04-01*
