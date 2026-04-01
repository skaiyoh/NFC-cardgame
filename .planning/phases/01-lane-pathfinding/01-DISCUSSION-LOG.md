# Phase 1: Lane Pathfinding - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-03-28
**Phase:** 01-lane-pathfinding
**Areas discussed:** Curve spread & shape, End-of-path stacking, Sprite direction, Movement smoothness, Waypoint count & tuning, Lane assignment at spawn, Debug visualization

---

## Curve Spread & Shape

Initial assumption was outer lanes fanning outward from spawn to enemy base (wider at enemy end). User corrected: the shape is a parenthesis `( | )` — widest at the midpoint, tapering back at both ends.

| Option | Description | Selected |
|--------|-------------|----------|
| Subtle | ~25% bow at midpoint | |
| Moderate | ~50% bow at midpoint | ✓ |
| Dramatic | ~75-100% bow at midpoint | |

**User's choice:** Moderate (~50% wider at midpoint)
**Notes:** User explicitly described the shape: spawn spread out, curve outward to midpoint (widest), then come back in toward enemy side. Like actual parentheses `( | )`.

---

## End-of-Path Stacking

| Option | Description | Selected |
|--------|-------------|----------|
| Stack in place | Troops overlap on same pixel | |
| Offset along lane end | Queue formation along lane | |
| Random jitter | Loose cluster around endpoint | ✓ |

**User's choice:** Random jitter
**Notes:** None

---

## Sprite Direction

| Option | Description | Selected |
|--------|-------------|----------|
| Always face forward | DIR_UP toward enemy, flip at border | |
| Face movement direction | DIR_SIDE/DIR_UP based on curve tangent | ✓ |

**User's choice:** Face movement direction
**Notes:** User confirmed 3-direction system works well. Described left lane flow: "face left at spawn, forward near middle, right on enemy side." Sprite system has DIR_DOWN, DIR_SIDE, DIR_UP with flipH for left/right.

---

## Movement Smoothness

| Option | Description | Selected |
|--------|-------------|----------|
| Linear segments | Constant speed, straight lines between waypoints | ✓ |
| Smooth interpolation | Catmull-Rom spline through waypoints | |
| Linear with easing | Straight lines but slow at turn points | |

**User's choice:** Linear segments (Recommended)
**Notes:** None

---

## Waypoint Count & Tuning

| Option | Description | Selected |
|--------|-------------|----------|
| ~8 waypoints, config defines | Tweakable via config.h | ✓ |
| ~5 waypoints, hardcoded | Simpler, baked in | |
| ~12 waypoints, data-driven | Dense, future-proof | |

**User's choice:** ~8 waypoints, config defines (Recommended)
**Notes:** None

---

## Lane Assignment at Spawn

| Option | Description | Selected |
|--------|-------------|----------|
| Slot = lane | Direct 1:1 mapping | ✓ |
| Slot = lane with override | Default to slot, card can override | |

**User's choice:** Yes, slot = lane (Recommended)
**Notes:** None

---

## Debug Visualization

| Option | Description | Selected |
|--------|-------------|----------|
| Togglable overlay | Colored lane lines, debug key toggle | ✓ |
| No debug overlay | Trust the math | |

**User's choice:** Yes, togglable overlay (Recommended)
**Notes:** None

---

## Claude's Discretion

- Exact waypoint count (around 8)
- Curve math function (sine, quadratic, etc.)
- Jitter radius for endpoint clustering
- Debug key choice

## Deferred Ideas

None — discussion stayed within phase scope
