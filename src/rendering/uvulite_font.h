//
// Bitmap font renderer for src/assets/environment/Objects/uvulite_lettering.png.
//
// The sheet is 100x100, laid out as a 10x10 grid of 10x10 glyph cells.
// Row 0: white digits 0-9
// Row 1: gold digits 0-9
// Row 2: A-J (gold)
// Row 3: K-T (gold)
// Row 4, cols 0-5: U-Z (gold)
// Row 4, col 6: gold ':'
// Row 4, col 7: '!'
// Row 4, col 8: white ':'
// Row 5: A-J (red)
// Row 6: K-T (red)
// Row 7: U-Z (red)
// All other characters (including space) advance one glyph without drawing.
//
// Colors are baked into the sheet. The font draws with a WHITE tint and does
// not take a Color parameter; callers that need colored text must fall back
// to Raylib's default-font rendering when the texture fails to load.
//

#ifndef NFC_CARDGAME_UVULITE_FONT_H
#define NFC_CARDGAME_UVULITE_FONT_H

#include <raylib.h>

typedef enum {
    UVULITE_TEXT_WHITE_DIGITS_GOLD_LETTERS,
    UVULITE_TEXT_WHITE_DIGITS_GOLD_LETTERS_WHITE_COLON,
    UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS,
    UVULITE_TEXT_GOLD_DIGITS_RED_LETTERS,
} UvuliteTextStyle;

#define UVULITE_FONT_GLYPH_PIXELS 10
#define UVULITE_FONT_SHEET_PIXELS 100

// Load the lettering sheet. Returns a zero-id Texture2D on load or
// dimension-validation failure; callers should treat that as "fall back to
// the default-font text path."
Texture2D uvulite_font_load(void);

// Unload a texture previously returned by uvulite_font_load. Safe to call
// with a zero-id texture (no-op).
void uvulite_font_unload(Texture2D texture);

// Compute the unrotated bounding box of `text` rendered at `scale` with
// `spacing` source-pixels of artificial gap between adjacent letters/digits.
// Letters and digits trim a built-in 1px blank column on both left and right
// sides before scaling; punctuation keeps its full 10px cell width.
//
// Height is UVULITE_FONT_GLYPH_PIXELS*scale. Returns (0,0) for NULL or empty
// text.
Vector2 uvulite_font_measure(const char *text, float scale, float spacing);

// Draw `text` starting at `topLeft`, rotated `rotationDegrees` around topLeft
// (pivot = topLeft). Letters and digits trim their 1px side bearings before
// scaling; punctuation keeps its full-cell width. `spacing` is measured in
// source pixels and is added only between adjacent letters/digits.
//
// If `texture.id == 0`, this is a no-op. Unsupported chars and spaces advance
// without drawing.
void uvulite_font_draw(Texture2D texture, const char *text, Vector2 topLeft,
                       float rotationDegrees, float scale, float spacing,
                       UvuliteTextStyle textStyle);

#endif //NFC_CARDGAME_UVULITE_FONT_H
