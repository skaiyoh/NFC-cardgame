/*
 * Unit tests for src/logic/win_condition.c
 *
 * Self-contained: redefines minimal type stubs and includes win_condition.c
 * directly to avoid the heavy types.h include chain (Raylib, sqlite3, biome).
 *
 * SYNC REQUIREMENT: These struct stubs must match the field order and sizes
 * from src/core/types.h. If types.h changes, update these stubs.
 * Run `make test` after any types.h change to catch layout mismatches.
 */

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/* ---- Prevent win_condition.c's includes from pulling in heavy headers ---- */
#define NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_WIN_CONDTION_H

/* ---- Config defines (must match src/core/config.h) ---- */
#define NUM_CARD_SLOTS 3

/* ---- Minimal type stubs ---- */
typedef struct { float x; float y; } Vector2;
typedef struct { float x; float y; float width; float height; } Rectangle;

typedef enum { ANIM_IDLE, ANIM_RUN, ANIM_WALK, ANIM_HURT, ANIM_DEATH, ANIM_ATTACK, ANIM_COUNT } AnimationType;
typedef enum { DIR_SIDE, DIR_DOWN, DIR_UP, DIR_COUNT } SpriteDirection;
typedef enum { ESTATE_IDLE, ESTATE_WALKING, ESTATE_ATTACKING, ESTATE_DEAD } EntityState;
typedef enum { TARGET_NEAREST, TARGET_BUILDING, TARGET_SPECIFIC_TYPE } TargetingMode;
typedef enum { ENTITY_TROOP, ENTITY_BUILDING, ENTITY_PROJECTILE } EntityType;
typedef enum { FACTION_PLAYER1, FACTION_PLAYER2 } Faction;
typedef enum { UNIT_ROLE_COMBAT, UNIT_ROLE_FARMER } UnitRole;
typedef enum { FARMER_SEEKING, FARMER_WALKING_TO_SUSTENANCE, FARMER_GATHERING, FARMER_RETURNING, FARMER_DEPOSITING } FarmerState;

typedef struct {
    AnimationType anim;
    SpriteDirection dir;
    float elapsed;
    float cycleDuration;
    float normalizedTime;
    bool oneShot;
    bool finished;
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

typedef enum { SIDE_BOTTOM, SIDE_TOP } BattleSide;

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
    int spriteType;
    float spriteScale;
    float spriteRotationDegrees;
    int ownerID;
    int lane;
    int waypointIndex;
    float hitFlashTimer;
    UnitRole unitRole;
    FarmerState farmerState;
    int claimedSustenanceNodeId;
    int carriedSustenanceValue;
    float workTimer;
    bool alive;
    bool markedForRemoval;
};

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
    Entity *base;
    int sustenanceCollected;
};

/* Minimal GameState stub -- only players needed for win condition */
struct GameState {
    char _pad_db[8];
    char _pad_deck[8];
    char _pad_cardAtlas[8];
    Player players[2];
    char _pad_rest[4096]; /* covers biomeDefs, battlefield, spriteAtlas, halfWidth, p2RT, nfc */
    bool gameOver;
    int winnerID;
};

/* ---- Include win_condition.c directly ---- */
#include "../src/logic/win_condition.c"

/* ---- Test helpers ---- */
static Entity make_base(int ownerID) {
    Entity e = {0};
    e.id = ownerID + 100;
    e.type = ENTITY_BUILDING;
    e.faction = (ownerID == 0) ? FACTION_PLAYER1 : FACTION_PLAYER2;
    e.state = ESTATE_IDLE;
    e.hp = 5000;
    e.maxHP = 5000;
    e.ownerID = ownerID;
    e.alive = true;
    e.markedForRemoval = false;
    return e;
}

static GameState make_game_state(Entity *base0, Entity *base1) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    gs.gameOver = false;
    gs.winnerID = -1;

    gs.players[0].id = 0;
    gs.players[0].side = SIDE_BOTTOM;
    gs.players[0].base = base0;

    gs.players[1].id = 1;
    gs.players[1].side = SIDE_TOP;
    gs.players[1].base = base1;

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

static void test_no_winner_initially(void) {
    Entity b0 = make_base(0);
    Entity b1 = make_base(1);
    GameState gs = make_game_state(&b0, &b1);

    win_check(&gs);

    assert(gs.gameOver == false);
    assert(gs.winnerID == -1);
}

static void test_p1_base_dead_p2_wins(void) {
    Entity b0 = make_base(0);
    Entity b1 = make_base(1);
    GameState gs = make_game_state(&b0, &b1);

    b0.hp = 0;
    b0.alive = false;
    b0.state = ESTATE_DEAD;

    win_check(&gs);

    assert(gs.gameOver == true);
    assert(gs.winnerID == 1);
}

static void test_p2_base_dead_p1_wins(void) {
    Entity b0 = make_base(0);
    Entity b1 = make_base(1);
    GameState gs = make_game_state(&b0, &b1);

    b1.hp = 0;
    b1.alive = false;
    b1.state = ESTATE_DEAD;

    win_check(&gs);

    assert(gs.gameOver == true);
    assert(gs.winnerID == 0);
}

