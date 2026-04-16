/*
 * Unit tests for src/entities/troop.c and src/entities/building.c.
 *
 * Self-contained: provides minimal local stubs and includes the production
 * translation units directly so render-layer defaults can be verified without
 * the full game include chain.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Prevent heavy include chains ---- */
#define NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_TROOP_H
#define NFC_CARDGAME_BUILDING_H
#define NFC_CARDGAME_ENTITIES_H
#define NFC_CARDGAME_PATHFINDING_H
#define NFC_CARDGAME_CONFIG_H
#define NFC_CARDGAME_COMBAT_H
#define NFC_CARDGAME_DEPOSIT_SLOTS_H
#define NFC_CARDGAME_WIN_CONDTION_H
#define NFC_CARDGAME_SPRITE_RENDERER_H
#define NFC_CARDGAME_BATTLEFIELD_H
#define NFC_CARDGAME_BATTLEFIELD_MATH_H
#define NFC_CARDGAME_SUSTENANCE_H
#define NFC_CARDGAME_NAV_FRAME_H
#define RAYLIB_H
#define cJSON__h

/* ---- Minimal config ---- */
#define BASE_NAV_RADIUS 56.0f
#define BASE_DEPOSIT_PRIMARY_SLOT_COUNT 4
#define BASE_DEPOSIT_QUEUE_SLOT_COUNT   6

/* ---- Minimal type stubs ---- */
typedef struct { float x; float y; } Vector2;

typedef enum { ENTITY_TROOP, ENTITY_BUILDING, ENTITY_PROJECTILE } EntityType;
typedef enum { FACTION_PLAYER1, FACTION_PLAYER2 } Faction;
typedef enum { ESTATE_IDLE, ESTATE_WALKING, ESTATE_ATTACKING, ESTATE_DEAD } EntityState;
typedef enum {
    ENTITY_RENDER_LAYER_GROUND = 0,
    ENTITY_RENDER_LAYER_FLYING
} EntityRenderLayer;
typedef enum {
    ATTACK_ENGAGEMENT_CONTACT = 0,
    ATTACK_ENGAGEMENT_DIRECT_RANGE
} AttackEngagementMode;
typedef enum {
    ATTACK_DELIVERY_INSTANT = 0,
    ATTACK_DELIVERY_PROJECTILE
} AttackDeliveryMode;
typedef enum {
    PROJECTILE_VISUAL_NONE = 0,
    PROJECTILE_VISUAL_FISH,
    PROJECTILE_VISUAL_HEALER_BLOB,
    PROJECTILE_VISUAL_BIRD_BOMB
} ProjectileVisualType;
typedef enum {
    COMBAT_PROFILE_DEFAULT_MELEE = 0,
    COMBAT_PROFILE_HEALER,
    COMBAT_PROFILE_FISHFING,
    COMBAT_PROFILE_BIRD
} CombatProfileId;
typedef enum { TARGET_NEAREST, TARGET_BUILDING, TARGET_SPECIFIC_TYPE } TargetingMode;
typedef enum { UNIT_ROLE_COMBAT, UNIT_ROLE_FARMER } UnitRole;
typedef enum {
    FARMER_SEEKING,
    FARMER_WALKING_TO_SUSTENANCE,
    FARMER_GATHERING,
    FARMER_RETURNING,
    FARMER_DEPOSITING
} FarmerState;
typedef enum { SIDE_BOTTOM, SIDE_TOP } BattleSide;
typedef enum { DIR_SIDE, DIR_DOWN, DIR_UP, DIR_COUNT } SpriteDirection;
typedef enum {
    SPRITE_TYPE_KNIGHT,
    SPRITE_TYPE_HEALER,
    SPRITE_TYPE_ASSASSIN,
    SPRITE_TYPE_BRUTE,
    SPRITE_TYPE_FARMER,
    SPRITE_TYPE_FARMER_FULL,
    SPRITE_TYPE_BIRD,
    SPRITE_TYPE_FISHFING,
    SPRITE_TYPE_BASE,
    SPRITE_TYPE_COUNT
} SpriteType;
typedef enum {
    NAV_PROFILE_LANE = 0,
    NAV_PROFILE_ASSAULT,
    NAV_PROFILE_FREE_GOAL,
    NAV_PROFILE_STATIC
} UnitNavProfile;

typedef struct {
    SpriteDirection dir;
    bool flipH;
} AnimState;

typedef struct { int unused; } CharacterSprite;
typedef struct { int unused; } SpriteAtlas;

typedef struct {
    Vector2 worldPos;
    int claimedByEntityId;
} DepositSlot;

