/*
 * Unit tests for src/logic/combat.c
 *
 * Self-contained: redefines minimal type stubs and includes combat.c
 * directly to avoid the heavy types.h include chain (Raylib, sqlite3, biome).
 * Compiles with `make test` using only -lm.
 *
 * SYNC REQUIREMENT: These struct stubs must match the field order and sizes
 * from src/core/types.h. If types.h changes, update these stubs.
 * Run `make test` after any types.h change to catch layout mismatches.
 *
 * Updated for Plan 11-03: combat now uses canonical coordinates and
 * bf_distance instead of map_to_opponent_space. All positions are in
 * the same canonical world space (0..1080 x, 0..1920 y).
 */

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>

/* ---- Prevent combat.c's includes from pulling in heavy headers ---- */
#define NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_COMBAT_H
#define NFC_CARDGAME_ENTITIES_H
#define NFC_CARDGAME_BATTLEFIELD_H
#define NFC_CARDGAME_BATTLEFIELD_MATH_H
#define NFC_CARDGAME_DEBUG_EVENTS_H
#define NFC_CARDGAME_WIN_CONDTION_H
#define NFC_CARDGAME_FARMER_H
#define NFC_CARDGAME_ASSAULT_SLOTS_H
#define NFC_CARDGAME_DEPOSIT_SLOTS_H
#define NFC_CARDGAME_TROOP_H

/* ---- Config defines (must match src/core/config.h) ---- */
#define LANE_WAYPOINT_COUNT  8
#define NUM_CARD_SLOTS 3
#define MAX_ENTITIES 64
#define SUSTENANCE_MATCH_COUNT_PER_SIDE 8
#define BASE_INTERACTION_BACK_OFFSET 48.0f
#define BASE_NAV_RADIUS 56.0f
#define DEFAULT_MELEE_BODY_RADIUS 14.0f
#define BASE_ASSAULT_PRIMARY_SLOT_COUNT 8
#define BASE_ASSAULT_QUEUE_SLOT_COUNT   8
#define BASE_ASSAULT_SLOT_GAP 2.0f
#define BASE_ASSAULT_QUEUE_RADIAL_OFFSET 22.0f
#define PATHFIND_CONTACT_GAP 2.0f
#define COMBAT_BUILDING_MELEE_INSET 30.0f
#define COMBAT_MELEE_GOAL_SLACK_MAX 8.0f
#define COMBAT_PERIMETER_TANGENT_SCALE 0.65f
#define COMBAT_STATIC_TARGET_FLOW_TANGENT_SCALE 0.70f
#define COMBAT_STATIC_TARGET_FLOW_ANGLE_MIN_DEG 10.0f
#define COMBAT_STATIC_TARGET_FLOW_ANGLE_MAX_DEG 20.0f
#define BASE_DEPOSIT_PRIMARY_SLOT_COUNT 4
#define BASE_DEPOSIT_QUEUE_SLOT_COUNT   6

/* ---- Minimal type stubs ---- */
typedef struct { float x; float y; } Vector2;
typedef struct { float x; float y; float width; float height; } Rectangle;

typedef enum { ANIM_IDLE, ANIM_RUN, ANIM_WALK, ANIM_HURT, ANIM_DEATH, ANIM_ATTACK, ANIM_COUNT } AnimationType;
typedef enum { DIR_SIDE, DIR_DOWN, DIR_UP, DIR_COUNT } SpriteDirection;
typedef enum { ANIM_PLAY_LOOP, ANIM_PLAY_ONCE, ANIM_PLAY_IDLE_BURST } AnimPlayMode;
typedef enum { ESTATE_IDLE, ESTATE_WALKING, ESTATE_ATTACKING, ESTATE_DEAD } EntityState;
typedef enum {
    ATTACK_ENGAGEMENT_CONTACT = 0,
    ATTACK_ENGAGEMENT_DIRECT_RANGE
} AttackEngagementMode;
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
    PROJECTILE_VISUAL_HEALER_BLOB
} ProjectileVisualType;
typedef enum {
    COMBAT_PROFILE_DEFAULT_MELEE = 0,
    COMBAT_PROFILE_HEALER,
    COMBAT_PROFILE_FISHFING
} CombatProfileId;
typedef enum { TARGET_NEAREST, TARGET_BUILDING, TARGET_SPECIFIC_TYPE } TargetingMode;
typedef enum { ENTITY_TROOP, ENTITY_BUILDING, ENTITY_PROJECTILE } EntityType;
typedef enum { FACTION_PLAYER1, FACTION_PLAYER2 } Faction;
typedef enum { UNIT_ROLE_COMBAT, UNIT_ROLE_FARMER } UnitRole;
typedef enum { FARMER_SEEKING, FARMER_WALKING_TO_SUSTENANCE, FARMER_GATHERING, FARMER_RETURNING, FARMER_DEPOSITING } FarmerState;
typedef enum {
    SPRITE_TYPE_KNIGHT,
    SPRITE_TYPE_HEALER,
    SPRITE_TYPE_ASSASSIN,
    SPRITE_TYPE_BRUTE,
    SPRITE_TYPE_FARMER,
    SPRITE_TYPE_FARMER_FULL,
    SPRITE_TYPE_BASE,
    SPRITE_TYPE_COUNT
} SpriteType;

typedef struct {
    AnimationType anim;
    SpriteDirection dir;
    float elapsed;
    float cycleDuration;
    float normalizedTime;
    AnimPlayMode mode;
    bool oneShot;
    bool finished;
    bool flipH;
    int visualLoops;
    float idleHoldMinSeconds;
    float idleHoldMaxSeconds;
    float idleHoldDuration;
    unsigned int idleSeed;
    unsigned int idleCycleIndex;
    bool idleHolding;
} AnimState;

typedef struct {
    float prevNormalized;
    float currNormalized;
    bool finishedThisTick;
    bool loopedThisTick;
} AnimPlaybackEvent;

typedef struct { void *texture; int frameWidth; int frameHeight; int frameCount; } AnimSheet;
typedef struct { AnimSheet sheets[ANIM_COUNT]; } CharacterSprite;

typedef struct {
    Vector2 worldPos;
    void *activeCard;
    float cooldownTimer;
} CardSlot;

typedef struct Entity Entity;
typedef struct Player Player;
typedef struct GameState GameState;

/* ---- Battlefield math stubs (must precede Player for BattleSide field) ---- */
typedef enum { SIDE_BOTTOM, SIDE_TOP } BattleSide;
typedef struct { Vector2 v; } CanonicalPos;

/* ---- Deposit slot type stubs (mirrors src/core/types.h) ---- */
typedef enum {
    NAV_PROFILE_LANE = 0,
    NAV_PROFILE_ASSAULT,
    NAV_PROFILE_FREE_GOAL,
    NAV_PROFILE_STATIC
} UnitNavProfile;

typedef enum {
    DEPOSIT_SLOT_NONE = 0,
    DEPOSIT_SLOT_PRIMARY,
    DEPOSIT_SLOT_QUEUE
} DepositSlotKind;

typedef struct {
    Vector2 worldPos;
    int     claimedByEntityId;
} DepositSlot;

