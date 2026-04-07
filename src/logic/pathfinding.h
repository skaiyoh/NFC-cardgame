#ifndef NFC_CARDGAME_PATHFINDING_H
#define NFC_CARDGAME_PATHFINDING_H

#include "../core/types.h"
#include "../core/battlefield.h"

// Advance entity one frame along canonical Battlefield waypoints.
// Returns true if entity is still walking, false if it reached the end (transitioned to IDLE).
bool pathfind_step_entity(Entity *e, const Battlefield *bf, float deltaTime);

// Apply waypoint-based facing to an animation state.
// Horizontal movement uses DIR_SIDE with flipH; vertical uses DIR_UP / DIR_DOWN.
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

// Recompute walking-facing from the current waypoint using the entity's current
// presentation side.
void pathfind_update_walk_facing(Entity *e, const Battlefield *bf);

// Apply facing using the supplied side's perspective.
// SIDE_BOTTOM treats decreasing y as "away" (DIR_UP), while SIDE_TOP treats
// increasing y as "away" (DIR_UP).
void pathfind_apply_direction_for_side(AnimState *anim, Vector2 diff, BattleSide side);

// Top-side presentation uses a 180-degree sprite rotation so entities read
// correctly from that side of the board, regardless of facing row.
float pathfind_sprite_rotation_for_side(SpriteDirection dir, BattleSide side);

#endif //NFC_CARDGAME_PATHFINDING_H
