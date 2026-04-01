# Phase 1: Lane Pathfinding - Research

**Researched:** 2026-03-28
**Domain:** 2D waypoint pathfinding in C with Raylib, lane-based troop movement
**Confidence:** HIGH

## Summary

Phase 1 replaces the existing straight-line `position.y -= moveSpeed * dt` movement in `entity_update()` with per-lane waypoint following. The center lane is a straight path, while left and right lanes bow outward in a parenthesis `( | )` shape -- widest at the midpoint, tapering at both ends. Movement is linear interpolation between ~8 waypoints per lane at constant speed, with no spline easing required.

The codebase already provides every building block needed: `player_lane_pos()` computes lane X/Y from lane index and depth, `entity_set_state()` handles animation transitions, `AnimState` tracks direction and `flipH`, and `Entity.lane` exists but is only set in `spawn_troop_from_card()`. The implementation adds waypoint arrays to the Player struct (pre-computed at init time), a `waypointIndex` tracker to Entity, and replaces the ESTATE_WALKING case in `entity_update()` with waypoint-stepping logic.

The coordinate system uses a rotated split-screen layout where each player's playArea is 1080x960 world units. Depth 0.0 (base) maps to Y=864 and depth 1.0 (front) maps to Y=96, meaning troops walk in the -Y direction through their own area and into the opponent's space. The existing `game_draw_entities_for_viewport()` already handles mirroring entities that cross the border into the opponent's viewport.

**Primary recommendation:** Pre-compute waypoint arrays per-lane in `player_init()`, store on Player, and have `entity_update()` step through them using the entity's `waypointIndex` field. Use a sine-based bow function for the outer lanes.

<user_constraints>

## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** Center lane is perfectly straight. Left and right lanes bow outward in a parenthesis `( | )` shape -- widest at the midpoint, tapering back at both spawn and enemy end.
- **D-02:** Outer lanes bow ~50% wider at midpoint compared to their spawn offset from center (moderate spread).
- **D-03:** All 3 lanes are symmetric -- outer lanes mirror each other, only differing in X offset direction.
- **D-04:** ~8 waypoints per lane, evenly spaced along depth. Waypoint count and bow intensity defined as `#defines` in `config.h` for easy tuning.
- **D-05:** Reuse `player_lane_pos()` as the base calculation, adding a depth-dependent X spread factor (sine or quadratic curve peaking at depth 0.5).
- **D-06:** Linear segments between waypoints at constant speed. No spline interpolation or easing -- with 8 waypoints the curve appears smooth.
- **D-07:** Sprites face their movement direction using the 3-direction sprite system (`DIR_UP`, `DIR_SIDE`, `DIR_DOWN`). Direction determined by the dominant component of the movement vector toward the next waypoint.
- **D-08:** Left lane example: `DIR_SIDE` (facing left) at spawn -> `DIR_UP` near midpoint -> `DIR_SIDE` (facing right) on enemy side. Right lane mirrors this. Center lane stays `DIR_UP`.
- **D-09:** `flipH` on `AnimState` handles left vs right for `DIR_SIDE`.
- **D-10:** Troops transition to `ESTATE_IDLE` when reaching the last waypoint (replacing the current off-screen despawn).
- **D-11:** Random jitter applied around the endpoint so troops cluster loosely rather than stacking on one pixel.
- **D-12:** `entity->lane` set from slot index at spawn time: slot 0 = left, slot 1 = center, slot 2 = right. Direct 1:1 mapping with NFC reader position.
- **D-13:** Colored line overlay drawing waypoint paths per lane, togglable with a debug key (e.g., F1). For development tuning only.

### Claude's Discretion
- Number of depth samples (around 8, Claude can adjust for smoothness)
- Exact curve function (sine, quadratic, etc.) for the bow shape -- whatever produces the cleanest `( | )` visual
- Jitter radius for end-of-path clustering
- Which debug key to use for the overlay toggle

### Deferred Ideas (OUT OF SCOPE)
None -- discussion stayed within phase scope

