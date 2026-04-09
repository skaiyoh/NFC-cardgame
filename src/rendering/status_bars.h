//
// World-anchored, screen-aligned status bars for troops and bases.
//

#ifndef NFC_CARDGAME_STATUS_BARS_H
#define NFC_CARDGAME_STATUS_BARS_H

#include "../core/types.h"

Texture2D status_bars_load(void);
void status_bars_unload(Texture2D texture);
void status_bars_draw_screen(const GameState *gs, Camera2D camera,
                             float rotationDegrees,
                             float labelRotationDegrees);

#endif //NFC_CARDGAME_STATUS_BARS_H
