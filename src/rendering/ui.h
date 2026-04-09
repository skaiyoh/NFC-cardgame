//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_UI_H
#define NFC_CARDGAME_UI_H

#include "../core/types.h"

void ui_draw_sustenance_counter(const Player *p, Rectangle viewport,
                         float rotation, Color color);
void ui_draw_match_result(const Player *p, const char *text, float rotation,
                          Color color);

#endif //NFC_CARDGAME_UI_H
