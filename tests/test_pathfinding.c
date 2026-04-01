/*
 * Unit tests for src/logic/pathfinding.c
 *
 * Self-contained: redefines minimal type stubs and includes pathfinding.c
 * directly to avoid the heavy types.h include chain (Raylib, sqlite3, biome).
 * Compiles with `make test` using only -lm.
 *
 * SYNC REQUIREMENT: These struct stubs must match the field order and sizes
 * from src/core/types.h so that laneWaypoints is at the correct offset.
 * If types.h changes (fields added/removed/reordered), update these stubs.
 * Run `make test` after any types.h change to catch layout mismatches.
 */

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

/* ---- Prevent pathfinding.c's includes from pulling in heavy headers ---- */
#define NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_PATHFINDING_H
#define NFC_CARDGAME_PLAYER_H
#define NFC_CARDGAME_ENTITIES_H

/* ---- Config defines (must match src/core/config.h) ---- */
#define LANE_WAYPOINT_COUNT  8
#define LANE_BOW_INTENSITY   0.3f
#define LANE_JITTER_RADIUS   10.0f
#define PI_F 3.14159265f

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

#define NUM_CARD_SLOTS 3
#define MAX_ENTITIES 64

typedef struct {
    Vector2 worldPos;
    void *activeCard;
    float cooldownTimer;
} CardSlot;

typedef struct Entity Entity;
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

/*
 * Player stub: we only need playArea, slots, and laneWaypoints to be at
 * correct offsets. Other fields are padded to match types.h layout.
 *
 * Camera2D = { Vector2 offset, target; float rotation, zoom; } = 24 bytes
 * TileMap = { int width, height, tileSize; int *cells; int cellCount; void *texture; } -- platform-dependent
 *
 * Rather than pad precisely, we replicate the exact field list from types.h
 * using void* and char arrays for opaque types.
 */
typedef struct { int width; int height; int tileSize; int *cells; int cellCount; void *texture; } TileMap_stub;
typedef struct { Rectangle src; } TileDef_stub;

#define TILE_COUNT 32
#define MAX_DETAIL_DEFS 64

struct Player {
    int id;
    Rectangle playArea;
    Rectangle screenArea;
    /* Camera2D: offset(8) + target(8) + rotation(4) + zoom(4) = 24 bytes */
    char _camera_pad[24];
    float cameraRotation;

