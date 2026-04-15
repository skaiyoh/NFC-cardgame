/*
 * Unit tests for src/entities/entities.c attack-state behavior.
 *
 * Self-contained: redefines minimal type stubs and includes entities.c
 * directly to avoid the heavy types.h include chain.
 *
 * Focus: attack-state behavior for troops and building one-shot clips.
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Prevent entities.c's includes from pulling in heavy headers ---- */
#define NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_CONFIG_H
#define NFC_CARDGAME_ENTITIES_H
#define NFC_CARDGAME_ENTITY_ANIMATION_H
#define NFC_CARDGAME_BATTLEFIELD_H
#define NFC_CARDGAME_DEBUG_EVENTS_H
#define NFC_CARDGAME_ASSAULT_SLOTS_H
#define NFC_CARDGAME_PATHFINDING_H
#define NFC_CARDGAME_COMBAT_H
#define NFC_CARDGAME_FARMER_H
#define NFC_CARDGAME_PROGRESSION_H
#define PROGRESSION_KING_BURST_RADIUS 160.0f

/* ---- Local steering constants (must mirror src/core/config.h) ---- */
#define LANE_WAYPOINT_COUNT              8
#define PATHFIND_AGGRO_RADIUS             192.0f
#define PATHFIND_AGGRO_HYSTERESIS         32.0f
#define PATHFIND_CANDIDATE_ANGLE_SOFT_DEG 30.0f
#define PATHFIND_CANDIDATE_ANGLE_HARD_DEG 60.0f
#define PATHFIND_CONTACT_GAP              2.0f
#define PATHFIND_WAYPOINT_REACH_GAP       4.0f
#define PATHFIND_LANE_LOOKAHEAD_DISTANCE  48.0f
#define PATHFIND_PURSUIT_REAR_TOLERANCE   32.0f
#define PATHFIND_LIVELOCK_TICKS           15

/* ---- Minimal type stubs ---- */
typedef struct { float x; float y; } Vector2;
typedef struct { float x; float y; float width; float height; } Rectangle;

typedef enum { ENTITY_TROOP, ENTITY_BUILDING, ENTITY_PROJECTILE } EntityType;
typedef enum { FACTION_PLAYER1, FACTION_PLAYER2 } Faction;
typedef enum { ESTATE_IDLE, ESTATE_WALKING, ESTATE_ATTACKING, ESTATE_DEAD } EntityState;
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
typedef enum {
    ANIM_IDLE,
    ANIM_RUN,
    ANIM_WALK,
    ANIM_HURT,
    ANIM_DEATH,
    ANIM_ATTACK,
    ANIM_COUNT
} AnimationType;
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
    ANIM_PLAY_LOOP,
    ANIM_PLAY_ONCE
} AnimPlayMode;

typedef struct {
    AnimationType anim;
    SpriteDirection dir;
    float elapsed;
    float cycleDuration;
    float normalizedTime;
    bool oneShot;
    bool finished;
    bool flipH;
    int visualLoops;
} AnimState;

typedef struct {
    float prevNormalized;
    float currNormalized;
    bool finishedThisTick;
    bool loopedThisTick;
} AnimPlaybackEvent;

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

typedef struct { int dummy; } CharacterSprite;

/* ---- Deposit slot type stubs (mirrors src/core/types.h) ---- */
#define BASE_DEPOSIT_PRIMARY_SLOT_COUNT 4
#define BASE_DEPOSIT_QUEUE_SLOT_COUNT   6

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

typedef struct Entity Entity;
typedef struct GameState GameState;
typedef struct Player Player;

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
    TargetingMode targeting;
    const char *targetType;
    AnimState anim;
    const CharacterSprite *sprite;
    SpriteType spriteType;
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

typedef struct Battlefield {
    Entity *entities[16];
    int entityCount;
} Battlefield;

struct Player {
    Entity *base;
};

/* Phase 3a: entities.c reads &gs->nav when calling pathfind_step_entity.
 * Stub NavFrame as a complete (but empty-ish) struct so a by-value member
 * works in GameState and &gs->nav yields a NavFrame*. The stub below never
 * dereferences the pointer. */
typedef struct NavFrame { int _phase3a_placeholder; } NavFrame;
struct GameState {
    Battlefield battlefield;
    Player players[2];
    NavFrame nav;
};

/* ---- Test globals ---- */
static Entity *g_findTargetResult = NULL;
static Entity *g_findWithinRadiusResult = NULL;
static int g_applyHitCalls = 0;
static Entity *g_lastApplyHitAttacker = NULL;
static Entity *g_lastApplyHitTarget = NULL;
static int g_applyKingBurstCalls = 0;
static Entity *g_lastKingBurstBase = NULL;
static float g_lastKingBurstRadius = 0.0f;
static int g_lastKingBurstDamage = 0;

