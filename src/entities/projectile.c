//
// Created by Nathan Davis on 2/16/26.
//

#include "projectile.h"

#include "../core/config.h"
#include "../logic/combat.h"
#include "../rendering/sprite_renderer.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

enum {
    PROJECTILE_FISH_FRAME_COUNT = 3,
    PROJECTILE_FISH_FRAME_WIDTH = 22,
    PROJECTILE_FISH_FRAME_HEIGHT = 19,
    PROJECTILE_BLOB_FRAME_COUNT = 5,
    PROJECTILE_BLOB_FRAME_WIDTH = 64,
    PROJECTILE_BLOB_FRAME_HEIGHT = 64,
    PROJECTILE_BIRD_BOMB_FRAME_COUNT = 5,
    PROJECTILE_BIRD_BOMB_FRAME_WIDTH = 32,
    PROJECTILE_BIRD_BOMB_FRAME_HEIGHT = 32,
};

static const float kFishFramesPerSecond = 12.0f;
static const float kHealerBlobFramesPerSecond = 12.0f;
static const float kBirdBombFramesPerSecond = 12.0f;
static const float kBirdBombExplosionScale = 2.0f;

typedef struct {
    Texture2D texture;
    int frameCount;
    int frameWidth;
    int frameHeight;
    float framesPerSecond;
    float originX;
    float originY;
} ProjectileVisualDef;

static const ProjectileVisualDef *projectile_visual_def(const GameState *gs,
                                                        ProjectileVisualType visualType) {
    static const ProjectileVisualDef s_none = {0};
    static ProjectileVisualDef s_fish;
    static ProjectileVisualDef s_blob;
    static ProjectileVisualDef s_birdBomb;

    if (!gs) return &s_none;

    s_fish = (ProjectileVisualDef){
        .texture = gs->projectileAssets.fishTexture,
        .frameCount = PROJECTILE_FISH_FRAME_COUNT,
        .frameWidth = PROJECTILE_FISH_FRAME_WIDTH,
        .frameHeight = PROJECTILE_FISH_FRAME_HEIGHT,
        .framesPerSecond = kFishFramesPerSecond,
        .originX = (float)PROJECTILE_FISH_FRAME_WIDTH * 0.5f,
        .originY = (float)PROJECTILE_FISH_FRAME_HEIGHT * 0.5f,
    };
    s_blob = (ProjectileVisualDef){
        .texture = gs->projectileAssets.healerBlobTexture,
        .frameCount = PROJECTILE_BLOB_FRAME_COUNT,
        .frameWidth = PROJECTILE_BLOB_FRAME_WIDTH,
        .frameHeight = PROJECTILE_BLOB_FRAME_HEIGHT,
        .framesPerSecond = kHealerBlobFramesPerSecond,
        .originX = 12.0f,
        .originY = 31.0f,
    };
    s_birdBomb = (ProjectileVisualDef){
        .texture = gs->projectileAssets.birdBombTexture,
        .frameCount = PROJECTILE_BIRD_BOMB_FRAME_COUNT,
        .frameWidth = PROJECTILE_BIRD_BOMB_FRAME_WIDTH,
        .frameHeight = PROJECTILE_BIRD_BOMB_FRAME_HEIGHT,
        .framesPerSecond = kBirdBombFramesPerSecond,
        .originX = (float)PROJECTILE_BIRD_BOMB_FRAME_WIDTH * 0.5f,
        .originY = (float)PROJECTILE_BIRD_BOMB_FRAME_HEIGHT * 0.5f,
    };

    switch (visualType) {
        case PROJECTILE_VISUAL_FISH: return &s_fish;
        case PROJECTILE_VISUAL_HEALER_BLOB: return &s_blob;
        case PROJECTILE_VISUAL_BIRD_BOMB: return &s_birdBomb;
        default: return &s_none;
    }
}

