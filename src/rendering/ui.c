//
// Created by Nathan Davis on 2/16/26.
//

#include "ui.h"
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

void ui_draw_sustenance_counter(const Player *p, Rectangle viewport,
                         float rotation, Color color) {
    Font font = GetFontDefault();
    const int fontSize = 40;
    const float spacing = 2.0f;
    const float padding = 40.0f;

    char label[32];
    snprintf(label, sizeof(label), "SUSTENANCE: %d", p->sustenanceCollected);

    Vector2 textSize = MeasureTextEx(font, label, (float)fontSize, spacing);
    Vector2 boundsSize = ui_rotated_text_bounds(textSize, rotation);

    Vector2 boundsTopLeft;
    int rot = ui_normalize_rotation(rotation);
    if (rot == 90) {
        // P1: top-right corner
        boundsTopLeft = (Vector2){
            viewport.x + viewport.width - padding - boundsSize.x,
            viewport.y + padding
        };
    } else {
        // P2: bottom-left corner
        boundsTopLeft = (Vector2){
            viewport.x + padding,
            viewport.y + viewport.height - padding - boundsSize.y
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
