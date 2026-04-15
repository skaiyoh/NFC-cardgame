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
static Rectangle g_lastDrawSource;
static int g_drawTextureProCalls = 0;
static void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
                           Vector2 origin, float rotation, Color tint) {
    (void)texture; (void)dest;
    (void)origin; (void)rotation; (void)tint;
    g_lastDrawSource = source;
    g_drawTextureProCalls++;
}

/* ---- Config stub (needed by sprite_renderer.c) ---- */
#define NFC_CARDGAME_CONFIG_H
#define CHAR_BASE_PATH      "stub/"
#define CHAR_KNIGHT_PATH    "stub/"
#define CHAR_HEALER_PATH    "stub/"
#define CHAR_ASSASSIN_PATH  "stub/"
#define CHAR_BRUTE_PATH     "stub/"
#define CHAR_FARMER_PATH    "stub/"
#define CHAR_BIRD_PATH      "stub/"
#define CHAR_FISHFING_PATH  "stub/"
#define CHAR_KING_PATH      "stub/"
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
    int visualLoops;
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
    assert(approx_eq(spec->cycleSeconds, 0.0f, 0.01f));
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

static void test_spec_lookup_healer_blubert_timing(void) {
    const EntityAnimSpec *walk = anim_spec_get(SPRITE_TYPE_HEALER, ANIM_WALK);
    const EntityAnimSpec *attack = anim_spec_get(SPRITE_TYPE_HEALER, ANIM_ATTACK);

    assert(walk != NULL);
    assert(attack != NULL);
    assert(walk->mode == ANIM_PLAY_LOOP);
    assert(approx_eq(walk->cycleSeconds, 0.70f, 0.01f));
    assert(attack->mode == ANIM_PLAY_ONCE);
    assert(approx_eq(attack->cycleSeconds, 0.60f, 0.01f));
    assert(approx_eq(attack->hitNormalized, 0.5f, 0.01f));
    assert(attack->lockFacing == true);
}

