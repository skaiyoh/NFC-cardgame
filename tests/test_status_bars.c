/*
 * Unit tests for src/rendering/status_bars.c
 *
 * Self-contained: includes production code directly with minimal Raylib/type
 * stubs so troop/base bar draw behavior can be validated without the full app.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ---- Prevent production headers from pulling in heavy dependencies ---- */
#define RAYLIB_H
#define NFC_CARDGAME_CONFIG_H
#define NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_STATUS_BARS_H
#define NFC_CARDGAME_SPRITE_RENDERER_H

/* ---- Raylib stubs ---- */
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
typedef struct {
    Vector2 offset;
    Vector2 target;
    float rotation;
    float zoom;
} Camera2D;
/* Opaque Font stub — the production label draw path only passes it through. */
typedef struct { int _unused; } Font;

#define WHITE (Color){255, 255, 255, 255}
#define BLACK (Color){  0,   0,   0, 255}
#define TEXTURE_FILTER_POINT 0

static int g_drawCalls = 0;          /* DrawTexturePro */
static int g_rectCalls = 0;          /* DrawRectanglePro */
static int g_textCalls = 0;          /* DrawTextPro */
static Rectangle g_drawSrcs[16];
static Rectangle g_drawDsts[16];
static Rectangle g_rectDsts[16];

static Texture2D LoadTexture(const char *fileName) {
    (void)fileName;
    return (Texture2D){ .id = 1, .width = 2048, .height = 64, .mipmaps = 1, .format = 7 };
}

static void UnloadTexture(Texture2D texture) {
    (void)texture;
}

static void SetTextureFilter(Texture2D texture, int filter) {
    (void)texture;
    (void)filter;
}

static void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
                           Vector2 origin, float rotation, Color tint) {
    (void)texture;
    (void)origin;
    (void)rotation;
    (void)tint;

    if (g_drawCalls < (int)(sizeof(g_drawSrcs) / sizeof(g_drawSrcs[0]))) {
        g_drawSrcs[g_drawCalls] = source;
        g_drawDsts[g_drawCalls] = dest;
    }
    g_drawCalls++;
}

static void DrawRectanglePro(Rectangle rec, Vector2 origin, float rotation, Color color) {
    (void)rec;
    (void)origin;
    (void)rotation;
    (void)color;
    if (g_rectCalls < (int)(sizeof(g_rectDsts) / sizeof(g_rectDsts[0]))) {
        g_rectDsts[g_rectCalls] = rec;
    }
    g_rectCalls++;
}

static Font GetFontDefault(void) {
    return (Font){0};
}

static Vector2 MeasureTextEx(Font font, const char *text, float fontSize, float spacing) {
    (void)font;
    (void)fontSize;
    (void)spacing;
    /* Approximate: the production code only uses this for centering. */
    float chars = text ? (float)strlen(text) : 0.0f;
    return (Vector2){ chars * 6.0f, fontSize };
}

static void DrawTextPro(Font font, const char *text, Vector2 position, Vector2 origin,
                        float rotation, float fontSize, float spacing, Color tint) {
    (void)font;
    (void)text;
    (void)position;
    (void)origin;
    (void)rotation;
    (void)fontSize;
    (void)spacing;
    (void)tint;
    g_textCalls++;
}

static Vector2 GetWorldToScreen2D(Vector2 position, Camera2D camera) {
    (void)camera;
    return position;
}

/* ---- Config stubs ---- */
#define STATUS_BARS_PATH "src/assets/environment/Objects/health_energy_bars.png"
#define PI_F 3.14159265f
#define MAX_ENTITIES 64

/* ---- Minimal sprite/type stubs ---- */
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
    DIR_SIDE,
    DIR_DOWN,
    DIR_UP,
    DIR_COUNT
} SpriteDirection;

typedef struct {
    Texture2D texture;
    int frameWidth;
    int frameHeight;
    int frameCount;
    Rectangle *visibleBounds;
} SpriteSheet;

typedef struct {
    SpriteSheet anims[ANIM_COUNT];
} CharacterSprite;

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

static const SpriteSheet *sprite_sheet_get(const CharacterSprite *cs, AnimationType anim) {
    (void)cs;
    (void)anim;
    return NULL;
}

static Rectangle sprite_visible_bounds(const CharacterSprite *cs, const AnimState *state,
                                       Vector2 pos, float scale, float rotationDegrees) {
    (void)cs;
    (void)state;
    (void)pos;
    (void)scale;
    (void)rotationDegrees;
    return (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};
}

