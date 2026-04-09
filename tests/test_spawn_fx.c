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
static int g_unloadTextureCalls = 0;
static int g_drawCalls = 0;
static Texture2D g_lastDrawTexture = {0};
static Rectangle g_lastDrawSrc = {0};
static Rectangle g_lastDrawDst = {0};
static Vector2 g_lastDrawOrigin = {0};
static float g_lastDrawRotation = 0.0f;

static Texture2D LoadTexture(const char *fileName) {
    g_lastTexturePath = fileName;
    return (Texture2D){ .id = 1, .width = 704, .height = 960, .mipmaps = 1, .format = 7 };
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

/* ---- Config stub ---- */
#define NFC_CARDGAME_CONFIG_H
#define FX_SMOKE_PATH "src/assets/fx/smoke.png"

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

static int count_active_smoke(const SpawnFxSystem *fx) {
    int count = 0;
    for (int i = 0; i < SPAWN_FX_CAPACITY; i++) {
        if (fx->smoke[i].active) count++;
    }
    return count;
}

/* ---- Tests ---- */

static void test_init_loads_smoke_sheet(void) {
    SpawnFxSystem fx = {0};
    spawn_fx_init(&fx);

    assert(fx.smokeTexture.id == 1);
    assert(strcmp(g_lastTexturePath, FX_SMOKE_PATH) == 0);

    spawn_fx_cleanup(&fx);
    assert(g_unloadTextureCalls > 0);
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

    RUN_TEST(test_init_loads_smoke_sheet);
    RUN_TEST(test_draw_uses_first_frame_of_row_eleven);
    RUN_TEST(test_frame_selection_advances_and_clamps_to_last_frame);
    RUN_TEST(test_effect_expires_at_duration);
    RUN_TEST(test_capacity_overwrites_oldest_slot);
    RUN_TEST(test_spawn_register_entity_adds_battlefield_entity_and_smoke);
    RUN_TEST(test_spawn_register_none_skips_smoke);

    printf("\nAll %d tests passed!\n", tests_passed);
    return 0;
}
