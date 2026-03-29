//
// Created by Nathan Davis on 2/16/26.
//

#include "ui.h"
#include "../core/config.h"
#include <stdio.h>

// Draws an energy bar in screen space for one player.
// screenX: left edge of player's viewport (0 for P1, 960 for P2)
// viewportWidth: 960
void ui_draw_energy_bar(Player *p, int screenX, int viewportWidth) {
    float ratio = (p->maxEnergy > 0.0f) ? (p->energy / p->maxEnergy) : 0.0f;

    int barW = ENERGY_BAR_WIDTH;
    int barH = ENERGY_BAR_HEIGHT;
    int x = screenX + (viewportWidth - barW) / 2;
    int y = SCREEN_HEIGHT - ENERGY_BAR_Y_OFFSET - barH;

    DrawRectangle(x, y, barW, barH, DARKGRAY);
    DrawRectangle(x, y, (int) (barW * ratio), barH, GOLD);
    DrawRectangleLines(x, y, barW, barH, WHITE);

    char label[32];
    snprintf(label, sizeof(label), "%.0f / %.0f", p->energy, p->maxEnergy);
    int textW = MeasureText(label, 14);
    DrawText(label, x + (barW - textW) / 2, y + 3, 14, WHITE);
}

//   TODO: ui_draw_card_hand()   — render the active card hand above each viewport
//   TODO:  ui_draw_health()      — show base HP for each player
//   TODO: ui_draw_game_over()   — display winner text when win_trigger() fires