typedef struct {
    DepositSlot primary[BASE_DEPOSIT_PRIMARY_SLOT_COUNT];
    DepositSlot queue  [BASE_DEPOSIT_QUEUE_SLOT_COUNT];
    bool initialized;
} DepositSlotRing;

typedef struct {
    ProjectileEffectKind kind;
    int amount;
    int sourceEntityId;
    int sourceOwnerId;
} CombatEffectPayload;

struct Entity {
    int id;
    EntityType type;
    Faction faction;
    EntityState state;
    Vector2 position;
    float moveSpeed;
    int hp, maxHP;
    int attack;
    float attackSpeed;
    float attackRange;
    float attackCooldown;
    int attackTargetId;
    bool attackReleaseFired;
    TargetingMode targeting;
    const char *targetType;
    CombatProfileId combatProfileId;
    AttackEngagementMode engagementMode;
    AttackDeliveryMode deliveryMode;
    ProjectileVisualType projectileVisualType;
    float projectileSpeed;
    float projectileHitRadius;
    float projectileRenderScale;
    Vector2 projectileLaunchOffset;
    AnimState anim;
    const CharacterSprite *sprite;
    int spriteType; // SpriteType enum, but int to avoid pulling in sprite_renderer.h
    float spriteScale;
    float spriteRotationDegrees;
    BattleSide presentationSide;
    int ownerID;
    int lane;
    int waypointIndex;
    float laneProgress;
    float hitFlashTimer;
    UnitRole unitRole;
    FarmerState farmerState;
    int claimedSustenanceNodeId;
    int carriedSustenanceValue;
    float workTimer;
    bool alive;
    bool markedForRemoval;
    int healAmount;
    float bodyRadius;
    float navRadius;
    UnitNavProfile navProfile;
    int movementTargetId;
    int ticksSinceProgress;
    int lastSteerSideSign;
    int reservedDepositSlotIndex;
    DepositSlotKind reservedDepositSlotKind;
    DepositSlotRing depositSlots;
    int baseLevel;
    bool basePendingKingBurst;
    int basePendingKingBurstDamage;
};

/* Player stub -- lean struct matching Plan 11-05 types.h */
typedef struct { float x; float y; float rotation; float zoom; } Camera2D_stub;

struct Player {
    int id;
    BattleSide side;
    Rectangle screenArea;
    Camera2D_stub camera;
    float cameraRotation;
    CardSlot slots[NUM_CARD_SLOTS];
    float energy;
    float maxEnergy;
    float energyRegenRate;
    void *base; // Entity *base (void * to avoid pulling in full Entity for stub)
    int sustenanceCollected;
};

float bf_distance(CanonicalPos a, CanonicalPos b) {
    float dx = a.v.x - b.v.x;
    float dy = a.v.y - b.v.y;
    return sqrtf(dx * dx + dy * dy);
}

/* GameState stub -- only players and battlefield needed for combat */
#define BIOME_COUNT 2
typedef struct { int dummy; } SpriteAtlas;
typedef struct { int dummy; } DB_stub;
typedef struct { int dummy; } Deck_stub;
typedef struct { int dummy; } CardAtlas_stub;
typedef struct { int dummy; } BiomeDef_stub;
typedef struct { int fds[2]; } NFCReader_stub;

/* Battlefield stub -- combat now iterates bf->entities[] for targeting.
 * Must have entities array and entityCount to match battlefield.h layout. */
typedef struct { int dummy; } Territory_stub;

/* SustenanceNode/SustenanceField stubs -- must match sustenance.h struct sizes for correct layout */
typedef struct {
    int id;
    BattleSide side;
    int slotIndex;
    int gridRow, gridCol;
    CanonicalPos worldPos;
    bool active;
    int claimedByEntityId;
    int sustenanceType;
    int value;
    int durability;
    int maxDurability;
} SustenanceNode_stub;

typedef struct {
    SustenanceNode_stub nodes[2][SUSTENANCE_MATCH_COUNT_PER_SIDE];
    uint32_t rngState;
} SustenanceField_stub;

typedef struct Battlefield {
    float boardWidth, boardHeight, seamY;
    Territory_stub territories[2];
    /* Canonical lane waypoints -- not accessed by combat */
    char _waypoints_pad[2 * 3 * LANE_WAYPOINT_COUNT * sizeof(CanonicalPos)];
    /* Slot spawn anchors -- not accessed by combat */
    char _spawn_pad[2 * NUM_CARD_SLOTS * sizeof(CanonicalPos)];
    /* Sustenance field -- not accessed by combat but must be present for layout */
    SustenanceField_stub sustenanceField;
    /* Entity registry -- used by combat_find_target */
    Entity *entities[MAX_ENTITIES * 2];
    int entityCount;
} Battlefield;

struct GameState {
    DB_stub db;
    Deck_stub deck;
    CardAtlas_stub cardAtlas;
    Player players[2];
    BiomeDef_stub biomeDefs[BIOME_COUNT];
    Battlefield battlefield;
    SpriteAtlas spriteAtlas;
    int halfWidth;
    NFCReader_stub nfc;
    bool gameOver;
    int winnerID;
};

/* ---- Stub functions required by combat.c ---- */
void anim_state_init(AnimState *state, AnimationType anim, SpriteDirection dir,
                     float cycleDuration, bool oneShot) {
    state->anim = anim;
    state->dir = dir;
    state->elapsed = 0.0f;
    state->cycleDuration = cycleDuration;
    state->normalizedTime = 0.0f;
    state->mode = oneShot ? ANIM_PLAY_ONCE : ANIM_PLAY_LOOP;
    state->oneShot = oneShot;
    state->finished = false;
    state->flipH = false;
}

enum { DEBUG_EVT_STATE_CHANGE = 0, DEBUG_EVT_HIT = 1, DEBUG_EVT_DEATH_FINISH = 2 };
void debug_event_emit_xy(float x, float y, int type) {
    (void)x; (void)y; (void)type;
}

void entity_set_state(Entity *e, EntityState newState) {
    if (!e || e->state == newState) return;
    e->state = newState;
}

/* win_condition stubs — must precede combat.c include */
void win_trigger(GameState *gs, int winnerID) {
    if (!gs || gs->gameOver) return;
    gs->gameOver = true;
    gs->winnerID = winnerID;
}

void win_latch_from_destroyed_base(GameState *gs, const Entity *destroyedBase) {
    if (!gs || !destroyedBase || gs->gameOver) return;
    for (int i = 0; i < 2; i++) {
        if (gs->players[i].base == (void *)destroyedBase) {
            win_trigger(gs, 1 - i);
            return;
        }
    }
}

static int s_farmerOnDeathCalls = 0;
static Entity *s_lastFarmerOnDeath = NULL;
static GameState *s_lastFarmerOnDeathGameState = NULL;

/* farmer_on_death stub -- combat.c now calls this on farmer kills */
void farmer_on_death(Entity *farmer, GameState *gs) {
    s_farmerOnDeathCalls++;
    s_lastFarmerOnDeath = farmer;
    s_lastFarmerOnDeathGameState = gs;
}

/* ---- Include combat.c directly ---- */
#include "../src/logic/combat.c"

/* ---- Stub functions required by building.c ---- */
Entity *entity_create(EntityType type, Faction faction, Vector2 pos) {
    (void)type;
    (void)faction;
    (void)pos;
    return NULL;
}

