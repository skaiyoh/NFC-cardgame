//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_UI_H
#define NFC_CARDGAME_UI_H

#include "../core/types.h"

typedef enum UICorner {
    UI_CORNER_TOP_LEFT,
    UI_CORNER_TOP_RIGHT,
    UI_CORNER_BOTTOM_LEFT,
    UI_CORNER_BOTTOM_RIGHT
} UICorner;

void ui_draw_energy_bar(Player *p, int screenX, int viewportWidth);
void ui_draw_viewport_label(const char *label, Rectangle viewport,
                            UICorner corner, float rotation, Color color);
void ui_draw_sustenance_counter(const Player *p, Rectangle viewport,
                         float rotation, Color color);
void ui_draw_match_result(const Player *p, const char *text, float rotation,
                          Color color);

#endif //NFC_CARDGAME_UI_H
