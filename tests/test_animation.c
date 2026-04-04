/*
 * Unit tests for animation playback (sprite_renderer.c anim_state_*)
 * and animation policy (entity_animation.c).
 *
 * Self-contained: includes sprite_renderer.c and entity_animation.c directly
 * with Raylib stubs. This tests the PRODUCTION playback implementation.
 *
 * SYNC REQUIREMENT: The Raylib type stubs below must be layout-compatible
 * with the real Raylib types used by sprite_renderer.h.
 */

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

/* ---- Raylib stubs (layout-compatible, no GPU calls) ---- */
#define RAYLIB_H  /* prevent real raylib.h from being included */

typedef struct { float x; float y; } Vector2;
typedef struct { float x; float y; float width; float height; } Rectangle;
typedef struct {
    unsigned int id;
    int width;
    int height;
    int mipmaps;
    int format;
} Texture2D;
typedef Texture2D Texture;
typedef struct {
    void *data;
    int width;
    int height;
    int mipmaps;
    int format;
} Image;
typedef struct { unsigned char r, g, b, a; } Color;

#define WHITE (Color){255, 255, 255, 255}
#define TEXTURE_FILTER_POINT 0

/* Raylib function stubs — never called during playback tests */
static Texture2D LoadTexture(const char *fileName) {
    (void)fileName;
    return (Texture2D){0};
}
static void UnloadTexture(Texture2D texture) { (void)texture; }
static void SetTextureFilter(Texture2D texture, int filter) {
    (void)texture; (void)filter;
}
static Image LoadImage(const char *fileName) {
    (void)fileName;
    return (Image){0};
}
static void UnloadImage(Image image) { (void)image; }
static Color *LoadImageColors(Image image) { (void)image; return NULL; }
static void UnloadImageColors(Color *colors) { (void)colors; }
static void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
                           Vector2 origin, float rotation, Color tint) {
    (void)texture; (void)source; (void)dest;
    (void)origin; (void)rotation; (void)tint;
}

/* ---- Config stub (needed by sprite_renderer.c) ---- */
#define NFC_CARDGAME_CONFIG_H
#define CHAR_BASE_PATH      "stub/"
#define CHAR_KNIGHT_PATH    "stub/"
#define CHAR_HEALER_PATH    "stub/"
#define CHAR_ASSASSIN_PATH  "stub/"
#define CHAR_BRUTE_PATH     "stub/"
#define CHAR_FARMER_PATH    "stub/"
#define SPRITE_FRAME_SIZE   79

/* ---- Include production sprite_renderer.c (real playback code) ---- */
/* sprite_frame_atlas.h is included normally (static const data, harmless). */
#include "../src/rendering/sprite_renderer.c"

/* ---- Prevent entity_animation.h from re-including sprite_renderer.h ---- */
#define NFC_CARDGAME_ENTITY_ANIMATION_H

typedef enum { ANIM_PLAY_LOOP, ANIM_PLAY_ONCE } AnimPlayMode;

typedef struct {
    AnimationType anim;
    AnimPlayMode mode;
    float cycleSeconds;
    float hitNormalized;
    bool lockFacing;
    bool removeOnFinish;
} EntityAnimSpec;

#define WALK_PIXELS_PER_CYCLE 64.0f
#define CYCLE_MIN_SECONDS 0.3f
#define CYCLE_MAX_SECONDS 3.0f

const EntityAnimSpec *anim_spec_get(SpriteType spriteType, AnimationType animType);
float anim_walk_cycle_seconds(float moveSpeed, float pixelsPerCycle);
float anim_attack_cycle_seconds(float attackSpeed);

/* ---- Include production entity_animation.c ---- */
#include "../src/entities/entity_animation.c"

/* ---- Test helpers ---- */
static bool approx_eq(float a, float b, float eps) {
    return fabsf(a - b) < eps;
}

// Check if a normalized marker was crossed between prev and curr
static bool marker_crossed(float prev, float curr, float marker) {
    return prev < marker && curr >= marker;
}

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

/* ==== Playback core tests ==== */

static void test_looping_wraps(void) {
    AnimState s;
    anim_state_init(&s, ANIM_WALK, DIR_DOWN, 0.8f, false);

    // Advance to 90% of cycle
    AnimPlaybackEvent e1 = anim_state_update(&s, 0.72f);
    assert(!e1.loopedThisTick);
    assert(approx_eq(s.normalizedTime, 0.9f, 0.01f));

    // Advance past the cycle boundary
    AnimPlaybackEvent e2 = anim_state_update(&s, 0.16f);
    assert(e2.loopedThisTick);
    assert(s.normalizedTime < 0.2f); // wrapped around
    assert(!s.finished);
}