const CharacterSprite *sprite_atlas_get(const SpriteAtlas *atlas, SpriteType type) {
    (void)atlas;
    (void)type;
    return NULL;
}

void entity_sync_animation(Entity *e) {
    (void)e;
}

float troop_default_body_radius(SpriteType type) {
    (void)type;
    return 16.0f;
}

/* ---- Deposit slot stub (building.c calls this on base creation) ---- */
void deposit_slots_build_for_base(Entity *base) {
    if (!base) return;
    base->depositSlots.initialized = true;
}

/* ---- Include building.c directly ---- */
#include "../src/entities/building.c"

/* ---- Test helpers ---- */
static int s_nextID = 1;

static Entity make_entity(int ownerID, EntityType type, Vector2 pos) {
    Entity e = {0};
    e.id = s_nextID++;
    e.type = type;
    e.faction = (ownerID == 0) ? FACTION_PLAYER1 : FACTION_PLAYER2;
    e.state = ESTATE_WALKING;
    e.position = pos;
    e.hp = 100;
    e.maxHP = 100;
    e.attack = 10;
    e.attackSpeed = 1.0f;
    e.attackRange = 50.0f;
    e.moveSpeed = 60.0f;
    e.targeting = TARGET_NEAREST;
    e.targetType = NULL;
    e.ownerID = ownerID;
    e.lane = 1;
    e.alive = true;
    e.markedForRemoval = false;
    e.attackCooldown = 0.0f;
    e.attackReleaseFired = false;
    e.combatProfileId = COMBAT_PROFILE_DEFAULT_MELEE;
    e.engagementMode = ATTACK_ENGAGEMENT_CONTACT;
    e.deliveryMode = ATTACK_DELIVERY_INSTANT;
    e.projectileVisualType = PROJECTILE_VISUAL_NONE;
    e.projectileSpeed = 0.0f;
    e.projectileHitRadius = 0.0f;
    e.projectileRenderScale = 1.0f;
    e.projectileLaunchOffset = (Vector2){0};
    e.bodyRadius = 14.0f;
    e.navRadius = 14.0f;
    e.navProfile = NAV_PROFILE_LANE;
    anim_state_init(&e.anim, ANIM_WALK, DIR_DOWN, 0.8f, false);
    return e;
}

static Vector2 test_base_anchor(const Entity *base) {
    Vector2 anchor = base->position;
    if (base->presentationSide == SIDE_TOP) {
        anchor.y -= BASE_INTERACTION_BACK_OFFSET;
    } else {
        anchor.y += BASE_INTERACTION_BACK_OFFSET;
    }
    return anchor;
}

static GameState make_game_state(void) {
    GameState gs = {0};
    /* All entities now use canonical coordinates (0..1080 x, 0..1920 y).
     * P1 (SIDE_BOTTOM) territory: y=960..1920.
     * P2 (SIDE_TOP) territory: y=0..960.
     * Entities are in the Battlefield registry, not on Player. */
    gs.players[0].id = 0;
    gs.players[0].side = SIDE_BOTTOM;

    gs.players[1].id = 1;
    gs.players[1].side = SIDE_TOP;

    gs.battlefield.boardWidth = 1080;
    gs.battlefield.boardHeight = 1920;
    gs.battlefield.seamY = 960;
    gs.battlefield.entityCount = 0;

    gs.halfWidth = 960;
    gs.gameOver = false;
    gs.winnerID = -1;
    return gs;
}

/* Helper: add entity to Battlefield registry for targeting */
static void bf_test_add_entity(GameState *gs, Entity *e) {
    gs->battlefield.entities[gs->battlefield.entityCount++] = e;
}

/* ---- Tests ---- */
static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(fn) do { \
    printf("  "); \
    fn(); \
    tests_run++; \
    tests_passed++; \
    printf("PASS: %s\n", #fn); \
} while(0)

static void reset_test_observers(void) {
    s_farmerOnDeathCalls = 0;
    s_lastFarmerOnDeath = NULL;
    s_lastFarmerOnDeathGameState = NULL;
}

static void test_in_range_same_space(void) {
    GameState gs = make_game_state();
    /* Two entities in canonical space -- direct distance */
    Entity a = make_entity(0, ENTITY_TROOP, (Vector2){100, 1200});
    Entity b = make_entity(0, ENTITY_TROOP, (Vector2){130, 1200});
    a.attackRange = 50.0f;

    assert(combat_in_range(&a, &b, &gs) == true);  /* dist=30 <= 50 */

    b.position = (Vector2){200, 1200};
    assert(combat_in_range(&a, &b, &gs) == false);  /* dist=100 > 50 */
}

static void test_in_range_cross_space(void) {
    GameState gs = make_game_state();
    /* Canonical-space melee checks still work across the seam because combat
     * no longer remaps coordinates per player. */
    Entity a = make_entity(0, ENTITY_TROOP, (Vector2){540, 970});
    a.attackRange = 50.0f;

    Entity b = make_entity(1, ENTITY_TROOP, (Vector2){540, 950});

    assert(combat_in_range(&a, &b, &gs) == true);

    b.position = (Vector2){540, 890};
    assert(combat_in_range(&a, &b, &gs) == false);
}

static void test_in_range_null_safety(void) {
    GameState gs = make_game_state();
    Entity a = make_entity(0, ENTITY_TROOP, (Vector2){100, 1200});
    assert(combat_in_range(NULL, &a, &gs) == false);
    assert(combat_in_range(&a, NULL, &gs) == false);
}

/* Static-target attacks are gated only by distance now, not by assault-slot
 * reservations. */
static void test_in_range_static_target_uses_attack_radius(void) {
    GameState gs = make_game_state();

    Entity knight = make_entity(0, ENTITY_TROOP, (Vector2){540, 1568});
    knight.attackRange = 50.0f;
    knight.bodyRadius = 14.0f;

    Entity base = make_entity(1, ENTITY_BUILDING, (Vector2){540, 1616});
    base.id = 2;
    base.bodyRadius = 16.0f;
    base.navRadius = 56.0f;
    base.navProfile = NAV_PROFILE_STATIC;
    Vector2 anchor = test_base_anchor(&base);

    float attackRadius = combat_static_target_attack_radius(&knight, &base);

    knight.position = (Vector2){540.0f, anchor.y - attackRadius + 0.5f};
    assert(combat_in_range(&knight, &base, &gs) == true);

    knight.position = (Vector2){540.0f, anchor.y - attackRadius - 0.5f};
    assert(combat_in_range(&knight, &base, &gs) == false);
}

/* Multiple attackers can all hit the same static target once they enter the
 * attack shell; there is no hidden slot-cap on base DPS. */
static void test_in_range_multiple_attackers_share_static_target_cloud(void) {
    GameState gs = make_game_state();

    Entity knightA = make_entity(0, ENTITY_TROOP, (Vector2){540, 1568});
    knightA.attackRange = 50.0f;
    knightA.bodyRadius = 14.0f;

    Entity knightB = make_entity(0, ENTITY_TROOP, (Vector2){568, 1568});
    knightB.attackRange = 50.0f;
    knightB.bodyRadius = 14.0f;

    Entity base = make_entity(1, ENTITY_BUILDING, (Vector2){540, 1616});
    base.id = 2;
    base.bodyRadius = 16.0f;
    base.navRadius = 56.0f;
    base.navProfile = NAV_PROFILE_STATIC;
    Vector2 anchor = test_base_anchor(&base);

    float attackRadiusA = combat_static_target_attack_radius(&knightA, &base);
    float attackRadiusB = combat_static_target_attack_radius(&knightB, &base);
    knightA.position = (Vector2){540.0f, anchor.y - attackRadiusA + 0.5f};
    knightB.position = (Vector2){anchor.x + attackRadiusB - 0.5f, anchor.y};

    assert(combat_in_range(&knightA, &base, &gs) == true);
    assert(combat_in_range(&knightB, &base, &gs) == true);
}

/* Troop-vs-troop melee uses near-contact geometry plus a small authored
 * slack, not raw center-distance-to-target-range. */
static void test_in_range_unit_vs_unit_uses_contact_geometry(void) {
    GameState gs = make_game_state();
    Entity a = make_entity(0, ENTITY_TROOP, (Vector2){100, 1200});
    Entity b = make_entity(0, ENTITY_TROOP, (Vector2){150, 1200});
    a.attackRange = 40.0f;
    a.bodyRadius = 14.0f;
    b.bodyRadius = 14.0f;
    b.navRadius = 14.0f; /* matches bodyRadius -> zero slack */

    assert(combat_in_range(&a, &b, &gs) == false);

    b.position = (Vector2){138, 1200};
    assert(combat_in_range(&a, &b, &gs) == true);
}

static void test_find_target_nearest(void) {
    GameState gs = make_game_state();
    /* Attacker at P1 near seam */
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 970});
    attacker.targeting = TARGET_NEAREST;

    /* Two enemy entities in canonical P2 territory */
    Entity far_e = make_entity(1, ENTITY_TROOP, (Vector2){540, 300});
    Entity near_e = make_entity(1, ENTITY_TROOP, (Vector2){540, 900});

    bf_test_add_entity(&gs, &far_e);
    bf_test_add_entity(&gs, &near_e);

    Entity *target = combat_find_target(&attacker, &gs);
    assert(target == &near_e);
}