static void test_spec_lookup_death_oneshot(void) {
    for (int t = 0; t < SPRITE_TYPE_COUNT; t++) {
        const EntityAnimSpec *spec = anim_spec_get((SpriteType)t, ANIM_DEATH);
        assert(spec->anim == ANIM_DEATH);
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

static void test_sheet_lookup_resolves_idle_and_run_to_walk(void) {
    CharacterSprite cs = {0};
    cs.anims[ANIM_WALK].frameCount = 8;
    cs.anims[ANIM_WALK].frameWidth = 79;
    cs.anims[ANIM_WALK].frameHeight = 79;

    assert(sprite_sheet_get(&cs, ANIM_IDLE) == &cs.anims[ANIM_WALK]);
    assert(sprite_sheet_get(&cs, ANIM_RUN) == &cs.anims[ANIM_WALK]);
}

static void test_sheet_lookup_resolves_death_to_hurt(void) {
    CharacterSprite cs = {0};
    cs.anims[ANIM_HURT].frameCount = 4;
    cs.anims[ANIM_HURT].frameWidth = 79;
    cs.anims[ANIM_HURT].frameHeight = 79;

    assert(sprite_sheet_get(&cs, ANIM_DEATH) == &cs.anims[ANIM_HURT]);
}

static void test_sheet_lookup_prefers_authored_clip(void) {
    CharacterSprite cs = {0};
    cs.anims[ANIM_IDLE].frameCount = 4;
    cs.anims[ANIM_IDLE].frameWidth = 79;
    cs.anims[ANIM_IDLE].frameHeight = 79;
    cs.anims[ANIM_WALK].frameCount = 8;
    cs.anims[ANIM_WALK].frameWidth = 79;
    cs.anims[ANIM_WALK].frameHeight = 79;

    assert(sprite_sheet_get(&cs, ANIM_IDLE) == &cs.anims[ANIM_IDLE]);
}

static void test_sheet_lookup_resolves_walk_to_idle_when_needed(void) {
    CharacterSprite cs = {0};
    cs.anims[ANIM_IDLE].frameCount = 4;
    cs.anims[ANIM_IDLE].frameWidth = 79;
    cs.anims[ANIM_IDLE].frameHeight = 79;

    assert(sprite_sheet_get(&cs, ANIM_WALK) == &cs.anims[ANIM_IDLE]);
    assert(sprite_sheet_get(&cs, ANIM_RUN) == &cs.anims[ANIM_IDLE]);
}

static void test_single_row_sheet_reuses_row_zero_for_all_directions(void) {
    SpriteSheet sheet = {0};
    sheet.frameCount = 8;
    sheet.sourceRowCount = 1;

    assert(sheet_source_row(&sheet, DIR_SIDE) == 0);
    assert(sheet_source_row(&sheet, DIR_DOWN) == 0);
    assert(sheet_source_row(&sheet, DIR_UP) == 0);
}

static void test_king_idle_manifest_and_atlas_match_new_sheet(void) {
    const SpriteSheetManifestEntry *manifestEntry = NULL;
    const SpriteSheetAtlasEntry *atlasEntry = NULL;

    for (int i = 0; i < kSpriteSheetManifestCount; i++) {
        const SpriteSheetManifestEntry *entry = &kSpriteSheetManifest[i];
        if (!entry->isBaseFallback &&
            entry->spriteType == SPRITE_TYPE_BASE &&
            entry->anim == ANIM_IDLE) {
            manifestEntry = entry;
            break;
        }
    }

    assert(manifestEntry != NULL);
    assert(strcmp(manifestEntry->path, "src/assets/characters/King/king_idle_sheet.png") == 0);
    assert(manifestEntry->frameCount == 8);
    assert(manifestEntry->sourceRowCount == 1);

    for (int i = 0; i < kSpriteSheetAtlasCount; i++) {
        const SpriteSheetAtlasEntry *entry = &kSpriteSheetAtlas[i];
        if (strcmp(entry->path, "src/assets/characters/King/king_idle_sheet.png") == 0) {
            atlasEntry = entry;
            break;
        }
    }

    assert(atlasEntry != NULL);
    assert(atlasEntry->frameCount == 8);
    assert(atlasEntry->sourceRowCount == 1);
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

static void test_spec_farmer_idle_is_real_loop(void) {
    // Both Cheffy variants play a real looping idle (used for no-food wait and
    // queue-wait), unlike other troops that degenerate to frame 0.
    const EntityAnimSpec *empty = anim_spec_get(SPRITE_TYPE_FARMER, ANIM_IDLE);
    const EntityAnimSpec *full = anim_spec_get(SPRITE_TYPE_FARMER_FULL, ANIM_IDLE);
    assert(empty->mode == ANIM_PLAY_LOOP);
    assert(empty->cycleSeconds > 0.0f);
    assert(full->mode == ANIM_PLAY_LOOP);
    assert(full->cycleSeconds > 0.0f);
}

static void test_spec_farmer_attack_is_gather_one_shot(void) {
    // Farmer ATTACK is reused as the gather/deposit one-shot. Both variants
    // must have a non-zero duration and satisfy the attack-spec invariants.
    const EntityAnimSpec *empty = anim_spec_get(SPRITE_TYPE_FARMER, ANIM_ATTACK);
    const EntityAnimSpec *full = anim_spec_get(SPRITE_TYPE_FARMER_FULL, ANIM_ATTACK);
    assert(empty->mode == ANIM_PLAY_ONCE);
    assert(empty->cycleSeconds > 0.0f);
    assert(empty->lockFacing == true);
    assert(empty->visualLoops == 2);
    assert(full->mode == ANIM_PLAY_ONCE);
    assert(full->cycleSeconds > 0.0f);
    assert(full->lockFacing == true);
    assert(full->visualLoops == 2);
}

/* ==== Visible-bounds rotation tests ==== */

// Helper: build a minimal 1-frame CharacterSprite with known visibleBounds.
// frameWidth=64, frameHeight=64, 1 frame, DIR_COUNT rows.
// visibleBounds for DIR_SIDE frame 0: off-center rect at (10, 8, 20, 40)
// — intentionally asymmetric so rotation-center bugs are visible.
static CharacterSprite make_test_sprite(void) {
    CharacterSprite cs = {0};
    SpriteSheet *sheet = &cs.anims[ANIM_IDLE];
    sheet->frameWidth = 64;
    sheet->frameHeight = 64;
    sheet->frameCount = 1;
    sheet->visibleBounds = calloc(1 * DIR_COUNT, sizeof(Rectangle));
    for (int d = 0; d < DIR_COUNT; d++) {
        sheet->visibleBounds[d] = (Rectangle){10.0f, 8.0f, 20.0f, 40.0f};
    }
    return cs;
}

static CharacterSprite make_draw_test_sprite(AnimationType anim, int frameCount) {
    CharacterSprite cs = {0};
    SpriteSheet *sheet = &cs.anims[anim];
    sheet->texture = (Texture2D){ .id = 1, .width = frameCount * 10, .height = 10 };
    sheet->frameWidth = 10;
    sheet->frameHeight = 10;
    sheet->frameCount = frameCount;
    return cs;
}

static void free_test_sprite(CharacterSprite *cs) {
    free(cs->anims[ANIM_IDLE].visibleBounds);
    cs->anims[ANIM_IDLE].visibleBounds = NULL;
}

static bool rect_approx_eq(Rectangle a, Rectangle b, float eps) {
    return approx_eq(a.x, b.x, eps) &&
           approx_eq(a.y, b.y, eps) &&
           approx_eq(a.width, b.width, eps) &&
           approx_eq(a.height, b.height, eps);
}

static void test_bounds_unrotated_unflipped(void) {
    CharacterSprite cs = make_test_sprite();
    AnimState s;
    anim_state_init(&s, ANIM_IDLE, DIR_SIDE, 1.0f, false);
    s.flipH = false;

    Vector2 pos = {100.0f, 100.0f};
    float scale = 1.0f;
    Rectangle vb = sprite_visible_bounds(&cs, &s, pos, scale, 0.0f);

    // Frame top-left = pos - frame_size/2 = (68, 68)
    // Visible rect at (10,8) size (20,40) → world (78, 76, 20, 40)
    Rectangle expected = {78.0f, 76.0f, 20.0f, 40.0f};
    assert(rect_approx_eq(vb, expected, 0.01f));
    free_test_sprite(&cs);
}

static void test_bounds_unrotated_flipped(void) {
    CharacterSprite cs = make_test_sprite();
    AnimState s;
    anim_state_init(&s, ANIM_IDLE, DIR_SIDE, 1.0f, false);
    s.flipH = true;

    Vector2 pos = {100.0f, 100.0f};
    float scale = 1.0f;
    Rectangle vb = sprite_visible_bounds(&cs, &s, pos, scale, 0.0f);

    // flipH mirrors: vx = 64 - (10 + 20) = 34
    // World: (68 + 34, 68 + 8, 20, 40) = (102, 76, 20, 40)
    Rectangle expected = {102.0f, 76.0f, 20.0f, 40.0f};
    assert(rect_approx_eq(vb, expected, 0.01f));
    free_test_sprite(&cs);
}

static void test_bounds_180_rotation(void) {
    CharacterSprite cs = make_test_sprite();
    AnimState s;
    anim_state_init(&s, ANIM_IDLE, DIR_SIDE, 1.0f, false);
    s.flipH = false;

    Vector2 pos = {100.0f, 100.0f};
    float scale = 1.0f;
    Rectangle vb = sprite_visible_bounds(&cs, &s, pos, scale, 180.0f);

    // Corners relative to frame center (32, 32):
    //   TL=(-22,-24), TR=(-2,-24), BR=(-2,16), BL=(-22,16)
    // 180° rotation (negate both): (22,24), (2,24), (2,-16), (22,-16)
    // World x: 100 + [2, 22] = [102, 122], width=20
    // World y: 100 + [-16, 24] = [84, 124], height=40
    Rectangle expected = {102.0f, 84.0f, 20.0f, 40.0f};
    assert(rect_approx_eq(vb, expected, 0.1f));
    free_test_sprite(&cs);
}

static void test_bounds_90_rotation(void) {
    CharacterSprite cs = make_test_sprite();
    AnimState s;
    anim_state_init(&s, ANIM_IDLE, DIR_SIDE, 1.0f, false);
    s.flipH = false;

    Vector2 pos = {100.0f, 100.0f};
    float scale = 1.0f;
    Rectangle vb = sprite_visible_bounds(&cs, &s, pos, scale, 90.0f);

    // Corners relative to center (32, 32):
    //   TL=(-22,-24), TR=(-2,-24), BR=(-2,16), BL=(-22,16)
    // 90° CW: (x,y) → (y, -x) via cos90=0, sin90=1
    //   → (−24,22), (−24,2), (16,2), (16,−22)  [wait, let me recalculate]
    //   rx = x*cos - y*sin = x*0 - y*1 = -y
    //   ry = x*sin + y*cos = x*1 + y*0 = x
    //   TL: (-22,-24)→(24,-22), TR: (-2,-24)→(24,-2),
    //   BR: (-2,16)→(-16,-2), BL: (-22,16)→(-16,-22)
    // World x: 100 + [-16, 24] = [84, 124], width=40
    // World y: 100 + [-22, -2] = [78, 98], height=20
    Rectangle expected = {84.0f, 78.0f, 40.0f, 20.0f};
    assert(rect_approx_eq(vb, expected, 0.1f));
    free_test_sprite(&cs);
}

static void test_bounds_flipped_plus_rotated(void) {
    CharacterSprite cs = make_test_sprite();
    AnimState s;
    anim_state_init(&s, ANIM_IDLE, DIR_SIDE, 1.0f, false);
    s.flipH = true;

    Vector2 pos = {100.0f, 100.0f};
    float scale = 1.0f;
    Rectangle vb = sprite_visible_bounds(&cs, &s, pos, scale, 90.0f);

    // FlipH first: vx = 64 - (10+20) = 34, so visible rect = (34,8,20,40)
    // Corners relative to center (32,32):
    //   TL=(2,-24), TR=(22,-24), BR=(22,16), BL=(2,16)
    // 90° CW: rx=-y, ry=x
    //   TL:(24,2), TR:(24,22), BR:(-16,22), BL:(-16,2)
    // World x: 100 + [-16, 24] = [84, 124], width=40
    // World y: 100 + [2, 22] = [102, 122], height=20
    Rectangle expected = {84.0f, 102.0f, 40.0f, 20.0f};
    assert(rect_approx_eq(vb, expected, 0.1f));
    free_test_sprite(&cs);
}

static void test_visual_loops_repeat_frames_within_one_shot(void) {
    CharacterSprite cs = make_draw_test_sprite(ANIM_ATTACK, 4);
    AnimState s;
    anim_state_init_with_loops(&s, ANIM_ATTACK, DIR_SIDE, 0.8f, true, 2);

    g_drawTextureProCalls = 0;

    s.normalizedTime = 0.10f;
    sprite_draw(&cs, &s, (Vector2){0.0f, 0.0f}, 1.0f, 0.0f);
    assert(g_drawTextureProCalls == 1);
    assert(approx_eq(g_lastDrawSource.x, 0.0f, 0.001f));

    s.normalizedTime = 0.15f;
    sprite_draw(&cs, &s, (Vector2){0.0f, 0.0f}, 1.0f, 0.0f);
    assert(approx_eq(g_lastDrawSource.x, 10.0f, 0.001f));

    s.normalizedTime = 0.40f;
    sprite_draw(&cs, &s, (Vector2){0.0f, 0.0f}, 1.0f, 0.0f);
    assert(approx_eq(g_lastDrawSource.x, 30.0f, 0.001f));

    s.normalizedTime = 0.55f;
    sprite_draw(&cs, &s, (Vector2){0.0f, 0.0f}, 1.0f, 0.0f);
    assert(approx_eq(g_lastDrawSource.x, 0.0f, 0.001f));

    s.normalizedTime = 0.65f;
    sprite_draw(&cs, &s, (Vector2){0.0f, 0.0f}, 1.0f, 0.0f);
    assert(approx_eq(g_lastDrawSource.x, 10.0f, 0.001f));

    s.normalizedTime = 1.0f;
    s.finished = true;
    sprite_draw(&cs, &s, (Vector2){0.0f, 0.0f}, 1.0f, 0.0f);
    assert(approx_eq(g_lastDrawSource.x, 30.0f, 0.001f));
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
    RUN_TEST(test_spec_lookup_healer_blubert_timing);
    RUN_TEST(test_spec_lookup_death_oneshot);
    RUN_TEST(test_spec_lookup_out_of_bounds);
    RUN_TEST(test_sheet_lookup_resolves_idle_and_run_to_walk);
    RUN_TEST(test_sheet_lookup_resolves_death_to_hurt);
    RUN_TEST(test_sheet_lookup_prefers_authored_clip);
    RUN_TEST(test_sheet_lookup_resolves_walk_to_idle_when_needed);
    RUN_TEST(test_single_row_sheet_reuses_row_zero_for_all_directions);
    RUN_TEST(test_king_idle_manifest_and_atlas_match_new_sheet);

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
    RUN_TEST(test_spec_farmer_idle_is_real_loop);
    RUN_TEST(test_spec_farmer_attack_is_gather_one_shot);

    // Visible bounds rotation
    RUN_TEST(test_bounds_unrotated_unflipped);
    RUN_TEST(test_bounds_unrotated_flipped);
    RUN_TEST(test_bounds_180_rotation);
    RUN_TEST(test_bounds_90_rotation);
    RUN_TEST(test_bounds_flipped_plus_rotated);
    RUN_TEST(test_visual_loops_repeat_frames_within_one_shot);

    printf("\nAll %d tests passed!\n", tests_passed);
    return 0;
}