/* ---- Stubs required by entities.c ---- */
enum { DEBUG_EVT_STATE_CHANGE = 0, DEBUG_EVT_HIT = 1, DEBUG_EVT_DEATH_FINISH = 2 };

void debug_event_emit_xy(float x, float y, int type) {
    (void)x;
    (void)y;
    (void)type;
}

void farmer_update(Entity *e, GameState *gs, float deltaTime) {
    (void)e;
    (void)gs;
    (void)deltaTime;
}

static float distance_between(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return sqrtf(dx * dx + dy * dy);
}

bool combat_in_range(const Entity *a, const Entity *b, const GameState *gs) {
    (void)gs;
    if (!a || !b) return false;
    return distance_between(a->position, b->position) <= a->attackRange;
}

Entity *combat_find_target(Entity *attacker, GameState *gs) {
    (void)attacker;
    (void)gs;
    return g_findTargetResult;
}

Entity *combat_find_target_within_radius(Entity *attacker, GameState *gs, float maxRadius) {
    (void)gs;
    if (!attacker || !g_findWithinRadiusResult) return NULL;
    if (g_findWithinRadiusResult->ownerID == attacker->ownerID) return NULL;
    if (!g_findWithinRadiusResult->alive || g_findWithinRadiusResult->markedForRemoval) return NULL;
    if (distance_between(attacker->position, g_findWithinRadiusResult->position) > maxRadius) {
        return NULL;
    }
    return g_findWithinRadiusResult;
}

bool combat_can_heal_target(const Entity *attacker, const Entity *target) {
    if (!attacker || !target) return false;
    if (attacker->healAmount <= 0) return false;
    if (target == attacker) return false;
    if (target->ownerID != attacker->ownerID) return false;
    if (target->type != ENTITY_TROOP) return false;
    if (!target->alive || target->markedForRemoval) return false;
    if (target->hp >= target->maxHP) return false;
    return true;
}

void combat_apply_hit(Entity *attacker, Entity *target, GameState *gs) {
    (void)gs;
    g_applyHitCalls++;
    g_lastApplyHitAttacker = attacker;
    g_lastApplyHitTarget = target;
}

void combat_apply_king_burst(Entity *base, float radius, int damage, GameState *gs) {
    (void)gs;
    g_applyKingBurstCalls++;
    g_lastKingBurstBase = base;
    g_lastKingBurstRadius = radius;
    g_lastKingBurstDamage = damage;
}

/* Phase 3a: signature gained a NavFrame* parameter. The test harness does
 * not exercise nav, so the stub just ignores it. Also corrects a stale
 * lie-stub that used to declare void where the real signature returns bool. */
bool pathfind_step_entity(Entity *e, NavFrame *nav, const Battlefield *bf, float deltaTime) {
    (void)e;
    (void)nav;
    (void)bf;
    (void)deltaTime;
    return false;
}

void pathfind_commit_presentation(Entity *e, const Battlefield *bf) {
    (void)e;
    (void)bf;
}

float pathfind_lane_progress_for_position(const Entity *e, const Battlefield *bf,
                                          Vector2 position) {
    (void)bf;
    if (!e) return 0.0f;
    return (e->ownerID == 0) ? (2000.0f - position.y) : position.y;
}

void pathfind_sync_lane_progress(Entity *e, const Battlefield *bf) {
    (void)bf;
    if (!e) return;
    float projected = pathfind_lane_progress_for_position(e, bf, e->position);
    if (projected > e->laneProgress) {
        e->laneProgress = projected;
    }
}

void pathfind_apply_direction_for_side(AnimState *anim, Vector2 diff, BattleSide side) {
    (void)side;
    if (fabsf(diff.x) >= fabsf(diff.y)) {
        anim->dir = DIR_SIDE;
    } else {
        anim->dir = (diff.y >= 0.0f) ? DIR_DOWN : DIR_UP;
    }
}

float pathfind_sprite_rotation_for_side(SpriteDirection dir, BattleSide side) {
    (void)dir;
    (void)side;
    return 0.0f;
}

void pathfind_update_walk_facing(Entity *e, const Battlefield *bf) {
    (void)e;
    (void)bf;
}

Entity *bf_find_entity(const Battlefield *bf, int id) {
    if (!bf) return NULL;
    for (int i = 0; i < bf->entityCount; i++) {
        Entity *candidate = bf->entities[i];
        if (candidate && candidate->id == id) return candidate;
    }
    return NULL;
}