static void test_find_target_skips_dead(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 970});

    Entity dead = make_entity(1, ENTITY_TROOP, (Vector2){540, 900});
    dead.alive = false;

    Entity alive_e = make_entity(1, ENTITY_TROOP, (Vector2){540, 300});

    bf_test_add_entity(&gs, &dead);
    bf_test_add_entity(&gs, &alive_e);

    Entity *target = combat_find_target(&attacker, &gs);
    assert(target == &alive_e);
}

static void test_find_target_skips_marked(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 970});

    Entity marked = make_entity(1, ENTITY_TROOP, (Vector2){540, 900});
    marked.markedForRemoval = true;

    Entity valid = make_entity(1, ENTITY_TROOP, (Vector2){540, 300});

    bf_test_add_entity(&gs, &marked);
    bf_test_add_entity(&gs, &valid);

    Entity *target = combat_find_target(&attacker, &gs);
    assert(target == &valid);
}

static void test_find_target_building_priority(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 970});
    attacker.targeting = TARGET_BUILDING;

    /* Troop is closer, building is farther */
    Entity troop = make_entity(1, ENTITY_TROOP, (Vector2){540, 900});
    Entity building = make_entity(1, ENTITY_BUILDING, (Vector2){540, 300});

    bf_test_add_entity(&gs, &troop);
    bf_test_add_entity(&gs, &building);

    Entity *target = combat_find_target(&attacker, &gs);
    assert(target == &building);
}

static void test_find_target_returns_null_no_enemies(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 970});

    /* No entities in Battlefield registry */
    Entity *target = combat_find_target(&attacker, &gs);
    assert(target == NULL);
}

static void test_find_target_skips_friendly(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 970});

    /* Only friendly entities in the registry -- should return NULL */
    Entity friendly = make_entity(0, ENTITY_TROOP, (Vector2){540, 1200});

    bf_test_add_entity(&gs, &friendly);

    Entity *target = combat_find_target(&attacker, &gs);
    assert(target == NULL);
}

static void test_resolve_deals_damage(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});
    attacker.attack = 25;
    attacker.attackSpeed = 1.0f;
    attacker.attackCooldown = 0.0f;

    Entity target = make_entity(1, ENTITY_TROOP, (Vector2){0, 0});
    target.hp = 100;
    target.maxHP = 100;

    combat_resolve(&attacker, &target, &gs, 0.016f);

    assert(target.hp == 75);
    assert(attacker.attackCooldown > 0.0f);
}

static void test_resolve_respects_cooldown(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});
    attacker.attack = 25;
    attacker.attackSpeed = 1.0f;
    attacker.attackCooldown = 0.5f;  /* still on cooldown */

    Entity target = make_entity(1, ENTITY_TROOP, (Vector2){0, 0});
    target.hp = 100;

    combat_resolve(&attacker, &target, &gs, 0.016f);

    /* Cooldown decremented but no damage dealt */
    assert(target.hp == 100);
    assert(attacker.attackCooldown < 0.5f);
}

static void test_resolve_kills_at_zero_hp(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});
    attacker.attack = 150;  /* overkill */
    attacker.attackSpeed = 1.0f;
    attacker.attackCooldown = 0.0f;

    Entity target = make_entity(1, ENTITY_TROOP, (Vector2){0, 0});
    target.hp = 100;
    target.maxHP = 100;

    combat_resolve(&attacker, &target, &gs, 0.016f);

    assert(target.hp == 0);
    assert(target.alive == false);
    assert(target.state == ESTATE_DEAD);
}

static void test_resolve_skips_dead_target(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});
    attacker.attackCooldown = 0.0f;

    Entity target = make_entity(1, ENTITY_TROOP, (Vector2){0, 0});
    target.alive = false;
    target.hp = 0;

    combat_resolve(&attacker, &target, &gs, 0.016f);

    /* Cooldown should not have been set (no attack happened) */
    assert(attacker.attackCooldown == 0.0f);
}

static void test_take_damage_basic(void) {
    Entity e = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});
    e.hp = 100;
    e.maxHP = 100;

    entity_take_damage(&e, 30);

    assert(e.hp == 70);
    assert(e.alive == true);
    assert(e.state == ESTATE_WALKING);  /* unchanged */
}

static void test_take_damage_kills(void) {
    Entity e = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});
    e.hp = 20;
    e.maxHP = 100;

    entity_take_damage(&e, 50);

    assert(e.hp == 0);
    assert(e.alive == false);
    assert(e.state == ESTATE_DEAD);
}

static void test_take_damage_clamps_zero(void) {
    Entity e = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});
    e.hp = 10;

    entity_take_damage(&e, 999);

    assert(e.hp == 0);  /* clamped, not negative */
    assert(e.alive == false);
}

static void test_take_damage_null_safety(void) {
    /* Should not crash */
    entity_take_damage(NULL, 10);
}

