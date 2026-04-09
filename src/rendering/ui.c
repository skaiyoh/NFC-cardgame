//
// Created by Nathan Davis on 2/16/26.
//

#include "ui.h"
#include "../core/config.h"
#include <stdio.h>

static int ui_normalize_rotation(float rotation) {
    int normalized = ((int) rotation) % 360;
    if (normalized < 0) {
        normalized += 360;
    }
    return normalized;
}

static Vector2 ui_rotated_text_bounds(Vector2 textSize, float rotation) {
    switch (ui_normalize_rotation(rotation)) {
        case 90:
        case 270:
            return (Vector2){ textSize.y, textSize.x };
        default:
            return textSize;
    }
}

static Vector2 ui_text_position_from_bounds(Vector2 boundsTopLeft,
                                            Vector2 textSize,
                                            float rotation) {
    switch (ui_normalize_rotation(rotation)) {
        case 90:
            return (Vector2){ boundsTopLeft.x + textSize.y, boundsTopLeft.y };
        case 180:
            return (Vector2){ boundsTopLeft.x + textSize.x,
                              boundsTopLeft.y + textSize.y };
        case 270:
            return (Vector2){ boundsTopLeft.x, boundsTopLeft.y + textSize.x };
        default:
            return boundsTopLeft;
    }
}

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

void ui_draw_viewport_label(const char *label, Rectangle viewport,
                            UICorner corner, float rotation, Color color) {
    Font font = GetFontDefault();
    const int fontSize = 40;
    const float spacing = 2.0f;
    const float padding = 40.0f;
    Vector2 textSize = MeasureTextEx(font, label, (float)fontSize, spacing);
    Vector2 boundsSize = ui_rotated_text_bounds(textSize, rotation);
    Vector2 boundsTopLeft = { viewport.x + padding, viewport.y + padding };

    switch (corner) {
        case UI_CORNER_TOP_RIGHT:
            boundsTopLeft.x = viewport.x + viewport.width - padding - boundsSize.x;
            break;
        case UI_CORNER_BOTTOM_LEFT:
            boundsTopLeft.y = viewport.y + viewport.height - padding - boundsSize.y;
            break;
        case UI_CORNER_BOTTOM_RIGHT:
            boundsTopLeft.x = viewport.x + viewport.width - padding - boundsSize.x;
            boundsTopLeft.y = viewport.y + viewport.height - padding - boundsSize.y;
            break;
        case UI_CORNER_TOP_LEFT:
        default:
            break;
    }

    DrawTextPro(font, label,
                ui_text_position_from_bounds(boundsTopLeft, textSize, rotation),
                (Vector2){ 0.0f, 0.0f },
                rotation, (float)fontSize, spacing, color);
}

void ui_draw_sustenance_counter(const Player *p, Rectangle viewport,
                         float rotation, Color color) {
    Font font = GetFontDefault();
    const int fontSize = 24;
    const float spacing = 1.0f;
    const float padding = 40.0f;

    // Measure the player label above so we can position below it
    const int labelFontSize = 40;
    const float labelSpacing = 2.0f;
    const char *playerLabel = (p->id == 0) ? "PLAYER 1" : "PLAYER 2";
    Vector2 labelTextSize = MeasureTextEx(font, playerLabel, (float)labelFontSize, labelSpacing);
    Vector2 labelBoundsSize = ui_rotated_text_bounds(labelTextSize, rotation);

    char label[32];
    snprintf(label, sizeof(label), "SUSTENANCE: %d", p->sustenanceCollected);

    Vector2 textSize = MeasureTextEx(font, label, (float)fontSize, spacing);
    Vector2 boundsSize = ui_rotated_text_bounds(textSize, rotation);

    // Position below the player label in the same corner.
    // For rotated text, "below" is along the screen y-axis.
    const float gap = 8.0f;
    Vector2 boundsTopLeft;
    int rot = ui_normalize_rotation(rotation);
    if (rot == 90) {
        // P1: top-right corner — label starts at y=padding, sustenance goes below it
        boundsTopLeft = (Vector2){
            viewport.x + viewport.width - padding - boundsSize.x,
            viewport.y + padding + labelBoundsSize.y + gap
        };
    } else {
        // P2: bottom-left corner — label ends at y=height-padding, sustenance goes above it
        boundsTopLeft = (Vector2){
            viewport.x + padding,
            viewport.y + viewport.height - padding - labelBoundsSize.y - gap - boundsSize.y
        };
    }

    DrawTextPro(font, label,
                ui_text_position_from_bounds(boundsTopLeft, textSize, rotation),
                (Vector2){ 0.0f, 0.0f },
                rotation, (float)fontSize, spacing, color);
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
