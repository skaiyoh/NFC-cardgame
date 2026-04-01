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
 */

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>

/* ---- Prevent combat.c's includes from pulling in heavy headers ---- */
#define NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_COMBAT_H
#define NFC_CARDGAME_ENTITIES_H

/* ---- Config defines (must match src/core/config.h) ---- */
#define LANE_WAYPOINT_COUNT  8
#define NUM_CARD_SLOTS 3
#define MAX_ENTITIES 64
#define TILE_COUNT 32
#define MAX_DETAIL_DEFS 64

/* ---- Minimal type stubs ---- */
typedef struct { float x; float y; } Vector2;
typedef struct { float x; float y; float width; float height; } Rectangle;

typedef enum { ANIM_IDLE, ANIM_RUN, ANIM_WALK, ANIM_HURT, ANIM_DEATH, ANIM_ATTACK, ANIM_COUNT } AnimationType;
typedef enum { DIR_SIDE, DIR_DOWN, DIR_UP, DIR_COUNT } SpriteDirection;
typedef enum { ESTATE_IDLE, ESTATE_WALKING, ESTATE_ATTACKING, ESTATE_DEAD } EntityState;
typedef enum { TARGET_NEAREST, TARGET_BUILDING, TARGET_SPECIFIC_TYPE } TargetingMode;
typedef enum { ENTITY_TROOP, ENTITY_BUILDING, ENTITY_PROJECTILE } EntityType;
typedef enum { FACTION_PLAYER1, FACTION_PLAYER2 } Faction;

typedef struct {
    AnimationType anim;
    SpriteDirection dir;
    int frame;
    float timer;
    float fps;
    bool flipH;
} AnimState;

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
    TargetingMode targeting;
    const char *targetType;
    AnimState anim;
    const CharacterSprite *sprite;
    float spriteScale;
    int ownerID;
    int lane;
    int waypointIndex;
    bool alive;
    bool markedForRemoval;
};

/* Player stub — padded to match types.h layout */
typedef struct { int width; int height; int tileSize; int *cells; int cellCount; void *texture; } TileMap_stub;
typedef struct { Rectangle src; } TileDef_stub;
typedef struct { float x; float y; float rotation; float zoom; } Camera2D_stub;

/* BiomeType/BiomeDef stubs — minimal to match field layout */
typedef int BiomeType;
typedef struct { int dummy; } BiomeDef_stub;

struct Player {
    int id;
    Rectangle playArea;
    Rectangle screenArea;
    Camera2D_stub camera;
    float cameraRotation;
    TileMap_stub tilemap;
    BiomeType biome;
    const BiomeDef_stub *biomeDef;
    TileDef_stub tileDefs[TILE_COUNT];
    int tileDefCount;
    TileDef_stub detailDefs[MAX_DETAIL_DEFS];
    int detailDefCount;
    CardSlot slots[NUM_CARD_SLOTS];
    Entity *entities[MAX_ENTITIES];
    int entityCount;
    float energy;
    float maxEnergy;
    float energyRegenRate;
    Entity *base;
    Vector2 laneWaypoints[3][LANE_WAYPOINT_COUNT];
};

/* GameState stub — only players needed for combat */
typedef int BiomeCount_stub;
#define BIOME_COUNT 2
typedef struct { int dummy; } SpriteAtlas_stub;
typedef struct { int dummy; } DB_stub;
typedef struct { int dummy; } Deck_stub;
typedef struct { int dummy; } CardAtlas_stub;
typedef struct { int fds[2]; } NFCReader_stub;

struct GameState {
    DB_stub db;
    Deck_stub deck;
    CardAtlas_stub cardAtlas;
    Player players[2];
    BiomeDef_stub biomeDefs[BIOME_COUNT];
    SpriteAtlas_stub spriteAtlas;
    int halfWidth;
    NFCReader_stub nfc;
};

/* ---- Stub functions required by combat.c ---- */
void anim_state_init(AnimState *state, AnimationType anim, SpriteDirection dir, float fps) {
    state->anim = anim;
    state->dir = dir;
    state->frame = 0;
    state->timer = 0.0f;
    state->fps = fps;
    state->flipH = false;
}

void entity_set_state(Entity *e, EntityState newState) {
    if (!e || e->state == newState) return;
    e->state = newState;
}

/* ---- Include combat.c directly ---- */
#include "../src/logic/combat.c"

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
    anim_state_init(&e.anim, ANIM_WALK, DIR_DOWN, 10.0f);
    return e;
}

