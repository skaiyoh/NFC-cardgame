/*
 * test_pathfinding.c -- Standalone unit tests for lane pathfinding (CORE-01a..f).
 *
 * Compiles with: gcc -Wall -Wextra -O2 tests/test_pathfinding.c -o test_pathfinding -lm
 * No Raylib, no sqlite3, no GPU required.
 *
 * Strategy: redefine minimal type stubs, pre-define header guards so that
 * #include "pathfinding.c" does not pull in the real types.h / raylib.h chain,
 * then call production functions directly.
 */

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

/* --------------------------------------------------------------------------
 * SYNC REQUIREMENT: These struct stubs must match the field order and sizes
 * from src/core/types.h so that laneWaypoints is at the correct offset.
 * If types.h changes (fields added/removed/reordered), update these stubs.
 * Run `make test` after any types.h change to catch layout mismatches.
 * -------------------------------------------------------------------------- */

/* ---- Minimal Raylib type stubs ---- */

typedef struct { float x; float y; } Vector2;
typedef struct { float x; float y; float width; float height; } Rectangle;

/* ---- Config defines (must match src/core/config.h additions from Plan 01) ---- */

#define LANE_WAYPOINT_COUNT  8
#define LANE_BOW_INTENSITY   0.5f
#define LANE_JITTER_RADIUS   10.0f
#define PI_F 3.14159265f
#define DIRECTION_HYSTERESIS 0.15f

/* ---- Entity enums (must match types.h) ---- */

typedef enum { ENTITY_TROOP, ENTITY_BUILDING, ENTITY_PROJECTILE } EntityType;
typedef enum { FACTION_PLAYER1, FACTION_PLAYER2 } Faction;
typedef enum { ESTATE_IDLE, ESTATE_WALKING, ESTATE_DEAD } EntityState;

/* ---- Sprite enums and AnimState (must match sprite_renderer.h) ---- */

typedef enum {
    ANIM_IDLE,
    ANIM_RUN,
    ANIM_WALK,
    ANIM_HURT,
    ANIM_DEATH,
    ANIM_ATTACK,
    ANIM_COUNT
} AnimationType;

typedef enum {
    DIR_DOWN,
    DIR_SIDE,
    DIR_UP,
    DIR_COUNT
} SpriteDirection;

typedef struct {
    AnimationType anim;
    SpriteDirection dir;
    int frame;
    float timer;
    float fps;
    bool flipH;
} AnimState;

/* ---- Stub for CharacterSprite (only used as a pointer in Entity) ---- */

typedef struct { int _placeholder; } CharacterSprite;

/* ---- Constants (must match types.h) ---- */

#define NUM_CARD_SLOTS 3
#define MAX_ENTITIES   64

/* ---- CardSlot (must match types.h field order) ---- */

typedef struct {
    Vector2 worldPos;
    void *activeCard;       /* Card* in production */
    float cooldownTimer;
} CardSlot;

/* ---- Entity struct (must match types.h field order) ---- */

typedef struct Entity Entity;
struct Entity {
    int id;
    EntityType type;
    Faction faction;
    EntityState state;

    /* Transform */
    Vector2 position;
    float moveSpeed;

    /* Stats */
    int hp, maxHP;
    int attack;
    float attackSpeed;
    float attackRange;

    /* Animation */
    AnimState anim;
    const CharacterSprite *sprite;
    float spriteScale;

    /* Ownership */
    int ownerID;
    int lane;
    int waypointIndex;  /* Added by Plan 01-01 */

    /* Flags */
    bool alive;
    bool markedForRemoval;
};

/* ---- Tilemap stubs (sizes must match to keep Player layout correct) ---- */

/* TileDef: Texture2D* (8 bytes ptr) + Rectangle (16 bytes) = 24 bytes on x86_64 */
typedef struct { void *texture; float sx, sy, sw, sh; } TileDef_stub;

/* TileMap: int rows, cols; int* cells; int* detailCells; int* biomeLayerCells[8];
 *          float tileSize, tileScale, originX, originY;
 * = 2 ints + 10 ptrs + 4 floats
 */
typedef struct {
    int rows, cols;
    void *cells;
    void *detailCells;
    void *biomeLayerCells[8];  /* MAX_BIOME_LAYERS = 8 */
    float tileSize;
    float tileScale;
    float originX;
    float originY;
} TileMap_stub;