</user_constraints>

<phase_requirements>

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| CORE-01 | Troops move along per-lane waypoints toward the enemy base (not raw straight-line Y movement) | Waypoint array generation using `player_lane_pos()` + bow function; `entity_update()` ESTATE_WALKING replacement with waypoint stepping; sprite direction from movement vector; end-of-path idle transition |

</phase_requirements>

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Raylib | bundled (lib/) | 2D rendering, input, math types (Vector2, Rectangle) | Already used throughout the project; provides DrawLineV, Vector2 math |
| C standard library | C11 | math.h (sinf, fabsf, sqrtf), stdlib.h (rand) | Already in use; no external dependencies needed |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| raymath.h | bundled with Raylib | Vector2Add, Vector2Subtract, Vector2Normalize, Vector2Distance, Vector2Scale | For waypoint movement math -- already available via Raylib |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| sinf() bow curve | Quadratic `4*t*(1-t)` | Quadratic peaks sharper; sine has smoother shoulders. Both produce valid `( | )` shape. Sine recommended for smoother visual. |
| Pre-computed waypoint arrays | Runtime curve evaluation per frame | Runtime is wasteful -- same curve every frame. Pre-compute once at player init. |

**Installation:**
No new dependencies. All needed functions exist in Raylib's bundled headers and C standard math.

## Architecture Patterns

### Recommended Project Structure
```
src/
  core/
    config.h          # Add LANE_WAYPOINT_COUNT, LANE_BOW_INTENSITY defines
    types.h           # Add waypointIndex to Entity, lane waypoint arrays to Player
  logic/
    pathfinding.h     # Declare lane_generate_waypoints(), pathfind_step_waypoint()
    pathfinding.c     # Implement waypoint generation and stepping logic
  entities/
    entities.c        # Replace ESTATE_WALKING case with waypoint following
  systems/
    player.c          # Call lane_generate_waypoints() during player_init()
  rendering/
    viewport.c        # Add debug_draw_lane_paths() for F1 overlay
```

### Pattern 1: Pre-computed Waypoint Arrays
**What:** Generate all lane waypoints once at player initialization, store as `Vector2` arrays on the Player struct.
**When to use:** Always -- waypoints are deterministic per-player and never change during a match.
**Example:**
```c
// In config.h
#define LANE_WAYPOINT_COUNT  8
#define LANE_BOW_INTENSITY   0.5f  // 50% of lane offset at midpoint

// In types.h -- add to Player struct
Vector2 laneWaypoints[3][LANE_WAYPOINT_COUNT]; // [lane][waypointIndex]

// In types.h -- add to Entity struct
int waypointIndex;  // Current target waypoint along lane path

// In pathfinding.h
void lane_generate_waypoints(Player *p);
```

### Pattern 2: Sine-based Bow Function
**What:** Apply a depth-dependent lateral offset to outer lanes using `sinf(depth * PI)`, producing a smooth parenthesis curve that peaks at midpoint and tapers to zero at both ends.
**When to use:** For generating the X offset of left/right lane waypoints.
**Why sine:** `sin(t * PI)` for t in [0,1] produces values [0, 1, 0] with smooth zero-derivative endpoints, giving the cleanest `( | )` visual. A quadratic `4*t*(1-t)` also peaks at 0.5 but has sharper shoulders. Sine is recommended.
**Example:**
```c
// bow_offset computes lateral displacement for a given depth
// depth: 0.0 = spawn end, 1.0 = enemy end
// Returns: signed offset in world units (negative = left, positive = right)
static float bow_offset(int lane, float depth, float laneWidth) {
    if (lane == 1) return 0.0f;  // Center lane: no bow

    // sin(depth * PI) peaks at depth=0.5, zero at both ends
    float bow = sinf(depth * 3.14159265f) * LANE_BOW_INTENSITY * laneWidth;

    // Lane 0 (left) bows left (negative X), lane 2 (right) bows right (positive X)
    return (lane == 0) ? -bow : bow;
}

void lane_generate_waypoints(Player *p) {
    float laneWidth = p->playArea.width / 3.0f;

    for (int lane = 0; lane < 3; lane++) {
        for (int i = 0; i < LANE_WAYPOINT_COUNT; i++) {
            // Evenly space depth samples from spawn (depth ~0.1) to front (depth ~0.9)
            // Avoid exact 0.0 and 1.0 to stay within playable area
            float depth = (float)(i + 1) / (float)(LANE_WAYPOINT_COUNT + 1);

            // Base position from existing lane calculation
            Vector2 pos = player_lane_pos(p, lane, depth);

            // Add bow offset
            pos.x += bow_offset(lane, depth, laneWidth);

            p->laneWaypoints[lane][i] = pos;
        }
    }
}
```