static void test_oneshot_finishes(void) {
    AnimState s;
    anim_state_init(&s, ANIM_DEATH, DIR_DOWN, 0.75f, true);

    // Advance to 80%
    AnimPlaybackEvent e1 = anim_state_update(&s, 0.6f);
    assert(!e1.finishedThisTick);
    assert(!s.finished);

    // Advance past end
    AnimPlaybackEvent e2 = anim_state_update(&s, 0.3f);
    assert(e2.finishedThisTick);
    assert(s.finished);
    assert(approx_eq(s.normalizedTime, 1.0f, 0.001f));

    // Subsequent updates should NOT re-fire finishedThisTick
    AnimPlaybackEvent e3 = anim_state_update(&s, 0.1f);
    assert(!e3.finishedThisTick);
    assert(s.finished);
}

static void test_normalized_time_accuracy(void) {
    AnimState s;
    anim_state_init(&s, ANIM_IDLE, DIR_DOWN, 1.0f, false);

    anim_state_update(&s, 0.5f);
    assert(approx_eq(s.normalizedTime, 0.5f, 0.001f));

    anim_state_update(&s, 0.25f);
    assert(approx_eq(s.normalizedTime, 0.75f, 0.001f));
}

static void test_zero_cycle_duration_safety(void) {
    AnimState s;
    anim_state_init(&s, ANIM_IDLE, DIR_DOWN, 0.0f, false);

    // Should not crash or divide by zero
    AnimPlaybackEvent e = anim_state_update(&s, 0.1f);
    assert(approx_eq(s.normalizedTime, 0.0f, 0.001f));
    (void)e;

    // One-shot with zero duration should finish immediately
    AnimState s2;
    anim_state_init(&s2, ANIM_DEATH, DIR_DOWN, 0.0f, true);
    AnimPlaybackEvent e2 = anim_state_update(&s2, 0.0f);
    assert(e2.finishedThisTick);
    assert(s2.finished);
}

static void test_marker_crossing_normal(void) {
    AnimState s;
    anim_state_init(&s, ANIM_ATTACK, DIR_DOWN, 1.0f, true);

    // Advance to 40%
    AnimPlaybackEvent e1 = anim_state_update(&s, 0.4f);
    float marker = 0.5f;
    assert(!marker_crossed(e1.prevNormalized, e1.currNormalized, marker));

    // Advance to 60% — crosses 0.5 marker
    AnimPlaybackEvent e2 = anim_state_update(&s, 0.2f);
    assert(marker_crossed(e2.prevNormalized, e2.currNormalized, marker));
}

static void test_marker_crossing_large_dt(void) {
    // Simulate a large frame skip that jumps from 10% to 90%
    AnimState s;
    anim_state_init(&s, ANIM_ATTACK, DIR_DOWN, 1.0f, true);

    anim_state_update(&s, 0.1f); // at 10%
    AnimPlaybackEvent e = anim_state_update(&s, 0.8f); // jumps to 90%

    float marker = 0.5f;
    assert(marker_crossed(e.prevNormalized, e.currNormalized, marker));
}

static void test_no_double_hit_on_finish(void) {
    // One-shot: marker at 0.5, advance in one big step past both marker and end
    AnimState s;
    anim_state_init(&s, ANIM_ATTACK, DIR_DOWN, 0.5f, true);

    AnimPlaybackEvent e = anim_state_update(&s, 1.0f); // way past end

    float marker = 0.5f;
    // prevNormalized=0.0, currNormalized=1.0 — marker crossed exactly once
    assert(marker_crossed(e.prevNormalized, e.currNormalized, marker));
    assert(e.finishedThisTick);

    // Next tick: already finished, no more crossing
    AnimPlaybackEvent e2 = anim_state_update(&s, 0.1f);
    assert(!e2.finishedThisTick);
    // prevNormalized and currNormalized are both 1.0
    assert(!marker_crossed(e2.prevNormalized, e2.currNormalized, marker));
}

static void test_prev_curr_on_loop(void) {
    // Looping: verify prev < curr on normal tick, and wrap behavior
    AnimState s;
    anim_state_init(&s, ANIM_WALK, DIR_DOWN, 1.0f, false);

    anim_state_update(&s, 0.8f); // at 80%
    AnimPlaybackEvent e = anim_state_update(&s, 0.3f); // wraps: 80%→10%

    assert(e.loopedThisTick);
    assert(e.prevNormalized > 0.7f); // was at ~0.8
    assert(e.currNormalized < 0.2f); // wrapped to ~0.1
}

/* ==== Spec lookup tests ==== */

static void test_spec_lookup_knight_idle(void) {
    const EntityAnimSpec *spec = anim_spec_get(SPRITE_TYPE_KNIGHT, ANIM_IDLE);
    assert(spec != NULL);
    assert(spec->anim == ANIM_IDLE);
    assert(spec->mode == ANIM_PLAY_LOOP);
    assert(approx_eq(spec->cycleSeconds, 0.5f, 0.01f));
    assert(spec->hitNormalized < 0.0f); // -1.0
}

