//
// Transient world FX rendered in layered world-space passes.
//

#ifndef NFC_CARDGAME_SPAWN_FX_H
#define NFC_CARDGAME_SPAWN_FX_H

#include <raylib.h>
#include <stdbool.h>

#define SPAWN_FX_CAPACITY 32

typedef struct {
    Vector2 position;
    float scale;
    float elapsed;
    bool active;
} SpawnSmokeFx;

typedef struct {
    Vector2 position;
    float scale;
    float elapsed;
    bool active;
} SpawnExplosionFx;

typedef struct {
    Texture2D smokeTexture;
    Texture2D explosionTexture;
    SpawnSmokeFx smoke[SPAWN_FX_CAPACITY];
    SpawnExplosionFx explosions[SPAWN_FX_CAPACITY];
    int nextSmokeIndex;
    int nextExplosionIndex;
} SpawnFxSystem;

void spawn_fx_init(SpawnFxSystem *fx);
void spawn_fx_cleanup(SpawnFxSystem *fx);
void spawn_fx_update(SpawnFxSystem *fx, float dt);
void spawn_fx_emit_smoke(SpawnFxSystem *fx, Vector2 position, float scale);
void spawn_fx_emit_explosion(SpawnFxSystem *fx, Vector2 position, float scale);
void spawn_fx_draw(const SpawnFxSystem *fx, float rotationDegrees);
void spawn_fx_draw_overlay(const SpawnFxSystem *fx, float rotationDegrees);

#endif //NFC_CARDGAME_SPAWN_FX_H
