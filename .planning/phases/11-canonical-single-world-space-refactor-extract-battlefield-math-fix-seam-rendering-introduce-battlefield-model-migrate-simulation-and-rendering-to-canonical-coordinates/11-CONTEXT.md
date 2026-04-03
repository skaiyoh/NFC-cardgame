# Phase 11: Canonical Single-World-Space Refactor - Context

**Gathered:** 2026-04-01
**Status:** Replanned for full canonical rewrite

<domain>
## Phase Boundary

Unify the match into one canonical 1080x1920 battlefield with two rotated split-screen cameras viewing the same world. This phase removes the dual-space remap model, the duplicated opponent-space math, and the seam-specific render handoff logic that currently lives in `src/core/game.c` and `src/logic/combat.c`.

The refactor does NOT add new gameplay capabilities. Combat, movement pacing, card spawning, and split-screen camera orientation must remain behaviorally equivalent where intended. The rotated `Camera2D` approach stays; the cross-space remap architecture does not.

</domain>

<decisions>
## Implementation Decisions

### Canonical Board Geometry
- **D-01:** Canonical board is `1080 x 1920`, origin `(0,0)` at the top-left of the full shared battlefield.
- **D-02:** Seam at `y = 960` divides the board into two territories:
  - Top territory: `{0, 0, 1080, 960}`
  - Bottom territory: `{0, 960, 1080, 960}`
- **D-03:** P1 occupies the bottom half. P1 forward motion remains toward decreasing `y`.
- **D-04:** P2 occupies the top half. P2 forward motion is toward increasing `y`.
- **D-05:** Canonical board is one 1080-wide column. The current `P1 x=0..1080` / `P2 x=960..2040` overlap is deleted from the final architecture.

### Lateral Mirroring
- **D-06:** P2 lateral mirroring happens exactly once at the local-to-canonical boundary.
  - P1 local lateral `u` -> canonical `x = u * boardWidth`
  - P2 local lateral `u` -> canonical `x = (1 - u) * boardWidth`
- **D-07:** Canonical lane indices are world-global:
  - lane `0` = world left
  - lane `1` = center
  - lane `2` = world right
- **D-08:** Slot-to-lane mapping:
  - P1: `0 -> 0`, `1 -> 1`, `2 -> 2`
  - P2: `0 -> 2`, `1 -> 1`, `2 -> 0`

### Migration Order
- **D-09:** Phase 11 no longer includes a required standalone seam-fix deliverable on the dual-space codebase. The deliverable is the full canonical Battlefield rewrite.
- **D-10:** Phase 11 is executed as 5 plans:
  1. Extract geometry and seam math (`battlefield_math`) -- complete
  2. Introduce `Battlefield` / `Territory`, canonical board geometry, and a temporary adapter boundary
  3. Migrate simulation/state ownership to canonical Battlefield coordinates
  4. Migrate rendering to canonical world space and delete seam remap / RenderTexture special cases
  5. Remove remaining adapters, add invariants, and complete validation

### Battlefield Ownership
- **D-11:** `Battlefield` owns the authoritative world:
  - canonical board geometry
  - top/bottom territories and their terrain
  - canonical lane waypoints and slot spawn anchors
  - authoritative entity registry
  - world query helpers
- **D-12:** `Player` becomes seat/view/input/resource state:
  - keep: `id`, `side`, `screenArea`, `camera`, `cameraRotation`, slot card/cooldown state, energy fields
  - remove by end of Phase 11: `playArea`, tilemap/biome ownership, lane waypoints, entity arrays, base pointer, slot world positions
- **D-13:** Terrain is owned by `Battlefield.territories[SIDE_TOP/SIDE_BOTTOM]`, not by `Player`.
- **D-14:** Biome assignment attaches to `BattleSide`, not `player.id`.
- **D-15:** Temporary adapter fields may remain on `Player` during Plans 02-04 only if they are explicitly marked non-authoritative and kept in sync from `Battlefield`. Plan 05 removes them.