static GameState make_game_state(void) {
    GameState gs = {0};
    // P1: x=0, w=1080, h=960
    gs.players[0].id = 0;
    gs.players[0].playArea = (Rectangle){0, 0, 1080, 960};
    gs.players[0].entityCount = 0;
    gs.players[0].base = NULL;

    // P2: x=960, w=1080, h=960
    gs.players[1].id = 1;
    gs.players[1].playArea = (Rectangle){960, 0, 1080, 960};
    gs.players[1].entityCount = 0;
    gs.players[1].base = NULL;

    gs.halfWidth = 960;
    return gs;
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

static void test_in_range_same_space(void) {
    GameState gs = make_game_state();
    // Two entities from same player — raw distance check
    Entity a = make_entity(0, ENTITY_TROOP, (Vector2){100, 100});
    Entity b = make_entity(0, ENTITY_TROOP, (Vector2){130, 100});
    a.attackRange = 50.0f;

    assert(combat_in_range(&a, &b, &gs) == true);  // dist=30 <= 50

    b.position = (Vector2){200, 100};
    assert(combat_in_range(&a, &b, &gs) == false);  // dist=100 > 50
}

static void test_in_range_cross_space(void) {
    GameState gs = make_game_state();
    // P1 entity at its front line (low Y), P2 entity at its front line (low Y)
    // These should be close together in mapped space
    Entity a = make_entity(0, ENTITY_TROOP, (Vector2){540, -100});  // crossed border
    a.attackRange = 100.0f;

    Entity b = make_entity(1, ENTITY_TROOP, (Vector2){1500, -100}); // crossed border

    // Map b into a's space: lateral = (1500-960)/1080 = 0.5, mirrored = 0.5
    // mapped.x = 0 + 0.5 * 1080 = 540, depth = 0 - (-100) = 100, mapped.y = 0 + 100 = 100
    // But a is at (540, -100). Hmm, let me reconsider.
    // Actually map_to_opponent_space maps FROM b's owner space TO a's owner space.
    // b is owned by P1, so owner=P1, opponent=P0
    // lateral = (1500 - 960)/1080 = 540/1080 = 0.5
    // mirroredLateral = 0.5
    // depth = 0 - (-100) = 100  (P1.playArea.y = 0)
    // mapped = (0 + 0.5*1080, 0 + 100) = (540, 100)
    // a is at (540, -100), dist = |100 - (-100)| = 200
    // That's > 100, so not in range... let me adjust the test

    a.position = (Vector2){540, 50};  // shallow crossing
    // Now dist to mapped (540, 100) = 50, within range
    assert(combat_in_range(&a, &b, &gs) == true);

    a.attackRange = 10.0f;
    assert(combat_in_range(&a, &b, &gs) == false);
}

static void test_in_range_null_safety(void) {
    GameState gs = make_game_state();
    Entity a = make_entity(0, ENTITY_TROOP, (Vector2){100, 100});
    assert(combat_in_range(NULL, &a, &gs) == false);
    assert(combat_in_range(&a, NULL, &gs) == false);
}

static void test_find_target_nearest(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 50});
    attacker.targeting = TARGET_NEAREST;

    // Place two enemy entities at different distances in P2's space
    Entity far = make_entity(1, ENTITY_TROOP, (Vector2){1500, -300});
    Entity near = make_entity(1, ENTITY_TROOP, (Vector2){1500, -50});

    gs.players[1].entities[0] = &far;
    gs.players[1].entities[1] = &near;
    gs.players[1].entityCount = 2;

    Entity *target = combat_find_target(&attacker, &gs);
    assert(target == &near);
}

static void test_find_target_skips_dead(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 50});

    Entity dead = make_entity(1, ENTITY_TROOP, (Vector2){1500, -50});
    dead.alive = false;

    Entity alive_e = make_entity(1, ENTITY_TROOP, (Vector2){1500, -200});

    gs.players[1].entities[0] = &dead;
    gs.players[1].entities[1] = &alive_e;
    gs.players[1].entityCount = 2;

    Entity *target = combat_find_target(&attacker, &gs);
    assert(target == &alive_e);
}

static void test_find_target_skips_marked(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 50});

    Entity marked = make_entity(1, ENTITY_TROOP, (Vector2){1500, -50});
    marked.markedForRemoval = true;

    Entity valid = make_entity(1, ENTITY_TROOP, (Vector2){1500, -200});

    gs.players[1].entities[0] = &marked;
    gs.players[1].entities[1] = &valid;
    gs.players[1].entityCount = 2;

    Entity *target = combat_find_target(&attacker, &gs);
    assert(target == &valid);
}

static void test_find_target_building_priority(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 50});
    attacker.targeting = TARGET_BUILDING;

    // Troop is closer, building is farther
    Entity troop = make_entity(1, ENTITY_TROOP, (Vector2){1500, -50});
    Entity building = make_entity(1, ENTITY_BUILDING, (Vector2){1500, -300});

    gs.players[1].entities[0] = &troop;
    gs.players[1].entities[1] = &building;
    gs.players[1].entityCount = 2;

    Entity *target = combat_find_target(&attacker, &gs);
    assert(target == &building);
}

static void test_find_target_returns_null_no_enemies(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 50});

    gs.players[1].entityCount = 0;
    gs.players[1].base = NULL;

    Entity *target = combat_find_target(&attacker, &gs);
    assert(target == NULL);
}

