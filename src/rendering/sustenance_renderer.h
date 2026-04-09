//
// Sustenance node renderer -- standalone texture draw for sustenance resource nodes.
//

#ifndef NFC_CARDGAME_SUSTENANCE_RENDERER_H
#define NFC_CARDGAME_SUSTENANCE_RENDERER_H

#include <raylib.h>

// Raylib's Vector2 is already defined; suppress battlefield_math.h's fallback.
#ifndef VECTOR2_DEFINED
#define VECTOR2_DEFINED
#endif
#include "../core/sustenance.h"

// Load sustenance texture. Call once during game_init after InitWindow.
Texture2D sustenance_renderer_load(void);

// Draw all active sustenance nodes for one side. Call inside Camera2D.
void sustenance_renderer_draw(const SustenanceField *field, BattleSide side, Texture2D texture,
                       float rotationDegrees);

#endif //NFC_CARDGAME_SUSTENANCE_RENDERER_H