const EntityAnimSpec *anim_spec_get(SpriteType spriteType, AnimationType animType) {
    (void)spriteType;
    static const EntityAnimSpec s_idle = { ANIM_IDLE, ANIM_PLAY_LOOP, 0.5f, -1.0f, false, false, 1 };
    static const EntityAnimSpec s_walk = { ANIM_WALK, ANIM_PLAY_LOOP, 0.8f, -1.0f, false, false, 1 };
    static const EntityAnimSpec s_attack = { ANIM_ATTACK, ANIM_PLAY_ONCE, 1.0f, 0.5f, true, false, 1 };
    static const EntityAnimSpec s_death = { ANIM_DEATH, ANIM_PLAY_ONCE, 0.75f, -1.0f, false, true, 1 };

    switch (animType) {
        case ANIM_ATTACK: return &s_attack;
        case ANIM_WALK: return &s_walk;
        case ANIM_DEATH: return &s_death;
        case ANIM_IDLE:
        case ANIM_RUN:
        case ANIM_HURT:
        default:
            return &s_idle;
    }
}

float anim_walk_cycle_seconds(float moveSpeed, float pixelsPerCycle) {
    (void)moveSpeed;
    (void)pixelsPerCycle;
    return 1.0f;
}

float anim_attack_cycle_seconds(float attackSpeed) {
    if (attackSpeed <= 0.0f) return 1.0f;
    return 1.0f / attackSpeed;
}

void anim_state_init_with_loops(AnimState *state, AnimationType anim, SpriteDirection dir,
                                float cycleDuration, bool oneShot, int visualLoops);

void anim_state_init(AnimState *state, AnimationType anim, SpriteDirection dir,
                     float cycleDuration, bool oneShot) {
    anim_state_init_with_loops(state, anim, dir, cycleDuration, oneShot, 1);
}

void anim_state_init_with_loops(AnimState *state, AnimationType anim, SpriteDirection dir,
                                float cycleDuration, bool oneShot, int visualLoops) {
    state->anim = anim;
    state->dir = dir;
    state->elapsed = 0.0f;
    state->cycleDuration = cycleDuration;
    state->normalizedTime = 0.0f;
    state->oneShot = oneShot;
    state->finished = false;
    state->flipH = false;
    state->visualLoops = (visualLoops > 0) ? visualLoops : 1;
}

AnimPlaybackEvent anim_state_update(AnimState *state, float dt) {
    AnimPlaybackEvent evt = {0};
    if (!state) return evt;

    evt.prevNormalized = state->normalizedTime;
    if (state->finished) {
        evt.currNormalized = state->normalizedTime;
        return evt;
    }

    if (state->cycleDuration <= 0.0f) {
        if (state->oneShot) {
            state->finished = true;
            state->normalizedTime = 1.0f;
            evt.finishedThisTick = true;
        }
        evt.currNormalized = state->normalizedTime;
        return evt;
    }

    state->elapsed += dt;
    if (state->oneShot) {
        if (state->elapsed >= state->cycleDuration) {
            state->elapsed = state->cycleDuration;
            state->normalizedTime = 1.0f;
            state->finished = true;
            evt.finishedThisTick = true;
        } else {
            state->normalizedTime = state->elapsed / state->cycleDuration;
        }
    } else {
        while (state->elapsed >= state->cycleDuration) {
            state->elapsed -= state->cycleDuration;
            evt.loopedThisTick = true;
        }
        state->normalizedTime = state->elapsed / state->cycleDuration;
    }

    evt.currNormalized = state->normalizedTime;
    return evt;
}

void sprite_draw(const CharacterSprite *cs, const AnimState *state,
                 Vector2 pos, float scale, float rotationDegrees) {
    (void)cs;
    (void)state;
    (void)pos;
    (void)scale;
    (void)rotationDegrees;
}

void entity_set_state(Entity *e, EntityState newState);
void entity_restart_clip(Entity *e);

/* ---- Include production code under test ---- */
#include "../src/entities/entities.c"

/* ---- Test helpers ---- */
static int testsRun = 0;
static int testsPassed = 0;

#define RUN_TEST(fn) do { \
    printf("  "); \
    fn(); \
    testsRun++; \
    testsPassed++; \
    printf("PASS: %s\n", #fn); \
} while (0)

static void reset_globals(void) {
    g_findTargetResult = NULL;
    g_findWithinRadiusResult = NULL;
    g_applyHitCalls = 0;
    g_lastApplyHitAttacker = NULL;
    g_lastApplyHitTarget = NULL;
    g_applyKingBurstCalls = 0;
    g_lastKingBurstBase = NULL;
    g_lastKingBurstRadius = 0.0f;
    g_lastKingBurstDamage = 0;
}

