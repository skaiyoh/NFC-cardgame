/*
 * Unit tests for src/rendering/hand_ui.c
 *
 * Scope: hand layout math plus occupancy-based draw behavior for hand_ui.c.
 * We stub raylib and the game type headers so the test compiles with only
 * -lm, matching the header-guard-override pattern used by test_battlefield.c
 * and test_spawn_fx.c.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>

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

/* Stubs for the raylib calls made by the non-tested parts of hand_ui.c.
 * hand_ui_card_center_for_index does not touch any of these, but the other
 * functions in the same translation unit do, so the linker needs them. */
static Texture2D LoadTexture(const char *fileName) {
    (void)fileName;
    return (Texture2D){ .id = 1, .width = 128, .height = 160,
                        .mipmaps = 1, .format = 0 };
}
static void UnloadTexture(Texture2D t) { (void)t; }
static void SetTextureFilter(Texture2D t, int f) { (void)t; (void)f; }

/* ---- Config stub: only the constants hand_ui.c depends on ---- */
#define NFC_CARDGAME_CONFIG_H
#define HAND_UI_DEPTH_PX               180
#define HAND_MAX_CARDS                 8
#define HAND_CARD_WIDTH                128
#define HAND_CARD_HEIGHT               160
#define HAND_CARD_GAP                  4
#define HAND_CARD_SHEET_PATH           "src/assets/cards/card_sheet.png"
#define HAND_CARD_SHEET_ROWS           8
#define HAND_CARD_FRAME_COUNT          6
#define HAND_CARD_FRAME_TIME           0.05f
#define HAND_CARD_PLAY_LIFT_PEAK_SCALE 1.06f

static int g_draw_texture_calls = 0;
static Rectangle g_drawn_dst[HAND_MAX_CARDS];
static Rectangle g_drawn_src[HAND_MAX_CARDS];
static float g_drawn_rotation[HAND_MAX_CARDS];
static unsigned int g_drawn_texture_id[HAND_MAX_CARDS];

static void DrawRectangleRec(Rectangle r, Color c) { (void)r; (void)c; }
static void DrawTexturePro(Texture2D t, Rectangle src, Rectangle dst,
                           Vector2 origin, float rotation, Color tint) {
    (void)t; (void)src; (void)dst; (void)origin; (void)rotation; (void)tint;
    if (g_draw_texture_calls < HAND_MAX_CARDS) {
        g_drawn_src[g_draw_texture_calls] = src;
        g_drawn_dst[g_draw_texture_calls] = dst;
        g_drawn_rotation[g_draw_texture_calls] = rotation;
        g_drawn_texture_id[g_draw_texture_calls] = t.id;
    }
    g_draw_texture_calls++;
}

/* ---- Skip the heavy types.h chain ---- */
#define NFC_CARDGAME_TYPES_H

typedef enum { SIDE_BOTTOM = 0, SIDE_TOP = 1 } BattleSide;

typedef struct Card {
    char *card_id;
    char *name;
    int cost;
    char *type;
    char *rules_text;
    char *data;
} Card;

typedef struct Player {
    int id;
    BattleSide side;
    Rectangle screenArea;
    Rectangle battlefieldArea;
    Rectangle handArea;
    Card *handCards[HAND_MAX_CARDS];
    bool handCardAnimating[HAND_MAX_CARDS];
    float handCardAnimElapsed[HAND_MAX_CARDS];
} Player;

static bool player_hand_slot_is_occupied(const Player *p, int handIndex) {
    if (!p || handIndex < 0 || handIndex >= HAND_MAX_CARDS) {
        return false;
    }
    return p->handCards[handIndex] != NULL;
}

static int player_hand_occupied_count(const Player *p) {
    if (!p) return 0;

    int count = 0;
    for (int i = 0; i < HAND_MAX_CARDS; i++) {
        if (player_hand_slot_is_occupied(p, i)) count++;
    }
    return count;
}