/* ---- Minimal game type stubs ---- */
typedef enum { ENTITY_TROOP, ENTITY_BUILDING, ENTITY_PROJECTILE } EntityType;
typedef enum { FACTION_PLAYER1, FACTION_PLAYER2 } Faction;
typedef enum { ESTATE_IDLE, ESTATE_WALKING, ESTATE_ATTACKING, ESTATE_DEAD } EntityState;

typedef struct Entity {
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
    int targeting;
    const char *targetType;
    AnimState anim;
    const CharacterSprite *sprite;
    int spriteType;
    float spriteScale;
    float spriteRotationDegrees;
    int presentationSide;
    int ownerID;
    int lane;
    int waypointIndex;
    float hitFlashTimer;
    int unitRole;
    int farmerState;
    int claimedSustenanceNodeId;
    int carriedSustenanceValue;
    float workTimer;
    bool alive;
    bool markedForRemoval;
} Entity;

typedef struct Player {
    int id;
    int side;
    Rectangle screenArea;
    Camera2D camera;
    float cameraRotation;
    float energy;
    float maxEnergy;
    float energyRegenRate;
    Entity *base;
    int sustenanceCollected;
} Player;

typedef struct Battlefield {
    Entity *entities[MAX_ENTITIES];
    int entityCount;
} Battlefield;

typedef struct GameState {
    Player players[2];
    Battlefield battlefield;
    Texture2D statusBarsTexture;
} GameState;

/* ---- Include production code ---- */
#include "../src/rendering/status_bars.c"

/* ---- Test helpers ---- */
static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(fn) do { \
    reset_draw_state(); \
    printf("  "); \
    fn(); \
    tests_run++; \
    tests_passed++; \
    printf("PASS: %s\n", #fn); \
} while (0)

static bool approx_eq(float a, float b, float eps) {
    return fabsf(a - b) < eps;
}

static void reset_draw_state(void) {
    g_drawCalls = 0;
    g_rectCalls = 0;
    g_textCalls = 0;
    for (int i = 0; i < (int)(sizeof(g_drawSrcs) / sizeof(g_drawSrcs[0])); i++) {
        g_drawSrcs[i] = (Rectangle){0};
        g_drawDsts[i] = (Rectangle){0};
        g_rectDsts[i] = (Rectangle){0};
    }
}

static Entity make_troop(int hp, int maxHP) {
    Entity troop = {0};
    troop.type = ENTITY_TROOP;
    troop.hp = hp;
    troop.maxHP = maxHP;
    troop.alive = true;
    troop.position = (Vector2){100.0f, 200.0f};
    return troop;
}

static Entity make_base(int hp, int maxHP) {
    Entity base = {0};
    base.type = ENTITY_BUILDING;
    base.hp = hp;
    base.maxHP = maxHP;
    base.alive = true;
    base.position = (Vector2){400.0f, 500.0f};
    return base;
}

static GameState make_game_state(void) {
    GameState gs = {0};
    gs.statusBarsTexture = (Texture2D){ .id = 1, .width = 2048, .height = 64, .mipmaps = 1, .format = 7 };
    gs.players[0].maxEnergy = 10.0f;
    gs.players[1].maxEnergy = 10.0f;
    return gs;
}

/* ---- Tests ---- */
static void test_full_health_troop_frame_hidden(void) {
    Entity troop = make_troop(100, 100);

    assert(troop_health_frame(&troop) == -1);
}

static void test_damaged_troop_frame_visible(void) {
    Entity troop = make_troop(75, 100);
    int frame = troop_health_frame(&troop);

    assert(frame >= 0);
    assert(frame < STATUS_BAR_TROOP_FRAMES);
}

static void test_status_bars_draw_skips_undamaged_troops(void) {
    GameState gs = make_game_state();
    Entity troop = make_troop(100, 100);
    Camera2D camera = {0};

    gs.battlefield.entities[0] = &troop;
    gs.battlefield.entityCount = 1;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f);

    assert(g_drawCalls == 0);
}

static void test_status_bars_draws_damaged_troop_bar(void) {
    GameState gs = make_game_state();
    Entity troop = make_troop(80, 100);
    Camera2D camera = {0};

    gs.battlefield.entities[0] = &troop;
    gs.battlefield.entityCount = 1;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f);

    assert(g_drawCalls == 1);
    assert(approx_eq(g_drawSrcs[0].y, 0.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[0].width, 42.0f, 0.001f));
    assert(approx_eq(g_drawDsts[0].width, 42.0f, 0.001f));
    assert(approx_eq(g_drawDsts[0].height, 16.0f, 0.001f));
}

/* Base bars: 4500/5000 HP and 7/10 energy → blended renderer (neither
 * degenerate case). Each bar issues two DrawTexturePro calls (left half from
 * the full frame, right half from the empty frame), and each label emits
 * two DrawTextPro calls (shadow + main). */
