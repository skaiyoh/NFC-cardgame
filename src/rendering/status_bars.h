//
// World-anchored, screen-aligned status bars for troops and bases.
//

#ifndef NFC_CARDGAME_STATUS_BARS_H
#define NFC_CARDGAME_STATUS_BARS_H

#include "../core/types.h"

Texture2D status_bars_load(void);
Texture2D troop_health_bar_load(void);
void status_bars_unload(Texture2D texture);
void troop_health_bar_unload(Texture2D texture);
void status_bars_draw_screen(const GameState *gs, const Player *hudPlayer,
                             Camera2D camera,
                             Rectangle viewportRect,
                             float rotationDegrees,
                             float labelRotationDegrees,
                             bool reverseFillDirection);

#endif //NFC_CARDGAME_STATUS_BARS_H