typedef struct {
    DepositSlot primary[BASE_DEPOSIT_PRIMARY_SLOT_COUNT];
    DepositSlot queue[BASE_DEPOSIT_QUEUE_SLOT_COUNT];
    bool initialized;
} DepositSlotRing;

typedef struct Card {
    char *card_id;
    char *name;
    int cost;
    char *type;
    char *rules_text;
    char *data;
} Card;

typedef struct Entity {
    int id;
    EntityType type;
    Faction faction;
    EntityState state;
    Vector2 position;
    float moveSpeed;
    int hp, maxHP;
    int attack;
    int healAmount;
    float attackSpeed;
    float attackRange;
    TargetingMode targeting;
    const char *targetType;
    SpriteType spriteType;
    float bodyRadius;
    EntityRenderLayer renderLayer;
    CombatProfileId combatProfileId;
    AttackEngagementMode engagementMode;
    AttackDeliveryMode deliveryMode;
    ProjectileVisualType projectileVisualType;
    float projectileSpeed;
    float projectileHitRadius;
    float projectileSplashRadius;
    float projectileRenderScale;
    Vector2 projectileLaunchOffset;
    UnitNavProfile navProfile;
    int ownerID;
    BattleSide presentationSide;
    const CharacterSprite *sprite;
    float spriteScale;
    AnimState anim;
    float spriteRotationDegrees;
    FarmerState farmerState;
    int claimedSustenanceNodeId;
    int carriedSustenanceValue;
    float workTimer;
    int lane;
    float laneProgress;
    int waypointIndex;
    UnitRole unitRole;
    float navRadius;
    DepositSlotRing depositSlots;
    int baseLevel;
    bool basePendingKingBurst;
    int basePendingKingBurstDamage;
} Entity;

typedef struct Player {
    int id;
    BattleSide side;
} Player;

typedef struct GameState {
    int unused;
} GameState;

typedef struct {
    const char *name;
    int hp, maxHP;
    int attack;
    int healAmount;
    float attackSpeed;
    float attackRange;
    float moveSpeed;
    TargetingMode targeting;
    const char *targetType;
    SpriteType spriteType;
    float bodyRadius;
    EntityRenderLayer renderLayer;
    CombatProfileId combatProfileId;
    AttackEngagementMode engagementMode;
    AttackDeliveryMode deliveryMode;
    ProjectileVisualType projectileVisualType;
    float projectileSpeed;
    float projectileHitRadius;
    float projectileSplashRadius;
    float projectileRenderScale;
    Vector2 projectileLaunchOffset;
} TroopData;

/* ---- Minimal cJSON stubs ---- */
typedef struct cJSON {
    int valueint;
    double valuedouble;
    char *valuestring;
} cJSON;

static cJSON *cJSON_Parse(const char *value) {
    (void)value;
    return NULL;
}

static cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string) {
    (void)object;
    (void)string;
    return NULL;
}

static void cJSON_Delete(cJSON *item) {
    (void)item;
}

#define cJSON_IsNumber(item) (0)
#define cJSON_IsString(item) (0)

/* ---- Local prototypes for production code dependencies ---- */
static Entity *entity_create(EntityType type, Faction faction, Vector2 pos);
static void entity_set_state(Entity *e, EntityState newState);
static void entity_sync_animation(Entity *e);
static const CharacterSprite *sprite_atlas_get(const SpriteAtlas *atlas, SpriteType type);
static SpriteType sprite_type_from_card(const char *cardType);
static void pathfind_apply_direction_for_side(AnimState *anim, Vector2 diff, BattleSide side);
static float pathfind_sprite_rotation_for_side(SpriteDirection dir, BattleSide side);
static void deposit_slots_build_for_base(Entity *base);
static bool entity_take_damage(Entity *entity, int damage);
static void win_latch_from_destroyed_base(GameState *gs, const Entity *destroyedBase);

/* ---- Include production code ---- */
#include "../src/data/card_catalog.h"
#include "../src/entities/troop.c"
#include "../src/entities/building.c"

/* ---- Stub implementations ---- */
static int g_nextEntityId = 1;

static Entity *entity_create(EntityType type, Faction faction, Vector2 pos) {
    Entity *e = malloc(sizeof(*e));
    assert(e != NULL);
    memset(e, 0, sizeof(*e));
    e->id = g_nextEntityId++;
    e->type = type;
    e->faction = faction;
    e->position = pos;
    e->state = ESTATE_IDLE;
    e->renderLayer = ENTITY_RENDER_LAYER_GROUND;
    e->spriteScale = 2.0f;
    e->presentationSide = SIDE_BOTTOM;
    e->unitRole = UNIT_ROLE_COMBAT;
    return e;
}