static void test_canonical_distance_direct(void) {
    /* Verify that combat_in_range uses direct canonical distance for
     * cross-side entities. Two entities at the seam from opposite sides
     * should be close together. */
    GameState gs = make_game_state();

    /* P1 entity at seam center */
    Entity a = make_entity(0, ENTITY_TROOP, (Vector2){540, 960});
    a.attackRange = 10.0f;

    /* P2 entity at seam center */
    Entity b = make_entity(1, ENTITY_TROOP, (Vector2){540, 960});

    /* Same position -- distance 0 <= 10 */
    assert(combat_in_range(&a, &b, &gs) == true);
}

/* ---- combat_apply_hit tests ---- */

static void test_apply_hit_deals_damage(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});
    attacker.attack = 25;

    Entity target = make_entity(1, ENTITY_TROOP, (Vector2){0, 0});
    target.hp = 100;
    target.maxHP = 100;

    combat_apply_hit(&attacker, &target, &gs);

    assert(target.hp == 75);
    assert(target.alive == true);
}

static void test_apply_hit_kills(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});
    attacker.attack = 150;

    Entity target = make_entity(1, ENTITY_TROOP, (Vector2){0, 0});
    target.hp = 100;
    target.maxHP = 100;

    combat_apply_hit(&attacker, &target, &gs);

    assert(target.hp == 0);
    assert(target.alive == false);
    assert(target.state == ESTATE_DEAD);
}

static void test_apply_hit_skips_dead(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});
    attacker.attack = 25;

    Entity target = make_entity(1, ENTITY_TROOP, (Vector2){0, 0});
    target.hp = 0;
    target.alive = false;

    combat_apply_hit(&attacker, &target, &gs);

    /* Dead target should not take further damage */
    assert(target.hp == 0);
}

static void test_apply_hit_null_safety(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});
    /* Should not crash */
    combat_apply_hit(NULL, &attacker, &gs);
    combat_apply_hit(&attacker, NULL, &gs);
}

/* ---- combat_apply_king_burst tests ---- */

static void test_king_burst_damages_only_live_enemy_non_projectiles_in_radius(void) {
    reset_test_observers();
    GameState gs = make_game_state();

    Entity base = make_entity(0, ENTITY_BUILDING, (Vector2){540, 1600});
    base.hp = 5000;
    base.maxHP = 5000;

    Entity enemyTroop = make_entity(1, ENTITY_TROOP, (Vector2){600, 1600});
    Entity enemyBuilding = make_entity(1, ENTITY_BUILDING, (Vector2){540, 1490});
    Entity friendlyTroop = make_entity(0, ENTITY_TROOP, (Vector2){520, 1600});
    Entity friendlyBuilding = make_entity(0, ENTITY_BUILDING, (Vector2){540, 1510});
    Entity projectile = make_entity(1, ENTITY_PROJECTILE, (Vector2){540, 1580});
    Entity deadEnemy = make_entity(1, ENTITY_TROOP, (Vector2){560, 1600});
    Entity markedEnemy = make_entity(1, ENTITY_TROOP, (Vector2){580, 1600});
    Entity farEnemy = make_entity(1, ENTITY_TROOP, (Vector2){540, 1785});

    deadEnemy.alive = false;
    deadEnemy.hp = 0;
    markedEnemy.markedForRemoval = true;

    bf_test_add_entity(&gs, &base);
    bf_test_add_entity(&gs, &enemyTroop);
    bf_test_add_entity(&gs, &enemyBuilding);
    bf_test_add_entity(&gs, &friendlyTroop);
    bf_test_add_entity(&gs, &friendlyBuilding);
    bf_test_add_entity(&gs, &projectile);
    bf_test_add_entity(&gs, &deadEnemy);
    bf_test_add_entity(&gs, &markedEnemy);
    bf_test_add_entity(&gs, &farEnemy);

    combat_apply_king_burst(&base, 160.0f, 30, &gs);

    assert(base.hp == 5000);
    assert(enemyTroop.hp == 70);
    assert(enemyBuilding.hp == 70);
    assert(friendlyTroop.hp == 100);
    assert(friendlyBuilding.hp == 100);
    assert(projectile.hp == 100);
    assert(deadEnemy.hp == 0);
    assert(markedEnemy.hp == 100);
    assert(farEnemy.hp == 100);
    assert(s_farmerOnDeathCalls == 0);
    assert(gs.gameOver == false);
}

static void test_king_burst_farmer_kill_calls_farmer_on_death(void) {
    reset_test_observers();
    GameState gs = make_game_state();

    Entity base = make_entity(0, ENTITY_BUILDING, (Vector2){540, 1600});
    Entity farmer = make_entity(1, ENTITY_TROOP, (Vector2){600, 1600});
    farmer.unitRole = UNIT_ROLE_FARMER;
    farmer.hp = 20;
    farmer.maxHP = 20;

    bf_test_add_entity(&gs, &base);
    bf_test_add_entity(&gs, &farmer);

    combat_apply_king_burst(&base, 160.0f, 30, &gs);

    assert(farmer.hp == 0);
    assert(farmer.alive == false);
    assert(farmer.state == ESTATE_DEAD);
    assert(s_farmerOnDeathCalls == 1);
    assert(s_lastFarmerOnDeath == &farmer);
    assert(s_lastFarmerOnDeathGameState == &gs);
}

static void test_king_burst_kills_enemy_base_latches_win(void) {
    reset_test_observers();
    GameState gs = make_game_state();

    Entity base = make_entity(0, ENTITY_BUILDING, (Vector2){540, 1600});
    Entity enemyBase = make_entity(1, ENTITY_BUILDING, (Vector2){540, 1500});
    enemyBase.hp = 25;
    enemyBase.maxHP = 5000;
    gs.players[1].base = (void *)&enemyBase;

    bf_test_add_entity(&gs, &base);
    bf_test_add_entity(&gs, &enemyBase);

    combat_apply_king_burst(&base, 160.0f, 30, &gs);

    assert(enemyBase.hp == 0);
    assert(enemyBase.alive == false);
    assert(gs.gameOver == true);
    assert(gs.winnerID == 0);
}

static void test_king_burst_kills_non_base_building_without_match_end(void) {
    reset_test_observers();
    GameState gs = make_game_state();

    Entity base = make_entity(0, ENTITY_BUILDING, (Vector2){540, 1600});
    Entity enemyBuilding = make_entity(1, ENTITY_BUILDING, (Vector2){540, 1500});
    enemyBuilding.hp = 25;
    enemyBuilding.maxHP = 100;

    bf_test_add_entity(&gs, &base);
    bf_test_add_entity(&gs, &enemyBuilding);

    combat_apply_king_burst(&base, 160.0f, 30, &gs);

    assert(enemyBuilding.hp == 0);
    assert(enemyBuilding.alive == false);
    assert(gs.gameOver == false);
    assert(gs.winnerID == -1);
}

static void test_king_burst_null_and_invalid_params_are_noops(void) {
    reset_test_observers();
    GameState gs = make_game_state();

    Entity base = make_entity(0, ENTITY_BUILDING, (Vector2){540, 1600});
    Entity enemy = make_entity(1, ENTITY_TROOP, (Vector2){540, 1500});

    bf_test_add_entity(&gs, &base);
    bf_test_add_entity(&gs, &enemy);

    combat_apply_king_burst(NULL, 160.0f, 30, &gs);
    combat_apply_king_burst(&base, 0.0f, 30, &gs);
    combat_apply_king_burst(&base, 160.0f, 0, &gs);
    combat_apply_king_burst(&base, 160.0f, 30, NULL);

    assert(enemy.hp == 100);
    assert(enemy.alive == true);
    assert(s_farmerOnDeathCalls == 0);
    assert(gs.gameOver == false);
    assert(gs.winnerID == -1);
}

