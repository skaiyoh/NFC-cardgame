#ifndef NFC_CARDGAME_PATHFINDING_H
#define NFC_CARDGAME_PATHFINDING_H

#include "../core/types.h"
#include "../core/battlefield.h"
#include "nav_frame.h"

// Advance entity one frame along the owning lane polyline using continuous
// lane progress plus local steering. Returns true if the entity is still
// walking, false if it reached the lane end (transitioned to IDLE).
//
// `nav` is the frozen per-frame navigation snapshot (GameState.nav). Phase
// 3a threads it through without touching behavior; later sub-phases consume
// the flow fields it owns.
bool pathfind_step_entity(Entity *e, NavFrame *nav, const Battlefield *bf,
                           float deltaTime);

// Apply waypoint-based facing to an animation state.
// Troops now use only DIR_SIDE; pure vertical motion keeps a right-facing bias.
// diff: vector from current position toward target waypoint (target - position).
void pathfind_apply_direction(AnimState *anim, Vector2 diff);

// Return which side's seat/view should currently own sprite presentation for a
// world position. Crossing the seam swaps presentation to the defending side.
BattleSide pathfind_presentation_side_for_position(Vector2 position, float seamY);

// Refresh sprite rotation while preserving the current presentation side during
// walking. Use this every movement tick before a walk-loop boundary.
void pathfind_sync_presentation(Entity *e, const Battlefield *bf);

// Commit the resolved presentation side once a walk loop completes, or when a
// unit stops walking and needs its final side immediately.
void pathfind_commit_presentation(Entity *e, const Battlefield *bf);

// Recompute walking-facing from the current steering goal using the entity's
// current presentation side. Pursuit targets override lane waypoints; if the
// lane is exhausted, this faces the enemy base anchor.
void pathfind_update_walk_facing(Entity *e, const Battlefield *bf);

// Recompute facing using an arbitrary goal position (waypoint, engagement
// point, sustenance node, base, etc.). Replaces pathfind_update_walk_facing
// for goal-based callers like local steering and farmer movement.
void pathfind_face_goal(Entity *e, const Battlefield *bf, Vector2 goal);

// Project an arbitrary position onto the entity's owning lane polyline and
// return the forward distance along that lane. Used by combat/pursuit logic
// to compare "ahead vs behind" without relying on stale discrete waypoints.
float pathfind_lane_progress_for_position(const Entity *e, const Battlefield *bf,
                                          Vector2 position);

// Refresh the entity's monotonic laneProgress from its current world position
// and derive the compatibility waypointIndex from that progress.
void pathfind_sync_lane_progress(Entity *e, const Battlefield *bf);

// Move one step toward an arbitrary goal with obstacle-aware local steering.
// The candidate fan, blocker overlap check, facing, and ticksSinceProgress
// updates are shared with pathfind_step_entity. Lane-only jam relief and lane
// corridor enforcement are not applied here. Stops when the entity is already
// within `stopRadius` of the goal. Returns true when arrived (within
// stopRadius), false while still moving. Used by farmer movement to re-use the
// same obstacle avoidance without lane-bound crowd behavior.
bool pathfind_move_toward_goal(Entity *e, Vector2 goal, float stopRadius,
                               NavFrame *nav, const Battlefield *bf,
                               float deltaTime);

// Apply troop-facing using the supplied side's perspective.
// Troops always use DIR_SIDE; SIDE_TOP still inverts flipH because those
// sprites are rendered with a 180-degree rotation.
void pathfind_apply_direction_for_side(AnimState *anim, Vector2 diff, BattleSide side);

// Top-side presentation uses a 180-degree sprite rotation so entities read
// correctly from that side of the board, regardless of facing row.
float pathfind_sprite_rotation_for_side(SpriteDirection dir, BattleSide side);

#endif //NFC_CARDGAME_PATHFINDING_H