/* ---- Skip hand_ui.h include chain by providing the declarations ourselves ---- */
#define NFC_CARDGAME_HAND_UI_H
Texture2D hand_ui_load_card_sheet(void);
void hand_ui_unload_texture(Texture2D texture);
void hand_ui_draw(const Player *p, Texture2D cardSheet);
Vector2 hand_ui_card_center_for_index(Rectangle handArea, int visibleCardCount, int visibleIndex);

/* ---- Production code under test ---- */
#include "../src/rendering/hand_ui.c"

/* ---- Test helpers ---- */

static bool approx_eq(float a, float b, float eps) {
    return fabsf(a - b) < eps;
}

static float expected_center_y(Rectangle handArea, int visibleCardCount, int visibleIndex) {
    const float stride = (float)HAND_CARD_WIDTH + (float)HAND_CARD_GAP;
    const float runLength = (float)visibleCardCount * (float)HAND_CARD_WIDTH
                          + (float)(visibleCardCount - 1) * (float)HAND_CARD_GAP;
    const float startY = handArea.y + (handArea.height - runLength) * 0.5f
                       + (float)HAND_CARD_WIDTH * 0.5f;
    return startY + (float)visibleIndex * stride;
}

static void reset_draw_capture(void) {
    g_draw_texture_calls = 0;
    for (int i = 0; i < HAND_MAX_CARDS; i++) {
        g_drawn_src[i] = (Rectangle){0};
        g_drawn_dst[i] = (Rectangle){0};
        g_drawn_rotation[i] = 0.0f;
        g_drawn_texture_id[i] = 0;
    }
}

/* ---- Test: center X matches handArea midline for hand sizes 1..HAND_MAX_CARDS ---- */
static void test_center_x_matches_handarea_midline(void) {
    Rectangle p1Hand = { 0.0f, 0.0f, 180.0f, 1080.0f };
    Rectangle p2Hand = { 1740.0f, 0.0f, 180.0f, 1080.0f };

    for (int count = 1; count <= HAND_MAX_CARDS; count++) {
        for (int i = 0; i < count; i++) {
            Vector2 p1 = hand_ui_card_center_for_index(p1Hand, count, i);
            Vector2 p2 = hand_ui_card_center_for_index(p2Hand, count, i);
            assert(approx_eq(p1.x, p1Hand.x + p1Hand.width * 0.5f, 0.01f));
            assert(approx_eq(p2.x, p2Hand.x + p2Hand.width * 0.5f, 0.01f));
        }
    }
    printf("  PASS: test_center_x_matches_handarea_midline\n");
}

/* ---- Test: visible cards remain ordered top-to-bottom for hand sizes 1..HAND_MAX_CARDS ---- */
static void test_cards_monotonic_y(void) {
    Rectangle hand = { 0.0f, 0.0f, 180.0f, 1080.0f };
    for (int count = 2; count <= HAND_MAX_CARDS; count++) {
        for (int i = 1; i < count; i++) {
            Vector2 prev = hand_ui_card_center_for_index(hand, count, i - 1);
            Vector2 curr = hand_ui_card_center_for_index(hand, count, i);
            assert(curr.y > prev.y);
        }
    }
    printf("  PASS: test_cards_monotonic_y\n");
}

/* ---- Test: centers are evenly spaced by stride for hand sizes 1..HAND_MAX_CARDS ---- */
static void test_even_stride(void) {
    Rectangle hand = { 0.0f, 0.0f, 180.0f, 1080.0f };
    const float stride = (float)HAND_CARD_WIDTH + (float)HAND_CARD_GAP;
    for (int count = 2; count <= HAND_MAX_CARDS; count++) {
        for (int i = 1; i < count; i++) {
            Vector2 prev = hand_ui_card_center_for_index(hand, count, i - 1);
            Vector2 curr = hand_ui_card_center_for_index(hand, count, i);
            assert(approx_eq(curr.y - prev.y, stride, 0.01f));
        }
    }
    printf("  PASS: test_even_stride\n");
}

/* ---- Test: one-card run is vertically centered inside handArea ---- */
static void test_one_card_centered_in_handarea(void) {
    Rectangle hand = { 0.0f, 0.0f, 180.0f, 1080.0f };
    Vector2 only = hand_ui_card_center_for_index(hand, 1, 0);

    assert(approx_eq(only.y, hand.y + hand.height * 0.5f, 0.01f));
    printf("  PASS: test_one_card_centered_in_handarea\n");
}