/* ---- Win-latch path tests ---- */

static void test_apply_hit_kills_p1_base_latches_p2_win(void) {
    GameState gs = make_game_state();
    Entity base0 = make_entity(0, ENTITY_BUILDING, (Vector2){540, 1600});
    base0.hp = 10;
    base0.maxHP = 5000;
    gs.players[0].base = (void *)&base0;

    Entity attacker = make_entity(1, ENTITY_TROOP, (Vector2){540, 1590});
    attacker.attack = 100;

    combat_apply_hit(&attacker, &base0, &gs);

    assert(gs.gameOver == true);
    assert(gs.winnerID == 1);
}

static void test_resolve_kills_p2_base_latches_p1_win(void) {
    GameState gs = make_game_state();
    Entity base1 = make_entity(1, ENTITY_BUILDING, (Vector2){540, 300});
    base1.hp = 10;
    base1.maxHP = 5000;
    gs.players[1].base = (void *)&base1;

    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 310});
    attacker.attack = 100;
    attacker.attackSpeed = 1.0f;
    attacker.attackCooldown = 0.0f;

    combat_resolve(&attacker, &base1, &gs, 0.016f);

    assert(gs.gameOver == true);
    assert(gs.winnerID == 0);
}

static void test_killing_non_base_building_no_match_end(void) {
    GameState gs = make_game_state();
    Entity building = make_entity(1, ENTITY_BUILDING, (Vector2){540, 500});
    building.hp = 10;
    building.maxHP = 100;
    /* Not assigned as anyone's base */

    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 510});
    attacker.attack = 100;

    combat_apply_hit(&attacker, &building, &gs);

    assert(building.hp == 0);
    assert(building.alive == false);
    assert(gs.gameOver == false);
    assert(gs.winnerID == -1);
}

static void test_latch_holds_after_second_base_kill(void) {
    GameState gs = make_game_state();
    Entity base0 = make_entity(0, ENTITY_BUILDING, (Vector2){540, 1600});
    base0.hp = 10;
    base0.maxHP = 5000;
    gs.players[0].base = (void *)&base0;

    Entity base1 = make_entity(1, ENTITY_BUILDING, (Vector2){540, 300});
    base1.hp = 10;
    base1.maxHP = 5000;
    gs.players[1].base = (void *)&base1;

    Entity attacker0 = make_entity(1, ENTITY_TROOP, (Vector2){540, 1590});
    attacker0.attack = 100;

    Entity attacker1 = make_entity(0, ENTITY_TROOP, (Vector2){540, 310});
    attacker1.attack = 100;

    /* First kill latches */
    combat_apply_hit(&attacker0, &base0, &gs);
    assert(gs.gameOver == true);
    assert(gs.winnerID == 1);

    /* Second kill must not flip */
    combat_apply_hit(&attacker1, &base1, &gs);
    assert(gs.winnerID == 1);
}

static void test_building_take_damage_null_safety(void) {
    GameState gs = make_game_state();

    building_take_damage(NULL, 10, &gs);

    assert(gs.gameOver == false);
    assert(gs.winnerID == -1);
}

static void test_building_take_damage_kills_p1_base_latches_p2_win(void) {
    GameState gs = make_game_state();
    Entity base0 = make_entity(0, ENTITY_BUILDING, (Vector2){540, 1600});
    base0.hp = 10;
    base0.maxHP = 5000;
    gs.players[0].base = (void *)&base0;

    building_take_damage(&base0, 100, &gs);

    assert(base0.hp == 0);
    assert(base0.alive == false);
    assert(gs.gameOver == true);
    assert(gs.winnerID == 1);
}

static void test_building_take_damage_kills_non_base_no_match_end(void) {
    GameState gs = make_game_state();
    Entity building = make_entity(1, ENTITY_BUILDING, (Vector2){540, 500});
    building.hp = 10;
    building.maxHP = 100;

    building_take_damage(&building, 100, &gs);

    assert(building.hp == 0);
    assert(building.alive == false);
    assert(gs.gameOver == false);
    assert(gs.winnerID == -1);
}

/* ---- Healer support behavior tests ---- */

static Entity make_healer(int ownerID, Vector2 pos) {
    Entity h = make_entity(ownerID, ENTITY_TROOP, pos);
    h.attack = 5;
    h.healAmount = 8;
    h.attackRange = 80.0f;
    h.combatProfileId = COMBAT_PROFILE_HEALER;
    h.engagementMode = ATTACK_ENGAGEMENT_DIRECT_RANGE;
    h.deliveryMode = ATTACK_DELIVERY_PROJECTILE;
    return h;
}

static void test_healer_prefers_injured_ally_in_range(void) {
    GameState gs = make_game_state();
    Entity healer = make_healer(0, (Vector2){540, 970});

    Entity injured_ally = make_entity(0, ENTITY_TROOP, (Vector2){540, 1000});
    injured_ally.hp = 50;

    Entity enemy = make_entity(1, ENTITY_TROOP, (Vector2){540, 950});

    bf_test_add_entity(&gs, &injured_ally);
    bf_test_add_entity(&gs, &enemy);

    Entity *target = combat_find_target(&healer, &gs);
    assert(target == &injured_ally);
}

static void test_healer_falls_back_to_enemy_when_no_injured_ally(void) {
    GameState gs = make_game_state();
    Entity healer = make_healer(0, (Vector2){540, 970});

    Entity full_ally = make_entity(0, ENTITY_TROOP, (Vector2){540, 1000});
    Entity enemy = make_entity(1, ENTITY_TROOP, (Vector2){540, 950});

    bf_test_add_entity(&gs, &full_ally);
    bf_test_add_entity(&gs, &enemy);

    Entity *target = combat_find_target(&healer, &gs);
    assert(target == &enemy);
}

static void test_healer_ignores_off_range_injured_ally(void) {
    GameState gs = make_game_state();
    Entity healer = make_healer(0, (Vector2){540, 970});
    healer.attackRange = 50.0f;

    /* Ally is injured but far outside attack range */
    Entity far_injured_ally = make_entity(0, ENTITY_TROOP, (Vector2){540, 1500});
    far_injured_ally.hp = 50;

    Entity enemy = make_entity(1, ENTITY_TROOP, (Vector2){540, 950});

    bf_test_add_entity(&gs, &far_injured_ally);
    bf_test_add_entity(&gs, &enemy);

    Entity *target = combat_find_target(&healer, &gs);
    assert(target == &enemy);
}

static void test_healer_ignores_self(void) {
    GameState gs = make_game_state();
    Entity healer = make_healer(0, (Vector2){540, 970});
    healer.hp = 50;  /* injured -- if we forgot to skip self, healer would target itself */

    bf_test_add_entity(&gs, &healer);

    Entity *target = combat_find_target(&healer, &gs);
    assert(target == NULL);  /* no enemies, no other allies, no self */
}