static void entity_set_state(Entity *e, EntityState newState) {
    if (e) e->state = newState;
}

static void entity_sync_animation(Entity *e) {
    (void)e;
}

static const CharacterSprite *sprite_atlas_get(const SpriteAtlas *atlas, SpriteType type) {
    static CharacterSprite s_sprite;
    (void)atlas;
    (void)type;
    return &s_sprite;
}

static SpriteType sprite_type_from_card(const char *cardType) {
    if (!cardType) return SPRITE_TYPE_KNIGHT;
    if (strcmp(cardType, "healer") == 0) return SPRITE_TYPE_HEALER;
    if (strcmp(cardType, "assassin") == 0) return SPRITE_TYPE_ASSASSIN;
    if (strcmp(cardType, "brute") == 0) return SPRITE_TYPE_BRUTE;
    if (strcmp(cardType, "farmer") == 0) return SPRITE_TYPE_FARMER;
    if (strcmp(cardType, "bird") == 0) return SPRITE_TYPE_BIRD;
    if (strcmp(cardType, "fishfing") == 0) return SPRITE_TYPE_FISHFING;
    if (strcmp(cardType, "base") == 0) return SPRITE_TYPE_BASE;
    return SPRITE_TYPE_KNIGHT;
}

static void pathfind_apply_direction_for_side(AnimState *anim, Vector2 diff, BattleSide side) {
    (void)diff;
    (void)side;
    if (anim) anim->dir = DIR_SIDE;
}

static float pathfind_sprite_rotation_for_side(SpriteDirection dir, BattleSide side) {
    (void)dir;
    (void)side;
    return 0.0f;
}

static void deposit_slots_build_for_base(Entity *base) {
    (void)base;
}

static bool entity_take_damage(Entity *entity, int damage) {
    (void)entity;
    (void)damage;
    return false;
}

static void win_latch_from_destroyed_base(GameState *gs, const Entity *destroyedBase) {
    (void)gs;
    (void)destroyedBase;
}

/* ---- Tests ---- */
static void test_bird_defaults_to_flying_render_layer(void) {
    Card bird = {
        .card_id = "BIRD_01",
        .name = "Bird",
        .type = "bird",
        .data = NULL,
    };

    TroopData data = troop_create_data_from_card(&bird);

    assert(data.spriteType == SPRITE_TYPE_BIRD);
    assert(data.renderLayer == ENTITY_RENDER_LAYER_FLYING);
    printf("  PASS: test_bird_defaults_to_flying_render_layer\n");
}

static void test_knight_defaults_to_ground_render_layer(void) {
    Card knight = {
        .card_id = "KNIGHT_01",
        .name = "Knight",
        .type = "knight",
        .data = NULL,
    };

    TroopData data = troop_create_data_from_card(&knight);

    assert(data.spriteType == SPRITE_TYPE_KNIGHT);
    assert(data.renderLayer == ENTITY_RENDER_LAYER_GROUND);
    printf("  PASS: test_knight_defaults_to_ground_render_layer\n");
}

static void test_troop_spawn_copies_render_layer_from_data(void) {
    Player owner = { .id = 0, .side = SIDE_BOTTOM };
    SpriteAtlas atlas = {0};
    Card bird = {
        .card_id = "BIRD_01",
        .name = "Bird",
        .type = "bird",
        .data = NULL,
    };
    TroopData data = troop_create_data_from_card(&bird);

    Entity *spawned = troop_spawn(&owner, &data, (Vector2){100.0f, 200.0f}, &atlas);

    assert(spawned != NULL);
    assert(spawned->renderLayer == ENTITY_RENDER_LAYER_FLYING);
    free(spawned);
    printf("  PASS: test_troop_spawn_copies_render_layer_from_data\n");
}

static void test_building_create_base_keeps_ground_render_layer(void) {
    Player owner = { .id = 1, .side = SIDE_TOP };
    SpriteAtlas atlas = {0};

    Entity *base = building_create_base(&owner, (Vector2){540.0f, 1800.0f}, &atlas);

    assert(base != NULL);
    assert(base->renderLayer == ENTITY_RENDER_LAYER_GROUND);
    free(base);
    printf("  PASS: test_building_create_base_keeps_ground_render_layer\n");
}

int main(void) {
    printf("Running troop tests...\n");
    test_bird_defaults_to_flying_render_layer();
    test_knight_defaults_to_ground_render_layer();
    test_troop_spawn_copies_render_layer_from_data();
    test_building_create_base_keeps_ground_render_layer();
    printf("\nAll troop tests passed!\n");
    return 0;
}
