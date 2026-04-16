//
// Created by Nathan Davis on 2/16/26.
//

#include "ui.h"
#include "../core/config.h"
#include "../systems/player.h"
#include "uvulite_font.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Bitmap-font layout constants. Inter-glyph spacing is expressed in source
// pixels; the bitmap renderer scales it with the glyph size.
#define UI_SUSTENANCE_SCALE   2.0f
#define UI_SUSTENANCE_SPACING 1.0f
#define UI_SUSTENANCE_PADDING 40.0f
#define UI_SUSTENANCE_STACK_GAP 12.0f
#define UI_MEGA_BARF_INWARD_NUDGE 8.0f
#define UI_MEGA_BARF_ICON_SHEET_WIDTH 128
#define UI_MEGA_BARF_ICON_SHEET_HEIGHT 32
#define UI_MEGA_BARF_ICON_SIZE 32.0f
#define UI_MEGA_BARF_ICON_GAP 10.0f
#define UI_MEGA_BARF_ICON_ROW 0
#define UI_MEGA_BARF_ICON_COL 1
#define UI_MEGA_BARF_ICON_TILE_PIXELS 32.0f
#define UI_MATCH_RESULT_SCALE   8.0f
#define UI_MATCH_RESULT_SPACING 1.0f
#define UI_MATCH_RESULT_BACKDROP_ALPHA 0.35f

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

static Vector2 ui_rotate_local_offset(Vector2 localOffset, float rotation) {
    switch (ui_normalize_rotation(rotation)) {
        case 90:  return (Vector2){ -localOffset.y,  localOffset.x };
        case 180: return (Vector2){ -localOffset.x, -localOffset.y };
        case 270: return (Vector2){  localOffset.y, -localOffset.x };
        default:  return localOffset;
    }
}

static Texture2D ui_load_texture_checked(const char *path, int expectedWidth,
                                         int expectedHeight, const char *label) {
    Texture2D t = LoadTexture(path);
    if (t.id == 0) {
        fprintf(stderr, "[UI] Failed to load %s: %s\n", label, path);
        return t;
    }

    if (t.width != expectedWidth || t.height != expectedHeight) {
        fprintf(stderr,
                "[UI] Invalid %s texture: %s "
                "(expected %dx%d, got %dx%d)\n",
                label, path, expectedWidth, expectedHeight, t.width, t.height);
        UnloadTexture(t);
        return (Texture2D){0};
    }

    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    return t;
}

Texture2D ui_load_buff_icons(void) {
    return ui_load_texture_checked(BUFF_ICONS_PATH,
                                   UI_MEGA_BARF_ICON_SHEET_WIDTH,
                                   UI_MEGA_BARF_ICON_SHEET_HEIGHT,
                                   "buff icon sheet");
}

static Vector2 ui_measure_label(const char *label, Texture2D letteringTexture,
                                float scale, float spacing) {
    if (letteringTexture.id != 0) {
        return uvulite_font_measure(label, scale, spacing);
    }

    float fontSize = (float) UVULITE_FONT_GLYPH_PIXELS * scale;
    return MeasureTextEx(GetFontDefault(), label, fontSize,
                         ui_bitmap_spacing_screen(scale, spacing));
}

static void ui_draw_label(const char *label, Vector2 pivot, float rotation,
                          Texture2D letteringTexture, float scale,
                          float spacing, UvuliteTextStyle textStyle,
                          Color fallbackColor) {
    if (letteringTexture.id != 0) {
        uvulite_font_draw(letteringTexture, label, pivot, rotation,
                          scale, spacing, textStyle);
        return;
    }

    float fontSize = (float) UVULITE_FONT_GLYPH_PIXELS * scale;
    DrawTextPro(GetFontDefault(), label, pivot, (Vector2){0.0f, 0.0f},
                rotation, fontSize,
                ui_bitmap_spacing_screen(scale, spacing),
                fallbackColor);
}

static void ui_format_buff_timer(char *buf, size_t bufSize,
                                 float remainingSeconds) {
    int totalSeconds = 0;
    if (remainingSeconds > 0.0f) {
        totalSeconds = (int)ceilf(remainingSeconds);
    }

    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    snprintf(buf, bufSize, "%02d:%02d", minutes, seconds);
}

static Rectangle ui_mega_barf_icon_src_rect(void) {
    return (Rectangle){
        UI_MEGA_BARF_ICON_COL * UI_MEGA_BARF_ICON_TILE_PIXELS,
        UI_MEGA_BARF_ICON_ROW * UI_MEGA_BARF_ICON_TILE_PIXELS,
        UI_MEGA_BARF_ICON_TILE_PIXELS,
        UI_MEGA_BARF_ICON_TILE_PIXELS
    };
}