### Safety & Invariants
- **D-16:** Runtime debug assertions catch mixed-coordinate bugs during transition.
- **D-17:** New Battlefield APIs use typed wrappers from `battlefield_math` (`CanonicalPos`, `SideLocalPos`) where practical.
- **D-18:** Coordinate transforms are centralized in `battlefield_math`. No new ad hoc remap formulas are allowed.

### Rendering Contract
- **D-19:** Final rendering is two independent rotated cameras onto one canonical board. Entities are visible when their canonical sprite bounds intersect a viewport's visible region.
- **D-20:** Exact pixel-identical continuity at screen `x = 960` is NOT a success criterion. With `+90` / `-90` opposing cameras, both halves cannot own the exact same seam pixels and remain visually identical.
- **D-21:** `seamRT`, crossed remap rendering, crossed-facing adjustment, and bleed-only seam logic are transitional code and must be deleted by the end of Phase 11.

### Claude's Discretion
- Exact adapter implementation during transition
- Internal layout of `Battlefield` / `Territory`
- Whether to add helper APIs for `playerID -> BattleSide`
- Exact placement of debug assertions in hot paths

</decisions>

<canonical_refs>
## Canonical References

### Existing Dual-Space Problem Areas
- `src/core/game.c` -- `game_map_crossed_world_point`, crossed-facing logic, seam RT composite, and owner/opponent draw branching
- `src/logic/combat.c` -- `map_to_opponent_space`
- `src/rendering/viewport.c` -- current overlapping per-player `playArea` setup

### Existing Canonical Foundation
- `src/core/battlefield_math.h`
- `src/core/battlefield_math.c`
- `tests/test_battlefield_math.c`

### Simulation Modules To Migrate
- `src/logic/card_effects.c`
- `src/logic/pathfinding.c`
- `src/logic/combat.c`
- `src/entities/entities.c`

### State/Struct Ownership To Migrate
- `src/core/types.h`
- `src/systems/player.c`
- `src/systems/player.h`

### Rendering Modules To Rewrite
- `src/core/game.c`
- `src/rendering/viewport.c`
- `src/rendering/viewport.h`
- `src/rendering/sprite_renderer.c`

### Existing Tests
- `tests/test_battlefield_math.c`
- `tests/test_pathfinding.c`
- `tests/test_combat.c`

</canonical_refs>

<code_context>
## Existing Code Insights

### Confirmed Findings From Investigation
- `game_map_crossed_world_point()` is screen-correct for point positions under the current camera setup.
- Frame bounds / atlas metadata are useful, but they did not solve the disappearing seam bug.
- The remaining failure mode is architectural: crossed sprites fall between draw paths because the code treats seam crossing as a remap event.
- The current `seamRT` path in `src/core/game.c` is an experimental bridge, not the final answer.

### Reusable Assets
- `battlefield_math` already defines the canonical coordinate wrappers and core helper functions.
- `viewport_begin()` / `viewport_end()` and the rotated camera setup can stay conceptually intact.
- Existing sprite bounds metadata can still be reused for culling and future polish, even though it is not the architectural fix.

### Integration Points
- `GameState` needs a first-class `Battlefield battlefield`
- `viewport_init_split_screen()` must derive camera targets from Battlefield territory bounds
- `card_effects`, `pathfinding`, `combat`, and `entity_update` must read/write Battlefield-owned data
- `game_render()` must iterate Battlefield entities directly and remove owner/opponent remap branching

</code_context>

<specifics>
## Specific Guidance

- Do not spend more time perfecting the dual-space seam patch. The phase is complete only when the dual-space handoff is deleted.
- Preserve current movement depth proportions: canonical geometry must keep the effective per-side depth at 960 world units.
- Do not partially migrate without a declared adapter boundary. Battlefield may become authoritative before Player adapters are fully removed, but authority must be explicit.
- Success criterion for rendering is: no entity disappearance or pop caused by remap ownership changes. Not: pixel-identical art across the center line.

</specifics>

<deferred>
## Deferred Ideas

- Any optional presentation policy for the literal screen center seam after the rewrite
- Any packed texture atlas / sprite pipeline cleanup unrelated to canonical world ownership

</deferred>

---

*Phase: 11-canonical-single-world-space-refactor*
*Context updated: 2026-04-01*
