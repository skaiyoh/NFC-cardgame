# Phase 11: Canonical Single-World-Space Refactor - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-01
**Phase:** 11-canonical-single-world-space-refactor
**Areas discussed:** Canonical board dimensions, Migration order & seam fix, Battlefield struct design, Adapter boundary pattern

---

## Canonical Board Dimensions

| Option | Description | Selected |
|--------|-------------|----------|
| Top-left of P1 territory | Origin at P1's spawn end (top-left). Matches how P1's camera currently works. | ✓ |
| Seam-centered (0,0 at seam) | Symmetric math but requires negative coordinates throughout P1's half. | |
| Bottom-left (screen convention) | Y-up standard in game math but breaks Raylib's Y-down convention. | |

**User's choice:** Top-left of full shared battlefield (0,0). P1 on bottom half so forward motion trends toward decreasing y.
**Notes:** User provided detailed rationale: codebase already assumes top-left rectangle origins (player.c, tilemap_renderer), cameras stay as view-only concerns.

| Option | Description | Selected |
|--------|-------------|----------|
| Single 1080-wide board | Both players share one 1080px-wide strip. No X overlap. | ✓ |
| Keep current widths, fix overlap | Keep 1080 wide per territory but shift to avoid overlap. | |

**User's choice:** Single 1080-wide column. Current overlap is an artifact of dual-local-space trick, not real battlefield geometry.

| Option | Description | Selected |
|--------|-------------|----------|
| 1920 total (960 per side) | Matches current per-player territory depth. | ✓ |
| Match current screen dims exactly | Derive from SCREEN_WIDTH/HEIGHT. | |

**User's choice:** 1080 x 1920, seamY = 960. Preserves spawn depth, waypoint spacing, and seam approach timing.

| Option | Description | Selected |
|--------|-------------|----------|
| P2 spawns at mirrored X | P2's left lane spawns at board's right side. Camera rotation handles visual flip. | ✓ (detailed) |
| P2 spawns at same X, camera mirrors | Both spawn same X, camera handles flip. | |

**User's choice:** Mirror happens exactly once at local-to-canonical boundary. Canonical lanes: 0=world left, 1=center, 2=world right. P2 slot mapping: 0→2, 1→1, 2→0.

---

## Migration Order & Seam Fix

| Option | Description | Selected |
|--------|-------------|----------|
| Quick fix first, then refactor | Ship seam fix independently on current codebase. Two deliverables. | ✓ |
| All together in one refactor | Seam fix baked into canonical migration. Cleaner but riskier. | |

**User's choice:** Quick fix first — seam is fixed even if the big refactor stalls.

| Option | Description | Selected |
|--------|-------------|----------|
| 5 separate plans (1 per sub-phase) | Each sub-phase its own PLAN.md. Most granular. | ✓ |
| 3 plans (merge related sub-phases) | Fewer plans, bigger waves. | |

**User's choice:** 5 separate plans, 1 per sub-phase. Independent verification and rollback per sub-phase.

---

## Battlefield Struct Design

| Option | Description | Selected |
|--------|-------------|----------|
| Two biome halves on one board | Single tilemap spanning full board, two biome regions. | |
| Battlefield owns two tilemaps | tilemap[2], one per territory half. | ✓ (detailed) |
| Single biome per match | Simplify to one biome. | |

**User's choice:** Battlefield owns `territories[TOP]` and `territories[BOTTOM]`, each with its own bounds, BiomeType, BiomeDef, TileMap. Preserves per-player biome choice. Biome assignment attaches to BattleSide, not player.id.

| Option | Description | Selected |
|--------|-------------|----------|
| Battlefield: geometry + terrain + entities | Battlefield owns everything world-related including entity arrays. | ✓ (detailed) |
| Battlefield: geometry + terrain only | Entities stay on Player. Smaller refactor. | |

**User's choice:** Battlefield owns authoritative world: geometry, terrain, lane geometry, AND entity registry. Player keeps: id, screenArea, camera, cameraRotation, slot state (minus worldPos), energy. Player gains BattleSide side field.

---

## Adapter Boundary Pattern

| Option | Description | Selected |
|--------|-------------|----------|
| Module-at-a-time migration | Migrate one module fully before touching next. | |
| Thin conversion layer on Battlefield | bf_to_local/bf_from_local helpers during transition. | |
| Entity carries coordinate flag | bool canonical flag on Entity. | |
| You decide | Let Claude pick safest pattern. | ✓ |

**User's choice:** Claude decides adapter pattern.

| Option | Description | Selected |
|--------|-------------|----------|
| Debug assertions on coordinate ranges | Assert positions within canonical range in debug builds. | ✓ (detailed) |
| No special assertions | Trust testing. | |

**User's choice:** Yes to runtime assertions AND compile-time type separation. Introduced `CanonicalPos` and `SideLocalPos` typed wrappers for new Battlefield APIs. Centralize transforms in one module, ban ad hoc formulas.

---

## Claude's Discretion

- Adapter boundary pattern during transition (module-at-a-time vs conversion layer)
- Internal struct layout of Battlefield and Territory types
- BattleSide naming convention
- Debug assertion macro style
- battlefield_math module format (header-only vs .h/.c pair)

## Deferred Ideas

None — discussion stayed within phase scope

---

## Superseding Findings From Seam Investigation

**Date:** 2026-04-01
**Status:** supersedes the earlier "quick fix first" preference for Phase 11 planning

### What changed

- Point mapping across the seam was verified as screen-correct.
- Sprite frame bounds and atlas metadata were improved, but did not solve the disappearing seam bug.
- The failure remained in render-path ownership: a crossed sprite could fall between the owner pass, crossed pass, and seam-composite pass.
- The two opposite rotated cameras (`+90` / `-90`) mean the exact center seam cannot be a shared pixel-perfect image owned equally by both views.

### Updated conclusion

| Option | Description | Selected |
|--------|-------------|----------|
| Continue iterating on dual-space seam patches | Keep refining bleed / seam RT / atlas bounds in current architecture. | |
| Full canonical rewrite only | Treat seam investigations as disposable; solve disappearance by deleting cross-space handoff logic. | ✓ |

**Decision:** Phase 11 should be planned and completed as a full canonical Battlefield rewrite. Tactical seam-fix experiments are no longer a required deliverable.
