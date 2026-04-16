/*
 * Unit tests for src/entities/projectile.c.
 *
 * Self-contained: blocks the heavy include chain, defines a minimal local
 * runtime surface, and includes projectile.c directly.
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define NFC_CARDGAME_PROJECTILE_H
#define NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_CONFIG_H
#define NFC_CARDGAME_COMBAT_H
#define NFC_CARDGAME_SPRITE_RENDERER_H

#define MAX_ENTITIES 64
#define PROJECTILE_CAPACITY (MAX_ENTITIES * 2)
#define PROJECTILE_FISH_PATH "fish.png"
#define PROJECTILE_HEALER_BLOB_PATH "blob.png"
#define PROJECTILE_BIRD_BOMB_PATH "bird_bomb.png"
#define PI_F 3.14159265f

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

#define WHITE ((Color){255, 255, 255, 255})
#define TEXTURE_FILTER_POINT 0

typedef enum { ENTITY_TROOP, ENTITY_BUILDING, ENTITY_PROJECTILE } EntityType;
typedef enum {
    ATTACK_DELIVERY_INSTANT = 0,
    ATTACK_DELIVERY_PROJECTILE
} AttackDeliveryMode;
typedef enum {
    PROJECTILE_EFFECT_NONE = 0,
    PROJECTILE_EFFECT_DAMAGE,
    PROJECTILE_EFFECT_HEAL
} ProjectileEffectKind;
typedef enum {
    PROJECTILE_VISUAL_NONE = 0,
    PROJECTILE_VISUAL_FISH,
    PROJECTILE_VISUAL_HEALER_BLOB,
    PROJECTILE_VISUAL_BIRD_BOMB
} ProjectileVisualType;

typedef struct {
    bool flipH;
} AnimState;

typedef struct {
    ProjectileEffectKind kind;
    int amount;
    int sourceEntityId;
    int sourceOwnerId;
} CombatEffectPayload;

typedef struct {
    bool active;
    bool reserved;
    int sourceId;
    int sourceOwnerId;
    int lockedTargetId;
    CombatEffectPayload payload;
    Vector2 prevPos;
    Vector2 currentPos;
    Vector2 snapshotTargetPos;
    float speed;
    float hitRadius;
    float splashRadius;
    ProjectileVisualType visualType;
    float renderScale;
    float animElapsed;
} Projectile;

typedef struct {
    Texture2D fishTexture;
    Texture2D healerBlobTexture;
    Texture2D birdBombTexture;
} ProjectileAssets;

typedef struct {
    Projectile projectiles[PROJECTILE_CAPACITY];
} ProjectileSystem;

typedef struct {
    int explosionEmitCount;
    Vector2 lastExplosionPos;
    float lastExplosionScale;
} SpawnFxSystem;

typedef struct Entity {
    int id;
    int ownerID;
    EntityType type;
    bool alive;
    bool markedForRemoval;
    int hp;
    int maxHP;
    float bodyRadius;
    float navRadius;
    Vector2 position;
    float spriteScale;
    float spriteRotationDegrees;
    Vector2 projectileLaunchOffset;
    float projectileSpeed;
    float projectileHitRadius;
    float projectileSplashRadius;
    float projectileRenderScale;
    ProjectileVisualType projectileVisualType;
    AttackDeliveryMode deliveryMode;
    AnimState anim;
} Entity;

typedef struct Battlefield {
    Entity *entities[MAX_ENTITIES * 2];
    int entityCount;
} Battlefield;

typedef struct GameState {
    bool gameOver;
    SpawnFxSystem spawnFx;
    ProjectileAssets projectileAssets;
    ProjectileSystem projectileSystem;
    Battlefield battlefield;
} GameState;

static Rectangle g_lastDrawSource;
static Rectangle g_lastDrawDest;
static float g_lastDrawRotation = 0.0f;
static int g_drawCalls = 0;
static int g_applyCalls = 0;
static int g_burstCalls = 0;
static int g_explosionEmitCalls = 0;
static const CombatEffectPayload *g_lastPayload = NULL;
static Entity *g_lastAppliedTarget = NULL;
static bool g_nextImpactEndsGame = false;
static bool g_forceBuildPayloadFailure = false;
static Vector2 g_lastBurstCenter;
static float g_lastBurstRadius = 0.0f;
static int g_lastBurstDamage = 0;
static Vector2 g_lastExplosionPos;
static float g_lastExplosionScale = 0.0f;

static Texture2D LoadTexture(const char *fileName) {
    if (strcmp(fileName, PROJECTILE_FISH_PATH) == 0) {
        return (Texture2D){ .id = 1, .width = 66, .height = 19 };
    }
    if (strcmp(fileName, PROJECTILE_HEALER_BLOB_PATH) == 0) {
        return (Texture2D){ .id = 2, .width = 64, .height = 32 };
    }
    if (strcmp(fileName, PROJECTILE_BIRD_BOMB_PATH) == 0) {
        return (Texture2D){ .id = 3, .width = 160, .height = 32 };
    }
    return (Texture2D){0};
}
static void UnloadTexture(Texture2D texture) { (void)texture; }
static void SetTextureFilter(Texture2D texture, int filter) {
    (void)texture;
    (void)filter;
}
static void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
                           Vector2 origin, float rotation, Color tint) {
    (void)texture;
    (void)origin;
    (void)tint;
    g_lastDrawSource = source;
    g_lastDrawDest = dest;
    g_lastDrawRotation = rotation;
    g_drawCalls++;
}

static Entity *bf_find_entity(Battlefield *bf, int entityID) {
    if (!bf) return NULL;
    for (int i = 0; i < bf->entityCount; i++) {
        if (bf->entities[i] && bf->entities[i]->id == entityID) {
            return bf->entities[i];
        }
    }
    return NULL;
}

static float combat_target_contact_radius(const Entity *target) {
    if (!target) return 0.0f;
    if (target->type == ENTITY_BUILDING && target->navRadius > target->bodyRadius) {
        return target->navRadius;
    }
    return target->bodyRadius;
}

static bool combat_build_effect_payload(const Entity *attacker, const Entity *target,
                                        CombatEffectPayload *outPayload) {
    if (g_forceBuildPayloadFailure) return false;
    if (!attacker || !target || !outPayload) return false;

    outPayload->sourceEntityId = attacker->id;
    outPayload->sourceOwnerId = attacker->ownerID;
    outPayload->amount = 7;
    outPayload->kind = (target->ownerID == attacker->ownerID)
        ? PROJECTILE_EFFECT_HEAL
        : PROJECTILE_EFFECT_DAMAGE;
    return true;
}

static bool combat_apply_effect_payload(const CombatEffectPayload *payload,
                                        Entity *target, GameState *gs) {
    if (!payload || !target || !gs) return false;
    g_applyCalls++;
    g_lastPayload = payload;
    g_lastAppliedTarget = target;

    if (payload->kind == PROJECTILE_EFFECT_HEAL) {
        if (!target->alive || target->markedForRemoval || target->hp >= target->maxHP) {
            return false;
        }
        target->hp += payload->amount;
        if (target->hp > target->maxHP) target->hp = target->maxHP;
        return true;
    }

    if (!target->alive || target->markedForRemoval) return false;
    target->hp -= payload->amount;
    if (target->hp <= 0) {
        target->hp = 0;
        target->alive = false;
        if (g_nextImpactEndsGame) {
            gs->gameOver = true;
        }
    }
    return true;
}

static void combat_apply_enemy_burst(Vector2 center, float radius, int damage,
                                     int sourceEntityId, int sourceOwnerId,
                                     GameState *gs) {
    if (!gs) return;
    if (damage <= 0 || radius <= 0.0f) return;

    g_burstCalls++;
    g_lastBurstCenter = center;
    g_lastBurstRadius = radius;
    g_lastBurstDamage = damage;

    for (int i = 0; i < gs->battlefield.entityCount; i++) {
        Entity *target = gs->battlefield.entities[i];
        if (!target) continue;
        if (!target->alive || target->markedForRemoval) continue;
        if (target->type == ENTITY_PROJECTILE) continue;
        if (target->ownerID == sourceOwnerId) continue;

        float dx = target->position.x - center.x;
        float dy = target->position.y - center.y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > radius) continue;

        CombatEffectPayload payload = {
            .kind = PROJECTILE_EFFECT_DAMAGE,
            .amount = damage,
            .sourceEntityId = sourceEntityId,
            .sourceOwnerId = sourceOwnerId,
        };
        combat_apply_effect_payload(&payload, target, gs);
    }
}

static void spawn_fx_emit_explosion(SpawnFxSystem *fx, Vector2 position, float scale) {
    if (!fx) return;

    fx->explosionEmitCount++;
    fx->lastExplosionPos = position;
    fx->lastExplosionScale = scale;
    g_explosionEmitCalls++;
    g_lastExplosionPos = position;
    g_lastExplosionScale = scale;
}

#include "../src/entities/projectile.c"

static void reset_observers(void) {
    g_lastDrawSource = (Rectangle){0};
    g_lastDrawDest = (Rectangle){0};
    g_lastDrawRotation = 0.0f;
    g_drawCalls = 0;
    g_applyCalls = 0;
    g_burstCalls = 0;
    g_explosionEmitCalls = 0;
    g_lastPayload = NULL;
    g_lastAppliedTarget = NULL;
    g_nextImpactEndsGame = false;
    g_forceBuildPayloadFailure = false;
    g_lastBurstCenter = (Vector2){0};
    g_lastBurstRadius = 0.0f;
    g_lastBurstDamage = 0;
    g_lastExplosionPos = (Vector2){0};
    g_lastExplosionScale = 0.0f;
}

static GameState make_game_state(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    gs.projectileAssets.fishTexture = (Texture2D){ .id = 1, .width = 66, .height = 19 };
    gs.projectileAssets.healerBlobTexture = (Texture2D){ .id = 2, .width = 64, .height = 32 };
    gs.projectileAssets.birdBombTexture = (Texture2D){ .id = 3, .width = 160, .height = 32 };
    return gs;
}

static Entity make_entity(int id, int ownerID, EntityType type, Vector2 position) {
    Entity e;
    memset(&e, 0, sizeof(e));
    e.id = id;
    e.ownerID = ownerID;
    e.type = type;
    e.alive = true;
    e.hp = 20;
    e.maxHP = 20;
    e.bodyRadius = 6.0f;
    e.position = position;
    e.spriteScale = 1.0f;
    e.projectileSpeed = 20.0f;
    e.projectileHitRadius = 1.0f;
    e.projectileSplashRadius = 0.0f;
    e.projectileRenderScale = 1.0f;
    e.projectileVisualType = PROJECTILE_VISUAL_FISH;
    e.deliveryMode = ATTACK_DELIVERY_PROJECTILE;
    return e;
}

static void battlefield_add(Battlefield *bf, Entity *e) {
    bf->entities[bf->entityCount++] = e;
}

static void test_spawn_uses_launch_offset_and_snapshot_target(void) {
    reset_observers();
    GameState gs = make_game_state();
    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){100.0f, 200.0f});
    Entity target = make_entity(2, 1, ENTITY_TROOP, (Vector2){140.0f, 220.0f});
    attacker.spriteScale = 2.0f;
    attacker.projectileLaunchOffset = (Vector2){10.0f, -5.0f};
    attacker.projectileSpeed = 40.0f;
    attacker.projectileHitRadius = 3.0f;
    attacker.projectileRenderScale = 1.5f;

    assert(projectile_spawn_for_attack(&gs, &attacker, &target));
    Projectile *p = &gs.projectileSystem.projectiles[0];
    assert(p->active);
    assert(fabsf(p->currentPos.x - 120.0f) < 0.001f);
    assert(fabsf(p->currentPos.y - 190.0f) < 0.001f);
    assert(fabsf(p->snapshotTargetPos.x - target.position.x) < 0.001f);
    assert(fabsf(p->snapshotTargetPos.y - target.position.y) < 0.001f);
    assert(p->payload.kind == PROJECTILE_EFFECT_DAMAGE);
    printf("  PASS: test_spawn_uses_launch_offset_and_snapshot_target\n");
}

static void test_update_uses_swept_collision_against_live_target(void) {
    reset_observers();
    GameState gs = make_game_state();
    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity target = make_entity(2, 1, ENTITY_TROOP, (Vector2){20.0f, 0.0f});
    attacker.projectileSpeed = 20.0f;
    attacker.projectileHitRadius = 1.0f;
    target.bodyRadius = 1.0f;

    battlefield_add(&gs.battlefield, &target);
    assert(projectile_spawn_for_attack(&gs, &attacker, &target));

    target.position = (Vector2){10.0f, 0.0f};
    projectile_system_update(&gs, 1.0f);

    assert(g_applyCalls == 1);
    assert(g_lastAppliedTarget == &target);
    assert(g_explosionEmitCalls == 0);
    assert(!gs.projectileSystem.projectiles[0].active);
    printf("  PASS: test_update_uses_swept_collision_against_live_target\n");
}

static void test_heal_projectile_noops_when_target_is_full(void) {
    reset_observers();
    GameState gs = make_game_state();
    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity ally = make_entity(2, 0, ENTITY_TROOP, (Vector2){10.0f, 0.0f});
    attacker.projectileVisualType = PROJECTILE_VISUAL_HEALER_BLOB;
    ally.hp = ally.maxHP;

    battlefield_add(&gs.battlefield, &ally);
    assert(projectile_spawn_for_attack(&gs, &attacker, &ally));

    projectile_system_update(&gs, 1.0f);

    assert(g_applyCalls == 1);
    assert(ally.hp == ally.maxHP);
    assert(!gs.projectileSystem.projectiles[0].active);
    printf("  PASS: test_heal_projectile_noops_when_target_is_full\n");
}

static void test_bird_bomb_spawn_uses_beak_offset(void) {
    reset_observers();
    GameState gs = make_game_state();
    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){50.0f, 70.0f});
    Entity target = make_entity(2, 1, ENTITY_TROOP, (Vector2){100.0f, 70.0f});
    attacker.projectileVisualType = PROJECTILE_VISUAL_BIRD_BOMB;
    attacker.projectileLaunchOffset = (Vector2){8.0f, -8.0f};
    attacker.projectileSpeed = 240.0f;
    attacker.projectileHitRadius = 12.0f;
    attacker.projectileSplashRadius = 48.0f;

    assert(projectile_spawn_for_attack(&gs, &attacker, &target));
    Projectile *p = &gs.projectileSystem.projectiles[0];
    assert(p->active);
    assert(fabsf(p->currentPos.x - 58.0f) < 0.001f);
    assert(fabsf(p->currentPos.y - 62.0f) < 0.001f);
    assert(p->visualType == PROJECTILE_VISUAL_BIRD_BOMB);
    assert(fabsf(p->splashRadius - 48.0f) < 0.001f);
    printf("  PASS: test_bird_bomb_spawn_uses_beak_offset\n");
}

static void test_reserved_slot_is_ignored_until_activated(void) {
    reset_observers();
    GameState gs = make_game_state();

    int slotIndex = projectile_reserve_slot(&gs);
    assert(slotIndex == 0);
    assert(gs.projectileSystem.projectiles[slotIndex].reserved);
    assert(!gs.projectileSystem.projectiles[slotIndex].active);

    projectile_system_update(&gs, 1.0f);
    projectile_system_draw(&gs);

    assert(g_applyCalls == 0);
    assert(g_drawCalls == 0);
    assert(gs.projectileSystem.projectiles[slotIndex].reserved);
    assert(!gs.projectileSystem.projectiles[slotIndex].active);
    printf("  PASS: test_reserved_slot_is_ignored_until_activated\n");
}

static void test_reserved_slot_release_makes_slot_reusable(void) {
    reset_observers();
    GameState gs = make_game_state();

    int firstSlot = projectile_reserve_slot(&gs);
    assert(firstSlot == 0);
    projectile_release_slot(&gs, firstSlot);

    int secondSlot = projectile_reserve_slot(&gs);
    assert(secondSlot == 0);
    assert(gs.projectileSystem.projectiles[secondSlot].reserved);
    printf("  PASS: test_reserved_slot_release_makes_slot_reusable\n");
}

static void test_dead_target_despawns_without_effect(void) {
    reset_observers();
    GameState gs = make_game_state();
    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity target = make_entity(2, 1, ENTITY_TROOP, (Vector2){10.0f, 0.0f});

    battlefield_add(&gs.battlefield, &target);
    assert(projectile_spawn_for_attack(&gs, &attacker, &target));

    target.alive = false;
    projectile_system_update(&gs, 1.0f);

    assert(g_applyCalls == 0);
    assert(g_explosionEmitCalls == 0);
    assert(!gs.projectileSystem.projectiles[0].active);
    printf("  PASS: test_dead_target_despawns_without_effect\n");
}

static void test_reserved_attack_uses_snapshot_travel_and_target_can_dodge(void) {
    reset_observers();
    GameState gs = make_game_state();
    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity target = make_entity(2, 1, ENTITY_TROOP, (Vector2){20.0f, 0.0f});
    attacker.projectileSpeed = 20.0f;
    target.bodyRadius = 1.0f;

    battlefield_add(&gs.battlefield, &target);
    int slotIndex = projectile_reserve_slot(&gs);
    assert(slotIndex == 0);
    assert(projectile_activate_reserved_attack(&gs, slotIndex, &attacker, &target));
    assert(gs.projectileSystem.projectiles[slotIndex].active);
    assert(!gs.projectileSystem.projectiles[slotIndex].reserved);

    target.position = (Vector2){20.0f, 20.0f};
    projectile_system_update(&gs, 1.0f);

    assert(g_applyCalls == 0);
    assert(!gs.projectileSystem.projectiles[slotIndex].active);
    printf("  PASS: test_reserved_attack_uses_snapshot_travel_and_target_can_dodge\n");
}

static void test_gameover_breaks_remaining_projectile_updates(void) {
    reset_observers();
    GameState gs = make_game_state();
    Entity attackerA = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity attackerB = make_entity(2, 0, ENTITY_TROOP, (Vector2){0.0f, 50.0f});
    Entity targetA = make_entity(3, 1, ENTITY_BUILDING, (Vector2){5.0f, 0.0f});
    Entity targetB = make_entity(4, 1, ENTITY_TROOP, (Vector2){40.0f, 50.0f});
    targetA.hp = 7;
    targetA.maxHP = 7;
    attackerA.projectileSpeed = 20.0f;
    attackerB.projectileSpeed = 20.0f;

    battlefield_add(&gs.battlefield, &targetA);
    battlefield_add(&gs.battlefield, &targetB);
    assert(projectile_spawn_for_attack(&gs, &attackerA, &targetA));
    assert(projectile_spawn_for_attack(&gs, &attackerB, &targetB));

    g_nextImpactEndsGame = true;
    projectile_system_update(&gs, 1.0f);

    assert(gs.gameOver);
    assert(!gs.projectileSystem.projectiles[0].active);
    assert(gs.projectileSystem.projectiles[1].active);
    assert(fabsf(gs.projectileSystem.projectiles[1].currentPos.x - attackerB.position.x) < 0.001f);
    assert(fabsf(gs.projectileSystem.projectiles[1].currentPos.y - attackerB.position.y) < 0.001f);
    printf("  PASS: test_gameover_breaks_remaining_projectile_updates\n");
}

static void test_static_target_uses_combat_contact_shell(void) {
    reset_observers();
    GameState gs = make_game_state();
    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity target = make_entity(2, 1, ENTITY_BUILDING, (Vector2){20.0f, 0.0f});
    attacker.projectileSpeed = 20.0f;
    attacker.projectileHitRadius = 1.0f;
    target.bodyRadius = 2.0f;
    target.navRadius = 8.0f;

    battlefield_add(&gs.battlefield, &target);
    assert(projectile_spawn_for_attack(&gs, &attacker, &target));

    projectile_system_update(&gs, 0.6f);

    assert(g_applyCalls == 1);
    assert(g_lastAppliedTarget == &target);
    assert(!gs.projectileSystem.projectiles[0].active);
    printf("  PASS: test_static_target_uses_combat_contact_shell\n");
}

static void test_bird_bomb_splash_damages_multiple_enemies_on_impact(void) {
    reset_observers();
    GameState gs = make_game_state();
    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity target = make_entity(2, 1, ENTITY_TROOP, (Vector2){20.0f, 0.0f});
    Entity nearbyEnemy = make_entity(3, 1, ENTITY_TROOP, (Vector2){45.0f, 0.0f});

    attacker.projectileVisualType = PROJECTILE_VISUAL_BIRD_BOMB;
    attacker.projectileSpeed = 20.0f;
    attacker.projectileHitRadius = 1.0f;
    attacker.projectileSplashRadius = 48.0f;
    target.bodyRadius = 1.0f;
    nearbyEnemy.bodyRadius = 1.0f;

    battlefield_add(&gs.battlefield, &target);
    battlefield_add(&gs.battlefield, &nearbyEnemy);
    assert(projectile_spawn_for_attack(&gs, &attacker, &target));

    projectile_system_update(&gs, 1.0f);

    assert(g_burstCalls == 1);
    assert(g_explosionEmitCalls == 1);
    assert(g_applyCalls == 2);
    assert(target.hp == 13);
    assert(nearbyEnemy.hp == 13);
    assert(fabsf(g_lastBurstCenter.x - 20.0f) < 0.001f);
    assert(fabsf(g_lastBurstCenter.y - 0.0f) < 0.001f);
    assert(fabsf(g_lastBurstRadius - 48.0f) < 0.001f);
    assert(g_lastBurstDamage == 7);
    assert(fabsf(g_lastExplosionPos.x - 20.0f) < 0.001f);
    assert(fabsf(g_lastExplosionPos.y - 0.0f) < 0.001f);
    assert(fabsf(g_lastExplosionScale - 2.0f) < 0.001f);
    assert(!gs.projectileSystem.projectiles[0].active);
    printf("  PASS: test_bird_bomb_splash_damages_multiple_enemies_on_impact\n");
}

static void test_bird_bomb_splash_ignores_friendlies(void) {
    reset_observers();
    GameState gs = make_game_state();
    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity target = make_entity(2, 1, ENTITY_TROOP, (Vector2){20.0f, 0.0f});
    Entity friendly = make_entity(3, 0, ENTITY_TROOP, (Vector2){25.0f, 0.0f});

    attacker.projectileVisualType = PROJECTILE_VISUAL_BIRD_BOMB;
    attacker.projectileSpeed = 20.0f;
    attacker.projectileHitRadius = 1.0f;
    attacker.projectileSplashRadius = 48.0f;
    target.bodyRadius = 1.0f;
    friendly.bodyRadius = 1.0f;

    battlefield_add(&gs.battlefield, &target);
    battlefield_add(&gs.battlefield, &friendly);
    assert(projectile_spawn_for_attack(&gs, &attacker, &target));

    projectile_system_update(&gs, 1.0f);

    assert(g_burstCalls == 1);
    assert(g_explosionEmitCalls == 1);
    assert(g_applyCalls == 1);
    assert(target.hp == 13);
    assert(friendly.hp == 20);
    printf("  PASS: test_bird_bomb_splash_ignores_friendlies\n");
}

static void test_bird_bomb_splash_damages_enemy_buildings(void) {
    reset_observers();
    GameState gs = make_game_state();
    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity target = make_entity(2, 1, ENTITY_TROOP, (Vector2){20.0f, 0.0f});
    Entity building = make_entity(3, 1, ENTITY_BUILDING, (Vector2){35.0f, 0.0f});

    attacker.projectileVisualType = PROJECTILE_VISUAL_BIRD_BOMB;
    attacker.projectileSpeed = 20.0f;
    attacker.projectileHitRadius = 1.0f;
    attacker.projectileSplashRadius = 48.0f;
    target.bodyRadius = 1.0f;
    building.bodyRadius = 6.0f;
    building.navRadius = 6.0f;

    battlefield_add(&gs.battlefield, &target);
    battlefield_add(&gs.battlefield, &building);
    assert(projectile_spawn_for_attack(&gs, &attacker, &target));

    projectile_system_update(&gs, 1.0f);

    assert(g_burstCalls == 1);
    assert(g_explosionEmitCalls == 1);
    assert(g_applyCalls == 2);
    assert(target.hp == 13);
    assert(building.hp == 13);
    printf("  PASS: test_bird_bomb_splash_damages_enemy_buildings\n");
}

static void test_bird_bomb_reaches_snapshot_and_still_explodes_when_target_is_dead(void) {
    reset_observers();
    GameState gs = make_game_state();
    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity deadTarget = make_entity(2, 1, ENTITY_TROOP, (Vector2){20.0f, 0.0f});
    Entity nearbyEnemy = make_entity(3, 1, ENTITY_TROOP, (Vector2){25.0f, 0.0f});

    attacker.projectileVisualType = PROJECTILE_VISUAL_BIRD_BOMB;
    attacker.projectileSpeed = 20.0f;
    attacker.projectileHitRadius = 1.0f;
    attacker.projectileSplashRadius = 48.0f;
    deadTarget.alive = false;
    deadTarget.hp = 0;

    battlefield_add(&gs.battlefield, &deadTarget);
    battlefield_add(&gs.battlefield, &nearbyEnemy);
    assert(projectile_spawn_for_attack(&gs, &attacker, &deadTarget));

    projectile_system_update(&gs, 1.0f);

    assert(g_burstCalls == 1);
    assert(g_explosionEmitCalls == 1);
    assert(g_applyCalls == 1);
    assert(deadTarget.hp == 0);
    assert(nearbyEnemy.hp == 13);
    assert(fabsf(g_lastBurstCenter.x - 20.0f) < 0.001f);
    assert(fabsf(g_lastExplosionPos.x - 20.0f) < 0.001f);
    assert(fabsf(g_lastExplosionPos.y - 0.0f) < 0.001f);
    assert(fabsf(g_lastExplosionScale - 2.0f) < 0.001f);
    assert(!gs.projectileSystem.projectiles[0].active);
    printf("  PASS: test_bird_bomb_reaches_snapshot_and_still_explodes_when_target_is_dead\n");
}

static void test_draw_uses_fish_animation_frames(void) {
    reset_observers();
    GameState gs = make_game_state();
    Projectile *p = &gs.projectileSystem.projectiles[0];
    *p = (Projectile){
        .active = true,
        .prevPos = {0.0f, 0.0f},
        .currentPos = {10.0f, 0.0f},
        .snapshotTargetPos = {20.0f, 0.0f},
        .visualType = PROJECTILE_VISUAL_FISH,
        .renderScale = 1.0f,
        .animElapsed = 0.10f,
    };

    projectile_system_draw(&gs);

    assert(g_drawCalls == 1);
    assert(fabsf(g_lastDrawSource.x - 22.0f) < 0.001f);
    assert(fabsf(g_lastDrawRotation) < 0.001f);
    printf("  PASS: test_draw_uses_fish_animation_frames\n");
}

static void test_draw_uses_bird_bomb_animation_frames(void) {
    reset_observers();
    GameState gs = make_game_state();
    Projectile *p = &gs.projectileSystem.projectiles[0];
    *p = (Projectile){
        .active = true,
        .prevPos = {0.0f, 0.0f},
        .currentPos = {12.0f, 0.0f},
        .snapshotTargetPos = {24.0f, 0.0f},
        .visualType = PROJECTILE_VISUAL_BIRD_BOMB,
        .renderScale = 1.0f,
        .animElapsed = 0.10f,
    };

    projectile_system_draw(&gs);

    assert(g_drawCalls == 1);
    assert(fabsf(g_lastDrawSource.x - 32.0f) < 0.001f);
    assert(fabsf(g_lastDrawSource.width - 32.0f) < 0.001f);
    assert(fabsf(g_lastDrawSource.height - 32.0f) < 0.001f);
    printf("  PASS: test_draw_uses_bird_bomb_animation_frames\n");
}

int main(void) {
    printf("Running projectile tests...\n");
    test_spawn_uses_launch_offset_and_snapshot_target();
    test_update_uses_swept_collision_against_live_target();
    test_heal_projectile_noops_when_target_is_full();
    test_bird_bomb_spawn_uses_beak_offset();
    test_reserved_slot_is_ignored_until_activated();
    test_reserved_slot_release_makes_slot_reusable();
    test_dead_target_despawns_without_effect();
    test_reserved_attack_uses_snapshot_travel_and_target_can_dodge();
    test_gameover_breaks_remaining_projectile_updates();
    test_static_target_uses_combat_contact_shell();
    test_bird_bomb_splash_damages_multiple_enemies_on_impact();
    test_bird_bomb_splash_ignores_friendlies();
    test_bird_bomb_splash_damages_enemy_buildings();
    test_bird_bomb_reaches_snapshot_and_still_explodes_when_target_is_dead();
    test_draw_uses_fish_animation_frames();
    test_draw_uses_bird_bomb_animation_frames();
    printf("\nAll projectile tests passed!\n");
    return 0;
}
