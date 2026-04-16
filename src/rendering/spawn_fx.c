//
// Transient world FX rendered in world space.
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
    SPAWN_EXPLOSION_FRAME_WIDTH = 32,
    SPAWN_EXPLOSION_FRAME_HEIGHT = 32,
    SPAWN_EXPLOSION_FRAME_COUNT = 12,
    SPAWN_EXPLOSION_ROW_INDEX = 0,
};

static const float kSpawnSmokeDurationSeconds = 0.4f;
static const float kSpawnExplosionDurationSeconds = 0.5f;

static int spawn_fx_frame_index(float elapsed, float durationSeconds, int frameCount) {
    float normalized = elapsed / durationSeconds;
    int frame = (int)(normalized * (float)frameCount);
    if (frame < 0) frame = 0;
    if (frame >= frameCount) frame = frameCount - 1;
    return frame;
}

static void spawn_fx_update_smoke(SpawnSmokeFx *smoke, float dt) {
    if (!smoke || !smoke->active) return;

    smoke->elapsed += dt;
    if (smoke->elapsed >= kSpawnSmokeDurationSeconds) {
        smoke->active = false;
    }
}

static void spawn_fx_update_explosion(SpawnExplosionFx *explosion, float dt) {
    if (!explosion || !explosion->active) return;

    explosion->elapsed += dt;
    if (explosion->elapsed >= kSpawnExplosionDurationSeconds) {
        explosion->active = false;
    }
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

    fx->explosionTexture = LoadTexture(FX_EXPLOSION_PATH);
    if (fx->explosionTexture.id == 0) {
        fprintf(stderr, "[SpawnFX] Failed to load %s\n", FX_EXPLOSION_PATH);
        return;
    }

    SetTextureFilter(fx->explosionTexture, TEXTURE_FILTER_POINT);

    const int minExplosionWidth = SPAWN_EXPLOSION_FRAME_WIDTH * SPAWN_EXPLOSION_FRAME_COUNT;
    const int minExplosionHeight =
        SPAWN_EXPLOSION_FRAME_HEIGHT * (SPAWN_EXPLOSION_ROW_INDEX + 1);
    if (fx->explosionTexture.width < minExplosionWidth ||
        fx->explosionTexture.height < minExplosionHeight) {
        fprintf(stderr,
                "[SpawnFX] Explosion sheet too small (%dx%d); need at least %dx%d\n",
                fx->explosionTexture.width, fx->explosionTexture.height,
                minExplosionWidth, minExplosionHeight);
    }
}

void spawn_fx_cleanup(SpawnFxSystem *fx) {
    if (!fx) return;
    if (fx->smokeTexture.id > 0) {
        UnloadTexture(fx->smokeTexture);
        fx->smokeTexture = (Texture2D){0};
    }
    if (fx->explosionTexture.id > 0) {
        UnloadTexture(fx->explosionTexture);
        fx->explosionTexture = (Texture2D){0};
    }
    memset(fx->smoke, 0, sizeof(fx->smoke));
    memset(fx->explosions, 0, sizeof(fx->explosions));
    fx->nextSmokeIndex = 0;
    fx->nextExplosionIndex = 0;
}

void spawn_fx_update(SpawnFxSystem *fx, float dt) {
    if (!fx) return;

    for (int i = 0; i < SPAWN_FX_CAPACITY; i++) {
        spawn_fx_update_smoke(&fx->smoke[i], dt);
        spawn_fx_update_explosion(&fx->explosions[i], dt);
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

void spawn_fx_emit_explosion(SpawnFxSystem *fx, Vector2 position, float scale) {
    if (!fx) return;

    SpawnExplosionFx *explosion = &fx->explosions[fx->nextExplosionIndex];
    *explosion = (SpawnExplosionFx){
        .position = position,
        .scale = scale,
        .elapsed = 0.0f,
        .active = true,
    };

    fx->nextExplosionIndex = (fx->nextExplosionIndex + 1) % SPAWN_FX_CAPACITY;
}

void spawn_fx_draw(const SpawnFxSystem *fx, float rotationDegrees) {
    if (!fx || fx->smokeTexture.id == 0) return;

    for (int i = 0; i < SPAWN_FX_CAPACITY; i++) {
        const SpawnSmokeFx *smoke = &fx->smoke[i];
        if (!smoke->active) continue;

        int frame = spawn_fx_frame_index(smoke->elapsed, kSpawnSmokeDurationSeconds,
                                         SPAWN_SMOKE_FRAME_COUNT);
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

void spawn_fx_draw_overlay(const SpawnFxSystem *fx, float rotationDegrees) {
    if (!fx || fx->explosionTexture.id == 0) return;

    for (int i = 0; i < SPAWN_FX_CAPACITY; i++) {
        const SpawnExplosionFx *explosion = &fx->explosions[i];
        if (!explosion->active) continue;

        int frame = spawn_fx_frame_index(explosion->elapsed, kSpawnExplosionDurationSeconds,
                                         SPAWN_EXPLOSION_FRAME_COUNT);
        Rectangle src = {
            (float)(frame * SPAWN_EXPLOSION_FRAME_WIDTH),
            (float)(SPAWN_EXPLOSION_ROW_INDEX * SPAWN_EXPLOSION_FRAME_HEIGHT),
            (float)SPAWN_EXPLOSION_FRAME_WIDTH,
            (float)SPAWN_EXPLOSION_FRAME_HEIGHT,
        };

        float drawWidth = (float)SPAWN_EXPLOSION_FRAME_WIDTH * explosion->scale;
        float drawHeight = (float)SPAWN_EXPLOSION_FRAME_HEIGHT * explosion->scale;
        Rectangle dst = {
            explosion->position.x,
            explosion->position.y,
            drawWidth,
            drawHeight,
        };
        Vector2 origin = { drawWidth * 0.5f, drawHeight * 0.5f };

        DrawTexturePro(fx->explosionTexture, src, dst, origin, rotationDegrees, WHITE);
    }
}