static void battlefield_add(Battlefield *bf, Entity *entity) {
    bf->entities[bf->entityCount++] = entity;
}

static GameState make_game_state(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    return gs;
}

static Entity make_entity(int id, int ownerID, EntityType type, Vector2 pos) {
    Entity e;
    memset(&e, 0, sizeof(e));
    e.id = id;
    e.type = type;
    e.faction = (ownerID == 0) ? FACTION_PLAYER1 : FACTION_PLAYER2;
    e.state = ESTATE_WALKING;
    e.position = pos;
    e.moveSpeed = 60.0f;
    e.hp = 100;
    e.maxHP = 100;
    e.attack = 5;
    e.attackSpeed = 1.0f;
    e.attackRange = 80.0f;
    e.attackTargetId = -1;
    e.targeting = TARGET_NEAREST;
    e.presentationSide = SIDE_BOTTOM;
    e.ownerID = ownerID;
    e.lane = 1;
    e.waypointIndex = 1;
    e.unitRole = UNIT_ROLE_COMBAT;
    e.alive = true;
    e.markedForRemoval = false;
    e.spriteType = SPRITE_TYPE_KNIGHT;
    anim_state_init(&e.anim, ANIM_WALK, DIR_SIDE, 1.0f, false);
    return e;
}

static Entity make_healer(int id, Vector2 pos) {
    Entity healer = make_entity(id, 0, ENTITY_TROOP, pos);
    healer.state = ESTATE_ATTACKING;
    healer.healAmount = 8;
    healer.spriteType = SPRITE_TYPE_HEALER;
    anim_state_init(&healer.anim, ANIM_ATTACK, DIR_SIDE, 1.0f, true);
    healer.anim.elapsed = 0.49f;
    healer.anim.normalizedTime = 0.49f;
    return healer;
}

static Entity make_base_building(int id, int ownerID, Vector2 pos) {
    Entity base = make_entity(id, ownerID, ENTITY_BUILDING, pos);
    base.spriteType = SPRITE_TYPE_BASE;
    base.state = ESTATE_IDLE;
    anim_state_init(&base.anim, ANIM_IDLE, DIR_SIDE, 0.5f, false);
    return base;
}

static void test_healer_cancels_stale_heal_and_retargets_enemy(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity healer = make_healer(1, (Vector2){0.0f, 0.0f});
    Entity ally = make_entity(2, 0, ENTITY_TROOP, (Vector2){10.0f, 0.0f});
    Entity enemy = make_entity(3, 1, ENTITY_TROOP, (Vector2){-10.0f, 0.0f});
    healer.attackTargetId = ally.id;

    ally.hp = ally.maxHP; /* stale lock: no longer injured */

    battlefield_add(&gs.battlefield, &ally);
    battlefield_add(&gs.battlefield, &enemy);
    g_findTargetResult = &enemy;

    entity_update(&healer, &gs, 0.05f);

    assert(healer.state == ESTATE_ATTACKING);
    assert(healer.attackTargetId == enemy.id);
    assert(g_applyHitCalls == 0);
    assert(healer.anim.normalizedTime < 0.10f);
}

static void test_healer_cancels_stale_heal_and_walks_without_replacement(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity healer = make_healer(1, (Vector2){0.0f, 0.0f});
    Entity ally = make_entity(2, 0, ENTITY_TROOP, (Vector2){10.0f, 0.0f});
    healer.attackTargetId = ally.id;
    ally.hp = ally.maxHP;

    battlefield_add(&gs.battlefield, &ally);

    entity_update(&healer, &gs, 0.05f);

    assert(healer.state == ESTATE_WALKING);
    assert(healer.attackTargetId == -1);
    assert(g_applyHitCalls == 0);
}

/* ---- Phase 2: walking aggro probe + movementTargetId lifecycle ---- */

static void test_walking_unit_acquires_movement_target_in_aggro_radius(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity unit = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    /* enemy at distance 100 -- inside aggro (192) but outside attack (80) */
    Entity enemy = make_entity(2, 1, ENTITY_TROOP, (Vector2){100.0f, 0.0f});
    unit.state = ESTATE_WALKING;
    unit.movementTargetId = -1;

    battlefield_add(&gs.battlefield, &enemy);
    g_findWithinRadiusResult = &enemy;

    entity_update(&unit, &gs, 0.016f);

    assert(unit.movementTargetId == enemy.id);
    assert(unit.state == ESTATE_WALKING);
    assert(unit.attackTargetId == -1);
}