static void test_spec_lookup_knight_attack(void) {
    const EntityAnimSpec *spec = anim_spec_get(SPRITE_TYPE_KNIGHT, ANIM_ATTACK);
    assert(spec != NULL);
    assert(spec->anim == ANIM_ATTACK);
    assert(spec->mode == ANIM_PLAY_ONCE);
    assert(approx_eq(spec->hitNormalized, 0.5f, 0.01f));
    assert(spec->lockFacing == true);
}

static void test_spec_lookup_brute_attack(void) {
    const EntityAnimSpec *spec = anim_spec_get(SPRITE_TYPE_BRUTE, ANIM_ATTACK);
    assert(spec != NULL);
    assert(approx_eq(spec->hitNormalized, 0.6f, 0.01f)); // brute hits later
}

static void test_spec_lookup_death_oneshot(void) {
    for (int t = 0; t < SPRITE_TYPE_COUNT; t++) {
        const EntityAnimSpec *spec = anim_spec_get((SpriteType)t, ANIM_DEATH);
        assert(spec->mode == ANIM_PLAY_ONCE);
        assert(spec->removeOnFinish == true);
    }
}

static void test_spec_lookup_out_of_bounds(void) {
    const EntityAnimSpec *spec = anim_spec_get((SpriteType)999, ANIM_IDLE);
    assert(spec != NULL); // returns fallback
    assert(spec->anim == ANIM_IDLE);
    assert(spec->mode == ANIM_PLAY_LOOP);

    const EntityAnimSpec *spec2 = anim_spec_get(SPRITE_TYPE_KNIGHT, (AnimationType)999);
    assert(spec2 != NULL);
}

/* ==== Cycle calculation tests ==== */

static void test_walk_cycle_calculation(void) {
    // assassin: moveSpeed=100.0 → 64.0/100.0 = 0.64s
    float cycle = anim_walk_cycle_seconds(100.0f, 64.0f);
    assert(approx_eq(cycle, 0.64f, 0.01f));

    // brute: moveSpeed=28.0 → 64.0/28.0 = 2.29s
    float cycle2 = anim_walk_cycle_seconds(28.0f, 64.0f);
    assert(approx_eq(cycle2, 2.29f, 0.01f));
}

static void test_walk_cycle_clamped(void) {
    // Very fast: should clamp to minimum
    float fast = anim_walk_cycle_seconds(10000.0f, 64.0f);
    assert(approx_eq(fast, CYCLE_MIN_SECONDS, 0.01f));

    // Very slow: should clamp to maximum
    float slow = anim_walk_cycle_seconds(1.0f, 64.0f);
    assert(approx_eq(slow, CYCLE_MAX_SECONDS, 0.01f));
}

static void test_walk_cycle_zero_safety(void) {
    float zero_speed = anim_walk_cycle_seconds(0.0f, 64.0f);
    assert(approx_eq(zero_speed, CYCLE_MAX_SECONDS, 0.01f));

    float zero_pixels = anim_walk_cycle_seconds(100.0f, 0.0f);
    assert(approx_eq(zero_pixels, CYCLE_MAX_SECONDS, 0.01f));
}

static void test_attack_cycle_calculation(void) {
    // attackSpeed=1.0 → 1.0s
    float cycle = anim_attack_cycle_seconds(1.0f);
    assert(approx_eq(cycle, 1.0f, 0.01f));

    // assassin: attackSpeed=1.8 → 0.556s
    float fast = anim_attack_cycle_seconds(1.8f);
    assert(approx_eq(fast, 0.556f, 0.01f));

    // brute: attackSpeed=0.5 → 2.0s
    float slow = anim_attack_cycle_seconds(0.5f);
    assert(approx_eq(slow, 2.0f, 0.01f));
}

static void test_attack_cycle_clamped(void) {
    // Very fast attack: clamp to minimum
    float fast = anim_attack_cycle_seconds(100.0f);
    assert(approx_eq(fast, CYCLE_MIN_SECONDS, 0.01f));

    // Zero attack speed: clamp to maximum
    float zero = anim_attack_cycle_seconds(0.0f);
    assert(approx_eq(zero, CYCLE_MAX_SECONDS, 0.01f));
}

/* ==== Restart clip test ==== */

static void test_restart_clip(void) {
    AnimState s;
    anim_state_init(&s, ANIM_ATTACK, DIR_SIDE, 0.6f, true);

    // Advance to completion
    anim_state_update(&s, 1.0f);
    assert(s.finished);
    assert(approx_eq(s.normalizedTime, 1.0f, 0.001f));

    // Restart (simulating entity_restart_clip)
    s.elapsed = 0.0f;
    s.normalizedTime = 0.0f;
    s.finished = false;

    // Should be back at start
    assert(!s.finished);
    assert(approx_eq(s.normalizedTime, 0.0f, 0.001f));

    // Can advance again
    AnimPlaybackEvent e = anim_state_update(&s, 0.3f);
    assert(!e.finishedThisTick);
    assert(approx_eq(s.normalizedTime, 0.5f, 0.01f));
}