static void ui_draw_mega_barf_status(const Player *p, float rotation,
                                     Texture2D letteringTexture,
                                     Texture2D buffIconsTexture,
                                     Vector2 counterBoundsTopLeft,
                                     Vector2 counterBoundsSize) {
    if (!p || !player_energy_regen_boost_is_active(p)) return;

    char timerLabel[16];
    ui_format_buff_timer(timerLabel, sizeof(timerLabel),
                         p->energyRegenBoostRemaining);

    Vector2 timerSize = ui_measure_label(timerLabel, letteringTexture,
                                         UI_SUSTENANCE_SCALE,
                                         UI_SUSTENANCE_SPACING);
    const bool hasIcon = (buffIconsTexture.id != 0);
    const float iconWidth = hasIcon ? UI_MEGA_BARF_ICON_SIZE : 0.0f;
    const float iconGap = hasIcon ? UI_MEGA_BARF_ICON_GAP : 0.0f;
    const float counterGap = UI_SUSTENANCE_STACK_GAP + UI_MEGA_BARF_INWARD_NUDGE;
    Vector2 blockSize = {
        iconWidth + iconGap + timerSize.x,
        (iconWidth > timerSize.y) ? iconWidth : timerSize.y
    };

    Vector2 blockBoundsSize = ui_rotated_text_bounds(blockSize, rotation);
    Vector2 blockBoundsTopLeft;
    int rot = ui_normalize_rotation(rotation);
    if (rot == 90) {
        // Player-local "below" for a clockwise-rotated HUD block lands on the
        // screen-left side of its rotated AABB.
        blockBoundsTopLeft = (Vector2){
            counterBoundsTopLeft.x - counterGap - blockBoundsSize.x,
            counterBoundsTopLeft.y + (counterBoundsSize.y - blockBoundsSize.y) * 0.5f
        };
    } else if (rot == 270) {
        // Mirrored seat: player-local "below" lands on the screen-right side.
        blockBoundsTopLeft = (Vector2){
            counterBoundsTopLeft.x + counterBoundsSize.x + counterGap,
            counterBoundsTopLeft.y + (counterBoundsSize.y - blockBoundsSize.y) * 0.5f
        };
    } else {
        blockBoundsTopLeft = (Vector2){
            counterBoundsTopLeft.x + (counterBoundsSize.x - blockBoundsSize.x) * 0.5f,
            counterBoundsTopLeft.y + counterBoundsSize.y + UI_SUSTENANCE_STACK_GAP
        };
    }

    Vector2 blockOrigin = ui_text_position_from_bounds(blockBoundsTopLeft,
                                                       blockSize, rotation);

    if (hasIcon) {
        Vector2 iconLocalOffset = {
            0.0f,
            (blockSize.y - UI_MEGA_BARF_ICON_SIZE) * 0.5f
        };
        Vector2 iconRotatedOffset = ui_rotate_local_offset(iconLocalOffset,
                                                           rotation);
        Rectangle dstRect = {
            blockOrigin.x + iconRotatedOffset.x,
            blockOrigin.y + iconRotatedOffset.y,
            UI_MEGA_BARF_ICON_SIZE,
            UI_MEGA_BARF_ICON_SIZE
        };
        DrawTexturePro(buffIconsTexture, ui_mega_barf_icon_src_rect(),
                       dstRect, (Vector2){0.0f, 0.0f}, rotation, WHITE);
    }

    Vector2 timerLocalOffset = {
        iconWidth + iconGap,
        (blockSize.y - timerSize.y) * 0.5f
    };
    Vector2 timerRotatedOffset = ui_rotate_local_offset(timerLocalOffset,
                                                        rotation);
    Vector2 timerTopLeft = {
        blockOrigin.x + timerRotatedOffset.x,
        blockOrigin.y + timerRotatedOffset.y
    };
    ui_draw_label(timerLabel, timerTopLeft, rotation, letteringTexture,
                  UI_SUSTENANCE_SCALE, UI_SUSTENANCE_SPACING,
                  UVULITE_TEXT_WHITE_DIGITS_GOLD_LETTERS_WHITE_COLON,
                  ui_sustenance_fallback_color(p));
}

void ui_draw_sustenance_counter(const Player *p, Rectangle viewport,
                                float rotation, Texture2D letteringTexture,
                                Texture2D buffIconsTexture) {
    char bitmapLabel[32];
    char fallbackLabel[32];
    snprintf(bitmapLabel, sizeof(bitmapLabel), "SUSTENANCE:%d", p->sustenanceBank);
    snprintf(fallbackLabel, sizeof(fallbackLabel), "SUSTENANCE: %d", p->sustenanceBank);

    const char *label = (letteringTexture.id != 0) ? bitmapLabel : fallbackLabel;

    Vector2 textSize = ui_measure_label(label, letteringTexture,
                                        UI_SUSTENANCE_SCALE,
                                        UI_SUSTENANCE_SPACING);
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

    // Use white digits for the count while keeping the label letters on
    // the gold rows.
    ui_draw_label(label, pivot, rotation, letteringTexture,
                  UI_SUSTENANCE_SCALE, UI_SUSTENANCE_SPACING,
                  UVULITE_TEXT_WHITE_DIGITS_GOLD_LETTERS,
                  ui_sustenance_fallback_color(p));

    ui_draw_mega_barf_status(p, rotation, letteringTexture,
                             buffIconsTexture, boundsTopLeft, boundsSize);
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

void ui_draw_match_result_backdrop(void) {
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                  Fade(BLACK, UI_MATCH_RESULT_BACKDROP_ALPHA));
}