### Pattern 3: Waypoint Following in entity_update
**What:** Replace the ESTATE_WALKING case with logic that moves toward the current target waypoint, advancing to the next when reached.
**When to use:** Every frame for walking entities.
**Example:**
```c
case ESTATE_WALKING: {
    Player *owner = &gs->players[e->ownerID];

    // Safety: validate lane and waypoint index
    if (e->lane < 0 || e->lane >= 3) break;
    if (e->waypointIndex >= LANE_WAYPOINT_COUNT) {
        // Reached end of path -- idle with jitter
        entity_set_state(e, ESTATE_IDLE);
        break;
    }

    Vector2 target = owner->laneWaypoints[e->lane][e->waypointIndex];
    Vector2 diff = Vector2Subtract(target, e->position);
    float dist = Vector2Length(diff);
    float step = e->moveSpeed * deltaTime;

    if (dist <= step) {
        // Reached waypoint -- snap and advance
        e->position = target;
        e->waypointIndex++;

        if (e->waypointIndex >= LANE_WAYPOINT_COUNT) {
            // Apply end-of-path jitter
            float jx = ((float)(rand() % 20) - 10.0f);
            float jy = ((float)(rand() % 20) - 10.0f);
            e->position.x += jx;
            e->position.y += jy;
            entity_set_state(e, ESTATE_IDLE);
        }
    } else {
        // Move toward waypoint
        Vector2 dir = Vector2Scale(diff, 1.0f / dist);
        e->position = Vector2Add(e->position, Vector2Scale(dir, step));
    }

    // Update sprite direction based on movement vector
    // ... (see Pattern 4)
    break;
}
```

### Pattern 4: Sprite Direction from Movement Vector
**What:** Determine `DIR_UP`, `DIR_SIDE`, or `DIR_DOWN` from the movement vector's dominant axis component. Set `flipH` to distinguish left from right.
**When to use:** After computing the movement direction each frame.
**Example:**
```c
// Given diff = target - position (the movement vector)
float ax = fabsf(diff.x);
float ay = fabsf(diff.y);

if (ax > ay) {
    // Dominant horizontal movement
    e->anim.dir = DIR_SIDE;
    e->anim.flipH = (diff.x < 0);  // true = facing left
} else {
    // Dominant vertical movement
    e->anim.dir = (diff.y < 0) ? DIR_UP : DIR_DOWN;
    e->anim.flipH = false;
}
```

**Why this works for the parenthesis shape:**
- Left lane at spawn: troops move left and forward -> `DIR_SIDE` with `flipH=true` (facing left)
- Left lane near midpoint: vertical component dominates -> `DIR_UP`
- Left lane on enemy side: troops curve back inward (right) -> `DIR_SIDE` with `flipH=false` (facing right)
- Right lane mirrors exactly with opposite flipH values
- Center lane: always vertical movement -> `DIR_UP`

This matches the user's description: "when they first spawn on the left lane, they will face left, once near the middle they will face forward, then when they are on the enemy side walking inward, they will face right."