static void test_idle_unit_acquires_enemy_pursuit_in_aggro_radius(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity unit = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity enemy = make_entity(2, 1, ENTITY_TROOP, (Vector2){100.0f, 0.0f});
    unit.state = ESTATE_IDLE;
    unit.waypointIndex = 8;
    unit.movementTargetId = -1;

    battlefield_add(&gs.battlefield, &enemy);
    g_findTargetResult = NULL;
    g_findWithinRadiusResult = &enemy;

    entity_update(&unit, &gs, 0.016f);

    assert(unit.state == ESTATE_WALKING);
    assert(unit.movementTargetId == enemy.id);
    assert(unit.attackTargetId == -1);
}

static void test_walking_unit_keeps_target_inside_hysteresis(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity unit = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    /* Enemy at distance 200 -- outside aggro (192) but inside hysteresis (224). */
    Entity enemy = make_entity(2, 1, ENTITY_TROOP, (Vector2){200.0f, 0.0f});
    unit.state = ESTATE_WALKING;
    unit.movementTargetId = enemy.id;

    battlefield_add(&gs.battlefield, &enemy);
    /* Probe stub returns NULL; hysteresis-keep should skip the re-probe. */
    g_findWithinRadiusResult = NULL;

    entity_update(&unit, &gs, 0.016f);

    assert(unit.movementTargetId == enemy.id);
    assert(unit.state == ESTATE_WALKING);
}

static void test_idle_healer_does_not_chase_injured_ally_out_of_range(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity healer = make_healer(1, (Vector2){0.0f, 0.0f});
    Entity ally = make_entity(2, 0, ENTITY_TROOP, (Vector2){100.0f, 0.0f});
    healer.state = ESTATE_IDLE;
    healer.attackTargetId = -1;
    anim_state_init(&healer.anim, ANIM_IDLE, DIR_SIDE, 1.0f, false);
    ally.hp = 50;

    battlefield_add(&gs.battlefield, &ally);
    g_findTargetResult = &ally;
    g_findWithinRadiusResult = &ally;

    entity_update(&healer, &gs, 0.016f);

    assert(healer.state == ESTATE_IDLE);
    assert(healer.attackTargetId == -1);
    assert(healer.movementTargetId == -1);
}

static void test_walking_healer_does_not_chase_injured_ally_out_of_range(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity healer = make_healer(1, (Vector2){0.0f, 0.0f});
    Entity ally = make_entity(2, 0, ENTITY_TROOP, (Vector2){100.0f, 0.0f});
    healer.state = ESTATE_WALKING;
    healer.attackTargetId = -1;
    healer.movementTargetId = -1;
    anim_state_init(&healer.anim, ANIM_WALK, DIR_SIDE, 1.0f, false);
    ally.hp = 50;

    battlefield_add(&gs.battlefield, &ally);
    g_findTargetResult = &ally;
    g_findWithinRadiusResult = &ally;

    entity_update(&healer, &gs, 0.016f);

    assert(healer.state == ESTATE_WALKING);
    assert(healer.attackTargetId == -1);
    assert(healer.movementTargetId == -1);
}

static void test_walking_unit_releases_target_beyond_hysteresis(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity unit = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    /* Enemy at distance 250 -- beyond hysteresis (224). */
    Entity enemy = make_entity(2, 1, ENTITY_TROOP, (Vector2){250.0f, 0.0f});
    unit.state = ESTATE_WALKING;
    unit.movementTargetId = enemy.id;

    battlefield_add(&gs.battlefield, &enemy);
    g_findWithinRadiusResult = NULL;  /* re-probe returns nothing */

    entity_update(&unit, &gs, 0.016f);

    assert(unit.movementTargetId == -1);
    assert(unit.state == ESTATE_WALKING);
}

static void test_walking_unit_clears_target_when_target_dies(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity unit = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity enemy = make_entity(2, 1, ENTITY_TROOP, (Vector2){50.0f, 0.0f});
    Entity base = make_entity(3, 1, ENTITY_BUILDING, (Vector2){540.0f, 64.0f});
    enemy.alive = false;  /* dead mid-pursuit */
    unit.state = ESTATE_WALKING;
    unit.movementTargetId = enemy.id;
    gs.players[1].base = &base;

    battlefield_add(&gs.battlefield, &enemy);
    battlefield_add(&gs.battlefield, &base);
    g_findWithinRadiusResult = NULL;

    entity_update(&unit, &gs, 0.016f);

    assert(unit.movementTargetId == -1);
}

