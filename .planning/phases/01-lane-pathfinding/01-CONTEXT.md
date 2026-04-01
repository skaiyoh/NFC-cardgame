# Phase 1: Lane Pathfinding - Context

**Gathered:** 2026-03-28
**Status:** Ready for planning

<domain>
## Phase Boundary

Troops follow per-lane waypoints with curved outer lanes instead of raw straight-line Y movement. Implements CORE-01: troops move along per-lane waypoints toward the enemy base.

This phase replaces the current `e->position.y -= moveSpeed * dt` in `entity_update()` with waypoint-based pathfinding through `pathfind_next_step()`. It does NOT include combat, base creation, or any interaction between troops.

</domain>

<decisions>
## Implementation Decisions

### Curve Shape & Spread
- **D-01:** Center lane is perfectly straight. Left and right lanes bow outward in a parenthesis `( | )` shape — widest at the midpoint, tapering back at both spawn and enemy end.
- **D-02:** Outer lanes bow ~50% wider at midpoint compared to their spawn offset from center (moderate spread).
- **D-03:** All 3 lanes are symmetric — outer lanes mirror each other, only differing in X offset direction.

### Waypoint Configuration
- **D-04:** ~8 waypoints per lane, evenly spaced along depth. Waypoint count and bow intensity defined as `#defines` in `config.h` for easy tuning.
- **D-05:** Reuse `player_lane_pos()` as the base calculation, adding a depth-dependent X spread factor (sine or quadratic curve peaking at depth 0.5).

### Movement
- **D-06:** Linear segments between waypoints at constant speed. No spline interpolation or easing — with 8 waypoints the curve appears smooth.

### Sprite Direction
- **D-07:** Sprites face their movement direction using the 3-direction sprite system (`DIR_UP`, `DIR_SIDE`, `DIR_DOWN`). Direction determined by the dominant component of the movement vector toward the next waypoint.
- **D-08:** Left lane example: `DIR_SIDE` (facing left) at spawn → `DIR_UP` near midpoint → `DIR_SIDE` (facing right) on enemy side. Right lane mirrors this. Center lane stays `DIR_UP`.
- **D-09:** `flipH` on `AnimState` handles left vs right for `DIR_SIDE`.

### End-of-Path Behavior
- **D-10:** Troops transition to `ESTATE_IDLE` when reaching the last waypoint (replacing the current off-screen despawn).
- **D-11:** Random jitter applied around the endpoint so troops cluster loosely rather than stacking on one pixel.

### Lane Assignment
- **D-12:** `entity->lane` set from slot index at spawn time: slot 0 = left, slot 1 = center, slot 2 = right. Direct 1:1 mapping with NFC reader position.

### Debug Visualization
- **D-13:** Colored line overlay drawing waypoint paths per lane, togglable with a debug key (e.g., F1). For development tuning only.

### Claude's Discretion
- Number of depth samples (around 8, Claude can adjust for smoothness)
- Exact curve function (sine, quadratic, etc.) for the bow shape — whatever produces the cleanest `( | )` visual
- Jitter radius for end-of-path clustering
- Which debug key to use for the overlay toggle

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Pathfinding
- `src/logic/pathfinding.h` / `src/logic/pathfinding.c` — Stub with `pathfind_next_step()` declaration and TODO comments describing expected behavior
- `src/entities/entities.c` — `entity_update()` contains current straight-line movement (line 77-98) that must be replaced

### Lane & Spawn Infrastructure
- `src/systems/player.c` — `player_lane_pos()` (line 237) computes lane X/Y from lane+depth; `player_init_card_slots()` (line 70) defines 3 lanes
- `src/core/types.h` — `Entity` struct with `lane` field (line 52), `EntityState` enum, `Player` struct

### Sprite System
- `src/rendering/sprite_renderer.h` — `SpriteDirection` enum (`DIR_DOWN`, `DIR_SIDE`, `DIR_UP`), `AnimState` with `flipH` field

### Configuration
- `src/core/config.h` — Compile-time constants; new waypoint defines go here

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `player_lane_pos(Player *p, int lane, float depth)` — Already computes lane position from lane index and depth (0.0=base, 1.0=front). Add bow factor to this or wrap it.
- `entity_set_state()` — Handles animation transitions when changing to `ESTATE_IDLE`
- `anim_state_init()` — Resets animation with direction; already supports `DIR_SIDE`

### Established Patterns
- Movement in `entity_update()` switch on `e->state` — new pathfinding logic slots into the `ESTATE_WALKING` case
- Player owns entity data; entity references owner via `ownerID`
- Gameplay tuning via `#define` constants in `config.h`

### Integration Points
- `entity_update()` ESTATE_WALKING case — replace straight-line with waypoint following
- `spawn_troop_from_card()` in `card_effects.c` — set `e->lane = slotIndex`
- `Player` struct — add lane path data (waypoint arrays)
- `Entity` struct — add `waypointIndex` for tracking progress along path

</code_context>

<specifics>
## Specific Ideas

- The curve shape is explicitly a parenthesis `( | )` — widest at midpoint, not at the ends
- User described the left lane direction flow as: "when they first spawn on the left lane, they will face left, once near the middle they will face forward, then when they are on the enemy side walking inward, they will face right"
- Eventually an enemy base will exist at the end of the lane (Phase 2) — idle position should be near where that base will be placed

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-lane-pathfinding*
*Context gathered: 2026-03-28*