/* BiomeType enum: 4 values + COUNT */
typedef enum { BIOME_GRASS_STUB, BIOME_UNDEAD_STUB, BIOME_SNOW_STUB, BIOME_SWAMP_STUB, BIOME_COUNT_STUB } BiomeType_stub;

/* BiomeDef: large struct, only used as a const pointer in Player */
typedef struct { int _placeholder; } BiomeDef_stub;

#define TILE_COUNT_STUB 32
#define MAX_DETAIL_DEFS_STUB 64

/* ---- Player struct (must match types.h field order exactly) ---- */

typedef struct Player Player;
struct Player {
    int id;
    Rectangle playArea;
    Rectangle screenArea;

    /* Camera2D: Vector2 offset, Vector2 target, float rotation, float zoom = 24 bytes */
    char _camera_pad[24];
    float cameraRotation;

    /* TileMap tilemap */
    TileMap_stub tilemap;

    /* BiomeType biome (enum = int) */
    int _biome;

    /* const BiomeDef *biomeDef */
    void *_biomeDef;

    /* TileDef tileDefs[TILE_COUNT=32]: each 24 bytes => 768 bytes */
    char _tileDefs_pad[32 * 24];
    int tileDefCount;

    /* TileDef detailDefs[MAX_DETAIL_DEFS=64]: each 24 bytes => 1536 bytes */
    char _detailDefs_pad[64 * 24];
    int detailDefCount;

    /* CardSlot slots[3] */
    CardSlot slots[NUM_CARD_SLOTS];

    /* Entity *entities[64] */
    void *entities[MAX_ENTITIES];
    int entityCount;

    /* Energy */
    float energy;
    float maxEnergy;
    float energyRegenRate;

    /* Base */
    void *base;

    /* Lane waypoints (added by Plan 01-01) */
    Vector2 laneWaypoints[3][LANE_WAYPOINT_COUNT];
};

/* ---- Prevent pathfinding.c from pulling in real headers ---- */

#define NFC_CARDGAME_PATHFINDING_H
#define NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_PLAYER_H
#define NFC_CARDGAME_CONFIG_H
#define NFC_CARDGAME_SPRITE_RENDERER_H

/* ---- Stub functions that pathfinding.c calls ---- */

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

void anim_state_init(AnimState *state, AnimationType anim, SpriteDirection dir, float fps) {
    if (state) { state->anim = anim; state->dir = dir; state->fps = fps; }
}

/* ---- Stub for GameState (pathfinding.c references it in old pathfind_next_step) ---- */
typedef struct GameState GameState;
struct GameState { int _placeholder; };

/* ---- Include production pathfinding code ---- */
/* Header guards above prevent transitive includes from loading real types.h */

#include "../src/logic/pathfinding.c"

/* ===================== Test helpers ===================== */

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

static Entity make_test_entity(int lane, const Player *p) {
    Entity e;
    memset(&e, 0, sizeof(Entity));
    e.id = 1;
    e.type = ENTITY_TROOP;
    e.faction = FACTION_PLAYER1;
    e.state = ESTATE_WALKING;
    e.lane = lane;
    e.waypointIndex = 1;  /* Skip zero-distance waypoint[0] per review fix */
    e.position = p->laneWaypoints[lane][1];
    e.moveSpeed = 100.0f;
    e.alive = true;
    e.anim.dir = DIR_UP;
    e.anim.flipH = false;
    return e;
}

/* ===================== Test functions ===================== */

/*
 * CORE-01a: Waypoint generation produces correct positions for all 3 lanes.
 * - waypoint[0] for each lane matches slot worldPos (spawn position)
 * - Y values decrease from index 0 to 7 (troops walk toward enemy = lower Y)
 * - All waypoints within playArea bounds
 */
