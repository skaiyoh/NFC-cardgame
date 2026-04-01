---
phase: 1
reviewers: [gemini, codex]
reviewed_at: 2026-03-28T23:45:00Z
plans_reviewed: [01-00-PLAN.md, 01-01-PLAN.md, 01-02-PLAN.md]
review_type: post-execution (plans + actual implementation)
---

# Cross-AI Plan Review — Phase 1 (Post-Execution)

## Gemini Review

Phase 1 successfully achieves its core functional requirement: troops now navigate along curved, lane-specific paths using a waypoint system rather than moving in a straight line. The use of a sine-based "bow" function provides a clean, symmetric aesthetic for the outer lanes. However, the implementation phase suffered from significant "drift" from the original engineering plan. While the visual result is correct, the process introduced technical debt -- specifically misleading naming conventions, unused configuration constants, and the total abandonment of the automated testing infrastructure established in Plan 00.

### Strengths
- **Mathematical Simplicity:** Using a sine function for the lateral "bow" is computationally cheap and ensures perfect symmetry across lanes without needing complex spline libraries.
- **Flexible Waypoint System:** The implementation allows for easy adjustment of path resolution via `LANE_WAYPOINT_COUNT`.
- **Strategic Refactoring:** Adding `pathfind_apply_direction` to operate on `AnimState` (rather than just `Entity`) was a proactive move that anticipates the needs of the split-screen viewport mirroring logic.
- **Visual Polish:** The inclusion of endpoint jitter and a debug overlay shows attention to the "feel" of the game and the developer experience.

### Concerns
- **HIGH: Regression Risk (Deleted Tests):** The deletion of `test_pathfinding.c` is a major setback. Without these unit tests, future changes to movement speed, lane logic, or collision (Phase 3) could break pathfinding silently.
- **HIGH: Semantic Mismatch (Enum Names):** Mapping `DIR_DOWN` to a side-facing sprite and `DIR_SIDE` to a front-facing sprite is a "landmine" for future development. It violates the principle of least astonishment and will likely lead to bugs when new artists or developers join the project.
- **MEDIUM: Dead Code/Configuration:** `DIRECTION_HYSTERESIS` is defined in `config.h` but unused in the logic. This clutters the configuration space and suggests a half-implemented feature.
- **MEDIUM: Lack of State Guards:** The code does not check if an entity is `ESTATE_DEAD` before moving it. This could lead to "zombie" movement where corpses slide toward the enemy base during their death animation.
- **LOW: Magic Numbers in Pathfinding:** The `endDepth` of `2.125f` is a highly specific tuned value. While it works, it should ideally be derived from the coordinate system or defined as a constant to avoid confusion.

### Suggestions
- Fix the Enums: Rename the `Direction` enum values to match their actual visual representation (e.g., `DIR_SIDE`, `DIR_FRONT`, `DIR_BACK`) or fix the `pathfind_apply_direction` mapping to align with the names.
- Restore the Test Suite: Re-implement `tests/test_pathfinding.c`. Automated verification is critical for a project with 10+ phases.
- Implement Hysteresis: If `DIRECTION_HYSTERESIS` was intended to prevent "sprite flickering" on curves, it should be implemented. If not, remove it from `config.h`.
- Add State Validation: Wrap the movement logic in `pathfind_step_entity` with a check: `if (e->state == ESTATE_DEAD) return false;`.
- Seed the RNG: Ensure `srand()` is called in `game.c` so that the endpoint jitter is not identical every time the game launches.

### Risk Assessment
**MEDIUM.** The implementation is functionally sound for the current scope, but the structural integrity is concerning. The combination of misleading enums, deleted tests, and known dangling pointer issues in the broader codebase creates a "fragile" foundation.

---

## Codex Review

Phase 1 is good enough as a visual prototype, but not yet a stable foundation for the roadmap. The plan decomposition was strong: test harness first, then path generation/state integration, then render/debug verification. The implementation does achieve the immediate demo goals: troops follow per-lane waypoints, outer lanes curve, center stays straight, and units idle at the end. Fidelity drops in the parts that matter for later phases, though: the planned tests are gone, several documented contracts drifted, and the current "cross into enemy territory" behavior is mostly a render-space illusion rather than a shared gameplay-space model.

### Strengths
- The 3-wave plan was well structured. Separating test infrastructure, movement logic, and viewport/debug work reduced integration risk.
- Precomputing `laneWaypoints` on `Player` is a good design choice for determinism and cheap per-frame updates.
- The spawn integration is clean. Setting `e->lane` and starting at `waypointIndex = 1` avoids the zero-distance pause at spawn.
- Movement is centralized through `pathfind_step_entity()` instead of being inlined in update code, which is the right direction for maintainability.
- The F1 lane overlay is a practical verification tool and was the right addition for this phase.
- Adding `pathfind_apply_direction()` was a reasonable extension once mirrored rendering was introduced.