/* ---- Test: three-card run is vertically centered inside handArea ---- */
static void test_three_card_run_centered_in_handarea(void) {
    Rectangle hand = { 0.0f, 0.0f, 180.0f, 1080.0f };

    Vector2 first = hand_ui_card_center_for_index(hand, 3, 0);
    Vector2 last  = hand_ui_card_center_for_index(hand, 3, 2);

    const float halfCardWidth = (float)HAND_CARD_WIDTH * 0.5f;
    const float topEdge = first.y - halfCardWidth;
    const float bottomEdge = last.y + halfCardWidth;
    const float topSlack = topEdge - hand.y;
    const float bottomSlack = (hand.y + hand.height) - bottomEdge;

    assert(approx_eq(topSlack, bottomSlack, 0.01f));
    assert(topSlack > 0.0f);

    printf("  PASS: test_three_card_run_centered_in_handarea\n");
}

/* ---- Test: every hand size stays vertically centered inside handArea ---- */
static void test_all_run_sizes_centered_in_handarea(void) {
    Rectangle hand = { 0.0f, 0.0f, 180.0f, 1080.0f };

    for (int count = 1; count <= HAND_MAX_CARDS; count++) {
        Vector2 first = hand_ui_card_center_for_index(hand, count, 0);
        Vector2 last = hand_ui_card_center_for_index(hand, count, count - 1);

        const float halfCardWidth = (float)HAND_CARD_WIDTH * 0.5f;
        const float topEdge = first.y - halfCardWidth;
        const float bottomEdge = last.y + halfCardWidth;
        const float topSlack = topEdge - hand.y;
        const float bottomSlack = (hand.y + hand.height) - bottomEdge;

        assert(approx_eq(topSlack, bottomSlack, 0.01f));
        assert(topSlack >= 0.0f);
    }

    printf("  PASS: test_all_run_sizes_centered_in_handarea\n");
}

/* ---- Test: expected absolute values for P1 {0,0,180,1080} ---- */
static void test_expected_absolute_positions_p1(void) {
    Rectangle p1Hand = { 0.0f, 0.0f, 180.0f, 1080.0f };

    for (int count = 1; count <= HAND_MAX_CARDS; count++) {
        for (int i = 0; i < count; i++) {
            Vector2 c = hand_ui_card_center_for_index(p1Hand, count, i);
            assert(approx_eq(c.x, 90.0f, 0.01f));
            assert(approx_eq(c.y, expected_center_y(p1Hand, count, i), 0.01f));
        }
    }

    printf("  PASS: test_expected_absolute_positions_p1\n");
}

/* ---- Test: expected absolute values for P2 {1740,0,180,1080} ---- */
static void test_expected_absolute_positions_p2(void) {
    Rectangle p2Hand = { 1740.0f, 0.0f, 180.0f, 1080.0f };

    for (int count = 1; count <= HAND_MAX_CARDS; count++) {
        for (int i = 0; i < count; i++) {
            Vector2 c = hand_ui_card_center_for_index(p2Hand, count, i);
            assert(approx_eq(c.x, 1830.0f, 0.01f));
            assert(approx_eq(c.y, expected_center_y(p2Hand, count, i), 0.01f));
        }
    }

    printf("  PASS: test_expected_absolute_positions_p2\n");
}

/* ---- Test: empty hand emits no draw calls ---- */
static void test_empty_hand_draws_nothing(void) {
    Player p = {0};
    p.side = SIDE_BOTTOM;
    p.handArea = (Rectangle){ 0.0f, 0.0f, 180.0f, 1080.0f };
    Texture2D cardSheet = { .id = 2, .width = 768, .height = 1280, .mipmaps = 1, .format = 0 };

    reset_draw_capture();
    hand_ui_draw(&p, cardSheet);
    assert(g_draw_texture_calls == 0);

    printf("  PASS: test_empty_hand_draws_nothing\n");
}

