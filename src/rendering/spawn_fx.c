//
// Transient spawn-time smoke effect rendered in world space.
//

#include "spawn_fx.h"
#include "../core/config.h"
#include <string.h>
#include <stdio.h>

enum {
    SPAWN_SMOKE_FRAME_WIDTH = 64,
    SPAWN_SMOKE_FRAME_HEIGHT = 64,
    SPAWN_SMOKE_FRAME_COUNT = 11,
    SPAWN_SMOKE_ROW_INDEX = 10,
};

static const float kSpawnSmokeDurationSeconds = 0.4f;

static int spawn_smoke_frame_index(float elapsed) {
    float normalized = elapsed / kSpawnSmokeDurationSeconds;
    int frame = (int)(normalized * (float)SPAWN_SMOKE_FRAME_COUNT);
    if (frame < 0) frame = 0;
    if (frame >= SPAWN_SMOKE_FRAME_COUNT) frame = SPAWN_SMOKE_FRAME_COUNT - 1;
    return frame;
}

void spawn_fx_init(SpawnFxSystem *fx) {
    if (!fx) return;

    memset(fx, 0, sizeof(*fx));
    fx->smokeTexture = LoadTexture(FX_SMOKE_PATH);
    if (fx->smokeTexture.id == 0) {
        fprintf(stderr, "[SpawnFX] Failed to load %s\n", FX_SMOKE_PATH);
        return;
    }

    SetTextureFilter(fx->smokeTexture, TEXTURE_FILTER_POINT);

    const int minWidth = SPAWN_SMOKE_FRAME_WIDTH * SPAWN_SMOKE_FRAME_COUNT;
    const int minHeight = SPAWN_SMOKE_FRAME_HEIGHT * (SPAWN_SMOKE_ROW_INDEX + 1);
    if (fx->smokeTexture.width < minWidth || fx->smokeTexture.height < minHeight) {
        fprintf(stderr,
                "[SpawnFX] Smoke sheet too small (%dx%d); need at least %dx%d\n",
                fx->smokeTexture.width, fx->smokeTexture.height, minWidth, minHeight);
    }
}

void spawn_fx_cleanup(SpawnFxSystem *fx) {
    if (!fx) return;
    if (fx->smokeTexture.id > 0) {
        UnloadTexture(fx->smokeTexture);
        fx->smokeTexture = (Texture2D){0};
    }
    memset(fx->smoke, 0, sizeof(fx->smoke));
    fx->nextSmokeIndex = 0;
}

void spawn_fx_update(SpawnFxSystem *fx, float dt) {
    if (!fx) return;

    for (int i = 0; i < SPAWN_FX_CAPACITY; i++) {
        SpawnSmokeFx *smoke = &fx->smoke[i];
        if (!smoke->active) continue;

        smoke->elapsed += dt;
        if (smoke->elapsed >= kSpawnSmokeDurationSeconds) {
            smoke->active = false;
        }
    }
}

void spawn_fx_emit_smoke(SpawnFxSystem *fx, Vector2 position, float scale) {
    if (!fx) return;

    SpawnSmokeFx *smoke = &fx->smoke[fx->nextSmokeIndex];
    *smoke = (SpawnSmokeFx){
        .position = position,
        .scale = scale,
        .elapsed = 0.0f,
        .active = true,
    };

    fx->nextSmokeIndex = (fx->nextSmokeIndex + 1) % SPAWN_FX_CAPACITY;
}

void spawn_fx_draw(const SpawnFxSystem *fx, float rotationDegrees) {
    if (!fx || fx->smokeTexture.id == 0) return;

    for (int i = 0; i < SPAWN_FX_CAPACITY; i++) {
        const SpawnSmokeFx *smoke = &fx->smoke[i];
        if (!smoke->active) continue;

        int frame = spawn_smoke_frame_index(smoke->elapsed);
        Rectangle src = {
            (float)(frame * SPAWN_SMOKE_FRAME_WIDTH),
            (float)(SPAWN_SMOKE_ROW_INDEX * SPAWN_SMOKE_FRAME_HEIGHT),
            (float)SPAWN_SMOKE_FRAME_WIDTH,
            (float)SPAWN_SMOKE_FRAME_HEIGHT,
        };

        float drawWidth = (float)SPAWN_SMOKE_FRAME_WIDTH * smoke->scale;
        float drawHeight = (float)SPAWN_SMOKE_FRAME_HEIGHT * smoke->scale;
        Rectangle dst = {
            smoke->position.x,
            smoke->position.y,
            drawWidth,
            drawHeight,
        };
        Vector2 origin = { drawWidth * 0.5f, drawHeight * 0.5f };

        DrawTexturePro(fx->smokeTexture, src, dst, origin, rotationDegrees, WHITE);
    }
}
