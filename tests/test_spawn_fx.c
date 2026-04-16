/*
 * Unit tests for src/rendering/spawn_fx.c and src/systems/spawn.c.
 *
 * Self-contained: includes production code directly with minimal Raylib/type stubs.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ---- Config stub ---- */
#define NFC_CARDGAME_CONFIG_H
#define FX_SMOKE_PATH "src/assets/fx/smoke.png"
#define FX_EXPLOSION_PATH "src/assets/fx/explosion.png"
#define FX_BLOOD_PATH "src/assets/fx/blood.png"

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

static const char *g_lastTexturePath = NULL;
static const char *g_loadedTexturePaths[8] = {0};
static int g_loadTextureCalls = 0;
static int g_unloadTextureCalls = 0;
static int g_drawCalls = 0;
static Texture2D g_lastDrawTexture = {0};
static Rectangle g_lastDrawSrc = {0};
static Rectangle g_lastDrawDst = {0};
static Vector2 g_lastDrawOrigin = {0};
static float g_lastDrawRotation = 0.0f;

static Texture2D LoadTexture(const char *fileName) {
    g_lastTexturePath = fileName;
    g_loadedTexturePaths[g_loadTextureCalls++] = fileName;
    if (strcmp(fileName, FX_SMOKE_PATH) == 0) {
        return (Texture2D){ .id = 1, .width = 704, .height = 960, .mipmaps = 1, .format = 7 };
    }
    if (strcmp(fileName, FX_EXPLOSION_PATH) == 0) {
        return (Texture2D){ .id = 2, .width = 384, .height = 32, .mipmaps = 1, .format = 7 };
    }
    if (strcmp(fileName, FX_BLOOD_PATH) == 0) {
        return (Texture2D){ .id = 3, .width = 768, .height = 32, .mipmaps = 1, .format = 7 };
    }
    return (Texture2D){0};
}

static void UnloadTexture(Texture2D texture) {
    (void)texture;
    g_unloadTextureCalls++;
}

static void SetTextureFilter(Texture2D texture, int filter) {
    (void)texture;
    (void)filter;
}

static void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
                           Vector2 origin, float rotation, Color tint) {
    (void)tint;
    g_drawCalls++;
    g_lastDrawTexture = texture;
    g_lastDrawSrc = source;
    g_lastDrawDst = dest;
    g_lastDrawOrigin = origin;
    g_lastDrawRotation = rotation;
}

/* ---- Production spawn FX types ---- */
#include "../src/rendering/spawn_fx.h"

/* ---- Minimal game/battlefield stubs for spawn.c ---- */
#define NFC_CARDGAME_BATTLEFIELD_H
#define NFC_CARDGAME_TYPES_H

typedef struct Entity {
    Vector2 position;
    float spriteScale;
} Entity;

typedef struct Battlefield {
    Entity *entities[8];
    int entityCount;
} Battlefield;

typedef struct GameState {
    SpawnFxSystem spawnFx;
    Battlefield battlefield;
} GameState;

void bf_add_entity(Battlefield *bf, Entity *e);

/* ---- Include production code ---- */
#include "../src/rendering/spawn_fx.c"
#include "../src/systems/spawn.c"

/* ---- Battlefield stub implementation ---- */
void bf_add_entity(Battlefield *bf, Entity *e) {
    bf->entities[bf->entityCount++] = e;
}

