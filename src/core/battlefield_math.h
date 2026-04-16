//
// Created by Nathan Davis on phase 4/1/26.
//

#ifndef NFC_CARDGAME_BATTLEFIELD_MATH_H
#define NFC_CARDGAME_BATTLEFIELD_MATH_H

#include <stdbool.h>
#include <assert.h>

/* Vector2: allow test files and non-Raylib consumers to use their own definition */
#ifndef VECTOR2_DEFINED
#define VECTOR2_DEFINED
typedef struct { float x; float y; } Vector2;
#endif

/* ---- Types ---- */

typedef enum { SIDE_BOTTOM, SIDE_TOP } BattleSide;
typedef struct { Vector2 v; } CanonicalPos;
typedef struct { Vector2 v; BattleSide side; } SideLocalPos;

/* ---- Coordinate transforms ---- */

CanonicalPos bf_to_canonical(SideLocalPos local, float boardWidth);
SideLocalPos bf_to_local(CanonicalPos canonical, BattleSide side, float boardWidth);

/* ---- Geometry helpers ---- */

float bf_distance(CanonicalPos a, CanonicalPos b);
bool bf_in_bounds(CanonicalPos pos, float boardWidth, float boardHeight);
int bf_slot_to_lane(BattleSide side, int slotIndex);
bool bf_crosses_seam(CanonicalPos pos, float spriteHeight, float seamY);
BattleSide bf_side_for_pos(CanonicalPos pos, float seamY);

/* ---- Debug / assertion macro ---- */

#define BF_ASSERT_IN_BOUNDS(pos, w, h) \
    assert((pos).v.x >= 0.0f && (pos).v.x <= (w) && \
           (pos).v.y >= 0.0f && (pos).v.y <= (h) && \
           "Entity position outside canonical board bounds")

#endif //NFC_CARDGAME_BATTLEFIELD_MATH_H