static void test_healer_ignores_friendly_base(void) {
    GameState gs = make_game_state();
    Entity healer = make_healer(0, (Vector2){540, 970});

    Entity friendly_base = make_entity(0, ENTITY_BUILDING, (Vector2){540, 1000});
    friendly_base.hp = 100;
    friendly_base.maxHP = 5000;  /* injured */

    Entity enemy = make_entity(1, ENTITY_TROOP, (Vector2){540, 950});

    bf_test_add_entity(&gs, &friendly_base);
    bf_test_add_entity(&gs, &enemy);

    Entity *target = combat_find_target(&healer, &gs);
    assert(target == &enemy);  /* base is skipped, enemy is selected */
}

static void test_healer_ignores_dead_ally(void) {
    GameState gs = make_game_state();
    Entity healer = make_healer(0, (Vector2){540, 970});

    Entity dead_ally = make_entity(0, ENTITY_TROOP, (Vector2){540, 1000});
    dead_ally.hp = 0;
    dead_ally.alive = false;

    Entity enemy = make_entity(1, ENTITY_TROOP, (Vector2){540, 950});

    bf_test_add_entity(&gs, &dead_ally);
    bf_test_add_entity(&gs, &enemy);

    Entity *target = combat_find_target(&healer, &gs);
    assert(target == &enemy);
}

static void test_healer_ignores_marked_ally(void) {
    GameState gs = make_game_state();
    Entity healer = make_healer(0, (Vector2){540, 970});

    Entity marked_ally = make_entity(0, ENTITY_TROOP, (Vector2){540, 1000});
    marked_ally.hp = 50;
    marked_ally.markedForRemoval = true;

    Entity enemy = make_entity(1, ENTITY_TROOP, (Vector2){540, 950});

    bf_test_add_entity(&gs, &marked_ally);
    bf_test_add_entity(&gs, &enemy);

    Entity *target = combat_find_target(&healer, &gs);
    assert(target == &enemy);
}

static void test_non_healer_never_targets_friendly(void) {
    GameState gs = make_game_state();
    /* Plain combat troop, no healAmount */
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 970});

    Entity injured_ally = make_entity(0, ENTITY_TROOP, (Vector2){540, 1000});
    injured_ally.hp = 50;

    Entity enemy = make_entity(1, ENTITY_TROOP, (Vector2){540, 950});

    bf_test_add_entity(&gs, &injured_ally);
    bf_test_add_entity(&gs, &enemy);

    Entity *target = combat_find_target(&attacker, &gs);
    assert(target == &enemy);
}

static void test_apply_hit_heals_ally(void) {
    GameState gs = make_game_state();
    Entity healer = make_healer(0, (Vector2){540, 970});

    Entity ally = make_entity(0, ENTITY_TROOP, (Vector2){540, 1000});
    ally.hp = 80;
    ally.maxHP = 100;

    combat_apply_hit(&healer, &ally, &gs);

    assert(ally.hp == 88);
    assert(ally.alive == true);
}

static void test_apply_hit_heal_clamps_at_maxhp(void) {
    GameState gs = make_game_state();
    Entity healer = make_healer(0, (Vector2){540, 970});

    Entity ally = make_entity(0, ENTITY_TROOP, (Vector2){540, 1000});
    ally.hp = 97;
    ally.maxHP = 100;

    combat_apply_hit(&healer, &ally, &gs);

    assert(ally.hp == 100);
}

static void test_apply_hit_rejects_full_health_friendly_troop(void) {
    GameState gs = make_game_state();
    Entity healer = make_healer(0, (Vector2){540, 970});

    Entity ally = make_entity(0, ENTITY_TROOP, (Vector2){540, 1000});
    ally.hp = 100;
    ally.maxHP = 100;

    int hp_before = ally.hp;
    combat_apply_hit(&healer, &ally, &gs);

    assert(ally.hp == hp_before);
    assert(ally.alive == true);
}

static void test_healer_retargets_to_enemy_after_ally_fully_healed(void) {
    GameState gs = make_game_state();
    Entity healer = make_healer(0, (Vector2){540, 970});

    Entity ally = make_entity(0, ENTITY_TROOP, (Vector2){540, 1000});
    ally.hp = 95;
    ally.maxHP = 100;

    Entity enemy = make_entity(1, ENTITY_TROOP, (Vector2){540, 950});

    bf_test_add_entity(&gs, &ally);
    bf_test_add_entity(&gs, &enemy);

    /* First target: injured ally */
    Entity *first = combat_find_target(&healer, &gs);
    assert(first == &ally);

    /* Heal lands -- ally is now full HP */
    combat_apply_hit(&healer, &ally, &gs);
    assert(ally.hp == 100);

    /* Retarget: should now pick the enemy */
    Entity *second = combat_find_target(&healer, &gs);
    assert(second == &enemy);
}

static void test_apply_hit_rejects_heal_on_friendly_base(void) {
    /* Stale/direct caller hands a healer a friendly base. This must be a
     * strict no-op: no healing and no fallback damage. */
    GameState gs = make_game_state();
    Entity healer = make_healer(0, (Vector2){540, 970});

    Entity friendly_base = make_entity(0, ENTITY_BUILDING, (Vector2){540, 1600});
    friendly_base.hp = 50;
    friendly_base.maxHP = 5000;

    int hp_before = friendly_base.hp;
    combat_apply_hit(&healer, &friendly_base, &gs);

    assert(friendly_base.hp == hp_before);
}

static void test_apply_hit_rejects_self_heal(void) {
    GameState gs = make_game_state();
    Entity healer = make_healer(0, (Vector2){540, 970});
    healer.hp = 50;
    healer.maxHP = 100;

    int hp_before = healer.hp;
    combat_apply_hit(&healer, &healer, &gs);

    assert(healer.hp == hp_before);
}

static void test_apply_hit_rejects_marked_friendly_troop(void) {
    GameState gs = make_game_state();
    Entity healer = make_healer(0, (Vector2){540, 970});

    Entity ally = make_entity(0, ENTITY_TROOP, (Vector2){540, 1000});
    ally.hp = 50;
    ally.maxHP = 100;
    ally.markedForRemoval = true;

    int hp_before = ally.hp;
    combat_apply_hit(&healer, &ally, &gs);

    assert(ally.hp == hp_before);
}

static void test_resolve_rejects_full_health_friendly_troop_without_cooldown(void) {
    GameState gs = make_game_state();
    Entity healer = make_healer(0, (Vector2){0, 0});
    healer.attackSpeed = 1.0f;
    healer.attackCooldown = 0.0f;

    Entity ally = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});
    ally.hp = 100;
    ally.maxHP = 100;

    combat_resolve(&healer, &ally, &gs, 0.016f);

    assert(ally.hp == 100);
    assert(healer.attackCooldown == 0.0f);
}

/* ---- combat_find_target_within_radius tests ---- */

static void test_find_within_radius_returns_enemy_inside_limit(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 970});
    Entity near_enemy = make_entity(1, ENTITY_TROOP, (Vector2){540, 900});  /* dist=70 */
    Entity far_enemy  = make_entity(1, ENTITY_TROOP, (Vector2){540, 500});  /* dist=470 */

    bf_test_add_entity(&gs, &near_enemy);
    bf_test_add_entity(&gs, &far_enemy);

    Entity *target = combat_find_target_within_radius(&attacker, &gs, 100.0f);
    assert(target == &near_enemy);
}

