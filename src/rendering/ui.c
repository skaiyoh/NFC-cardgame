//
// Created by Nathan Davis on 2/16/26.
//

#include "ui.h"
#include "uvulite_font.h"
#include <stdio.h>
#include <string.h>

// Bitmap-font layout constants. Inter-glyph spacing is expressed in source
// pixels; the bitmap renderer scales it with the glyph size.
#define UI_SUSTENANCE_SCALE   2.0f
#define UI_SUSTENANCE_SPACING 1.0f
#define UI_SUSTENANCE_PADDING 40.0f
#define UI_MATCH_RESULT_SCALE   8.0f
#define UI_MATCH_RESULT_SPACING 1.0f

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

// Fallback color for the default-font path. The bitmap font bakes its own
// palette so the caller no longer passes a Color; when the texture is
// missing we derive one here from player identity so P1 stays distinct
// from P2.
static Color ui_sustenance_fallback_color(const Player *p) {
    return (p && p->id == 0) ? DARKGREEN : MAROON;
}

// Fallback color for match-result text. Matches the semantic mapping that
// used to live in game.c: DRAW=LIGHTGRAY, VICTORY=GOLD, DEFEAT=RED.
static Color ui_match_result_fallback_color(const char *text) {
    if (!text) return GOLD;
    if (strcmp(text, "DRAW") == 0) return LIGHTGRAY;
    if (strcmp(text, "VICTORY") == 0) return GOLD;
    if (strcmp(text, "DEFEAT") == 0) return RED;
    return GOLD;
}

static UvuliteTextStyle ui_match_result_bitmap_style(const char *text) {
    if (text && strcmp(text, "DEFEAT") == 0) {
        return UVULITE_TEXT_GOLD_DIGITS_RED_LETTERS;
    }
    return UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS;
}

static float ui_bitmap_spacing_screen(float scale, float sourceSpacing) {
    return scale * sourceSpacing;
}

void ui_draw_sustenance_counter(const Player *p, Rectangle viewport,
                                float rotation, Texture2D letteringTexture) {
    char bitmapLabel[32];
    char fallbackLabel[32];
    snprintf(bitmapLabel, sizeof(bitmapLabel), "SUSTENANCE:%d", p->sustenanceCollected);
    snprintf(fallbackLabel, sizeof(fallbackLabel), "SUSTENANCE: %d", p->sustenanceCollected);

    const char *label = (letteringTexture.id != 0) ? bitmapLabel : fallbackLabel;

    Vector2 textSize;
    if (letteringTexture.id != 0) {
        textSize = uvulite_font_measure(label, UI_SUSTENANCE_SCALE,
                                        UI_SUSTENANCE_SPACING);
    } else {
        // Match the bitmap glyph height so fallback layout stays close.
        float fontSize = (float) UVULITE_FONT_GLYPH_PIXELS * UI_SUSTENANCE_SCALE;
        textSize = MeasureTextEx(GetFontDefault(), label, fontSize,
                                 ui_bitmap_spacing_screen(UI_SUSTENANCE_SCALE,
                                                          UI_SUSTENANCE_SPACING));
    }
    Vector2 boundsSize = ui_rotated_text_bounds(textSize, rotation);

    Vector2 boundsTopLeft;
    int rot = ui_normalize_rotation(rotation);
    if (rot == 90) {
        // P1: top-right corner
        boundsTopLeft = (Vector2){
            viewport.x + viewport.width - UI_SUSTENANCE_PADDING - boundsSize.x,
            viewport.y + UI_SUSTENANCE_PADDING
        };
    } else {
        // P2: bottom-left corner
        boundsTopLeft = (Vector2){
            viewport.x + UI_SUSTENANCE_PADDING,
            viewport.y + viewport.height - UI_SUSTENANCE_PADDING - boundsSize.y
        };
    }

    Vector2 pivot = ui_text_position_from_bounds(boundsTopLeft, textSize, rotation);

    if (letteringTexture.id != 0) {
        // Use white digits for the count while keeping the label letters on
        // the gold rows.
        uvulite_font_draw(letteringTexture, label, pivot, rotation,
                          UI_SUSTENANCE_SCALE, UI_SUSTENANCE_SPACING,
                          UVULITE_TEXT_WHITE_DIGITS_GOLD_LETTERS);
    } else {
        float fontSize = (float) UVULITE_FONT_GLYPH_PIXELS * UI_SUSTENANCE_SCALE;
        DrawTextPro(GetFontDefault(), label, pivot, (Vector2){0.0f, 0.0f},
                    rotation, fontSize,
                    ui_bitmap_spacing_screen(UI_SUSTENANCE_SCALE,
                                             UI_SUSTENANCE_SPACING),
                    ui_sustenance_fallback_color(p));
    }
}

void ui_draw_match_result(const Player *p, const char *text, float rotation,
                          Texture2D letteringTexture) {
    Vector2 textSize;
    if (letteringTexture.id != 0) {
        textSize = uvulite_font_measure(text, UI_MATCH_RESULT_SCALE,
                                        UI_MATCH_RESULT_SPACING);
    } else {
        float fontSize = (float) UVULITE_FONT_GLYPH_PIXELS * UI_MATCH_RESULT_SCALE;
        textSize = MeasureTextEx(GetFontDefault(), text, fontSize,
                                 ui_bitmap_spacing_screen(UI_MATCH_RESULT_SCALE,
                                                          UI_MATCH_RESULT_SPACING));
    }

    // Center on the player's battlefield sub-rect, not the full half-screen,
    // so the overlay stays off the hand bar on the player's outer edge.
    float cx = p->battlefieldArea.x + p->battlefieldArea.width / 2.0f;
    float cy = p->battlefieldArea.y + p->battlefieldArea.height / 2.0f;

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

    if (letteringTexture.id != 0) {
        uvulite_font_draw(letteringTexture, text, position, rotation,
                          UI_MATCH_RESULT_SCALE, UI_MATCH_RESULT_SPACING,
                          ui_match_result_bitmap_style(text));
    } else {
        float fontSize = (float) UVULITE_FONT_GLYPH_PIXELS * UI_MATCH_RESULT_SCALE;
        DrawTextPro(GetFontDefault(), text, position, (Vector2){0.0f, 0.0f},
                    rotation, fontSize,
                    ui_bitmap_spacing_screen(UI_MATCH_RESULT_SCALE,
                                             UI_MATCH_RESULT_SPACING),
                    ui_match_result_fallback_color(text));
    }
    (void) p;
}
