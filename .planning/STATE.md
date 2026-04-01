---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: executing
stopped_at: Completed 11-03-PLAN.md
last_updated: "2026-04-01T17:17:33.797Z"
last_activity: 2026-04-01
progress:
  total_phases: 11
  completed_phases: 1
  total_plans: 8
  completed_plans: 5
  percent: 20
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-03-10)

**Core value:** Two players can sit down, tap physical cards, watch troops fight, and one player wins.
**Current focus:** Phase 11 — canonical-single-world-space-refactor

## Current Position

Phase: 11 (canonical-single-world-space-refactor) — EXECUTING
Plan: 4 of 5
Status: Ready to execute
Last activity: 2026-04-01

Progress: [██░░░░░░░░] 20%

## Performance Metrics

**Velocity:**

- Total plans completed: 0
- Average duration: -
- Total execution time: 0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| - | - | - | - |

**Recent Trend:**

- Last 5 plans: -
- Trend: -

*Updated after each plan completion*
| Phase 01 P01 | 3min | 2 tasks | 6 files |
| Phase 01 P00 | 4min | 2 tasks | 2 files |
| Phase 01 P02 | 2min | 2 tasks | 4 files |
| Phase 11 P01 | 3min | 2 tasks | 7 files |
| Phase 11 P02 | 5min | 2 tasks | 9 files |
| Phase 11 P03 | 6min | 2 tasks | 9 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- [Roadmap]: Lane-based pathfinding chosen over A* (deterministic, fits card-lane mapping)
- [Roadmap]: Pathfinding and combat are independent phases (CORE-01 does not depend on combat)
- [Roadmap]: Base creation (CORE-06) must precede all combat/win-condition work
- [Phase 01]: Sine curve for lane bow (sinf(depth*PI)) -- smooth taper at endpoints
- [Phase 01]: Direction hysteresis at 15% threshold prevents sprite flicker near 45-degree angles
- [Phase 01]: waypointIndex=1 at spawn skips zero-distance first waypoint
- [Phase 01]: Header-guard-override pattern for isolating C unit tests from heavy include chains (Raylib, sqlite3, biome)
- [Phase 01]: Tests call production functions (not inline simulations) to prevent false confidence per cross-AI review
- [Phase 01]: ESTATE_WALKING is single-line delegation to pathfind_step_entity -- no inline logic duplication
- [Phase 01]: Debug overlay uses blue/green/red for left/center/right lanes with F1 toggle
- [Phase 11]: Vector2 guarded with VECTOR2_DEFINED for Raylib/test coexistence
- [Phase 11]: Mirror transform is self-inverse (same formula for canonical<->local SIDE_TOP)
- [Phase 11]: On-seam boundary (y==SEAM_Y) assigned to SIDE_BOTTOM territory
- [Phase 11]: Do not spend more work shipping a dual-space seam patch; Phase 11 deliverable is the full canonical Battlefield rewrite
- [Phase 11]: Exact pixel continuity at screen x=960 is not a valid success criterion under +90/-90 opposite cameras; success criterion is correct independent views with no remap-driven disappearance
- [Phase 11]: `seamRT`, crossed remap rendering, and duplicate cross-space math are transitional code and must be deleted before Phase 11 is complete
- [Phase 11]: NUM_CARD_SLOTS/MAX_ENTITIES moved to config.h to break battlefield.h <-> types.h circular dependency
- [Phase 11]: battlefield_math.c added to SRC_CORE since battlefield.c links against it at compile time
- [Phase 11]: VECTOR2_DEFINED guard set explicitly in battlefield.h before battlefield_math.h include to prevent Raylib conflict
- [Phase 11]: Pathfinding reads Battlefield canonical waypoints via bf_waypoint instead of Player.laneWaypoints
- [Phase 11]: Combat uses bf_distance on canonical CanonicalPos -- map_to_opponent_space deleted
- [Phase 11]: Dual-registry pattern: entities in both Battlefield (authoritative) and Player (adapter) during transition

### Pending Todos

- 11-02: Introduce Battlefield/Territory model and make it the authoritative source of board geometry
- 11-03: Migrate spawn, pathfinding, combat, and entity registry to canonical Battlefield coordinates
- 11-04: Rewrite rendering to draw canonical entities directly and delete seam remap / RenderTexture special cases
- 11-05: Remove remaining Player adapter fields, add debug assertions, and finish test/manual validation

### Roadmap Evolution

- Phase 11 refined: Canonical single-world-space refactor now proceeds as a full rewrite, not a tactical seam-fix plus refactor bundle

### Blockers/Concerns

- `building_create_base` currently returns NULL -- must be fixed in Phase 2 before combat phases can terminate matches
- The current dual-space seam fixes are not converging because the real problem is architectural ownership of world space, not frame bounds math
- Opposite rotated cameras cannot both own the exact same seam pixels and remain visually identical; validation must focus on canonical visibility correctness instead

## Session Continuity

Last session: 2026-04-01T17:17:33.796Z
Stopped at: Completed 11-03-PLAN.md
Resume file: None