static void test_find_within_radius_excludes_enemy_outside_limit(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 970});
    Entity far_enemy = make_entity(1, ENTITY_TROOP, (Vector2){540, 500});  /* dist=470 */

    bf_test_add_entity(&gs, &far_enemy);

    Entity *target = combat_find_target_within_radius(&attacker, &gs, 100.0f);
    assert(target == NULL);
}

static void test_find_within_radius_preserves_building_priority(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 970});
    attacker.targeting = TARGET_BUILDING;

    /* Troop is closer, building is farther -- both well inside radius */
    Entity troop    = make_entity(1, ENTITY_TROOP,    (Vector2){540, 940});  /* dist=30 */
    Entity building = make_entity(1, ENTITY_BUILDING, (Vector2){540, 870});  /* dist=100 */

    bf_test_add_entity(&gs, &troop);
    bf_test_add_entity(&gs, &building);

    Entity *target = combat_find_target_within_radius(&attacker, &gs, 200.0f);
    assert(target == &building);
}

static void test_find_within_radius_ignores_injured_ally_for_healer(void) {
    /* Walking healers use this probe. It must be enemy-only -- no heal branch --
     * so healers only seek heal targets when already in attack range (via
     * combat_find_target, which does run the heal-first branch). */
    GameState gs = make_game_state();
    Entity healer = make_healer(0, (Vector2){540, 970});

    Entity injured_ally = make_entity(0, ENTITY_TROOP, (Vector2){540, 1000});
    injured_ally.hp = 50;

    Entity enemy = make_entity(1, ENTITY_TROOP, (Vector2){540, 950});

    bf_test_add_entity(&gs, &injured_ally);
    bf_test_add_entity(&gs, &enemy);

    Entity *target = combat_find_target_within_radius(&healer, &gs, 200.0f);
    assert(target == &enemy);
}

static void test_find_within_radius_skips_dead_and_marked(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 970});

    Entity dead = make_entity(1, ENTITY_TROOP, (Vector2){540, 960});
    dead.alive = false;

    Entity marked = make_entity(1, ENTITY_TROOP, (Vector2){540, 955});
    marked.markedForRemoval = true;

    Entity valid = make_entity(1, ENTITY_TROOP, (Vector2){540, 950});

    bf_test_add_entity(&gs, &dead);
    bf_test_add_entity(&gs, &marked);
    bf_test_add_entity(&gs, &valid);

    Entity *target = combat_find_target_within_radius(&attacker, &gs, 100.0f);
    assert(target == &valid);
}

static void test_find_within_radius_null_safety(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});

    assert(combat_find_target_within_radius(NULL, &gs, 100.0f) == NULL);
    assert(combat_find_target_within_radius(&attacker, NULL, 100.0f) == NULL);
    assert(combat_find_target_within_radius(&attacker, &gs, -1.0f) == NULL);
}

/* ---- Main ---- */
int main(void) {
    printf("Running combat tests...\n");

    RUN_TEST(test_in_range_same_space);
    RUN_TEST(test_in_range_cross_space);
    RUN_TEST(test_in_range_null_safety);
    RUN_TEST(test_in_range_static_target_uses_attack_radius);
    RUN_TEST(test_in_range_multiple_attackers_share_static_target_cloud);
    RUN_TEST(test_in_range_unit_vs_unit_uses_contact_geometry);
    RUN_TEST(test_find_target_nearest);
    RUN_TEST(test_find_target_skips_dead);
    RUN_TEST(test_find_target_skips_marked);
    RUN_TEST(test_find_target_building_priority);
    RUN_TEST(test_find_target_returns_null_no_enemies);
    RUN_TEST(test_find_target_skips_friendly);
    RUN_TEST(test_resolve_deals_damage);
    RUN_TEST(test_resolve_respects_cooldown);
    RUN_TEST(test_resolve_kills_at_zero_hp);
    RUN_TEST(test_resolve_skips_dead_target);
    RUN_TEST(test_take_damage_basic);
    RUN_TEST(test_take_damage_kills);
    RUN_TEST(test_take_damage_clamps_zero);
    RUN_TEST(test_take_damage_null_safety);
    RUN_TEST(test_canonical_distance_direct);
    RUN_TEST(test_apply_hit_deals_damage);
    RUN_TEST(test_apply_hit_kills);
    RUN_TEST(test_apply_hit_skips_dead);
    RUN_TEST(test_apply_hit_null_safety);
    RUN_TEST(test_king_burst_damages_only_live_enemy_non_projectiles_in_radius);
    RUN_TEST(test_king_burst_farmer_kill_calls_farmer_on_death);
    RUN_TEST(test_king_burst_kills_enemy_base_latches_win);
    RUN_TEST(test_king_burst_kills_non_base_building_without_match_end);
    RUN_TEST(test_king_burst_null_and_invalid_params_are_noops);
    RUN_TEST(test_apply_hit_kills_p1_base_latches_p2_win);
    RUN_TEST(test_resolve_kills_p2_base_latches_p1_win);
    RUN_TEST(test_killing_non_base_building_no_match_end);
    RUN_TEST(test_latch_holds_after_second_base_kill);
    RUN_TEST(test_building_take_damage_null_safety);
    RUN_TEST(test_building_take_damage_kills_p1_base_latches_p2_win);
    RUN_TEST(test_building_take_damage_kills_non_base_no_match_end);

    RUN_TEST(test_healer_prefers_injured_ally_in_range);
    RUN_TEST(test_healer_falls_back_to_enemy_when_no_injured_ally);
    RUN_TEST(test_healer_ignores_off_range_injured_ally);
    RUN_TEST(test_healer_ignores_self);
    RUN_TEST(test_healer_ignores_friendly_base);
    RUN_TEST(test_healer_ignores_dead_ally);
    RUN_TEST(test_healer_ignores_marked_ally);
    RUN_TEST(test_non_healer_never_targets_friendly);
    RUN_TEST(test_apply_hit_heals_ally);
    RUN_TEST(test_apply_hit_heal_clamps_at_maxhp);
    RUN_TEST(test_apply_hit_rejects_full_health_friendly_troop);
    RUN_TEST(test_healer_retargets_to_enemy_after_ally_fully_healed);
    RUN_TEST(test_apply_hit_rejects_heal_on_friendly_base);
    RUN_TEST(test_apply_hit_rejects_self_heal);
    RUN_TEST(test_apply_hit_rejects_marked_friendly_troop);
    RUN_TEST(test_resolve_rejects_full_health_friendly_troop_without_cooldown);

    RUN_TEST(test_find_within_radius_returns_enemy_inside_limit);
    RUN_TEST(test_find_within_radius_excludes_enemy_outside_limit);
    RUN_TEST(test_find_within_radius_preserves_building_priority);
    RUN_TEST(test_find_within_radius_ignores_injured_ally_for_healer);
    RUN_TEST(test_find_within_radius_skips_dead_and_marked);
    RUN_TEST(test_find_within_radius_null_safety);

    printf("\nAll %d tests passed!\n", tests_passed);
    return 0;
}