static void test_idle_lane_end_unit_resumes_enemy_base_assault(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity unit = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity base = make_entity(3, 1, ENTITY_BUILDING, (Vector2){540.0f, 64.0f});
    unit.state = ESTATE_IDLE;
    unit.waypointIndex = LANE_WAYPOINT_COUNT;
    unit.movementTargetId = -1;
    gs.players[1].base = &base;

    battlefield_add(&gs.battlefield, &base);
    g_findTargetResult = NULL;
    g_findWithinRadiusResult = NULL;

    entity_update(&unit, &gs, 0.016f);

    assert(unit.state == ESTATE_WALKING);
    assert(unit.movementTargetId == base.id);
}

static void test_attack_fallback_uses_enemy_base_when_lane_end_target_dies(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity enemy = make_entity(2, 1, ENTITY_TROOP, (Vector2){50.0f, 0.0f});
    Entity base = make_entity(3, 1, ENTITY_BUILDING, (Vector2){540.0f, 64.0f});
    enemy.alive = false;
    attacker.state = ESTATE_ATTACKING;
    attacker.attackTargetId = enemy.id;
    attacker.waypointIndex = LANE_WAYPOINT_COUNT;
    anim_state_init(&attacker.anim, ANIM_ATTACK, DIR_SIDE, 1.0f, true);
    gs.players[1].base = &base;

    battlefield_add(&gs.battlefield, &enemy);
    battlefield_add(&gs.battlefield, &base);
    g_findTargetResult = NULL;
    g_findWithinRadiusResult = NULL;

    entity_update(&attacker, &gs, 0.016f);

    assert(attacker.state == ESTATE_WALKING);
    assert(attacker.attackTargetId == -1);
    assert(attacker.movementTargetId == base.id);
}

static void test_walking_unit_transitions_to_attacking_before_moving(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity unit = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    /* Enemy at distance 50 -- already in attackRange (80). */
    Entity enemy = make_entity(2, 1, ENTITY_TROOP, (Vector2){50.0f, 0.0f});
    unit.state = ESTATE_WALKING;
    unit.movementTargetId = -1;

    battlefield_add(&gs.battlefield, &enemy);
    g_findWithinRadiusResult = &enemy;

    entity_update(&unit, &gs, 0.016f);

    assert(unit.state == ESTATE_ATTACKING);
    assert(unit.attackTargetId == enemy.id);
    assert(unit.movementTargetId == enemy.id);
}

static void test_walking_unit_does_not_chase_enemy_behind_lane_progress(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity unit = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 1000.0f});
    Entity enemy = make_entity(2, 1, ENTITY_TROOP, (Vector2){0.0f, 1100.0f});
    unit.state = ESTATE_WALKING;
    unit.movementTargetId = -1;

    battlefield_add(&gs.battlefield, &enemy);
    g_findWithinRadiusResult = &enemy;

    entity_update(&unit, &gs, 0.016f);

    assert(unit.state == ESTATE_WALKING);
    assert(unit.attackTargetId == -1);
    assert(unit.movementTargetId == -1);
}

static void test_walking_unit_prefers_forward_enemy_over_behind_enemy_for_pursuit(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity unit = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 1000.0f});
    Entity behind = make_entity(2, 1, ENTITY_TROOP, (Vector2){0.0f, 1100.0f});
    Entity ahead = make_entity(3, 1, ENTITY_TROOP, (Vector2){0.0f, 860.0f});
    unit.state = ESTATE_WALKING;
    unit.movementTargetId = -1;

    battlefield_add(&gs.battlefield, &behind);
    battlefield_add(&gs.battlefield, &ahead);
    g_findWithinRadiusResult = &behind;

    entity_update(&unit, &gs, 0.016f);

    assert(unit.state == ESTATE_WALKING);
    assert(unit.movementTargetId == ahead.id);
    assert(unit.attackTargetId == -1);
}

static void test_walking_unit_still_attacks_enemy_behind_when_in_range(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity unit = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 1000.0f});
    Entity enemy = make_entity(2, 1, ENTITY_TROOP, (Vector2){0.0f, 1050.0f});
    unit.state = ESTATE_WALKING;
    unit.movementTargetId = -1;

    battlefield_add(&gs.battlefield, &enemy);
    g_findTargetResult = &enemy;
    g_findWithinRadiusResult = &enemy;

    entity_update(&unit, &gs, 0.016f);

    assert(unit.state == ESTATE_ATTACKING);
    assert(unit.attackTargetId == enemy.id);
    assert(unit.movementTargetId == -1);
}

