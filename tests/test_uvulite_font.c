/*
 * Unit tests for src/rendering/uvulite_font.c
 *
 * Scope: glyph cell lookup, measurement math, draw call sequence, and
 * rotation geometry. Raylib is stubbed so the test compiles with only -lm,
 * matching the pattern used by test_hand_ui.c.
 *
 * These tests cannot verify final pixel output — only the math the font
 * module computes before handing off to DrawTexturePro.
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ---- Raylib stubs ---- */
#define RAYLIB_H

typedef struct { float x; float y; } Vector2;
typedef struct { float x; float y; float width; float height; } Rectangle;
typedef struct {
    unsigned int id;
    int width;
    int height;
    int mipmaps;
    int format;
} Texture2D;
typedef struct { unsigned char r, g, b, a; } Color;

#define WHITE (Color){255, 255, 255, 255}
#define TEXTURE_FILTER_POINT 0

static Texture2D LoadTexture(const char *fileName) {
    (void)fileName;
    return (Texture2D){ .id = 1, .width = 100, .height = 100,
                        .mipmaps = 1, .format = 0 };
}
static void UnloadTexture(Texture2D t) { (void)t; }
static void SetTextureFilter(Texture2D t, int f) { (void)t; (void)f; }

#define MAX_CAPTURED_DRAWS 64
static int g_draw_calls = 0;
static Rectangle g_drawn_src[MAX_CAPTURED_DRAWS];
static Rectangle g_drawn_dst[MAX_CAPTURED_DRAWS];
static float g_drawn_rotation[MAX_CAPTURED_DRAWS];

static void DrawTexturePro(Texture2D t, Rectangle src, Rectangle dst,
                           Vector2 origin, float rotation, Color tint) {
    (void)t; (void)origin; (void)tint;
    if (g_draw_calls < MAX_CAPTURED_DRAWS) {
        g_drawn_src[g_draw_calls] = src;
        g_drawn_dst[g_draw_calls] = dst;
        g_drawn_rotation[g_draw_calls] = rotation;
    }
    g_draw_calls++;
}

static void reset_draws(void) {
    g_draw_calls = 0;
    memset(g_drawn_src, 0, sizeof(g_drawn_src));
    memset(g_drawn_dst, 0, sizeof(g_drawn_dst));
    memset(g_drawn_rotation, 0, sizeof(g_drawn_rotation));
}

/* ---- Include SUT ---- */
#include "../src/rendering/uvulite_font.c"