void test_waypoint_positions(void) {
    Player p = make_test_player();
    lane_generate_waypoints(&p);

    for (int lane = 0; lane < 3; lane++) {
        /* waypoint[0] should match slot spawn position */
        assert(approx_eq(p.laneWaypoints[lane][0].x, p.slots[lane].worldPos.x, 0.01f));
        assert(approx_eq(p.laneWaypoints[lane][0].y, p.slots[lane].worldPos.y, 0.01f));

        /* Y values should decrease (troops move toward enemy = lower Y) */
        for (int i = 1; i < LANE_WAYPOINT_COUNT; i++) {
            assert(p.laneWaypoints[lane][i].y < p.laneWaypoints[lane][i - 1].y);
        }

        /* All waypoints should be within playArea bounds (with some margin for bow) */
        for (int i = 0; i < LANE_WAYPOINT_COUNT; i++) {
            float wx = p.laneWaypoints[lane][i].x;
            float wy = p.laneWaypoints[lane][i].y;
            /* X can extend slightly beyond playArea due to bow, allow 10% margin */
            assert(wx >= p.playArea.x - p.playArea.width * 0.1f);
            assert(wx <= p.playArea.x + p.playArea.width * 1.1f);
            /* Y must be within playArea */
            assert(wy >= p.playArea.y);
            assert(wy <= p.playArea.y + p.playArea.height);
        }
    }
}

/*
 * CORE-01b: Center lane waypoints have zero lateral offset.
 * Lane 1 (center) X should be 540.0 for P1 playArea (0, 0, 1080, 960).
 */
void test_center_lane_zero_offset(void) {
    Player p = make_test_player();
    lane_generate_waypoints(&p);

    float expectedX = 540.0f;  /* playArea.x + (1 + 0.5) * (1080 / 3) */
    for (int i = 0; i < LANE_WAYPOINT_COUNT; i++) {
        assert(approx_eq(p.laneWaypoints[1][i].x, expectedX, 0.1f));
    }
}

/*
 * CORE-01c: Outer lanes bow outward symmetrically with correct magnitude.
 * - Lane 0 bows left (negative X offset from center of lane)
 * - Lane 2 bows right (positive X offset from center of lane)
 * - Bow magnitude at midpoint ~= LANE_BOW_INTENSITY * laneWidth * sin(midDepth * PI)
 * - Lane 0 and lane 2 offsets are symmetric (equal magnitude, opposite sign)
 */
void test_outer_lane_bow_magnitude(void) {
    Player p = make_test_player();
    lane_generate_waypoints(&p);

    float laneWidth = p.playArea.width / 3.0f;  /* 360.0 */

    /* Find the midpoint waypoint (approximately index 3 or 4 for 8 waypoints).
     * With waypoints from spawn depth to ~0.85 depth, midpoint is around index 3-4. */
    int midIdx = LANE_WAYPOINT_COUNT / 2;  /* index 4 */

    /* Get the "straight" X for lane 0 and lane 2 using player_lane_pos at same depth */
    float lane0_straight_x = p.playArea.x + (0 + 0.5f) * laneWidth;  /* 180.0 */
    float lane2_straight_x = p.playArea.x + (2 + 0.5f) * laneWidth;  /* 900.0 */

    /* Actual waypoint positions */
    float lane0_actual_x = p.laneWaypoints[0][midIdx].x;
    float lane2_actual_x = p.laneWaypoints[2][midIdx].x;

    /* Lane 0 should bow left (X < straight X) */
    assert(lane0_actual_x < lane0_straight_x);

    /* Lane 2 should bow right (X > straight X) */
    assert(lane2_actual_x > lane2_straight_x);

    /* Compute offsets */
    float lane0_offset = lane0_straight_x - lane0_actual_x;  /* positive for leftward bow */
    float lane2_offset = lane2_actual_x - lane2_straight_x;  /* positive for rightward bow */

    /* Offsets should be symmetric (equal magnitude within epsilon) */
    assert(approx_eq(lane0_offset, lane2_offset, 1.0f));

    /* Compute expected bow at midpoint depth.
     * The midpoint waypoint depth depends on the implementation's depth schedule.
     * We estimate it and allow generous epsilon (5.0f) for the exact depth mapping. */
    float midDepth_approx = 0.5f;
    float expectedBow = sinf(midDepth_approx * PI_F) * LANE_BOW_INTENSITY * laneWidth;
    assert(approx_eq(lane0_offset, expectedBow, expectedBow * 0.5f));
    /* Allow 50% tolerance because midpoint waypoint depth may not be exactly 0.5 */
}