/* ---- Test: sparse hand compacts visible cards with no gaps ---- */
static void test_sparse_hand_compacts_draw_positions(void) {
    Player p = {0};
    Card cardA = { .card_id = "ASSASSIN_01", .type = "assassin" };
    Card cardB = { .card_id = "HEALER_01", .type = "healer" };
    Texture2D cardSheet = { .id = 2, .width = 768, .height = 1280, .mipmaps = 1, .format = 0 };

    p.side = SIDE_TOP;
    p.handArea = (Rectangle){ 1740.0f, 0.0f, 180.0f, 1080.0f };
    p.handCards[0] = &cardA;
    p.handCards[3] = &cardB;

    reset_draw_capture();
    hand_ui_draw(&p, cardSheet);

    assert(g_draw_texture_calls == 2);
    assert(approx_eq(g_drawn_rotation[0], 270.0f, 0.01f));
    assert(approx_eq(g_drawn_rotation[1], 270.0f, 0.01f));

    Vector2 expected0 = hand_ui_card_center_for_index(p.handArea, 2, 0);
    Vector2 expected1 = hand_ui_card_center_for_index(p.handArea, 2, 1);
    assert(approx_eq(g_drawn_dst[0].x, expected0.x, 0.01f));
    assert(approx_eq(g_drawn_dst[0].y, expected0.y, 0.01f));
    assert(approx_eq(g_drawn_dst[1].x, expected1.x, 0.01f));
    assert(approx_eq(g_drawn_dst[1].y, expected1.y, 0.01f));

    printf("  PASS: test_sparse_hand_compacts_draw_positions\n");
}

/* ---- Test: idle knight uses row 0 from the shared card sheet ---- */
static void test_idle_knight_uses_sheet_row_zero(void) {
    Player p = {0};
    Card knight = { .card_id = "KNIGHT_01", .type = "knight" };
    Texture2D cardSheet = { .id = 22, .width = 768, .height = 1280, .mipmaps = 1, .format = 0 };

    p.side = SIDE_BOTTOM;
    p.handArea = (Rectangle){ 0.0f, 0.0f, 180.0f, 1080.0f };
    p.handCards[0] = &knight;

    reset_draw_capture();
    hand_ui_draw(&p, cardSheet);

    assert(g_draw_texture_calls == 1);
    assert(g_drawn_texture_id[0] == cardSheet.id);
    assert(approx_eq(g_drawn_src[0].x, 0.0f, 0.01f));
    assert(approx_eq(g_drawn_src[0].y, 0.0f, 0.01f));
    assert(approx_eq(g_drawn_src[0].width, 128.0f, 0.01f));
    assert(approx_eq(g_drawn_src[0].height, 160.0f, 0.01f));

    printf("  PASS: test_idle_knight_uses_sheet_row_zero\n");
}

/* ---- Test: assassin cards use their mapped row in the shared sheet ---- */
static void test_assassin_uses_mapped_sheet_row(void) {
    Player p = {0};
    Card assassin = { .card_id = "ASSASSIN_01", .type = "assassin" };
    Texture2D cardSheet = { .id = 22, .width = 768, .height = 1280, .mipmaps = 1, .format = 0 };

    p.side = SIDE_BOTTOM;
    p.handArea = (Rectangle){ 0.0f, 0.0f, 180.0f, 1080.0f };
    p.handCards[0] = &assassin;

    reset_draw_capture();
    hand_ui_draw(&p, cardSheet);

    assert(g_draw_texture_calls == 1);
    assert(g_drawn_texture_id[0] == cardSheet.id);
    assert(approx_eq(g_drawn_src[0].x, 0.0f, 0.01f));
    assert(approx_eq(g_drawn_src[0].y, 320.0f, 0.01f));
    assert(approx_eq(g_drawn_src[0].width, 128.0f, 0.01f));
    assert(approx_eq(g_drawn_src[0].height, 160.0f, 0.01f));

    printf("  PASS: test_assassin_uses_mapped_sheet_row\n");
}