static void test_base_bars_continuous_blend_draws_two_halves_per_bar(void) {
    GameState gs = make_game_state();
    Entity base = make_base(4500, 5000);
    Camera2D camera = {0};

    gs.players[0].base = &base;
    gs.players[0].energy = 7.0f;
    gs.players[0].maxEnergy = 10.0f;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f);

    /* 2 bars * 2 halves per bar = 4 texture draws. */
    assert(g_drawCalls == 4);
    /* 2 labels * (shadow + main) = 4 text draws. */
    assert(g_textCalls == 4);

    /* Per-bar row Y matches HP (16) / Energy (32). Row Y stays consistent
     * across both halves of a single bar. */
    assert(approx_eq(g_drawSrcs[0].y, 16.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[1].y, 16.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[2].y, 32.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[3].y, 32.0f, 0.001f));

    /* HP bar source halves must sum to BASE_SRC_WIDTH. 0.9 * 138 rounds to 124.
     * Left half width = 124, right half width = 14. */
    float hpLeftW  = g_drawSrcs[0].width;
    float hpRightW = g_drawSrcs[1].width;
    assert(approx_eq(hpLeftW + hpRightW, 138.0f, 0.5f));
    assert(approx_eq(hpLeftW, 124.0f, 0.5f));
    assert(approx_eq(g_drawDsts[0].width + g_drawDsts[1].width, 180.0f, 0.5f));
    assert(approx_eq(g_drawDsts[0].height, 24.0f, 0.001f));
    assert(approx_eq(g_drawDsts[1].height, 24.0f, 0.001f));

    /* Energy: 0.7 * 138 ≈ 96.6 → rounds to 97. */
    float enLeftW  = g_drawSrcs[2].width;
    float enRightW = g_drawSrcs[3].width;
    assert(approx_eq(enLeftW + enRightW, 138.0f, 0.5f));
    assert(approx_eq(enLeftW, 97.0f, 0.5f));
    assert(approx_eq(g_drawDsts[2].width + g_drawDsts[3].width, 180.0f, 0.5f));
    assert(approx_eq(g_drawDsts[2].height, 24.0f, 0.001f));
    assert(approx_eq(g_drawDsts[3].height, 24.0f, 0.001f));
}

/* Full bar short-circuits to a single DrawTexturePro call at frame 0 with
 * the full atlas width, rendered larger on screen. */
static void test_base_bar_full_draws_single_full_frame(void) {
    GameState gs = make_game_state();
    Entity base = make_base(5000, 5000);
    Camera2D camera = {0};

    gs.players[0].base = &base;
    gs.players[0].energy = 10.0f;
    gs.players[0].maxEnergy = 10.0f;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f);

    /* 1 call per bar (full-frame short-circuit), 2 bars = 2 calls. */
    assert(g_drawCalls == 2);

    /* HP bar: frame 0, full width, row y = 16. Frame 0 X offset = 3. */
    assert(approx_eq(g_drawSrcs[0].x, 3.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[0].y, 16.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[0].width, 138.0f, 0.001f));
    assert(approx_eq(g_drawDsts[0].width, 180.0f, 0.001f));
    assert(approx_eq(g_drawDsts[0].height, 24.0f, 0.001f));

    /* Energy bar: frame 0 (same X offset), row y = 32. */
    assert(approx_eq(g_drawSrcs[1].x, 3.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[1].y, 32.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[1].width, 138.0f, 0.001f));
    assert(approx_eq(g_drawDsts[1].width, 180.0f, 0.001f));
    assert(approx_eq(g_drawDsts[1].height, 24.0f, 0.001f));
}

/* Empty bar short-circuits to a single DrawTexturePro call at the last
 * frame (N-1). Frame 23 X offset = 3 + 144*23 = 3315. */
static void test_base_bar_empty_draws_single_empty_frame(void) {
    GameState gs = make_game_state();
    Entity base = make_base(0, 5000);
    Camera2D camera = {0};

    /* hp == 0 makes the base "dead" in the battlefield filter; set a tiny
     * positive HP so the bar still renders. Since fillPixels rounds to 0
     * for any ratio < 0.5/138 ≈ 0.00362, use 1/5000 = 0.0002. */
    base.hp = 1;

    gs.players[0].base = &base;
    gs.players[0].energy = 0.0f;
    gs.players[0].maxEnergy = 10.0f;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f);

    /* Both bars degenerate to empty-frame single draw. */
    assert(g_drawCalls == 2);

    const float lastFrameX = 3.0f + 144.0f * 23.0f;
    assert(approx_eq(g_drawSrcs[0].x, lastFrameX, 0.001f));
    assert(approx_eq(g_drawSrcs[1].x, lastFrameX, 0.001f));
    assert(approx_eq(g_drawSrcs[0].width, 138.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[1].width, 138.0f, 0.001f));
    assert(approx_eq(g_drawDsts[0].width, 180.0f, 0.001f));
    assert(approx_eq(g_drawDsts[0].height, 24.0f, 0.001f));
    assert(approx_eq(g_drawDsts[1].width, 180.0f, 0.001f));
    assert(approx_eq(g_drawDsts[1].height, 24.0f, 0.001f));
}