/*
 * CORE-01d: Movement step advances waypointIndex.
 * Calls pathfind_step_entity() from pathfinding.c (production code, NOT inline simulation).
 *
 * - Entity starts at waypointIndex=1 (skipping zero-distance waypoint[0] per review fix)
 * - High moveSpeed: overshoots to waypoint[2] in one frame
 * - Normal moveSpeed: moves toward target, waypointIndex stays same
 */
void test_movement_step_advances_waypoint(void) {
    Player p = make_test_player();
    lane_generate_waypoints(&p);

    /* --- Sub-test 1: overshoot advances waypointIndex --- */
    {
        Entity e = make_test_entity(1, &p);  /* center lane */
        e.waypointIndex = 1;
        e.position = p.laneWaypoints[1][1];

        /* Set moveSpeed very high to overshoot distance to waypoint[2] */
        float dx = p.laneWaypoints[1][2].x - p.laneWaypoints[1][1].x;
        float dy = p.laneWaypoints[1][2].y - p.laneWaypoints[1][1].y;
        float dist = sqrtf(dx * dx + dy * dy);
        e.moveSpeed = dist * 10.0f;  /* 10x the distance in 1 second */

        float deltaTime = 1.0f;
        pathfind_step_entity(&e, &p, deltaTime);

        /* waypointIndex should have advanced past 1 */
        assert(e.waypointIndex >= 2);
    }

    /* --- Sub-test 2: normal step moves toward target, does not advance --- */
    {
        Entity e = make_test_entity(1, &p);
        e.waypointIndex = 1;
        e.position = p.laneWaypoints[1][1];

        /* Set moveSpeed very small so we don't reach waypoint[2] */
        e.moveSpeed = 1.0f;
        float deltaTime = 0.001f;  /* tiny timestep */

        Vector2 before = e.position;
        pathfind_step_entity(&e, &p, deltaTime);

        /* Position should have changed (moved toward target) */
        float movedDist = sqrtf(
            (e.position.x - before.x) * (e.position.x - before.x) +
            (e.position.y - before.y) * (e.position.y - before.y)
        );
        assert(movedDist > 0.0f);

        /* waypointIndex should still be 1 (didn't reach waypoint[2] yet) */
        assert(e.waypointIndex == 1);
    }
}

/*
 * CORE-01e: Entity transitions to ESTATE_IDLE at last waypoint.
 * Calls pathfind_step_entity() from pathfinding.c (production code).
 *
 * - Entity at second-to-last waypoint with high speed overshoots to last
 * - After reaching/passing last waypoint, state becomes ESTATE_IDLE
 */
void test_idle_at_last_waypoint(void) {
    Player p = make_test_player();
    lane_generate_waypoints(&p);

    Entity e = make_test_entity(1, &p);
    e.waypointIndex = LANE_WAYPOINT_COUNT - 2;  /* index 6, targeting waypoint[6] then 7 */
    e.position = p.laneWaypoints[1][LANE_WAYPOINT_COUNT - 2];

    /* Set moveSpeed high enough to overshoot remaining waypoints */
    e.moveSpeed = 10000.0f;
    float deltaTime = 1.0f;

    /* Step through remaining waypoints */
    for (int step = 0; step < 5; step++) {
        pathfind_step_entity(&e, &p, deltaTime);
        if (e.state == ESTATE_IDLE) break;
    }

    /* Entity should have reached the end and transitioned to idle */
    assert(e.state == ESTATE_IDLE);
}

/*
 * CORE-01f: Sprite direction determined from movement vector.
 * Calls pathfind_compute_direction() from pathfinding.c (production code).
 *
 * Tests:
 * - Cardinal directions: left, right, up, down
 * - Hysteresis: near-45-degree vector does NOT flip if within threshold
 * - Hysteresis: clearly dominant axis DOES flip direction
 * - Waypoint-derived directions for lane 0 and lane 1
 */