### Concerns
- **HIGH: Automated validation removed.** `tests/test_pathfinding.c` is deleted, and the Makefile still advertises `test`/`test_pathfinding` as phony targets but provides no real recipe; `make test` currently no-ops. That creates false confidence around a geometry-heavy feature.
- **HIGH: Enemy-side crossing is render-only.** Future combat is planned around raw world-space distance checks. `game_map_crossed_world_point()` and the crossed draw path only affect rendering; entity positions remain in owner space, while combat TODOs assume direct `Vector2Distance(a, b)` semantics. Phase 3 will not work correctly without a shared-space or transform-aware combat model.
- **HIGH: Lane endpoints not aligned with future base contract.** `endDepth = 2.125f` maps the last waypoint to the opponent's spawn line, while `player_base_pos()` places the base deeper in the defensive end. Center lane stops about 96 px short in depth, and outer lanes stay hundreds of pixels off a centered base. Phase 2/6 will need either different endpoints or lane-specific base sockets.
- **MEDIUM: Tuned bow no longer matches documented decision D-02.** `LANE_BOW_INTENSITY = 0.3f` means 30% not ~50%. That may be visually fine, but it is requirements drift that should be recorded explicitly.
- **MEDIUM: Direction system has contract drift and dead config.** `DIRECTION_HYSTERESIS` is defined but unused, header comments describe one mapping, and the implementation uses `DIR_DOWN` for side-facing sprites because enum names no longer match sheet rows.
- **LOW: Hardcoded magic depths.** Path generation relies on `0.125f`, `2.125f` even though comments describe derived geometry. If slot placement or board proportions change, the path math will silently drift.

### Suggestions
- Restore the deleted pathfinding tests and make `make test` fail loudly if the harness is missing.
- Decide the battlefield coordinate contract before Phase 2/3. Either put both players into one real gameplay space, or formalize transform helpers that movement, combat, and base targeting all use -- not just rendering.
- Replace magic `spawnDepth`/`endDepth` with waypoint endpoints derived from landmarks such as slot positions, front line, and intended base attack points.
- Resolve the base-path relationship now. If the base is centered, outer lanes need final convergence or attack sockets. If lanes stay separate, the base system should expose per-lane targets.
- Clean up the direction API. Either implement the planned hysteresis or delete the dead config/commentary, and rename or remap the direction enum so names match actual sprite rows.
- Before combat phases, expand and lock the entity state machine. `ESTATE_ATTACKING` is still absent from the enum, and current transitions allow illegal future flows such as DEAD back to WALKING.
- Cap `deltaTime` before later phases so frame stalls do not distort movement/combat timing.

### Risk Assessment
**HIGH.** For the full roadmap, it is high risk because the current implementation does not yet define a gameplay-space model that combat and base destruction can build on, and it has no effective regression coverage to catch geometry or direction breakage while those systems are added.

---

## Consensus Summary

### Agreed Strengths
- Pre-computed waypoint arrays on Player struct is the right design (deterministic, O(1) per frame)
- 3-wave plan decomposition (tests -> data layer -> behavior) was well structured
- Centralized movement through `pathfind_step_entity()` improves maintainability
- Adding `pathfind_apply_direction(AnimState*)` for viewport mirroring was proactive and correct
- Sine bow function is clean, symmetric, and computationally cheap
- Debug overlay with F1 toggle is practical for tuning and verification

### Agreed Concerns
| Severity | Concern | Gemini | Codex |
|----------|---------|--------|-------|
| **HIGH** | Test file deleted -- zero regression coverage for geometry-heavy pathfinding | HIGH | HIGH |
| **HIGH** | Direction enum names (`DIR_DOWN`, `DIR_SIDE`, `DIR_UP`) do not match their sprite row mapping -- latent bug vector | HIGH | MEDIUM |
| **MEDIUM** | `DIRECTION_HYSTERESIS` defined in config.h but never used in any .c file -- dead config | MEDIUM | MEDIUM |
| **MEDIUM** | No `ESTATE_DEAD` guard in state transitions -- dead entities can re-enter WALKING | MEDIUM | raised in suggestions |
| **LOW** | Magic depth constants (`0.125f`, `2.125f`) should be derived or named | LOW | LOW |

### Divergent Views
| Topic | Gemini | Codex |
|-------|--------|-------|
| **Overall risk** | MEDIUM -- functionally sound for current scope | HIGH -- not a stable foundation for the full 10-phase roadmap |
| **Gameplay-space model** | Not addressed | HIGH concern -- render-only mirroring won't support combat distance checks in Phase 3 |
| **Base-path endpoint alignment** | Not addressed | HIGH concern -- lane endpoints don't converge on future base position |
| **Requirements drift (bow intensity 0.3 vs 0.5)** | Not flagged | MEDIUM -- documented decision D-02 says ~50%, implementation is 30% |
| **RNG seeding for jitter** | Suggested `srand()` call | Not mentioned |

### Key Takeaway
Both reviewers agree the visual outcome is correct but the engineering foundation needs cleanup before Phase 2. The top priorities are: (1) restore test coverage, (2) resolve the direction enum naming, (3) remove dead `DIRECTION_HYSTERESIS` config. Codex additionally flags that the gameplay-space coordinate model must be decided before combat phases -- entities live in per-player coordinate space but combat needs cross-player distance calculations.

---

*Review completed: 2026-03-28*
*Reviewers: Gemini CLI, Codex CLI (Claude excluded for independence)*