/* ==== Attack hit-sync simulation tests ==== */

// Simulate the attack loop logic from entity_update without needing the full
// entity system. Tests the marker-crossing + restart chaining pattern.

static void test_attack_hit_fires_once_at_marker(void) {
    // Simulate knight attack: 1.0s cycle, hitNormalized=0.5
    AnimState s;
    anim_state_init(&s, ANIM_ATTACK, DIR_SIDE, 1.0f, true);
    float hitMarker = 0.5f;
    int hitCount = 0;

    // Advance in 16ms ticks (~60fps) until finished
    for (int i = 0; i < 100 && !s.finished; i++) {
        AnimPlaybackEvent e = anim_state_update(&s, 1.0f / 60.0f);
        if (e.prevNormalized < hitMarker && e.currNormalized >= hitMarker) {
            hitCount++;
        }
    }

    assert(hitCount == 1);
    assert(s.finished);
}

static void test_attack_chained_swings(void) {
    // Simulate two chained attack swings via restart
    AnimState s;
    anim_state_init(&s, ANIM_ATTACK, DIR_SIDE, 0.5f, true);
    float hitMarker = 0.5f;
    int hitCount = 0;
    int swingCount = 0;

    for (int i = 0; i < 200; i++) {
        AnimPlaybackEvent e = anim_state_update(&s, 1.0f / 60.0f);
        if (e.prevNormalized < hitMarker && e.currNormalized >= hitMarker) {
            hitCount++;
        }
        if (e.finishedThisTick) {
            swingCount++;
            if (swingCount >= 2) break;
            // Chain: restart clip
            s.elapsed = 0.0f;
            s.normalizedTime = 0.0f;
            s.finished = false;
        }
    }

    assert(swingCount == 2);
    assert(hitCount == 2);
}

static void test_attack_spec_all_types_have_hit_marker(void) {
    // Every attack-capable sprite type should have a valid hit marker
    for (int t = 0; t < SPRITE_TYPE_COUNT; t++) {
        if (t == SPRITE_TYPE_BASE) continue; // buildings have no attack
        const EntityAnimSpec *spec = anim_spec_get((SpriteType)t, ANIM_ATTACK);
        assert(spec->hitNormalized > 0.0f && spec->hitNormalized <= 1.0f);
        assert(spec->mode == ANIM_PLAY_ONCE);
        assert(spec->lockFacing == true);
    }
}

static void test_attack_brute_hit_later_than_knight(void) {
    const EntityAnimSpec *knight = anim_spec_get(SPRITE_TYPE_KNIGHT, ANIM_ATTACK);
    const EntityAnimSpec *brute = anim_spec_get(SPRITE_TYPE_BRUTE, ANIM_ATTACK);
    assert(brute->hitNormalized > knight->hitNormalized);
}

/* ---- Main ---- */
int main(void) {
    printf("Running animation tests...\n");

    // Playback core (exercises PRODUCTION anim_state_init/anim_state_update)
    RUN_TEST(test_looping_wraps);
    RUN_TEST(test_oneshot_finishes);
    RUN_TEST(test_normalized_time_accuracy);
    RUN_TEST(test_zero_cycle_duration_safety);
    RUN_TEST(test_marker_crossing_normal);
    RUN_TEST(test_marker_crossing_large_dt);
    RUN_TEST(test_no_double_hit_on_finish);
    RUN_TEST(test_prev_curr_on_loop);

    // Spec lookup
    RUN_TEST(test_spec_lookup_knight_idle);
    RUN_TEST(test_spec_lookup_knight_attack);
    RUN_TEST(test_spec_lookup_brute_attack);
    RUN_TEST(test_spec_lookup_death_oneshot);
    RUN_TEST(test_spec_lookup_out_of_bounds);

    // Cycle calculations
    RUN_TEST(test_walk_cycle_calculation);
    RUN_TEST(test_walk_cycle_clamped);
    RUN_TEST(test_walk_cycle_zero_safety);
    RUN_TEST(test_attack_cycle_calculation);
    RUN_TEST(test_attack_cycle_clamped);

    // Restart
    RUN_TEST(test_restart_clip);

    // Attack hit-sync
    RUN_TEST(test_attack_hit_fires_once_at_marker);
    RUN_TEST(test_attack_chained_swings);
    RUN_TEST(test_attack_spec_all_types_have_hit_marker);
    RUN_TEST(test_attack_brute_hit_later_than_knight);

    printf("\nAll %d tests passed!\n", tests_passed);
    return 0;
}