static void test_attack_fallback_immediately_pursues_nearby_enemy(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    /* Enemy just moved out of attackRange (80) but well inside aggro (192). */
    Entity enemy = make_entity(2, 1, ENTITY_TROOP, (Vector2){100.0f, 0.0f});
    attacker.state = ESTATE_ATTACKING;
    attacker.attackTargetId = enemy.id;
    anim_state_init(&attacker.anim, ANIM_ATTACK, DIR_SIDE, 1.0f, true);

    battlefield_add(&gs.battlefield, &enemy);
    g_findTargetResult = NULL;

    entity_update(&attacker, &gs, 0.016f);

    assert(attacker.state == ESTATE_WALKING);
    assert(attacker.attackTargetId == -1);
    assert(attacker.movementTargetId == enemy.id);  /* immediate pursuit */
}

static void test_attack_fallback_does_not_pursue_enemy_behind_lane_progress(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 1000.0f});
    Entity enemy = make_entity(2, 1, ENTITY_TROOP, (Vector2){0.0f, 1100.0f});
    attacker.state = ESTATE_ATTACKING;
    attacker.attackTargetId = enemy.id;
    anim_state_init(&attacker.anim, ANIM_ATTACK, DIR_SIDE, 1.0f, true);

    battlefield_add(&gs.battlefield, &enemy);
    g_findTargetResult = NULL;

    entity_update(&attacker, &gs, 0.016f);

    assert(attacker.state == ESTATE_WALKING);
    assert(attacker.attackTargetId == -1);
    assert(attacker.movementTargetId == -1);
}

static void test_attack_fallback_keeps_pursuit_inside_hysteresis(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity enemy = make_entity(2, 1, ENTITY_TROOP, (Vector2){200.0f, 0.0f});
    attacker.state = ESTATE_ATTACKING;
    attacker.attackTargetId = enemy.id;
    anim_state_init(&attacker.anim, ANIM_ATTACK, DIR_SIDE, 1.0f, true);

    battlefield_add(&gs.battlefield, &enemy);
    g_findTargetResult = NULL;

    entity_update(&attacker, &gs, 0.016f);

    assert(attacker.state == ESTATE_WALKING);
    assert(attacker.attackTargetId == -1);
    assert(attacker.movementTargetId == enemy.id);
}

static void test_attack_fallback_clears_pursuit_beyond_hysteresis(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity enemy = make_entity(2, 1, ENTITY_TROOP, (Vector2){230.0f, 0.0f});
    attacker.state = ESTATE_ATTACKING;
    attacker.attackTargetId = enemy.id;
    anim_state_init(&attacker.anim, ANIM_ATTACK, DIR_SIDE, 1.0f, true);

    battlefield_add(&gs.battlefield, &enemy);
    g_findTargetResult = NULL;

    entity_update(&attacker, &gs, 0.016f);

    assert(attacker.state == ESTATE_WALKING);
    assert(attacker.attackTargetId == -1);
    assert(attacker.movementTargetId == -1);
}

static void test_attack_fallback_clears_pursuit_when_enemy_far(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    /* Enemy teleported beyond aggro radius entirely. */
    Entity enemy = make_entity(2, 1, ENTITY_TROOP, (Vector2){500.0f, 0.0f});
    attacker.state = ESTATE_ATTACKING;
    attacker.attackTargetId = enemy.id;
    anim_state_init(&attacker.anim, ANIM_ATTACK, DIR_SIDE, 1.0f, true);

    battlefield_add(&gs.battlefield, &enemy);
    g_findTargetResult = NULL;

    entity_update(&attacker, &gs, 0.016f);

    assert(attacker.state == ESTATE_WALKING);
    assert(attacker.attackTargetId == -1);
    assert(attacker.movementTargetId == -1);
}

static void test_non_healer_enemy_hit_flow_unchanged(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity attacker = make_entity(1, 0, ENTITY_TROOP, (Vector2){0.0f, 0.0f});
    Entity enemy = make_entity(2, 1, ENTITY_TROOP, (Vector2){10.0f, 0.0f});
    attacker.state = ESTATE_ATTACKING;
    attacker.attackTargetId = enemy.id;
    anim_state_init(&attacker.anim, ANIM_ATTACK, DIR_SIDE, 1.0f, true);
    attacker.anim.elapsed = 0.49f;
    attacker.anim.normalizedTime = 0.49f;

    battlefield_add(&gs.battlefield, &enemy);

    entity_update(&attacker, &gs, 0.05f);

    assert(attacker.state == ESTATE_ATTACKING);
    assert(attacker.attackTargetId == enemy.id);
    assert(g_applyHitCalls == 1);
    assert(g_lastApplyHitAttacker == &attacker);
    assert(g_lastApplyHitTarget == &enemy);
}

