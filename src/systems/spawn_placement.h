//
// Non-overlapping spawn placement for card-played troops.
//
// Spawned units must not land on top of existing entities, otherwise local
// steering's blocker overlap check would immediately stall them. This helper
// searches a small grid of offsets around the slot anchor and returns the
// first in-bounds, non-overlapping candidate. Card plays that cannot find a
// free position should cancel the spawn (and skip the energy consume) so
// players aren't silently charged for cards that don't appear on the board.
//

#ifndef NFC_CARDGAME_SPAWN_PLACEMENT_H
#define NFC_CARDGAME_SPAWN_PLACEMENT_H

#include "../core/types.h"
#include <stdbool.h>

// Find a non-overlapping spawn position near the given side/slot anchor
// for a troop with the specified body radius. Writes the chosen canonical
// position to *outPos on success. Returns false if every candidate in the
// search grid is out of bounds or overlaps an existing entity.
//
// Search pattern (in order, first valid candidate wins):
//   - 9 lateral offsets along the lane centerline axis:
//     0, +/- 1.25R, +/- 2.5R, +/- 3.75R, +/- 5.0R (where R = bodyRadius)
//   - Repeated at four depth rows: the anchor itself, then 1R, 2R, and 3R
//     behind the anchor toward the owner's home edge
// Total of 36 candidate positions, which gives same-lane mass spawns more
// room before they saturate into an immediate wedge.
bool spawn_find_free_anchor(GameState *gs, BattleSide side, int slotIndex,
                            float bodyRadius, Vector2 *outPos);

#endif //NFC_CARDGAME_SPAWN_PLACEMENT_H