static void test_find_target_falls_back_to_base(void) {
    GameState gs = make_game_state();
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){540, 50});

    Entity base = make_entity(1, ENTITY_BUILDING, (Vector2){1500, 800});
    gs.players[1].entityCount = 0;
    gs.players[1].base = &base;

    Entity *target = combat_find_target(&attacker, &gs);
    assert(target == &base);
}

static void test_resolve_deals_damage(void) {
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});
    attacker.attack = 25;
    attacker.attackSpeed = 1.0f;
    attacker.attackCooldown = 0.0f;

    Entity target = make_entity(1, ENTITY_TROOP, (Vector2){0, 0});
    target.hp = 100;
    target.maxHP = 100;

    combat_resolve(&attacker, &target, 0.016f);

    assert(target.hp == 75);
    assert(attacker.attackCooldown > 0.0f);
}

static void test_resolve_respects_cooldown(void) {
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});
    attacker.attack = 25;
    attacker.attackSpeed = 1.0f;
    attacker.attackCooldown = 0.5f;  // still on cooldown

    Entity target = make_entity(1, ENTITY_TROOP, (Vector2){0, 0});
    target.hp = 100;

    combat_resolve(&attacker, &target, 0.016f);

    // Cooldown decremented but no damage dealt
    assert(target.hp == 100);
    assert(attacker.attackCooldown < 0.5f);
}

static void test_resolve_kills_at_zero_hp(void) {
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});
    attacker.attack = 150;  // overkill
    attacker.attackSpeed = 1.0f;
    attacker.attackCooldown = 0.0f;

    Entity target = make_entity(1, ENTITY_TROOP, (Vector2){0, 0});
    target.hp = 100;
    target.maxHP = 100;

    combat_resolve(&attacker, &target, 0.016f);

    assert(target.hp == 0);
    assert(target.alive == false);
    assert(target.state == ESTATE_DEAD);
}

static void test_resolve_skips_dead_target(void) {
    Entity attacker = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});
    attacker.attackCooldown = 0.0f;

    Entity target = make_entity(1, ENTITY_TROOP, (Vector2){0, 0});
    target.alive = false;
    target.hp = 0;

    combat_resolve(&attacker, &target, 0.016f);

    // Cooldown should not have been set (no attack happened)
    assert(attacker.attackCooldown == 0.0f);
}

static void test_take_damage_basic(void) {
    Entity e = make_entity(0, ENTITY_TROOP, (Vector2){0, 0});
    e.hp = 100;
    e.maxHP = 100;

    entity_take_damage(&e, 30);

    assert(e.hp == 70);
    assert(e.alive == true);
    assert(e.state == ESTATE_WALKING);  // unchanged
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

    assert(e.hp == 0);  // clamped, not negative
    assert(e.alive == false);
}

static void test_take_damage_null_safety(void) {
    // Should not crash
    entity_take_damage(NULL, 10);
}

static void test_cross_space_mapping_symmetry(void) {
    // Verify that mapping an entity to opponent space and back gives a
    // position that correctly reflects mirror + depth semantics
    GameState gs = make_game_state();

    // P1 center at depth 0 (on the border)
    Entity a = make_entity(0, ENTITY_TROOP, (Vector2){540, 0});
    a.attackRange = 10.0f;

    // P2 center at depth 0 (on the border)
    Entity b = make_entity(1, ENTITY_TROOP, (Vector2){1500, 0});

    // These should be at the same mapped position (both at their border centers)
    // Map b to a's space: lateral=(1500-960)/1080=0.5, mirrored=0.5
    // mapped = (0+0.5*1080, 0+0) = (540, 0) — same as a!
    // Distance should be 0
    assert(combat_in_range(&a, &b, &gs) == true);
}

/* ---- Main ---- */
int main(void) {
    printf("Running combat tests...\n");

    RUN_TEST(test_in_range_same_space);
    RUN_TEST(test_in_range_cross_space);
    RUN_TEST(test_in_range_null_safety);
    RUN_TEST(test_find_target_nearest);
    RUN_TEST(test_find_target_skips_dead);
    RUN_TEST(test_find_target_skips_marked);
    RUN_TEST(test_find_target_building_priority);
    RUN_TEST(test_find_target_returns_null_no_enemies);
    RUN_TEST(test_find_target_falls_back_to_base);
    RUN_TEST(test_resolve_deals_damage);
    RUN_TEST(test_resolve_respects_cooldown);
    RUN_TEST(test_resolve_kills_at_zero_hp);
    RUN_TEST(test_resolve_skips_dead_target);
    RUN_TEST(test_take_damage_basic);
    RUN_TEST(test_take_damage_kills);
    RUN_TEST(test_take_damage_clamps_zero);
    RUN_TEST(test_take_damage_null_safety);
    RUN_TEST(test_cross_space_mapping_symmetry);

    printf("\nAll %d tests passed!\n", tests_passed);
    return 0;
}