/* Core motivating requirement: small HP damage must visibly move the bar.
 * 28 HP out of 5000 is 0.56% of full — with 138-pixel granularity, that's
 * 0.773 fill pixels which rounds to 1 fill pixel of movement. */
static void test_base_bar_granularity_moves_on_small_hit(void) {
    GameState gs = make_game_state();
    Camera2D camera = {0};
    gs.players[0].energy = 10.0f;
    gs.players[0].maxEnergy = 10.0f;

    /* Sample 1: 5000/5000 HP (full). */
    Entity baseFull = make_base(5000, 5000);
    gs.players[0].base = &baseFull;
    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f);
    /* Full-frame short-circuit → 1 call, the HP bar's first draw. */
    int fullCalls = g_drawCalls;

    /* Sample 2: 4972/5000 HP (damaged by 28). */
    reset_draw_state();
    Entity baseHit = make_base(4972, 5000);
    gs.players[0].base = &baseHit;
    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f);

    /* After a 28 HP hit, the HP bar should no longer short-circuit to the
     * full frame — it must go through the blended path (2 draws for the HP
     * bar) or fall back to empty frame short-circuit if rounding pushes it
     * all the way down. In either case, HP bar behavior changes from the
     * full case. */
    assert(fullCalls == 2);      /* full case: 1 HP draw + 1 energy draw */
    assert(g_drawCalls == 3);    /* hit case: 2 HP halves + 1 full-energy draw */

    /* HP left half width represents the filled portion: round(4972/5000 *
     * 138) = round(137.227) = 137 pixels. So the HP bar has shrunk by 1
     * visible pixel. */
    assert(approx_eq(g_drawSrcs[0].width, 137.0f, 0.5f));
    assert(approx_eq(g_drawSrcs[1].width, 1.0f, 0.5f));
    assert(approx_eq(g_drawDsts[0].width + g_drawDsts[1].width, 180.0f, 0.5f));
    assert(approx_eq(g_drawDsts[0].height, 24.0f, 0.001f));
    assert(approx_eq(g_drawDsts[1].height, 24.0f, 0.001f));
}

/* Atlas load failure routes to the fallback path, which must still draw
 * both base bars AND their numeric labels. */
static void test_fallback_renders_bars_and_labels(void) {
    GameState gs = make_game_state();
    gs.statusBarsTexture.id = 0;  /* simulate load failure */
    Entity base = make_base(3420, 5000);
    Camera2D camera = {0};

    gs.players[0].base = &base;
    gs.players[0].energy = 7.0f;
    gs.players[0].maxEnergy = 10.0f;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f);

    /* No texture draws in the fallback path. */
    assert(g_drawCalls == 0);

    /* Each bar emits: 1 border rect + 1 empty bg rect + 1 fill rect = 3
     * rect draws per bar. 2 bars → 6 rect draws. */
    assert(g_rectCalls == 6);
    assert(approx_eq(g_rectDsts[1].width, 180.0f, 0.001f));
    assert(approx_eq(g_rectDsts[1].height, 24.0f, 0.001f));
    assert(approx_eq(g_rectDsts[4].width, 180.0f, 0.001f));
    assert(approx_eq(g_rectDsts[4].height, 24.0f, 0.001f));

    /* Labels still render: 2 bars * (shadow + main) = 4 text draws. */
    assert(g_textCalls == 4);
}

int main(void) {
    printf("Running status bar tests...\n");

    RUN_TEST(test_full_health_troop_frame_hidden);
    RUN_TEST(test_damaged_troop_frame_visible);
    RUN_TEST(test_status_bars_draw_skips_undamaged_troops);
    RUN_TEST(test_status_bars_draws_damaged_troop_bar);
    RUN_TEST(test_base_bars_continuous_blend_draws_two_halves_per_bar);
    RUN_TEST(test_base_bar_full_draws_single_full_frame);
    RUN_TEST(test_base_bar_empty_draws_single_empty_frame);
    RUN_TEST(test_base_bar_granularity_moves_on_small_hit);
    RUN_TEST(test_fallback_renders_bars_and_labels);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return 0;
}