static void test_win_trigger_idempotent(void) {
    Entity b0 = make_base(0);
    Entity b1 = make_base(1);
    GameState gs = make_game_state(&b0, &b1);

    win_trigger(&gs, 0);
    assert(gs.gameOver == true);
    assert(gs.winnerID == 0);

    // Second trigger with different winner should be no-op
    win_trigger(&gs, 1);
    assert(gs.winnerID == 0);
}

static void test_win_check_stable_after_latch(void) {
    Entity b0 = make_base(0);
    Entity b1 = make_base(1);
    GameState gs = make_game_state(&b0, &b1);

    b0.hp = 0;
    b0.alive = false;

    win_check(&gs);
    assert(gs.gameOver == true);
    assert(gs.winnerID == 1);

    // Now kill second base too — result must not flip
    b1.hp = 0;
    b1.alive = false;

    win_check(&gs);
    assert(gs.winnerID == 1);
}

static void test_fallback_catches_single_dead_base(void) {
    // Simulates a damage path that bypassed the primary latch.
    // win_check() should still latch the correct winner for one dead base.
    Entity b0 = make_base(0);
    Entity b1 = make_base(1);
    GameState gs = make_game_state(&b0, &b1);

    b0.hp = 0;
    b0.alive = false;

    win_check(&gs);

    assert(gs.gameOver == true);
    assert(gs.winnerID == 1);
}

static void test_fallback_both_dead_latches_draw(void) {
    // Both bases dead without prior latch is ambiguous. The fallback should end
    // the match deterministically instead of inventing a winner by scan order.
    Entity b0 = make_base(0);
    Entity b1 = make_base(1);
    GameState gs = make_game_state(&b0, &b1);

    b0.hp = 0;
    b0.alive = false;
    b1.hp = 0;
    b1.alive = false;

    win_check(&gs);

    assert(gs.gameOver == true);
    assert(gs.winnerID == -1);
}

static void test_latch_from_destroyed_base_p1(void) {
    Entity b0 = make_base(0);
    Entity b1 = make_base(1);
    GameState gs = make_game_state(&b0, &b1);

    win_latch_from_destroyed_base(&gs, &b0);

    assert(gs.gameOver == true);
    assert(gs.winnerID == 1);
}

static void test_latch_from_destroyed_base_p2(void) {
    Entity b0 = make_base(0);
    Entity b1 = make_base(1);
    GameState gs = make_game_state(&b0, &b1);

    win_latch_from_destroyed_base(&gs, &b1);

    assert(gs.gameOver == true);
    assert(gs.winnerID == 0);
}

static void test_latch_from_non_player_building_noop(void) {
    Entity b0 = make_base(0);
    Entity b1 = make_base(1);
    Entity random_building = make_base(0);
    random_building.id = 999;
    GameState gs = make_game_state(&b0, &b1);

    // random_building is not either player's base pointer
    win_latch_from_destroyed_base(&gs, &random_building);

    assert(gs.gameOver == false);
    assert(gs.winnerID == -1);
}

static void test_latch_from_destroyed_base_idempotent(void) {
    Entity b0 = make_base(0);
    Entity b1 = make_base(1);
    GameState gs = make_game_state(&b0, &b1);

    win_latch_from_destroyed_base(&gs, &b0);
    assert(gs.winnerID == 1);

    // Second latch attempt must not flip
    win_latch_from_destroyed_base(&gs, &b1);
    assert(gs.winnerID == 1);
}

static void test_null_base_no_defeat(void) {
    Entity b1 = make_base(1);
    GameState gs = make_game_state(NULL, &b1);

    win_check(&gs);

    assert(gs.gameOver == false);
    assert(gs.winnerID == -1);
}

static void test_both_null_bases_no_defeat(void) {
    GameState gs = make_game_state(NULL, NULL);

    win_check(&gs);

    assert(gs.gameOver == false);
    assert(gs.winnerID == -1);
}

static void test_win_trigger_null_safety(void) {
    win_trigger(NULL, 0);
    // Should not crash
}

static void test_win_check_null_safety(void) {
    win_check(NULL);
    // Should not crash
}

/* ---- Main ---- */
int main(void) {
    printf("Running win condition tests...\n");

    RUN_TEST(test_no_winner_initially);
    RUN_TEST(test_p1_base_dead_p2_wins);
    RUN_TEST(test_p2_base_dead_p1_wins);
    RUN_TEST(test_win_trigger_idempotent);
    RUN_TEST(test_win_check_stable_after_latch);
    RUN_TEST(test_fallback_catches_single_dead_base);
    RUN_TEST(test_fallback_both_dead_latches_draw);
    RUN_TEST(test_latch_from_destroyed_base_p1);
    RUN_TEST(test_latch_from_destroyed_base_p2);
    RUN_TEST(test_latch_from_non_player_building_noop);
    RUN_TEST(test_latch_from_destroyed_base_idempotent);
    RUN_TEST(test_null_base_no_defeat);
    RUN_TEST(test_both_null_bases_no_defeat);
    RUN_TEST(test_win_trigger_null_safety);
    RUN_TEST(test_win_check_null_safety);

    printf("\nAll %d tests passed!\n", tests_passed);
    return 0;
}
