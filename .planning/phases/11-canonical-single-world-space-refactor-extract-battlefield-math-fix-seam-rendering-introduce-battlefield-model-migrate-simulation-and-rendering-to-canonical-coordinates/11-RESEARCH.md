# Phase 11: Canonical Single-World-Space Refactor - Research

**Researched:** 2026-04-01
**Domain:** C game architecture / split-screen rendering / coordinate-system unification
**Confidence:** HIGH

## Summary

The codebase should stop treating seam crossing as a remap event and instead migrate to one canonical battlefield. The current dual-space system duplicates the same cross-space mapping in `src/core/game.c` and `src/logic/combat.c`, stores world geometry on `Player`, and forces rendering to decide which viewport "owns" a sprite as it approaches the seam. That is the root cause of the disappearing sprite problem.

The `battlefield_math` foundation built in Plan 11-01 is correct and reusable. The next work should not be another seam patch. It should be a direct migration to a Battlefield-owned canonical world, with temporary adapters only where needed to keep the code compiling during the transition.

## Confirmed Findings

### 1. Point Mapping Was Never The Core Failure

`game_map_crossed_world_point()` maps points to the correct screen pixels under the current camera setup. That means the seam bug was not caused by bad point-projection math.

### 2. Sprite Bounds Were Secondary

Using `SPRITE_FRAME_SIZE` was too crude, and the later atlas/frame-bounds work was directionally correct. But even with correct visible bounds, sprites still disappeared. Bounds accuracy did not fix the architectural handoff problem.

### 3. The Render Ownership Model Is Wrong

The current renderer in `src/core/game.c` splits responsibility between:
- owner viewport pass
- opponent crossed-entity pass
- seam RT composite pass

That creates states where a crossed sprite can fall out of every draw path. Any solution that keeps this ownership model remains fragile.

### 4. Exact Pixel Continuity Across The Screen Center Is The Wrong Goal

With `+90` and `-90` rotated cameras and sprites drawn unrotated in `sprite_draw()`, the same sprite seen by both viewports is effectively observed through opposite camera rotations. So the exact center column cannot be owned identically by both views and still remain pixel-perfect. The correct goal is two correct independent views from one world, not one artifact-free shared seam image.

## Primary Recommendation

Execute the remainder of Phase 11 as a full canonical-world rewrite:

1. Introduce `Battlefield` / `Territory` and make Battlefield the authoritative owner of board geometry.
2. Migrate spawn, pathfinding, combat, and entity storage to canonical coordinates.
3. Rewrite rendering to draw canonical entities directly in both viewports.
4. Delete remap helpers, seam RT, and all seam-bleed special cases.
5. Remove temporary Player adapters and lock the final invariants with tests/assertions.

## Updated Phase Structure

### Plan 11-02
Introduce `Battlefield`, canonical territory geometry, tilemaps, slot anchors, lane paths, and the transition adapter boundary.

### Plan 11-03
Migrate simulation ownership:
- canonical spawn positions
- Battlefield entity registry
- Battlefield pathing
- direct-distance combat

### Plan 11-04
Migrate rendering ownership:
- canonical entity drawing
- Battlefield territory tilemaps
- delete crossed remap and seam RT

### Plan 11-05
Cleanup and hardening:
- remove Player adapter fields
- add `BF_ASSERT_IN_BOUNDS` at key mutation/query points
- update tests/manual validation for final architecture

## Architectural Patterns To Keep

### Typed Coordinate Wrappers
Use `CanonicalPos` and `SideLocalPos` for new Battlefield helpers where practical. They provide real friction against coordinate mixing in C.

### Side-Owned Terrain, Not Player-Owned Terrain
The board should preserve "P1 grass / P2 undead" as "bottom territory grass / top territory undead". Terrain ownership must be spatial, not based on `player.id`.

### Single Authoritative Entity Registry
Simulation and rendering should both read the same Battlefield entity list. If temporary per-owner caches remain during migration, they must be clearly labeled derived caches.

## Main Risks

### Mixed Coordinate Systems During Transition
This is the biggest risk. If spawn is canonical while combat or rendering still assumes per-player local space, behavior will become inconsistent immediately.

### Struct Drift In Tests
`tests/test_pathfinding.c` and `tests/test_combat.c` mirror production layouts. They must be updated in lockstep with `types.h`.

### Camera Target Drift
Once `Player.playArea` stops being authoritative, camera target setup must derive from Battlefield territory bounds instead.

### Adapter Fields Lingering Too Long
Temporary Player fields are acceptable only as a migration bridge. They must not quietly become permanent second sources of truth.

## Validation Strategy

Automated:
- `make test`
- `make`
- search-based checks for deleted remap code in later plans

Manual:
- troop crosses from one territory into the other without disappearing
- each viewport remains correctly oriented for its player
- both territory biomes render correctly from Battlefield-owned tilemaps

## Final Conclusion

The canonical Battlefield rewrite is the right architectural direction and the only credible way to solve the seam disappearance correctly. The tactical seam-fix track should be treated as disposable investigation, not as a required deliverable for Phase 11.

---

*Phase: 11-canonical-single-world-space-refactor*
*Research updated: 2026-04-01*