### Anti-Patterns to Avoid
- **Runtime curve computation every frame:** The waypoints are deterministic. Computing them once at init and storing them is strictly better than recomputing sinf() every frame for every entity.
- **Modifying `player_lane_pos()` directly:** The existing function is a clean utility used elsewhere. Wrap it or call it during waypoint generation rather than adding bow logic into it, so the original behavior is preserved for other callers.
- **Storing waypoints on Entity:** Waypoints are per-lane, not per-entity. Store on Player (3 lanes x N waypoints). Entities just need a `waypointIndex` to track progress.
- **Using the old despawnY check:** The old `despawnY = playArea.y - playArea.height * 0.9` logic should be fully removed, replaced by the waypoint-end idle transition.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Vector math (normalize, scale, distance) | Custom sqrt/normalize | Raylib's `Vector2Normalize`, `Vector2Length`, `Vector2Scale`, `Vector2Subtract`, `Vector2Add` from raymath.h | Tested, handles edge cases (zero-length vectors) |
| Trigonometric bow curve | Lookup table or polynomial approximation | `sinf()` from math.h | Standard library sinf is fast enough at 8 waypoints generated once at init |
| Line drawing for debug overlay | Custom line rasterization | Raylib `DrawLineV()`, `DrawCircleV()` | Built-in, handles camera transforms |

**Key insight:** This phase requires no new libraries. Everything needed (vector math, trig, line drawing, input detection) is already provided by Raylib and the C standard library.

## Common Pitfalls

### Pitfall 1: Coordinate System Direction Confusion
**What goes wrong:** Troops move in the wrong direction or the bow curves inward instead of outward because the Y-axis direction was misunderstood.
**Why it happens:** In this project, depth 0.0 = base (high Y ~864) and depth 1.0 = front (low Y ~96). Troops walk from high Y to low Y. The play area Y-axis is inverted relative to "forward."
**How to avoid:** Always use `player_lane_pos()` to convert depth to Y position. Never assume Y+ is "forward." The existing function handles the mapping: `y = playArea.y + playArea.height * (0.9 - depth * 0.8)`.
**Warning signs:** Troops walking backward, bow curving to the center instead of outward.

### Pitfall 2: Waypoint Index Not Initialized
**What goes wrong:** Entities start following waypoints from an undefined or zero index, causing them to teleport or skip waypoints.
**Why it happens:** `entity_create()` uses `memset(e, 0, sizeof(Entity))`, so `waypointIndex` will be 0. But the first waypoint (index 0) is at depth ~0.125, not at the spawn position (depth ~0.2 = 80% of play area height). If spawn position does not match waypoint 0, the entity may teleport or walk backward to reach it.
**How to avoid:** Either (a) make waypoint 0 match the spawn position exactly, or (b) find the nearest waypoint to spawn position and set `waypointIndex` accordingly. Option (a) is simpler: set the first waypoint at the spawn depth.
**Warning signs:** Troops snapping to a different position on spawn, brief backward movement.

### Pitfall 3: Spawn Position vs Waypoint Mismatch
**What goes wrong:** `player_init_card_slots()` sets spawn position at `playArea.y + playArea.height * 0.8` (depth ~0.125). `player_lane_pos()` uses a different X formula than the slot positions. If the first waypoint does not match the slot spawn position, troops will jerk sideways or backward on spawn.
**Why it happens:** Slot X uses `playArea.x + (slot + 0.5) * laneWidth` which matches `player_lane_pos(p, lane, 0.0)` x-component, but the Y values differ: slots use 0.8 while lane_pos depth 0.0 maps to 0.9.
**How to avoid:** Set the first waypoint position to exactly match the spawn slot position. Use the slot's worldPos as waypoint[0] directly, or compute waypoint depth 0 to match spawn depth.
**Warning signs:** Troops moving backward or sideways before heading toward the enemy.

