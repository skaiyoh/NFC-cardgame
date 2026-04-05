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

void ui_draw_viewport_label(const char *label, int screenX, bool seatOnRight,
                            Color color) {
    Font font = GetFontDefault();
    const int fontSize = 40;
    const float spacing = 2.0f;
    const int padding = 40;

    if (!seatOnRight) {
        DrawText(label, screenX + padding, padding, fontSize, color);
        return;
    }

    DrawTextPro(font, label,
                (Vector2){ (float)(screenX + padding), (float)(SCREEN_HEIGHT - padding) },
                (Vector2){ 0.0f, 0.0f },
                270.0f, (float)fontSize, spacing, color);
}

void ui_draw_match_result(const Player *p, const char *text, float rotation,
                          Color color) {
    Font font = GetFontDefault();
    const int fontSize = 80;
    const float spacing = 4.0f;

    Vector2 textSize = MeasureTextEx(font, text, (float)fontSize, spacing);

    float cx = p->screenArea.x + p->screenArea.width / 2.0f;
    float cy = p->screenArea.y + p->screenArea.height / 2.0f;

    Vector2 position;
    if (rotation == 90.0f) {
        // With origin at the text's top-left, a 90-degree rotation shifts the
        // text's bounds left by its height and down by its width.
        position = (Vector2){ cx + textSize.y / 2.0f, cy - textSize.x / 2.0f };
    } else if (rotation == 270.0f) {
        // A 270-degree rotation shifts the bounds upward by the text width.
        position = (Vector2){ cx - textSize.y / 2.0f, cy + textSize.x / 2.0f };
    } else {
        position = (Vector2){ cx - textSize.x / 2.0f, cy - textSize.y / 2.0f };
    }

    DrawTextPro(font, text, position, (Vector2){ 0.0f, 0.0f },
                rotation, (float)fontSize, spacing, color);
}

//   TODO: ui_draw_card_hand()   — render the active card hand above each viewport
//   TODO: ui_draw_health()      — show base HP for each player
