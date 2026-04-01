#ifndef NFC_CARDGAME_PATHFINDING_H
#define NFC_CARDGAME_PATHFINDING_H

#include "../core/types.h"
#include "../core/battlefield.h"

// [ADAPTER] Generate pre-computed waypoints for all 3 lanes for a player.
// Must be called after player_init_card_slots (uses slot worldPos for waypoint[0]).
// Stores results in p->laneWaypoints[3][LANE_WAYPOINT_COUNT].
// Battlefield.laneWaypoints is authoritative; this remains for adapter compatibility.
void lane_generate_waypoints(Player *p);

// Advance entity one frame along canonical Battlefield waypoints.
// Returns true if entity is still walking, false if it reached the end (transitioned to IDLE).
bool pathfind_step_entity(Entity *e, const Battlefield *bf, float deltaTime);

// Apply waypoint-based facing to an animation state.
// Horizontal movement uses DIR_SIDE with flipH; vertical uses DIR_UP / DIR_DOWN.
// diff: vector from current position toward target waypoint (target - position).
void pathfind_apply_direction(AnimState *anim, Vector2 diff);

#endif //NFC_CARDGAME_PATHFINDING_H