### Pitfall 4: Border Crossing and Viewport Mirroring
**What goes wrong:** The existing `game_draw_entities_for_viewport()` mirrors entities that cross `owner->playArea.y` into the opponent viewport. The old code forced `DIR_DOWN` for crossed entities. With waypoint-based direction, this override must still work correctly.
**Why it happens:** The mirroring code creates a separate `AnimState crossed` copy and sets `crossed.dir = DIR_DOWN`. This is correct behavior -- entities in the opponent's viewport should face downward toward the opponent. The waypoint direction update only affects the entity's own anim state, not the crossed copy.
**How to avoid:** Do not remove or modify the `crossed.dir = DIR_DOWN` override in `game_draw_entities_for_viewport()`. The waypoint direction logic in `entity_update()` and the viewport mirroring are independent systems that coexist correctly.
**Warning signs:** Entities facing sideways in the opponent's viewport when they should face down.

### Pitfall 5: End-of-Path Jitter Using Global rand()
**What goes wrong:** Using `rand()` for jitter contaminates the global PRNG state. An existing TODO in the codebase notes that `srand()` is called during tilemap generation with hardcoded seeds.
**Why it happens:** C's `rand()` is global state. Multiple subsystems calling it creates unpredictable sequences.
**How to avoid:** For this phase, `rand()` is acceptable for small jitter values -- the visual result is non-deterministic by design and does not affect gameplay. But use a small, bounded range (e.g., -10 to +10 pixels). The existing codebase already uses `rand()` elsewhere, so this is consistent.
**Warning signs:** None for this phase -- jitter is purely cosmetic.

### Pitfall 6: Debug Overlay Drawn Outside Camera Transform
**What goes wrong:** Debug lines appear at wrong positions or not at all because they are drawn outside of `BeginMode2D`/`EndMode2D`.
**Why it happens:** The viewport system uses `viewport_begin()` (which calls `BeginScissorMode` + `BeginMode2D`) and `viewport_end()`. Debug drawing must happen inside this bracket to use world coordinates.
**How to avoid:** Draw debug overlay inside `game_render()` between `viewport_begin()` and `viewport_end()`, just after `game_draw_entities_for_viewport()`.
**Warning signs:** Debug lines visible at screen coordinates instead of world coordinates, or not visible at all.

## Code Examples

### Coordinate System Reference (from existing code)

```c
// Player 1 playArea: x=0, y=0, width=1080, height=960
// Player 2 playArea: x=960, y=0, width=1080, height=960
// (viewport.c:viewport_init_split_screen)

// Lane geometry (player.c:player_lane_pos):
//   laneWidth = playArea.width / 3.0 = 360.0
//   Lane 0 center X = playArea.x + 0.5 * 360 = 180
//   Lane 1 center X = playArea.x + 1.5 * 360 = 540
//   Lane 2 center X = playArea.x + 2.5 * 360 = 900
//
//   depth 0.0 (base): Y = playArea.y + height * 0.9 = 864
//   depth 0.5 (mid):  Y = playArea.y + height * 0.5 = 480
//   depth 1.0 (front): Y = playArea.y + height * 0.1 = 96

// Spawn position (player.c:player_init_card_slots):
//   spawnY = playArea.y + playArea.height * 0.8 = 768
//   Corresponds to depth ~0.125 via inverse of lane_pos formula
```

### Waypoint Generation (recommended implementation)

```c
// Source: derived from existing player_lane_pos() + decisions D-01 through D-05

#include <math.h>  // sinf

// config.h additions:
#define LANE_WAYPOINT_COUNT  8
#define LANE_BOW_INTENSITY   0.5f
#define LANE_JITTER_RADIUS   10.0f
#define PI_F 3.14159265f

// Generate waypoints for all 3 lanes for this player
void lane_generate_waypoints(Player *p) {
    float laneWidth = p->playArea.width / 3.0f;

    for (int lane = 0; lane < 3; lane++) {
        for (int i = 0; i < LANE_WAYPOINT_COUNT; i++) {
            // Map waypoint index to depth:
            //   i=0 -> near spawn (depth ~0.1)
            //   i=LANE_WAYPOINT_COUNT-1 -> near enemy (depth ~0.9)
            float depth = 0.1f + (0.8f * (float)i / (float)(LANE_WAYPOINT_COUNT - 1));

            // Get base lane position
            Vector2 pos = player_lane_pos(p, lane, depth);

            // Add bow offset for outer lanes
            if (lane != 1) {
                float bow = sinf(depth * PI_F) * LANE_BOW_INTENSITY * laneWidth;
                pos.x += (lane == 0) ? -bow : bow;
            }

            p->laneWaypoints[lane][i] = pos;
        }
    }
}
```