    TileMap_stub tilemap;
    int biome;
    const void *biomeDef;
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

/* ---- Stubs for functions pathfinding.c calls ---- */

Vector2 player_lane_pos(Player *p, int lane, float depth) {
    float laneWidth = p->playArea.width / 3.0f;
    float x = p->playArea.x + (lane + 0.5f) * laneWidth;
    float t = 0.9f - depth * 0.8f;
    float y = p->playArea.y + p->playArea.height * t;
    return (Vector2){ x, y };
}

void entity_set_state(Entity *e, EntityState newState) {
    if (e) e->state = newState;
}

/* ---- Forward declarations for functions in pathfinding.c ---- */
void lane_generate_waypoints(Player *p);
bool pathfind_step_entity(Entity *e, const Player *owner, float deltaTime);
void pathfind_apply_direction(AnimState *anim, Vector2 diff);

/* ---- Include production code under test ---- */
#include "../src/logic/pathfinding.c"

/* ---- Test helpers ---- */

static bool approx_eq(float a, float b, float eps) {
    return fabsf(a - b) < eps;
}

static Player make_test_player(void) {
    Player p;
    memset(&p, 0, sizeof(Player));
    p.id = 0;
    p.playArea = (Rectangle){ 0, 0, 1080, 960 };
    float laneWidth = p.playArea.width / 3.0f;
    float spawnY = p.playArea.y + p.playArea.height * 0.8f;
    for (int i = 0; i < 3; i++) {
        p.slots[i].worldPos = (Vector2){
            p.playArea.x + (i + 0.5f) * laneWidth,
            spawnY
        };
    }
    return p;
}

static Entity make_test_entity(int lane, int waypointIndex, float moveSpeed) {
    Entity e;
    memset(&e, 0, sizeof(Entity));
    e.lane = lane;
    e.waypointIndex = waypointIndex;
    e.moveSpeed = moveSpeed;
    e.state = ESTATE_WALKING;
    e.anim.dir = DIR_UP;
    e.anim.flipH = false;
    e.alive = true;
    return e;
}

/* ---- CORE-01a: Waypoint positions ---- */
static void test_waypoint_positions(void) {
    Player p = make_test_player();
    lane_generate_waypoints(&p);

    for (int lane = 0; lane < 3; lane++) {
        /* Waypoint[0] matches slot spawn position */
        assert(approx_eq(p.laneWaypoints[lane][0].x, p.slots[lane].worldPos.x, 0.01f));
        assert(approx_eq(p.laneWaypoints[lane][0].y, p.slots[lane].worldPos.y, 0.01f));

        /* Waypoints progress toward enemy (Y decreases for P1) */
        for (int i = 1; i < LANE_WAYPOINT_COUNT; i++) {
            assert(p.laneWaypoints[lane][i].y < p.laneWaypoints[lane][i - 1].y);
        }
    }
}

/* ---- CORE-01b: Center lane has zero lateral offset ---- */
static void test_center_lane_zero_offset(void) {
    Player p = make_test_player();
    lane_generate_waypoints(&p);

    float centerX = p.playArea.x + (1 + 0.5f) * (p.playArea.width / 3.0f);
    for (int i = 0; i < LANE_WAYPOINT_COUNT; i++) {
        assert(approx_eq(p.laneWaypoints[1][i].x, centerX, 0.1f));
    }
}

/* ---- CORE-01c: Outer lane bow magnitude and symmetry ---- */
static void test_outer_lane_bow_symmetry(void) {
    Player p = make_test_player();
    lane_generate_waypoints(&p);

    float lane0CenterX = p.playArea.x + (0 + 0.5f) * (p.playArea.width / 3.0f);
    float lane2CenterX = p.playArea.x + (2 + 0.5f) * (p.playArea.width / 3.0f);

    /* Find midpoint waypoint (index 3-4 range) */
    int mid = LANE_WAYPOINT_COUNT / 2;

    /* Lane 0 bows left (X < base lane X) */
    float lane0_offset = p.laneWaypoints[0][mid].x - lane0CenterX;
    assert(lane0_offset < 0); /* bows left */

    /* Lane 2 bows right (X > base lane X) */
    float lane2_offset = p.laneWaypoints[2][mid].x - lane2CenterX;
    assert(lane2_offset > 0); /* bows right */

    /* Symmetric: magnitudes approximately equal */
    assert(approx_eq(fabsf(lane0_offset), fabsf(lane2_offset), 5.0f));

    /* Endpoints taper back toward base X */
    float lane0_end_offset = fabsf(p.laneWaypoints[0][LANE_WAYPOINT_COUNT - 1].x - lane0CenterX);
    float lane0_mid_offset = fabsf(lane0_offset);
    assert(lane0_end_offset < lane0_mid_offset); /* less bow at endpoints */
}

/* ---- CORE-01d: Movement stepping advances waypointIndex ---- */
static void test_movement_step_advances_waypoint(void) {
    Player p = make_test_player();
    lane_generate_waypoints(&p);

    /* Entity starts at waypoint[1] position — large dt to guarantee overshoot */
    Entity e = make_test_entity(1, 1, 50000.0f);
    e.position = p.laneWaypoints[1][1];

    bool walking = pathfind_step_entity(&e, &p, 1.0f);
    (void)walking;
    /* Should have advanced past waypoint 1 */
    assert(e.waypointIndex >= 2);

    /* Normal speed: entity moves toward target but doesn't overshoot */
    Entity e2 = make_test_entity(1, 1, 10.0f); /* slow speed */
    e2.position = p.laneWaypoints[1][1];
    /* Move slightly off the waypoint so there's distance to cover */
    e2.position.y += 50.0f;

    int prevIdx = e2.waypointIndex;
    pathfind_step_entity(&e2, &p, 1.0f / 60.0f);
    /* With slow speed and 50px distance, should still be on same waypoint */
    assert(e2.waypointIndex == prevIdx);
}

/* ---- CORE-01e: Idle at last waypoint ---- */
static void test_idle_at_last_waypoint(void) {
    Player p = make_test_player();
    lane_generate_waypoints(&p);

    /* Place entity near last waypoint with high speed + large dt to guarantee overshoot */
    Entity e = make_test_entity(1, LANE_WAYPOINT_COUNT - 1, 50000.0f);
    e.position = p.laneWaypoints[1][LANE_WAYPOINT_COUNT - 2];

    bool walking = pathfind_step_entity(&e, &p, 1.0f);

    /* Should have reached end and transitioned to idle */
    assert(walking == false);
    assert(e.state == ESTATE_IDLE);
    assert(e.waypointIndex >= LANE_WAYPOINT_COUNT);
}

/* ---- CORE-01f: Sprite direction from movement vector ---- */
static void test_sprite_direction(void) {
    AnimState anim;
    memset(&anim, 0, sizeof(AnimState));

    /* Moving left → DIR_SIDE, flipH=true */
    anim.dir = DIR_UP;
    pathfind_apply_direction(&anim, (Vector2){-100, -10});
    assert(anim.dir == DIR_SIDE);
    assert(anim.flipH == true);

    /* Moving right → DIR_SIDE, flipH=false */
    pathfind_apply_direction(&anim, (Vector2){100, -10});
    assert(anim.dir == DIR_SIDE);
    assert(anim.flipH == false);

    /* Moving up (negative Y) → DIR_UP */
    pathfind_apply_direction(&anim, (Vector2){0, -100});
    assert(anim.dir == DIR_UP);
    assert(anim.flipH == false);

    /* Moving down (positive Y) → DIR_DOWN */
    pathfind_apply_direction(&anim, (Vector2){0, 100});
    assert(anim.dir == DIR_DOWN);
    assert(anim.flipH == false);

    /* Zero vector: direction unchanged */
    anim.dir = DIR_UP;
    anim.flipH = false;
    pathfind_apply_direction(&anim, (Vector2){0, 0});
    assert(anim.dir == DIR_UP);

    /* Center lane waypoints should all produce DIR_UP */
    Player p = make_test_player();
    lane_generate_waypoints(&p);
    for (int i = 1; i < LANE_WAYPOINT_COUNT; i++) {
        Vector2 diff = {
            p.laneWaypoints[1][i].x - p.laneWaypoints[1][i - 1].x,
            p.laneWaypoints[1][i].y - p.laneWaypoints[1][i - 1].y
        };
        memset(&anim, 0, sizeof(AnimState));
        pathfind_apply_direction(&anim, diff);
        assert(anim.dir == DIR_UP);
    }

    /* Any nonzero X produces DIR_SIDE (bow curves should show side-facing) */
    memset(&anim, 0, sizeof(AnimState));
    pathfind_apply_direction(&anim, (Vector2){-1, -100});
    assert(anim.dir == DIR_SIDE);
    assert(anim.flipH == true);

    /* Early outer lane segment (bow actively curving) should produce DIR_SIDE */
    Vector2 early_diff = {
        p.laneWaypoints[0][2].x - p.laneWaypoints[0][1].x,
        p.laneWaypoints[0][2].y - p.laneWaypoints[0][1].y
    };
    /* Early segment should have nonzero X from the bow curve */
    assert(fabsf(early_diff.x) > 0.01f);
    memset(&anim, 0, sizeof(AnimState));
    pathfind_apply_direction(&anim, early_diff);
    assert(anim.dir == DIR_SIDE);
}

/* ---- CORE-01 bonus: Invalid lane produces IDLE ---- */
static void test_invalid_lane_idles(void) {
    Player p = make_test_player();
    lane_generate_waypoints(&p);

    Entity e = make_test_entity(-1, 1, 100.0f);
    e.position = (Vector2){500, 500};

    bool walking = pathfind_step_entity(&e, &p, 1.0f / 60.0f);
    assert(walking == false);
    assert(e.state == ESTATE_IDLE);

    Entity e2 = make_test_entity(3, 1, 100.0f);
    e2.position = (Vector2){500, 500};
    walking = pathfind_step_entity(&e2, &p, 1.0f / 60.0f);
    assert(walking == false);
    assert(e2.state == ESTATE_IDLE);
}

/* ---- main ---- */
int main(void) {
    printf("Running pathfinding tests...\n");
    test_waypoint_positions();              printf("  PASS: test_waypoint_positions\n");
    test_center_lane_zero_offset();         printf("  PASS: test_center_lane_zero_offset\n");
    test_outer_lane_bow_symmetry();         printf("  PASS: test_outer_lane_bow_symmetry\n");
    test_movement_step_advances_waypoint(); printf("  PASS: test_movement_step_advances_waypoint\n");
    test_idle_at_last_waypoint();           printf("  PASS: test_idle_at_last_waypoint\n");
    test_sprite_direction();                printf("  PASS: test_sprite_direction\n");
    test_invalid_lane_idles();              printf("  PASS: test_invalid_lane_idles\n");
    printf("\nAll 7 tests passed!\n");
    return 0;
}
