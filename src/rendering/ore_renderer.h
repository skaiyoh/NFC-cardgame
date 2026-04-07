//
// Ore node renderer -- standalone texture draw for ore resource nodes.
//

#ifndef NFC_CARDGAME_ORE_RENDERER_H
#define NFC_CARDGAME_ORE_RENDERER_H

#include <raylib.h>

// Raylib's Vector2 is already defined; suppress battlefield_math.h's fallback.
#ifndef VECTOR2_DEFINED
#define VECTOR2_DEFINED
#endif
#include "../core/ore.h"

// Load ore texture. Call once during game_init after InitWindow.
Texture2D ore_renderer_load(void);

// Draw all active ore nodes for one side. Call inside Camera2D.
void ore_renderer_draw(const OreField *field, BattleSide side, Texture2D texture);

#endif //NFC_CARDGAME_ORE_RENDERER_H
