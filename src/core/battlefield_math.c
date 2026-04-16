//
// Created by Nathan Davis on phase 4/1/26.
//

#include "battlefield_math.h"
#include <math.h>

CanonicalPos bf_to_canonical(SideLocalPos local, float boardWidth) {
    CanonicalPos result;
    if (local.side == SIDE_TOP) {
        result.v.x = boardWidth - local.v.x;
        result.v.y = local.v.y;
    } else {
        result.v.x = local.v.x;
        result.v.y = local.v.y;
    }
    return result;
}

SideLocalPos bf_to_local(CanonicalPos canonical, BattleSide side, float boardWidth) {
    SideLocalPos result;
    result.side = side;
    if (side == SIDE_TOP) {
        result.v.x = boardWidth - canonical.v.x;
        result.v.y = canonical.v.y;
    } else {
        result.v.x = canonical.v.x;
        result.v.y = canonical.v.y;
    }
    return result;
}

float bf_distance(CanonicalPos a, CanonicalPos b) {
    float dx = a.v.x - b.v.x;
    float dy = a.v.y - b.v.y;
    return sqrtf(dx * dx + dy * dy);
}

bool bf_in_bounds(CanonicalPos pos, float boardWidth, float boardHeight) {
    return pos.v.x >= 0.0f && pos.v.x <= boardWidth &&
           pos.v.y >= 0.0f && pos.v.y <= boardHeight;
}

int bf_slot_to_lane(BattleSide side, int slotIndex) {
    if (side == SIDE_TOP) return 2 - slotIndex;
    return slotIndex;
}

bool bf_crosses_seam(CanonicalPos pos, float spriteHeight, float seamY) {
    float halfH = spriteHeight * 0.5f;
    return (pos.v.y - halfH < seamY) && (pos.v.y + halfH > seamY);
}

BattleSide bf_side_for_pos(CanonicalPos pos, float seamY) {
    return (pos.v.y < seamY) ? SIDE_TOP : SIDE_BOTTOM;
}