/* ---- Test: animation helper follows 0->4->0 once, then clamps ---- */
static void test_frame_sequence_once_then_static(void) {
    assert(hand_ui_frame_for_elapsed(0.00f) == 0);
    assert(hand_ui_frame_for_elapsed(0.05f) == 1);
    assert(hand_ui_frame_for_elapsed(0.10f) == 2);
    assert(hand_ui_frame_for_elapsed(0.15f) == 3);
    assert(hand_ui_frame_for_elapsed(0.20f) == 4);
    assert(hand_ui_frame_for_elapsed(0.25f) == 0);
    assert(hand_ui_frame_for_elapsed(0.40f) == 0);

    printf("  PASS: test_frame_sequence_once_then_static\n");
}

/* ---- Test: lift pulse scales up early, then returns to rest ---- */
static void test_play_lift_scale_pulses_then_returns_to_rest(void) {
    const float duration = hand_ui_play_animation_duration();
    const float peakTime = duration * 0.25f;

    assert(approx_eq(hand_ui_play_lift_scale(0.00f), 1.0f, 0.001f));
    assert(approx_eq(hand_ui_play_lift_scale(peakTime), HAND_CARD_PLAY_LIFT_PEAK_SCALE, 0.001f));
    assert(hand_ui_play_lift_scale(duration * 0.50f) < HAND_CARD_PLAY_LIFT_PEAK_SCALE);
    assert(hand_ui_play_lift_scale(duration * 0.50f) > 1.0f);
    assert(approx_eq(hand_ui_play_lift_scale(duration), 1.0f, 0.001f));
    assert(approx_eq(hand_ui_play_lift_scale(duration + 0.01f), 1.0f, 0.001f));

    printf("  PASS: test_play_lift_scale_pulses_then_returns_to_rest\n");
}

/* ---- Test: animated mapped-row card scales up without changing center ---- */
static void test_animating_mapped_card_scales_around_center(void) {
    Player p = {0};
    Card assassin = { .card_id = "ASSASSIN_01", .type = "assassin" };
    Texture2D cardSheet = { .id = 22, .width = 768, .height = 1280, .mipmaps = 1, .format = 0 };
    const float peakTime = hand_ui_play_animation_duration() * 0.25f;

    p.side = SIDE_BOTTOM;
    p.handArea = (Rectangle){ 0.0f, 0.0f, 180.0f, 1080.0f };
    p.handCards[0] = &assassin;
    p.handCardAnimating[0] = true;
    p.handCardAnimElapsed[0] = peakTime;

    reset_draw_capture();
    hand_ui_draw(&p, cardSheet);

    assert(g_draw_texture_calls == 1);
    assert(g_drawn_texture_id[0] == cardSheet.id);
    assert(approx_eq(g_drawn_src[0].y, 320.0f, 0.01f));
    assert(approx_eq(g_drawn_dst[0].x, 90.0f, 0.01f));
    assert(approx_eq(g_drawn_dst[0].y, 540.0f, 0.01f));
    assert(approx_eq(g_drawn_dst[0].width,
                     (float)HAND_CARD_WIDTH * HAND_CARD_PLAY_LIFT_PEAK_SCALE, 0.01f));
    assert(approx_eq(g_drawn_dst[0].height,
                     (float)HAND_CARD_HEIGHT * HAND_CARD_PLAY_LIFT_PEAK_SCALE, 0.01f));

    printf("  PASS: test_animating_mapped_card_scales_around_center\n");
}