/* ---- Test harness ---- */
static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(fn) do { \
    printf("  "); \
    fn(); \
    tests_run++; \
    tests_passed++; \
    printf("PASS: %s\n", #fn); \
} while(0)

static bool approx_eq(float a, float b, float eps) {
    return fabsf(a - b) < eps;
}

/* ---- Tests ---- */

static void test_glyph_cell_white_digit(void) {
    int row = -1, col = -1;
    assert(uvulite_glyph_cell('0', UVULITE_TEXT_WHITE_DIGITS_GOLD_LETTERS, &row, &col));
    assert(row == 0 && col == 0);
    assert(uvulite_glyph_cell('9', UVULITE_TEXT_WHITE_DIGITS_GOLD_LETTERS, &row, &col));
    assert(row == 0 && col == 9);
}

static void test_glyph_cell_gold_digit(void) {
    int row = -1, col = -1;
    assert(uvulite_glyph_cell('0', UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS, &row, &col));
    assert(row == 1 && col == 0);
    assert(uvulite_glyph_cell('9', UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS, &row, &col));
    assert(row == 1 && col == 9);
}

static void test_glyph_cell_letters_a_through_j(void) {
    int row = -1, col = -1;
    assert(uvulite_glyph_cell('A', UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS, &row, &col));
    assert(row == 2 && col == 0);
    assert(uvulite_glyph_cell('J', UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS, &row, &col));
    assert(row == 2 && col == 9);
}

static void test_glyph_cell_letters_k_through_t(void) {
    int row = -1, col = -1;
    assert(uvulite_glyph_cell('K', UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS, &row, &col));
    assert(row == 3 && col == 0);
    assert(uvulite_glyph_cell('T', UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS, &row, &col));
    assert(row == 3 && col == 9);
}

static void test_glyph_cell_letters_u_through_z(void) {
    int row = -1, col = -1;
    assert(uvulite_glyph_cell('U', UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS, &row, &col));
    assert(row == 4 && col == 0);
    assert(uvulite_glyph_cell('Z', UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS, &row, &col));
    assert(row == 4 && col == 5);
}

static void test_glyph_cell_punctuation(void) {
    int row = -1, col = -1;
    assert(uvulite_glyph_cell(':', UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS, &row, &col));
    assert(row == 4 && col == 6);
    assert(uvulite_glyph_cell('!', UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS, &row, &col));
    assert(row == 4 && col == 7);
}

static void test_glyph_cell_red_letters(void) {
    int row = -1, col = -1;
    assert(uvulite_glyph_cell('A', UVULITE_TEXT_GOLD_DIGITS_RED_LETTERS, &row, &col));
    assert(row == 5 && col == 0);
    assert(uvulite_glyph_cell('T', UVULITE_TEXT_GOLD_DIGITS_RED_LETTERS, &row, &col));
    assert(row == 6 && col == 9);
    assert(uvulite_glyph_cell('Z', UVULITE_TEXT_GOLD_DIGITS_RED_LETTERS, &row, &col));
    assert(row == 7 && col == 5);
}

static void test_glyph_cell_unsupported(void) {
    int row = 99, col = 99;
    assert(!uvulite_glyph_cell(' ', UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS, &row, &col));
    assert(!uvulite_glyph_cell('a', UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS, &row, &col));
    assert(!uvulite_glyph_cell('@', UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS, &row, &col));
}

static void test_measure_empty_and_null(void) {
    Vector2 a = uvulite_font_measure(NULL, 4.0f, 2.0f);
    Vector2 b = uvulite_font_measure("", 4.0f, 2.0f);
    assert(approx_eq(a.x, 0.0f, 0.001f) && approx_eq(a.y, 0.0f, 0.001f));
    assert(approx_eq(b.x, 0.0f, 0.001f) && approx_eq(b.y, 0.0f, 0.001f));
}

static void test_measure_sustenance_label(void) {
    // "SUSTENANCE:8" = 10 trimmed letters, 9 one-pixel inter-letter gaps,
    // then ':' (full 10px cell) and '8' (trimmed digit). Source width:
    // 10*8 + 9*1 + 10 + 8 = 107. Scaled by 4 => 428. Height = 40.
    Vector2 v = uvulite_font_measure("SUSTENANCE:8", 4.0f, 1.0f);
    assert(approx_eq(v.x, 428.0f, 0.001f));
    assert(approx_eq(v.y, 40.0f, 0.001f));
}

static void test_measure_single_char(void) {
    Vector2 v = uvulite_font_measure("A", 4.0f, 1.0f);
    assert(approx_eq(v.x, 32.0f, 0.001f));
    assert(approx_eq(v.y, 40.0f, 0.001f));
}

static void test_draw_zero_texture_is_noop(void) {
    reset_draws();
    Texture2D zero = {0};
    uvulite_font_draw(zero, "VICTORY", (Vector2){100.0f, 100.0f}, 0.0f, 4.0f, 2.0f,
                      UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS);
    assert(g_draw_calls == 0);
}

static void test_draw_skips_unsupported_chars(void) {
    reset_draws();
    Texture2D tex = { .id = 1 };
    // "SUSTENANCE: 8" has 13 chars total; 12 drawable (space is skipped).
    uvulite_font_draw(tex, "SUSTENANCE: 8", (Vector2){0.0f, 0.0f}, 0.0f, 4.0f, 1.0f,
                      UVULITE_TEXT_WHITE_DIGITS_GOLD_LETTERS);
    assert(g_draw_calls == 12);
}

static void test_draw_victory_src_rects(void) {
    reset_draws();
    Texture2D tex = { .id = 1 };
    // "VICTORY" all gold letters: V(4,1) I(2,8) C(2,2) T(3,9) O(3,4) R(3,7) Y(4,4)
    uvulite_font_draw(tex, "VICTORY", (Vector2){0.0f, 0.0f}, 0.0f, 1.0f, 1.0f,
                      UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS);
    assert(g_draw_calls == 7);

    const struct { int row; int col; } expected[7] = {
        {4, 1}, // V
        {2, 8}, // I
        {2, 2}, // C
        {3, 9}, // T
        {3, 4}, // O
        {3, 7}, // R
        {4, 4}, // Y
    };
    for (int i = 0; i < 7; i++) {
        float ex = (float)(expected[i].col * 10 + 1);
        float ey = (float)(expected[i].row * 10);
        assert(approx_eq(g_drawn_src[i].x, ex, 0.001f));
        assert(approx_eq(g_drawn_src[i].y, ey, 0.001f));
        assert(approx_eq(g_drawn_src[i].width, 8.0f, 0.001f));
        assert(approx_eq(g_drawn_src[i].height, 10.0f, 0.001f));
    }
}

static void test_draw_defeat_src_rects_red_letters(void) {
    reset_draws();
    Texture2D tex = { .id = 1 };
    // "DEFEAT" on red rows: D(5,3) E(5,4) F(5,5) E(5,4) A(5,0) T(6,9)
    uvulite_font_draw(tex, "DEFEAT", (Vector2){0.0f, 0.0f}, 0.0f, 1.0f, 1.0f,
                      UVULITE_TEXT_GOLD_DIGITS_RED_LETTERS);
    assert(g_draw_calls == 6);

    const struct { int row; int col; } expected[6] = {
        {5, 3}, // D
        {5, 4}, // E
        {5, 5}, // F
        {5, 4}, // E
        {5, 0}, // A
        {6, 9}, // T
    };
    for (int i = 0; i < 6; i++) {
        float ex = (float)(expected[i].col * 10 + 1);
        float ey = (float)(expected[i].row * 10);
        assert(approx_eq(g_drawn_src[i].x, ex, 0.001f));
        assert(approx_eq(g_drawn_src[i].y, ey, 0.001f));
        assert(approx_eq(g_drawn_src[i].width, 8.0f, 0.001f));
        assert(approx_eq(g_drawn_src[i].height, 10.0f, 0.001f));
    }
}

static void test_draw_unrotated_glyph_positions(void) {
    reset_draws();
    Texture2D tex = { .id = 1 };
    // "AB" at scale=4: trimmed width=8*4=32 plus 1 source-pixel gap scaled
    // to 4 -> stride 36. Unrotated positions: (100,50), (136,50).
    uvulite_font_draw(tex, "AB", (Vector2){100.0f, 50.0f}, 0.0f, 4.0f, 1.0f,
                      UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS);
    assert(g_draw_calls == 2);
    assert(approx_eq(g_drawn_dst[0].x, 100.0f, 0.001f));
    assert(approx_eq(g_drawn_dst[0].y, 50.0f, 0.001f));
    assert(approx_eq(g_drawn_dst[1].x, 136.0f, 0.001f));
    assert(approx_eq(g_drawn_dst[1].y, 50.0f, 0.001f));
    assert(approx_eq(g_drawn_dst[0].width, 32.0f, 0.001f));
    assert(approx_eq(g_drawn_dst[0].height, 40.0f, 0.001f));
}

static void test_draw_90_degree_rotation(void) {
    reset_draws();
    Texture2D tex = { .id = 1 };
    // At rot=90, the block-local x-axis maps to the world y-axis. "AB" at
    // pivot (0,0) scale=1 spacing=1 -> glyph 0 at (0,0), glyph 1 at (0,9).
    uvulite_font_draw(tex, "AB", (Vector2){0.0f, 0.0f}, 90.0f, 1.0f, 1.0f,
                      UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS);
    assert(g_draw_calls == 2);
    assert(approx_eq(g_drawn_dst[0].x, 0.0f, 0.001f));
    assert(approx_eq(g_drawn_dst[0].y, 0.0f, 0.001f));
    assert(approx_eq(g_drawn_dst[1].x, 0.0f, 0.001f));
    assert(approx_eq(g_drawn_dst[1].y, 9.0f, 0.001f));
    // Each glyph draw is tagged with the block rotation so DrawTexturePro
    // rotates the dst rect about its own top-left.
    assert(approx_eq(g_drawn_rotation[0], 90.0f, 0.001f));
    assert(approx_eq(g_drawn_rotation[1], 90.0f, 0.001f));
}

static void test_draw_270_degree_rotation(void) {
    reset_draws();
    Texture2D tex = { .id = 1 };
    // At rot=270, block-local x-axis maps to negative world y.
    uvulite_font_draw(tex, "AB", (Vector2){0.0f, 0.0f}, 270.0f, 1.0f, 1.0f,
                      UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS);
    assert(g_draw_calls == 2);
    assert(approx_eq(g_drawn_dst[1].x, 0.0f, 0.001f));
    assert(approx_eq(g_drawn_dst[1].y, -9.0f, 0.001f));
}

static void test_draw_letter_to_punctuation_has_no_artificial_gap(void) {
    reset_draws();
    Texture2D tex = { .id = 1 };
    uvulite_font_draw(tex, "A:", (Vector2){0.0f, 0.0f}, 0.0f, 1.0f, 1.0f,
                      UVULITE_TEXT_GOLD_DIGITS_GOLD_LETTERS);
    assert(g_draw_calls == 2);
    assert(approx_eq(g_drawn_dst[0].x, 0.0f, 0.001f));
    assert(approx_eq(g_drawn_dst[1].x, 8.0f, 0.001f));
    assert(approx_eq(g_drawn_dst[1].width, 10.0f, 0.001f));
    assert(approx_eq(g_drawn_src[1].x, 60.0f, 0.001f));
    assert(approx_eq(g_drawn_src[1].y, 40.0f, 0.001f));
}

static void test_draw_punctuation_to_digit_has_no_artificial_gap(void) {
    reset_draws();
    Texture2D tex = { .id = 1 };
    uvulite_font_draw(tex, ":8", (Vector2){0.0f, 0.0f}, 0.0f, 1.0f, 1.0f,
                      UVULITE_TEXT_WHITE_DIGITS_GOLD_LETTERS);
    assert(g_draw_calls == 2);
    assert(approx_eq(g_drawn_dst[0].x, 0.0f, 0.001f));
    assert(approx_eq(g_drawn_dst[1].x, 10.0f, 0.001f));
    assert(approx_eq(g_drawn_dst[1].width, 8.0f, 0.001f));
    assert(approx_eq(g_drawn_src[0].x, 60.0f, 0.001f));
    assert(approx_eq(g_drawn_src[0].y, 40.0f, 0.001f));
}

/* ---- Main ---- */
int main(void) {
    printf("Running uvulite_font tests...\n");

    RUN_TEST(test_glyph_cell_white_digit);
    RUN_TEST(test_glyph_cell_gold_digit);
    RUN_TEST(test_glyph_cell_letters_a_through_j);
    RUN_TEST(test_glyph_cell_letters_k_through_t);
    RUN_TEST(test_glyph_cell_letters_u_through_z);
    RUN_TEST(test_glyph_cell_punctuation);
    RUN_TEST(test_glyph_cell_red_letters);
    RUN_TEST(test_glyph_cell_unsupported);

    RUN_TEST(test_measure_empty_and_null);
    RUN_TEST(test_measure_sustenance_label);
    RUN_TEST(test_measure_single_char);

    RUN_TEST(test_draw_zero_texture_is_noop);
    RUN_TEST(test_draw_skips_unsupported_chars);
    RUN_TEST(test_draw_victory_src_rects);
    RUN_TEST(test_draw_defeat_src_rects_red_letters);
    RUN_TEST(test_draw_unrotated_glyph_positions);
    RUN_TEST(test_draw_90_degree_rotation);
    RUN_TEST(test_draw_270_degree_rotation);
    RUN_TEST(test_draw_letter_to_punctuation_has_no_artificial_gap);
    RUN_TEST(test_draw_punctuation_to_digit_has_no_artificial_gap);

    printf("\nAll %d uvulite_font tests passed!\n", tests_passed);
    return 0;
}