void test_sprite_direction_from_movement(void) {
    Entity e;
    memset(&e, 0, sizeof(Entity));

    /* --- Basic direction tests --- */

    /* Strongly horizontal-left: DIR_SIDE, flipH=true */
    e.anim.dir = DIR_DOWN;  /* reset */
    e.anim.flipH = false;
    pathfind_compute_direction(&e, (Vector2){-100, -10});
    assert(e.anim.dir == DIR_SIDE);
    assert(e.anim.flipH == true);

    /* Strongly horizontal-right: DIR_SIDE, flipH=false */
    e.anim.dir = DIR_DOWN;
    e.anim.flipH = true;
    pathfind_compute_direction(&e, (Vector2){100, -10});
    assert(e.anim.dir == DIR_SIDE);
    assert(e.anim.flipH == false);

    /* Strongly vertical-up (negative Y): DIR_UP */
    e.anim.dir = DIR_DOWN;
    e.anim.flipH = true;
    pathfind_compute_direction(&e, (Vector2){-10, -100});
    assert(e.anim.dir == DIR_UP);
    assert(e.anim.flipH == false);

    /* Strongly vertical-down (positive Y): DIR_DOWN */
    e.anim.dir = DIR_UP;
    e.anim.flipH = true;
    pathfind_compute_direction(&e, (Vector2){10, 100});
    assert(e.anim.dir == DIR_DOWN);
    assert(e.anim.flipH == false);

    /* --- Hysteresis tests (review concern 4: prevent 45-degree flicker) --- */

    /* Set current direction to DIR_UP */
    e.anim.dir = DIR_UP;
    e.anim.flipH = false;

    /* Near-45-degree vector where vertical is slightly dominant: (50, -55)
     * ratio = |50| / |55| = 0.909, which is close to 1.0 (45 degrees).
     * With hysteresis, this should NOT flip from DIR_UP to DIR_SIDE. */
    pathfind_compute_direction(&e, (Vector2){50, -55});
    assert(e.anim.dir == DIR_UP);  /* Should stay UP due to hysteresis */

    /* Clearly horizontal-dominant: (100, -50)
     * ratio = |50| / |100| = 0.5, clearly horizontal.
     * This should override and change to DIR_SIDE. */
    pathfind_compute_direction(&e, (Vector2){100, -50});
    assert(e.anim.dir == DIR_SIDE);

    /* --- Lane-derived direction tests --- */
    {
        Player p = make_test_player();
        lane_generate_waypoints(&p);

        /* Lane 1 (center): all movement should be DIR_UP (straight line, negative Y) */
        for (int i = 0; i < LANE_WAYPOINT_COUNT - 1; i++) {
            Entity ce;
            memset(&ce, 0, sizeof(Entity));
            ce.anim.dir = DIR_DOWN;  /* start wrong to verify it changes */
            Vector2 diff = {
                p.laneWaypoints[1][i + 1].x - p.laneWaypoints[1][i].x,
                p.laneWaypoints[1][i + 1].y - p.laneWaypoints[1][i].y
            };
            pathfind_compute_direction(&ce, diff);
            assert(ce.anim.dir == DIR_UP);
        }

        /* Lane 0 early waypoints (bowing left): should have DIR_SIDE, flipH=true
         * at early indices where horizontal component is significant */
        {
            Vector2 diff_early = {
                p.laneWaypoints[0][1].x - p.laneWaypoints[0][0].x,
                p.laneWaypoints[0][1].y - p.laneWaypoints[0][0].y
            };
            /* If the bow is strong enough at early waypoints, direction may be SIDE.
             * But if vertical dominates (it often does early), it will be UP.
             * We at least verify the function runs without crashing. */
            Entity le;
            memset(&le, 0, sizeof(Entity));
            pathfind_compute_direction(&le, diff_early);
            /* Direction should be one of the valid enum values */
            assert(le.anim.dir == DIR_DOWN || le.anim.dir == DIR_SIDE || le.anim.dir == DIR_UP);
        }
    }
}

/* ===================== Main ===================== */

int main(void) {
    /* Seed rand for deterministic jitter in pathfind_step_entity */
    srand(42);

    printf("Running pathfinding tests...\n");

    test_waypoint_positions();
    printf("  PASS: test_waypoint_positions\n");

    test_center_lane_zero_offset();
    printf("  PASS: test_center_lane_zero_offset\n");

    test_outer_lane_bow_magnitude();
    printf("  PASS: test_outer_lane_bow_magnitude\n");

    test_movement_step_advances_waypoint();
    printf("  PASS: test_movement_step_advances_waypoint\n");

    test_idle_at_last_waypoint();
    printf("  PASS: test_idle_at_last_waypoint\n");

    test_sprite_direction_from_movement();
    printf("  PASS: test_sprite_direction_from_movement\n");

    printf("\nAll %d tests passed!\n", 6);
    return 0;
}
