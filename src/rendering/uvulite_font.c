//
// Bitmap font renderer — see uvulite_font.h for the sheet layout.
//

#include "uvulite_font.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define UVULITE_FONT_PATH "src/assets/environment/Objects/uvulite_lettering.png"
#define UVULITE_FONT_TRIM_SIDE_PIXELS 1.0f
#define UVULITE_FONT_TRIMMED_WIDTH_PIXELS \
    ((float)UVULITE_FONT_GLYPH_PIXELS - 2.0f * UVULITE_FONT_TRIM_SIDE_PIXELS)

// Resolve a character to its (row, col) glyph cell. Returns false for
// unsupported characters (including space) so the caller advances without
// drawing.
static int uvulite_digit_row(UvuliteTextStyle textStyle) {
    return (textStyle == UVULITE_TEXT_WHITE_DIGITS_GOLD_LETTERS ||
            textStyle == UVULITE_TEXT_WHITE_DIGITS_GOLD_LETTERS_WHITE_COLON)
        ? 0
        : 1;
}

static int uvulite_letter_row_base(UvuliteTextStyle textStyle) {
    return (textStyle == UVULITE_TEXT_GOLD_DIGITS_RED_LETTERS) ? 5 : 2;
}

static int uvulite_colon_col(UvuliteTextStyle textStyle) {
    return (textStyle == UVULITE_TEXT_WHITE_DIGITS_GOLD_LETTERS_WHITE_COLON)
        ? 8
        : 6;
}

static bool uvulite_glyph_cell(char c, UvuliteTextStyle textStyle,
                               int *row, int *col) {
    if (c >= '0' && c <= '9') {
        *row = uvulite_digit_row(textStyle);
        *col = c - '0';
        return true;
    }
    if (c >= 'A' && c <= 'J') {
        *row = uvulite_letter_row_base(textStyle);
        *col = c - 'A';
        return true;
    }
    if (c >= 'K' && c <= 'T') {
        *row = uvulite_letter_row_base(textStyle) + 1;
        *col = c - 'K';
        return true;
    }
    if (c >= 'U' && c <= 'Z') {
        *row = uvulite_letter_row_base(textStyle) + 2;
        *col = c - 'U';
        return true;
    }
    if (c == ':') {
        *row = 4;
        *col = uvulite_colon_col(textStyle);
        return true;
    }
    if (c == '!') { *row = 4; *col = 7; return true; }
    return false;
}

static bool uvulite_is_trimmed_alnum(char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z');
}

static float uvulite_glyph_source_width(char c) {
    return uvulite_is_trimmed_alnum(c)
        ? UVULITE_FONT_TRIMMED_WIDTH_PIXELS
        : (float)UVULITE_FONT_GLYPH_PIXELS;
}

static float uvulite_interglyph_gap_source(char current, char next, float spacing) {
    if (spacing <= 0.0f) return 0.0f;
    return (uvulite_is_trimmed_alnum(current) && uvulite_is_trimmed_alnum(next))
        ? spacing
        : 0.0f;
}

Texture2D uvulite_font_load(void) {
    Texture2D t = LoadTexture(UVULITE_FONT_PATH);
    if (t.id == 0) {
        fprintf(stderr, "[UvuliteFont] Failed to load %s\n", UVULITE_FONT_PATH);
        return t;
    }

    if (t.width != UVULITE_FONT_SHEET_PIXELS ||
        t.height != UVULITE_FONT_SHEET_PIXELS) {
        fprintf(stderr,
                "[UvuliteFont] Invalid sheet dimensions for %s "
                "(expected %dx%d, got %dx%d)\n",
                UVULITE_FONT_PATH, UVULITE_FONT_SHEET_PIXELS,
                UVULITE_FONT_SHEET_PIXELS, t.width, t.height);
        UnloadTexture(t);
        return (Texture2D){0};
    }

    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    return t;
}

void uvulite_font_unload(Texture2D texture) {
    if (texture.id != 0) {
        UnloadTexture(texture);
    }
}

Vector2 uvulite_font_measure(const char *text, float scale, float spacing) {
    if (!text || !*text) return (Vector2){0.0f, 0.0f};

    float widthSource = 0.0f;
    for (int i = 0; text[i] != '\0'; i++) {
        widthSource += uvulite_glyph_source_width(text[i]);
        if (text[i + 1] != '\0') {
            widthSource += uvulite_interglyph_gap_source(text[i], text[i + 1], spacing);
        }
    }

    return (Vector2){
        widthSource * scale,
        (float)UVULITE_FONT_GLYPH_PIXELS * scale
    };
}

void uvulite_font_draw(Texture2D texture, const char *text, Vector2 topLeft,
                       float rotationDegrees, float scale, float spacing,
                       UvuliteTextStyle textStyle) {
    if (texture.id == 0 || !text || !*text) return;

    float rad = rotationDegrees * (3.14159265358979323846f / 180.0f);
    float cosA = cosf(rad);
    float sinA = sinf(rad);
    float localX = 0.0f;

    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        int row = 0;
        int col = 0;
        float glyphSourceWidth = uvulite_glyph_source_width(c);

        if (uvulite_glyph_cell(c, textStyle, &row, &col)) {
            // Block-local unrotated offset of this glyph's top-left, rotated into
            // world space and added to the shared pivot. Each glyph is then drawn
            // with its own rotation about its own top-left — that composition
            // reproduces a uniformly rotated block (see uvulite_font.h).
            float dx = localX * cosA;
            float dy = localX * sinA;
            float srcX = (float)(col * UVULITE_FONT_GLYPH_PIXELS);
            if (uvulite_is_trimmed_alnum(c)) {
                srcX += UVULITE_FONT_TRIM_SIDE_PIXELS;
            }

            Rectangle src = {
                srcX,
                (float)(row * UVULITE_FONT_GLYPH_PIXELS),
                glyphSourceWidth,
                (float)UVULITE_FONT_GLYPH_PIXELS
            };
            Rectangle dst = {
                topLeft.x + dx,
                topLeft.y + dy,
                glyphSourceWidth * scale,
                (float)UVULITE_FONT_GLYPH_PIXELS * scale
            };
            DrawTexturePro(texture, src, dst, (Vector2){0.0f, 0.0f},
                           rotationDegrees, WHITE);
        }

        localX += glyphSourceWidth * scale;
        if (text[i + 1] != '\0') {
            localX += uvulite_interglyph_gap_source(c, text[i + 1], spacing) * scale;
        }
    }
}