/* ---- Test helpers ---- */
static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(fn) do { \
    reset_draw_state(); \
    printf("  "); \
    fn(); \
    tests_run++; \
    tests_passed++; \
    printf("PASS: %s\n", #fn); \
} while (0)

static bool approx_eq(float a, float b, float eps) {
    return fabsf(a - b) < eps;
}

static void reset_draw_state(void) {
    g_drawCalls = 0;
    g_lastDrawTexture = (Texture2D){0};
    g_lastDrawSrc = (Rectangle){0};
    g_lastDrawDst = (Rectangle){0};
    g_lastDrawOrigin = (Vector2){0};
    g_lastDrawRotation = 0.0f;
}

static void reset_load_state(void) {
    g_lastTexturePath = NULL;
    memset(g_loadedTexturePaths, 0, sizeof(g_loadedTexturePaths));
    g_loadTextureCalls = 0;
}

static int count_active_smoke(const SpawnFxSystem *fx) {
    int count = 0;
    for (int i = 0; i < SPAWN_FX_CAPACITY; i++) {
        if (fx->smoke[i].active) count++;
    }
    return count;
}

static int count_active_explosions(const SpawnFxSystem *fx) {
    int count = 0;
    for (int i = 0; i < SPAWN_FX_CAPACITY; i++) {
        if (fx->explosions[i].active) count++;
    }
    return count;
}

static int count_active_blood(const SpawnFxSystem *fx) {
    int count = 0;
    for (int i = 0; i < SPAWN_FX_CAPACITY; i++) {
        if (fx->blood[i].active) count++;
    }
    return count;
}

/* ---- Tests ---- */

static void test_init_loads_smoke_explosion_and_blood_sheets(void) {
    reset_load_state();
    SpawnFxSystem fx = {0};
    spawn_fx_init(&fx);

    assert(fx.smokeTexture.id == 1);
    assert(fx.explosionTexture.id == 2);
    assert(fx.bloodTexture.id == 3);
    assert(g_loadTextureCalls == 3);
    assert(strcmp(g_loadedTexturePaths[0], FX_SMOKE_PATH) == 0);
    assert(strcmp(g_loadedTexturePaths[1], FX_EXPLOSION_PATH) == 0);
    assert(strcmp(g_loadedTexturePaths[2], FX_BLOOD_PATH) == 0);

    spawn_fx_cleanup(&fx);
    assert(g_unloadTextureCalls >= 3);
}

static void test_draw_uses_first_frame_of_row_eleven(void) {
    SpawnFxSystem fx = {0};
    spawn_fx_init(&fx);
    spawn_fx_emit_smoke(&fx, (Vector2){100.0f, 200.0f}, 2.0f);

    spawn_fx_draw(&fx, 180.0f);

    assert(g_drawCalls == 1);
    assert(g_lastDrawTexture.id == 1);
    assert(approx_eq(g_lastDrawSrc.x, 0.0f, 0.001f));
    assert(approx_eq(g_lastDrawSrc.y, 640.0f, 0.001f));
    assert(approx_eq(g_lastDrawSrc.width, 64.0f, 0.001f));
    assert(approx_eq(g_lastDrawSrc.height, 64.0f, 0.001f));
    assert(approx_eq(g_lastDrawDst.x, 100.0f, 0.001f));
    assert(approx_eq(g_lastDrawDst.y, 200.0f, 0.001f));
    assert(approx_eq(g_lastDrawDst.width, 128.0f, 0.001f));
    assert(approx_eq(g_lastDrawDst.height, 128.0f, 0.001f));
    assert(approx_eq(g_lastDrawOrigin.x, 64.0f, 0.001f));
    assert(approx_eq(g_lastDrawOrigin.y, 64.0f, 0.001f));
    assert(approx_eq(g_lastDrawRotation, 180.0f, 0.001f));

    spawn_fx_cleanup(&fx);
}

static void test_frame_selection_advances_and_clamps_to_last_frame(void) {
    SpawnFxSystem fx = {0};
    spawn_fx_init(&fx);
    spawn_fx_emit_smoke(&fx, (Vector2){0}, 1.0f);

    spawn_fx_update(&fx, 0.2f);
    spawn_fx_draw(&fx, 0.0f);
    assert(g_drawCalls == 1);
    assert(approx_eq(g_lastDrawSrc.x, 320.0f, 0.001f));

    reset_draw_state();
    spawn_fx_update(&fx, 0.19f);
    spawn_fx_draw(&fx, 0.0f);
    assert(g_drawCalls == 1);
    assert(approx_eq(g_lastDrawSrc.x, 640.0f, 0.001f));

    spawn_fx_cleanup(&fx);
}

static void test_effect_expires_at_duration(void) {
    SpawnFxSystem fx = {0};
    spawn_fx_init(&fx);
    spawn_fx_emit_smoke(&fx, (Vector2){0}, 1.0f);

    spawn_fx_update(&fx, 0.4f);
    assert(count_active_smoke(&fx) == 0);

    spawn_fx_draw(&fx, 0.0f);
    assert(g_drawCalls == 0);

    spawn_fx_cleanup(&fx);
}

static void test_overlay_draw_uses_first_explosion_frame(void) {
    SpawnFxSystem fx = {0};
    spawn_fx_init(&fx);
    spawn_fx_emit_explosion(&fx, (Vector2){150.0f, 250.0f}, 2.0f);

    spawn_fx_draw_overlay(&fx, 180.0f);

    assert(g_drawCalls == 1);
    assert(g_lastDrawTexture.id == 2);
    assert(approx_eq(g_lastDrawSrc.x, 0.0f, 0.001f));
    assert(approx_eq(g_lastDrawSrc.y, 0.0f, 0.001f));
    assert(approx_eq(g_lastDrawSrc.width, 32.0f, 0.001f));
    assert(approx_eq(g_lastDrawSrc.height, 32.0f, 0.001f));
    assert(approx_eq(g_lastDrawDst.x, 150.0f, 0.001f));
    assert(approx_eq(g_lastDrawDst.y, 250.0f, 0.001f));
    assert(approx_eq(g_lastDrawDst.width, 64.0f, 0.001f));
    assert(approx_eq(g_lastDrawDst.height, 64.0f, 0.001f));
    assert(approx_eq(g_lastDrawOrigin.x, 32.0f, 0.001f));
    assert(approx_eq(g_lastDrawOrigin.y, 32.0f, 0.001f));
    assert(approx_eq(g_lastDrawRotation, 180.0f, 0.001f));

    spawn_fx_cleanup(&fx);
}

static void test_explosion_frame_selection_advances_and_clamps_to_last_frame(void) {
    SpawnFxSystem fx = {0};
    spawn_fx_init(&fx);
    spawn_fx_emit_explosion(&fx, (Vector2){0}, 2.0f);

    spawn_fx_update(&fx, 0.25f);
    spawn_fx_draw_overlay(&fx, 0.0f);
    assert(g_drawCalls == 1);
    assert(approx_eq(g_lastDrawSrc.x, 192.0f, 0.001f));

    reset_draw_state();
    spawn_fx_update(&fx, 0.24f);
    spawn_fx_draw_overlay(&fx, 0.0f);
    assert(g_drawCalls == 1);
    assert(approx_eq(g_lastDrawSrc.x, 352.0f, 0.001f));

    spawn_fx_cleanup(&fx);
}

static void test_explosion_effect_expires_at_duration(void) {
    SpawnFxSystem fx = {0};
    spawn_fx_init(&fx);
    spawn_fx_emit_explosion(&fx, (Vector2){0}, 2.0f);

    spawn_fx_update(&fx, 0.5f);
    assert(count_active_explosions(&fx) == 0);

    spawn_fx_draw_overlay(&fx, 0.0f);
    assert(g_drawCalls == 0);

    spawn_fx_cleanup(&fx);
}

static void test_overlay_draw_uses_first_blood_frame(void) {
    SpawnFxSystem fx = {0};
    spawn_fx_init(&fx);
    spawn_fx_emit_blood(&fx, (Vector2){175.0f, 275.0f}, 2.0f);

    spawn_fx_draw_overlay(&fx, 180.0f);

    assert(g_drawCalls == 1);
    assert(g_lastDrawTexture.id == 3);
    assert(approx_eq(g_lastDrawSrc.x, 0.0f, 0.001f));
    assert(approx_eq(g_lastDrawSrc.y, 0.0f, 0.001f));
    assert(approx_eq(g_lastDrawSrc.width, 32.0f, 0.001f));
    assert(approx_eq(g_lastDrawSrc.height, 32.0f, 0.001f));
    assert(approx_eq(g_lastDrawDst.x, 175.0f, 0.001f));
    assert(approx_eq(g_lastDrawDst.y, 275.0f, 0.001f));
    assert(approx_eq(g_lastDrawDst.width, 64.0f, 0.001f));
    assert(approx_eq(g_lastDrawDst.height, 64.0f, 0.001f));
    assert(approx_eq(g_lastDrawOrigin.x, 32.0f, 0.001f));
    assert(approx_eq(g_lastDrawOrigin.y, 32.0f, 0.001f));
    assert(approx_eq(g_lastDrawRotation, 180.0f, 0.001f));

    spawn_fx_cleanup(&fx);
}

static void test_blood_frame_selection_advances_and_clamps_to_last_frame(void) {
    SpawnFxSystem fx = {0};
    spawn_fx_init(&fx);
    spawn_fx_emit_blood(&fx, (Vector2){0}, 2.0f);

    spawn_fx_update(&fx, 0.225f);
    spawn_fx_draw_overlay(&fx, 0.0f);
    assert(g_drawCalls == 1);
    assert(approx_eq(g_lastDrawSrc.x, 384.0f, 0.001f));

    reset_draw_state();
    spawn_fx_update(&fx, 0.22f);
    spawn_fx_draw_overlay(&fx, 0.0f);
    assert(g_drawCalls == 1);
    assert(approx_eq(g_lastDrawSrc.x, 736.0f, 0.001f));

    spawn_fx_cleanup(&fx);
}

static void test_blood_effect_expires_at_duration(void) {
    SpawnFxSystem fx = {0};
    spawn_fx_init(&fx);
    spawn_fx_emit_blood(&fx, (Vector2){0}, 2.0f);

    spawn_fx_update(&fx, 0.45f);
    assert(count_active_blood(&fx) == 0);

    spawn_fx_draw_overlay(&fx, 0.0f);
    assert(g_drawCalls == 0);

    spawn_fx_cleanup(&fx);
}

static void test_smoke_and_overlay_draws_stay_separate(void) {
    SpawnFxSystem fx = {0};
    spawn_fx_init(&fx);
    spawn_fx_emit_smoke(&fx, (Vector2){100.0f, 200.0f}, 2.0f);
    spawn_fx_emit_explosion(&fx, (Vector2){150.0f, 250.0f}, 2.0f);

    spawn_fx_draw(&fx, 0.0f);
    assert(g_drawCalls == 1);
    assert(g_lastDrawTexture.id == 1);

    reset_draw_state();
    spawn_fx_draw_overlay(&fx, 0.0f);
    assert(g_drawCalls == 1);
    assert(g_lastDrawTexture.id == 2);

    spawn_fx_cleanup(&fx);
}

static void test_capacity_overwrites_oldest_slot(void) {
    SpawnFxSystem fx = {0};

    for (int i = 0; i < SPAWN_FX_CAPACITY; i++) {
        spawn_fx_emit_smoke(&fx, (Vector2){(float)i, 0.0f}, 1.0f);
    }
    assert(count_active_smoke(&fx) == SPAWN_FX_CAPACITY);
    assert(approx_eq(fx.smoke[0].position.x, 0.0f, 0.001f));

    spawn_fx_emit_smoke(&fx, (Vector2){99.0f, 0.0f}, 1.0f);

    assert(count_active_smoke(&fx) == SPAWN_FX_CAPACITY);
    assert(approx_eq(fx.smoke[0].position.x, 99.0f, 0.001f));
}

static void test_spawn_register_entity_adds_battlefield_entity_and_smoke(void) {
    GameState state = {0};
    Entity entity = {
        .position = {320.0f, 640.0f},
        .spriteScale = 2.0f,
    };

    spawn_register_entity(&state, &entity, SPAWN_FX_SMOKE);

    assert(state.battlefield.entityCount == 1);
    assert(state.battlefield.entities[0] == &entity);
    assert(count_active_smoke(&state.spawnFx) == 1);
    assert(approx_eq(state.spawnFx.smoke[0].position.x, 320.0f, 0.001f));
    assert(approx_eq(state.spawnFx.smoke[0].position.y, 640.0f, 0.001f));
    assert(approx_eq(state.spawnFx.smoke[0].scale, 2.0f, 0.001f));
}

static void test_spawn_register_none_skips_smoke(void) {
    GameState state = {0};
    Entity entity = {
        .position = {1.0f, 2.0f},
        .spriteScale = 3.0f,
    };

    spawn_register_entity(&state, &entity, SPAWN_FX_NONE);

    assert(state.battlefield.entityCount == 1);
    assert(count_active_smoke(&state.spawnFx) == 0);
}

int main(void) {
    printf("Running spawn_fx tests...\n");

    RUN_TEST(test_init_loads_smoke_explosion_and_blood_sheets);
    RUN_TEST(test_draw_uses_first_frame_of_row_eleven);
    RUN_TEST(test_frame_selection_advances_and_clamps_to_last_frame);
    RUN_TEST(test_effect_expires_at_duration);
    RUN_TEST(test_overlay_draw_uses_first_explosion_frame);
    RUN_TEST(test_explosion_frame_selection_advances_and_clamps_to_last_frame);
    RUN_TEST(test_explosion_effect_expires_at_duration);
    RUN_TEST(test_overlay_draw_uses_first_blood_frame);
    RUN_TEST(test_blood_frame_selection_advances_and_clamps_to_last_frame);
    RUN_TEST(test_blood_effect_expires_at_duration);
    RUN_TEST(test_smoke_and_overlay_draws_stay_separate);
    RUN_TEST(test_capacity_overwrites_oldest_slot);
    RUN_TEST(test_spawn_register_entity_adds_battlefield_entity_and_smoke);
    RUN_TEST(test_spawn_register_none_skips_smoke);

    printf("\nAll %d tests passed!\n", tests_passed);
    return 0;
}