static float projectile_length(Vector2 v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

static Vector2 projectile_normalize_or(Vector2 v, Vector2 fallback) {
    float len = projectile_length(v);
    if (len <= 0.0001f) return fallback;
    return (Vector2){ v.x / len, v.y / len };
}

static Vector2 projectile_rotate(Vector2 v, float rotationDegrees) {
    float radians = rotationDegrees * (PI_F / 180.0f);
    float cosA = cosf(radians);
    float sinA = sinf(radians);
    return (Vector2){
        v.x * cosA - v.y * sinA,
        v.x * sinA + v.y * cosA,
    };
}

static Vector2 projectile_launch_origin(const Entity *attacker) {
    if (!attacker) return (Vector2){ 0.0f, 0.0f };

    Vector2 local = attacker->projectileLaunchOffset;
    if (attacker->anim.flipH) {
        local.x = -local.x;
    }
    local.x *= attacker->spriteScale;
    local.y *= attacker->spriteScale;

    Vector2 rotated = projectile_rotate(local, attacker->spriteRotationDegrees);
    return (Vector2){
        attacker->position.x + rotated.x,
        attacker->position.y + rotated.y,
    };
}

static bool projectile_slot_in_use(const Projectile *projectile) {
    if (!projectile) return false;
    return projectile->active || projectile->reserved;
}

static Projectile *projectile_slot_at(ProjectileSystem *system, int slotIndex) {
    if (!system) return NULL;
    if (slotIndex < 0 || slotIndex >= PROJECTILE_CAPACITY) return NULL;
    return &system->projectiles[slotIndex];
}

static bool projectile_fill_attack(Projectile *projectile, const Entity *attacker,
                                   const Entity *target,
                                   const CombatEffectPayload *payload) {
    if (!projectile || !attacker || !target || !payload) return false;

    Vector2 startPos = projectile_launch_origin(attacker);
    *projectile = (Projectile){
        .active = true,
        .reserved = false,
        .sourceId = attacker->id,
        .sourceOwnerId = attacker->ownerID,
        .lockedTargetId = target->id,
        .payload = *payload,
        .prevPos = startPos,
        .currentPos = startPos,
        .snapshotTargetPos = target->position,
        .speed = attacker->projectileSpeed,
        .hitRadius = attacker->projectileHitRadius,
        .splashRadius = attacker->projectileSplashRadius,
        .visualType = attacker->projectileVisualType,
        .renderScale = attacker->projectileRenderScale,
        .animElapsed = 0.0f,
    };
    return true;
}

static float projectile_point_segment_distance_sq(Vector2 point, Vector2 a, Vector2 b) {
    float abx = b.x - a.x;
    float aby = b.y - a.y;
    float apx = point.x - a.x;
    float apy = point.y - a.y;
    float abLenSq = abx * abx + aby * aby;
    float t = 0.0f;

    if (abLenSq > 0.0001f) {
        t = (apx * abx + apy * aby) / abLenSq;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    }

    float closestX = a.x + abx * t;
    float closestY = a.y + aby * t;
    float dx = point.x - closestX;
    float dy = point.y - closestY;
    return dx * dx + dy * dy;
}

static Vector2 projectile_point_segment_closest_point(Vector2 point, Vector2 a, Vector2 b) {
    float abx = b.x - a.x;
    float aby = b.y - a.y;
    float apx = point.x - a.x;
    float apy = point.y - a.y;
    float abLenSq = abx * abx + aby * aby;
    float t = 0.0f;

    if (abLenSq > 0.0001f) {
        t = (apx * abx + apy * aby) / abLenSq;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    }

    return (Vector2){
        a.x + abx * t,
        a.y + aby * t,
    };
}

static bool projectile_reached_snapshot(const Projectile *projectile) {
    if (!projectile) return true;

    float totalDx = projectile->snapshotTargetPos.x - projectile->prevPos.x;
    float totalDy = projectile->snapshotTargetPos.y - projectile->prevPos.y;
    float stepDx = projectile->currentPos.x - projectile->prevPos.x;
    float stepDy = projectile->currentPos.y - projectile->prevPos.y;
    float totalLenSq = totalDx * totalDx + totalDy * totalDy;

    if (totalLenSq <= 0.0001f) return true;

    float stepDotTotal = stepDx * totalDx + stepDy * totalDy;
    return stepDotTotal >= totalLenSq;
}

static float projectile_rotation_degrees(const Projectile *projectile) {
    if (!projectile) return 0.0f;

    Vector2 dir = {
        projectile->currentPos.x - projectile->prevPos.x,
        projectile->currentPos.y - projectile->prevPos.y
    };
    if (projectile_length(dir) <= 0.0001f) {
        dir = (Vector2){
            projectile->snapshotTargetPos.x - projectile->currentPos.x,
            projectile->snapshotTargetPos.y - projectile->currentPos.y
        };
    }
    if (projectile_length(dir) <= 0.0001f) return 0.0f;
    return atan2f(dir.y, dir.x) * (180.0f / PI_F);
}

static bool projectile_uses_splash(const Projectile *projectile) {
    if (!projectile) return false;
    return projectile->splashRadius > 0.0f &&
           projectile->payload.kind == PROJECTILE_EFFECT_DAMAGE;
}

static bool projectile_emits_explosion(const Projectile *projectile) {
    if (!projectile) return false;
    return projectile->visualType == PROJECTILE_VISUAL_BIRD_BOMB;
}

static void projectile_detonate_at(GameState *gs, Projectile *projectile, Vector2 center) {
    if (!gs || !projectile) return;
    if (projectile_emits_explosion(projectile)) {
        spawn_fx_emit_explosion(&gs->spawnFx, center, kBirdBombExplosionScale);
    }
    // Keep detonation visuals at the impact point. Per-target blood comes from
    // combat resolution so it stays anchored to each damaged entity instead.
    if (projectile_uses_splash(projectile)) {
        combat_apply_enemy_burst(center, projectile->splashRadius,
                                 projectile->payload.amount, projectile->sourceId,
                                 projectile->sourceOwnerId, gs);
    }
    projectile->active = false;
}

void projectile_assets_init(ProjectileAssets *assets) {
    if (!assets) return;

    memset(assets, 0, sizeof(*assets));

    assets->fishTexture = LoadTexture(PROJECTILE_FISH_PATH);
    if (assets->fishTexture.id == 0) {
        fprintf(stderr, "[Projectile] Failed to load %s\n", PROJECTILE_FISH_PATH);
    } else {
        SetTextureFilter(assets->fishTexture, TEXTURE_FILTER_POINT);
    }

    assets->healerBlobTexture = LoadTexture(PROJECTILE_HEALER_BLOB_PATH);
    if (assets->healerBlobTexture.id == 0) {
        fprintf(stderr, "[Projectile] Failed to load %s\n", PROJECTILE_HEALER_BLOB_PATH);
    } else {
        SetTextureFilter(assets->healerBlobTexture, TEXTURE_FILTER_POINT);
    }

    assets->birdBombTexture = LoadTexture(PROJECTILE_BIRD_BOMB_PATH);
    if (assets->birdBombTexture.id == 0) {
        fprintf(stderr, "[Projectile] Failed to load %s\n", PROJECTILE_BIRD_BOMB_PATH);
    } else {
        SetTextureFilter(assets->birdBombTexture, TEXTURE_FILTER_POINT);
    }
}

void projectile_assets_cleanup(ProjectileAssets *assets) {
    if (!assets) return;

    if (assets->fishTexture.id > 0) {
        UnloadTexture(assets->fishTexture);
    }
    if (assets->healerBlobTexture.id > 0) {
        UnloadTexture(assets->healerBlobTexture);
    }
    if (assets->birdBombTexture.id > 0) {
        UnloadTexture(assets->birdBombTexture);
    }

    memset(assets, 0, sizeof(*assets));
}

void projectile_system_init(ProjectileSystem *system) {
    if (!system) return;
    memset(system, 0, sizeof(*system));
}

int projectile_reserve_slot(GameState *gs) {
    if (!gs) return -1;

    ProjectileSystem *system = &gs->projectileSystem;
    for (int i = 0; i < PROJECTILE_CAPACITY; i++) {
        Projectile *projectile = &system->projectiles[i];
        if (projectile_slot_in_use(projectile)) continue;

        memset(projectile, 0, sizeof(*projectile));
        projectile->reserved = true;
        return i;
    }

    return -1;
}

void projectile_release_slot(GameState *gs, int slotIndex) {
    if (!gs) return;

    Projectile *projectile = projectile_slot_at(&gs->projectileSystem, slotIndex);
    if (!projectile) return;
    memset(projectile, 0, sizeof(*projectile));
}

bool projectile_activate_reserved_attack(GameState *gs, int slotIndex,
                                         const Entity *attacker,
                                         const Entity *target) {
    CombatEffectPayload payload = {0};
    Projectile *projectile = NULL;

    if (!gs || !attacker || !target) return false;
    if (attacker->deliveryMode != ATTACK_DELIVERY_PROJECTILE) return false;

    projectile = projectile_slot_at(&gs->projectileSystem, slotIndex);
    if (!projectile || !projectile->reserved || projectile->active) return false;
    if (!combat_build_effect_payload(attacker, target, &payload)) {
        projectile_release_slot(gs, slotIndex);
        return false;
    }

    return projectile_fill_attack(projectile, attacker, target, &payload);
}

bool projectile_spawn_for_attack(GameState *gs, const Entity *attacker,
                                 const Entity *target) {
    int slotIndex = -1;

    if (!gs || !attacker || !target) return false;
    if (attacker->deliveryMode != ATTACK_DELIVERY_PROJECTILE) return false;
    slotIndex = projectile_reserve_slot(gs);
    if (slotIndex < 0) {
        fprintf(stderr, "[Projectile] Pool exhausted; dropping projectile from entity %d\n",
                attacker->id);
        return false;
    }

    if (!projectile_activate_reserved_attack(gs, slotIndex, attacker, target)) {
        projectile_release_slot(gs, slotIndex);
        return false;
    }

    return true;
}

void projectile_system_update(GameState *gs, float dt) {
    if (!gs || gs->gameOver) return;

    for (int i = 0; i < PROJECTILE_CAPACITY; i++) {
        Projectile *projectile = &gs->projectileSystem.projectiles[i];
        if (!projectile->active) continue;

        projectile->animElapsed += dt;
        projectile->prevPos = projectile->currentPos;

        Vector2 remaining = {
            projectile->snapshotTargetPos.x - projectile->currentPos.x,
            projectile->snapshotTargetPos.y - projectile->currentPos.y
        };
        float remainingDist = projectile_length(remaining);
        float stepDist = projectile->speed * dt;
        if (stepDist < 0.0f) stepDist = 0.0f;

        if (remainingDist <= stepDist || remainingDist <= 0.0001f) {
            projectile->currentPos = projectile->snapshotTargetPos;
        } else {
            Vector2 dir = projectile_normalize_or(remaining, (Vector2){ 1.0f, 0.0f });
            projectile->currentPos.x += dir.x * stepDist;
            projectile->currentPos.y += dir.y * stepDist;
        }

        Entity *target = bf_find_entity(&gs->battlefield, projectile->lockedTargetId);
        if (target && target->alive && !target->markedForRemoval) {
            float collisionRadius = projectile->hitRadius +
                                    combat_target_contact_radius(target);
            float distanceSq = projectile_point_segment_distance_sq(
                target->position, projectile->prevPos, projectile->currentPos
            );
            if (distanceSq <= collisionRadius * collisionRadius) {
                if (projectile_uses_splash(projectile)) {
                    Vector2 impactPoint = projectile_point_segment_closest_point(
                        target->position, projectile->prevPos, projectile->currentPos
                    );
                    projectile_detonate_at(gs, projectile, impactPoint);
                } else {
                    combat_apply_effect_payload(&projectile->payload, target, gs);
                    projectile->active = false;
                }
                if (gs->gameOver) break;
                continue;
            }
        }

        if (projectile_reached_snapshot(projectile)) {
            if (projectile_uses_splash(projectile)) {
                projectile_detonate_at(gs, projectile, projectile->snapshotTargetPos);
                if (gs->gameOver) break;
            } else {
                projectile->active = false;
            }
        }
    }
}

void projectile_system_draw(const GameState *gs) {
    if (!gs) return;

    for (int i = 0; i < PROJECTILE_CAPACITY; i++) {
        const Projectile *projectile = &gs->projectileSystem.projectiles[i];
        const ProjectileVisualDef *visual = projectile_visual_def(gs, projectile->visualType);
        if (!projectile->active) continue;
        if (!visual || visual->texture.id == 0) continue;

        int frameIndex = 0;
        if (visual->frameCount > 1 && visual->framesPerSecond > 0.0f) {
            frameIndex = (int)(projectile->animElapsed * visual->framesPerSecond);
            frameIndex %= visual->frameCount;
            if (frameIndex < 0) frameIndex = 0;
        }

        Rectangle src = {
            (float)(frameIndex * visual->frameWidth),
            0.0f,
            (float)visual->frameWidth,
            (float)visual->frameHeight
        };
        float drawWidth = (float)visual->frameWidth * projectile->renderScale;
        float drawHeight = (float)visual->frameHeight * projectile->renderScale;
        Rectangle dst = {
            projectile->currentPos.x,
            projectile->currentPos.y,
            drawWidth,
            drawHeight
        };
        Vector2 origin = {
            visual->originX * projectile->renderScale,
            visual->originY * projectile->renderScale
        };

        DrawTexturePro(visual->texture, src, dst, origin,
                       projectile_rotation_degrees(projectile), WHITE);
    }
}