/* ---- Test: sparse hand uses the current frame and each card's mapped row ---- */
static void test_sparse_hand_uses_current_frame_and_mapped_rows(void) {
    Player p = {0};
    Card knight = { .card_id = "KNIGHT_01", .type = "knight" };
    Card healer = { .card_id = "HEALER_01", .type = "healer" };
    Texture2D cardSheet = { .id = 22, .width = 768, .height = 1280, .mipmaps = 1, .format = 0 };

    p.side = SIDE_TOP;
    p.handArea = (Rectangle){ 1740.0f, 0.0f, 180.0f, 1080.0f };
    p.handCards[1] = &knight;
    p.handCards[4] = &healer;
    p.handCardAnimating[1] = true;
    p.handCardAnimElapsed[1] = 0.20f;

    reset_draw_capture();
    hand_ui_draw(&p, cardSheet);

    assert(g_draw_texture_calls == 2);
    assert(g_drawn_texture_id[0] == cardSheet.id);
    assert(approx_eq(g_drawn_src[0].x, 512.0f, 0.01f));
    assert(approx_eq(g_drawn_src[0].y, 0.0f, 0.01f));
    assert(g_drawn_texture_id[1] == cardSheet.id);
    assert(approx_eq(g_drawn_src[1].x, 0.0f, 0.01f));
    assert(approx_eq(g_drawn_src[1].y, 160.0f, 0.01f));

    Vector2 expected0 = hand_ui_card_center_for_index(p.handArea, 2, 0);
    Vector2 expected1 = hand_ui_card_center_for_index(p.handArea, 2, 1);
    assert(approx_eq(g_drawn_dst[0].x, expected0.x, 0.01f));
    assert(approx_eq(g_drawn_dst[0].y, expected0.y, 0.01f));
    assert(approx_eq(g_drawn_dst[1].x, expected1.x, 0.01f));
    assert(approx_eq(g_drawn_dst[1].y, expected1.y, 0.01f));

    printf("  PASS: test_sparse_hand_uses_current_frame_and_mapped_rows\n");
}

/* ---- Test: animated knight scales at center while keeping atlas row 0 ---- */
static void test_animating_knight_scales_around_center(void) {
    Player p = {0};
    Card knight = { .card_id = "KNIGHT_01", .type = "knight" };
    Texture2D cardSheet = { .id = 22, .width = 768, .height = 1280, .mipmaps = 1, .format = 0 };
    const float peakTime = hand_ui_play_animation_duration() * 0.25f;

    p.side = SIDE_TOP;
    p.handArea = (Rectangle){ 1740.0f, 0.0f, 180.0f, 1080.0f };
    p.handCards[0] = &knight;
    p.handCardAnimating[0] = true;
    p.handCardAnimElapsed[0] = peakTime;

    reset_draw_capture();
    hand_ui_draw(&p, cardSheet);

    assert(g_draw_texture_calls == 1);
    assert(g_drawn_texture_id[0] == cardSheet.id);
    assert(approx_eq(g_drawn_src[0].y, 0.0f, 0.01f));
    assert(approx_eq(g_drawn_rotation[0], 270.0f, 0.01f));
    assert(approx_eq(g_drawn_dst[0].x, 1830.0f, 0.01f));
    assert(approx_eq(g_drawn_dst[0].y, 540.0f, 0.01f));
    assert(approx_eq(g_drawn_dst[0].width,
                     (float)HAND_CARD_WIDTH * HAND_CARD_PLAY_LIFT_PEAK_SCALE, 0.01f));
    assert(approx_eq(g_drawn_dst[0].height,
                     (float)HAND_CARD_HEIGHT * HAND_CARD_PLAY_LIFT_PEAK_SCALE, 0.01f));

    printf("  PASS: test_animating_knight_scales_around_center\n");
}

/* ---- main ---- */
int main(void) {
    printf("Running hand_ui tests...\n");
    test_center_x_matches_handarea_midline();
    test_cards_monotonic_y();
    test_even_stride();
    test_one_card_centered_in_handarea();
    test_three_card_run_centered_in_handarea();
    test_all_run_sizes_centered_in_handarea();
    test_expected_absolute_positions_p1();
    test_expected_absolute_positions_p2();
    test_empty_hand_draws_nothing();
    test_sparse_hand_compacts_draw_positions();
    test_idle_knight_uses_sheet_row_zero();
    test_assassin_uses_mapped_sheet_row();
    test_frame_sequence_once_then_static();
    test_play_lift_scale_pulses_then_returns_to_rest();
    test_animating_mapped_card_scales_around_center();
    test_sparse_hand_uses_current_frame_and_mapped_rows();
    test_animating_knight_scales_around_center();
    printf("\nAll 17 tests passed!\n");
    return 0;
}