### Debug Overlay Drawing

```c
// Source: uses Raylib DrawLineV and DrawCircleV (raylib.h)

// Colors per lane for visual distinction
static const Color LANE_COLORS[3] = { BLUE, GREEN, RED };

void debug_draw_lane_paths(const Player *p) {
    for (int lane = 0; lane < 3; lane++) {
        Color c = LANE_COLORS[lane];

        for (int i = 0; i < LANE_WAYPOINT_COUNT; i++) {
            // Draw waypoint dot
            DrawCircleV(p->laneWaypoints[lane][i], 4.0f, c);

            // Draw line segment to next waypoint
            if (i < LANE_WAYPOINT_COUNT - 1) {
                DrawLineV(p->laneWaypoints[lane][i],
                          p->laneWaypoints[lane][i + 1], c);
            }
        }
    }
}
```

### Entity Spawn Integration Point

```c
// Source: card_effects.c:spawn_troop_from_card (line 55-86)
// Already sets e->lane = slotIndex (line 83)
// Entity waypointIndex will be 0 after memset in entity_create
// No changes needed to spawn_troop_from_card for this phase
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `e->position.y -= moveSpeed * dt` (straight line) | Waypoint-following through pre-computed lane paths | This phase | Troops follow curved lane paths, visual direction changes, end-of-path idle |
| `e->anim.dir = (y > border) ? DIR_UP : DIR_DOWN` (border-based) | Direction from movement vector dominant axis | This phase | Troops face their travel direction naturally through curves |
| `markedForRemoval` when past despawnY | `entity_set_state(e, ESTATE_IDLE)` at last waypoint | This phase | Troops stop at enemy side instead of despawning |

**Deprecated/outdated:**
- The `pathfind_next_step(current, goal, gs)` function signature in the existing stub is too generic for lane-based waypoints. It will be replaced with the waypoint-index approach that does not need a goal parameter -- the goal is implicit in the waypoint array.

## Open Questions

1. **First waypoint vs spawn position alignment**
   - What we know: Spawn Y is at `playArea.height * 0.8` (Y=768). The first waypoint at depth=0.1 maps to Y=`playArea.y + height * (0.9 - 0.1*0.8)` = Y=`960 * 0.82` = Y=787.
   - What's unclear: Whether the ~19px gap between spawn (768) and first waypoint (787) will cause a visible backward snap.
   - Recommendation: Set the first waypoint to match the spawn slot position exactly. Either use depth=0.125 for waypoint 0 (which maps to Y=768), or directly use `p->slots[lane].worldPos` as waypoint[0]. The latter is cleanest.

2. **Waypoint end position relative to future enemy base**
   - What we know: Phase 2 will place an enemy base. D-10 says troops idle at the last waypoint. The last waypoint at depth=0.9 maps to Y=`960 * 0.18` = Y=173.
   - What's unclear: Whether Y=173 is the right idle position relative to where the base will eventually be placed (Phase 2 will use `player_front_pos` which returns Y=96).
   - Recommendation: Use depth=0.85 for the last waypoint (Y=`960 * 0.22` = Y=211), leaving room for the base. This can be tuned later via the `config.h` defines.

3. **Interaction with viewport mirroring for waypoint-following entities**
   - What we know: `game_draw_entities_for_viewport()` mirrors entities crossing `playArea.y` (Y=0 for P1). The mirroring uses the entity's raw position and overrides direction to `DIR_DOWN`.
   - What's unclear: Whether entities following waypoints past the border (depth > ~1.0) will mirror correctly. The waypoint end (depth 0.9, Y=173 for P1) is well above Y=0, so entities should NOT cross the border during normal pathfinding.
   - Recommendation: No changes to mirroring needed for Phase 1. Entities idle before reaching the border. This is a non-issue unless waypoint depth extends past 1.0.

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | None detected -- C project with no test infrastructure |
| Config file | none -- see Wave 0 |
| Quick run command | `make cardgame && ./cardgame` (manual visual verification) |
| Full suite command | N/A -- no automated tests exist |

### Phase Requirements -> Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| CORE-01a | Waypoint generation produces correct positions for all 3 lanes | unit | `make test && ./test_pathfinding` | Wave 0 |
| CORE-01b | Center lane waypoints have no lateral offset | unit | `make test && ./test_pathfinding` | Wave 0 |
| CORE-01c | Outer lanes bow outward symmetrically with sine curve | unit | `make test && ./test_pathfinding` | Wave 0 |
| CORE-01d | Entity advances through waypoints at moveSpeed | unit | `make test && ./test_pathfinding` | Wave 0 |
| CORE-01e | Entity transitions to ESTATE_IDLE at last waypoint | unit | `make test && ./test_pathfinding` | Wave 0 |
| CORE-01f | Sprite direction matches movement vector dominant axis | unit | `make test && ./test_pathfinding` | Wave 0 |
| CORE-01g | Troop visibly follows curved path (left/right lanes) | manual-only | Visual inspection with debug overlay (F1) | N/A |
| CORE-01h | Troops in different lanes follow different paths | manual-only | Visual inspection spawning troops in all 3 lanes | N/A |

### Sampling Rate
- **Per task commit:** `make cardgame` (build succeeds)
- **Per wave merge:** `make test && ./test_pathfinding` (unit tests green) + visual spawn verification
- **Phase gate:** All unit tests green, visual verification of all 3 success criteria

### Wave 0 Gaps
- [ ] `tests/test_pathfinding.c` -- unit tests for waypoint generation and entity stepping (CORE-01a through CORE-01f)
- [ ] `Makefile` test target -- `make test` that compiles and runs test_pathfinding without Raylib window (pure logic tests)
- [ ] Test harness -- minimal `assert`-based C test file; no external test framework needed for a C project of this size. Use a simple `main()` that runs test functions and reports pass/fail.

**Note:** Raylib-dependent behavior (actual rendering, debug overlay) requires manual visual verification. Unit tests cover the pure math: waypoint positions, movement stepping, direction computation, state transitions.

## Sources

### Primary (HIGH confidence)
- `src/systems/player.c` lines 237-245 -- `player_lane_pos()` implementation, coordinate formula
- `src/entities/entities.c` lines 67-113 -- `entity_update()` current ESTATE_WALKING implementation
- `src/core/types.h` lines 29-57 -- Entity struct with lane field, Vector2 position
- `src/core/config.h` -- existing defines structure for gameplay tuning
- `src/rendering/sprite_renderer.h` lines 22-26 -- `SpriteDirection` enum, `AnimState` with flipH
- `src/rendering/viewport.c` lines 9-54 -- play area dimensions (1080x960), camera setup
- `src/logic/card_effects.c` lines 55-86 -- `spawn_troop_from_card()` sets `e->lane = slotIndex`
- `src/core/game.c` lines 126-159 -- `game_draw_entities_for_viewport()` border mirroring logic

### Secondary (MEDIUM confidence)
- C standard library `math.h` -- `sinf()` for bow curve (well-established, no verification needed)
- Raylib raymath.h -- Vector2 operations (confirmed available via bundled Raylib headers in lib/)

### Tertiary (LOW confidence)
- None -- all findings verified from source code

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH -- no new libraries needed, all tools already in project
- Architecture: HIGH -- all integration points verified in source code, coordinate math computed from actual values
- Pitfalls: HIGH -- derived from reading actual code paths and identifying concrete conflicts
- Validation: MEDIUM -- no existing test infrastructure; test plan is proposed but unverified

**Research date:** 2026-03-28
**Valid until:** Indefinite -- this is a C codebase with no external dependency version concerns
