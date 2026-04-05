//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_UI_H
#define NFC_CARDGAME_UI_H

#include "../core/types.h"

void ui_draw_energy_bar(Player *p, int screenX, int viewportWidth);
void ui_draw_viewport_label(const char *label, int screenX, bool seatOnRight,
                            Color color);
void ui_draw_match_result(const Player *p, const char *text, float rotation,
                          Color color);

#endif //NFC_CARDGAME_UI_H