static void test_building_attack_clip_finishes_and_returns_to_idle(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity base = make_base_building(1, 0, (Vector2){540.0f, 1800.0f});
    entity_set_state(&base, ESTATE_ATTACKING);
    base.attackTargetId = -1;

    entity_update(&base, &gs, 1.10f);

    assert(base.state == ESTATE_IDLE);
    assert(base.attackTargetId == -1);
    assert(!base.markedForRemoval);
    assert(base.anim.anim == ANIM_IDLE);
    assert(!base.anim.oneShot);
    assert(g_applyHitCalls == 0);
}

static void test_building_attack_hit_marker_dispatches_queued_king_burst_once(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity base = make_base_building(1, 0, (Vector2){540.0f, 1800.0f});
    entity_set_state(&base, ESTATE_ATTACKING);
    base.basePendingKingBurst = true;
    base.basePendingKingBurstDamage = 43;
    base.anim.elapsed = 0.49f;
    base.anim.normalizedTime = 0.49f;

    entity_update(&base, &gs, 0.05f);

    assert(base.state == ESTATE_ATTACKING);
    assert(g_applyKingBurstCalls == 1);
    assert(g_lastKingBurstBase == &base);
    assert(fabsf(g_lastKingBurstRadius - PROGRESSION_KING_BURST_RADIUS) < 0.001f);
    assert(g_lastKingBurstDamage == 43);
    assert(base.basePendingKingBurst == false);
    assert(base.basePendingKingBurstDamage == 0);
}

static void test_building_attack_finish_clears_pending_king_burst_without_dispatch(void) {
    reset_globals();
    GameState gs = make_game_state();

    Entity base = make_base_building(1, 0, (Vector2){540.0f, 1800.0f});
    entity_set_state(&base, ESTATE_ATTACKING);
    base.basePendingKingBurst = true;
    base.basePendingKingBurstDamage = 52;
    base.anim.elapsed = 0.95f;
    base.anim.normalizedTime = 0.95f;

    entity_update(&base, &gs, 0.10f);

    assert(g_applyKingBurstCalls == 0);
    assert(base.basePendingKingBurst == false);
    assert(base.basePendingKingBurstDamage == 0);
    assert(base.state == ESTATE_IDLE);
    assert(base.anim.anim == ANIM_IDLE);
    assert(base.anim.oneShot == false);
}

int main(void) {
    printf("Running entities tests...\n");

    RUN_TEST(test_healer_cancels_stale_heal_and_retargets_enemy);
    RUN_TEST(test_healer_cancels_stale_heal_and_walks_without_replacement);
    RUN_TEST(test_non_healer_enemy_hit_flow_unchanged);
    RUN_TEST(test_building_attack_clip_finishes_and_returns_to_idle);
    RUN_TEST(test_building_attack_hit_marker_dispatches_queued_king_burst_once);
    RUN_TEST(test_building_attack_finish_clears_pending_king_burst_without_dispatch);

    RUN_TEST(test_walking_unit_acquires_movement_target_in_aggro_radius);
    RUN_TEST(test_idle_unit_acquires_enemy_pursuit_in_aggro_radius);
    RUN_TEST(test_walking_unit_keeps_target_inside_hysteresis);
    RUN_TEST(test_idle_healer_does_not_chase_injured_ally_out_of_range);
    RUN_TEST(test_walking_healer_does_not_chase_injured_ally_out_of_range);
    RUN_TEST(test_walking_unit_releases_target_beyond_hysteresis);
    RUN_TEST(test_walking_unit_clears_target_when_target_dies);
    RUN_TEST(test_idle_lane_end_unit_resumes_enemy_base_assault);
    RUN_TEST(test_walking_unit_transitions_to_attacking_before_moving);
    RUN_TEST(test_walking_unit_does_not_chase_enemy_behind_lane_progress);
    RUN_TEST(test_walking_unit_prefers_forward_enemy_over_behind_enemy_for_pursuit);
    RUN_TEST(test_walking_unit_still_attacks_enemy_behind_when_in_range);
    RUN_TEST(test_attack_fallback_immediately_pursues_nearby_enemy);
    RUN_TEST(test_attack_fallback_does_not_pursue_enemy_behind_lane_progress);
    RUN_TEST(test_attack_fallback_uses_enemy_base_when_lane_end_target_dies);
    RUN_TEST(test_attack_fallback_keeps_pursuit_inside_hysteresis);
    RUN_TEST(test_attack_fallback_clears_pursuit_beyond_hysteresis);
    RUN_TEST(test_attack_fallback_clears_pursuit_when_enemy_far);

    printf("\nAll %d tests passed!\n", testsPassed);
    return 0;
}
