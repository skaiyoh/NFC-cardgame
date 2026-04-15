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

#define MAX_CAPTURED_DRAWS 64

static int g_drawCalls = 0;          /* DrawTexturePro */
static int g_rectCalls = 0;          /* DrawRectanglePro */
static int g_textCalls = 0;          /* DrawTextPro */
static Rectangle g_drawSrcs[MAX_CAPTURED_DRAWS];
static Rectangle g_drawDsts[MAX_CAPTURED_DRAWS];
static Color     g_drawTints[MAX_CAPTURED_DRAWS];
static Rectangle g_rectDsts[MAX_CAPTURED_DRAWS];
static Color     g_rectColors[MAX_CAPTURED_DRAWS];
static char      g_textStrings[MAX_CAPTURED_DRAWS][32];
static Vector2   g_textPositions[MAX_CAPTURED_DRAWS];
static float     g_textRotations[MAX_CAPTURED_DRAWS];

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

    if (g_drawCalls < MAX_CAPTURED_DRAWS) {
        g_drawSrcs[g_drawCalls] = source;
        g_drawDsts[g_drawCalls] = dest;
        g_drawTints[g_drawCalls] = tint;
    }
    g_drawCalls++;
}

static void DrawRectanglePro(Rectangle rec, Vector2 origin, float rotation, Color color) {
    (void)origin;
    (void)rotation;
    if (g_rectCalls < MAX_CAPTURED_DRAWS) {
        g_rectDsts[g_rectCalls] = rec;
        g_rectColors[g_rectCalls] = color;
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
    (void)origin;
    (void)fontSize;
    (void)spacing;
    (void)tint;
    if (g_textCalls < MAX_CAPTURED_DRAWS) {
        if (text) {
            size_t n = strlen(text);
            if (n >= sizeof(g_textStrings[0])) n = sizeof(g_textStrings[0]) - 1;
            memcpy(g_textStrings[g_textCalls], text, n);
            g_textStrings[g_textCalls][n] = '\0';
        } else {
            g_textStrings[g_textCalls][0] = '\0';
        }
        g_textPositions[g_textCalls] = position;
        g_textRotations[g_textCalls] = rotation;
    }
    g_textCalls++;
}

static Vector2 GetWorldToScreen2D(Vector2 position, Camera2D camera) {
    (void)camera;
    return position;
}

/* ---- Config stubs ---- */
#define STATUS_BARS_PATH "src/assets/environment/Objects/health_energy_bars_sheet.png"
#define TROOP_HEALTH_BAR_PATH "src/assets/environment/Objects/troop_health_bar_sheet.png"
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
typedef enum { ANIM_PLAY_LOOP, ANIM_PLAY_ONCE, ANIM_PLAY_IDLE_BURST } AnimPlayMode;

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

static bool sprite_sheet_has_content(const SpriteSheet *sheet) {
    return sheet &&
           (sheet->texture.id != 0 ||
            sheet->frameWidth > 0 ||
            sheet->frameHeight > 0 ||
            sheet->frameCount > 0 ||
            sheet->visibleBounds != NULL);
}

static const SpriteSheet *sprite_sheet_get(const CharacterSprite *cs, AnimationType anim) {
    if (!cs || anim < 0 || anim >= ANIM_COUNT) return NULL;

    const SpriteSheet *sheet = &cs->anims[anim];
    return sprite_sheet_has_content(sheet) ? sheet : NULL;
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
    int healAmount;
    int baseLevel;
    bool basePendingKingBurst;
    int basePendingKingBurstDamage;
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
    Texture2D troopHealthBarTexture;
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

static bool color_eq(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static void reset_draw_state(void) {
    g_drawCalls = 0;
    g_rectCalls = 0;
    g_textCalls = 0;
    for (int i = 0; i < MAX_CAPTURED_DRAWS; i++) {
        g_drawSrcs[i] = (Rectangle){0};
        g_drawDsts[i] = (Rectangle){0};
        g_drawTints[i] = (Color){0};
        g_rectDsts[i] = (Rectangle){0};
        g_rectColors[i] = (Color){0};
        g_textStrings[i][0] = '\0';
        g_textPositions[i] = (Vector2){0};
        g_textRotations[i] = 0.0f;
    }
}

static void set_sheet_visible_bounds(SpriteSheet *sheet, Rectangle *bounds,
                                     int frameCount, int frameWidth, int frameHeight,
                                     Rectangle visibleBounds) {
    sheet->frameCount = frameCount;
    sheet->frameWidth = frameWidth;
    sheet->frameHeight = frameHeight;
    sheet->visibleBounds = bounds;

    for (int dir = 0; dir < DIR_COUNT; dir++) {
        for (int frame = 0; frame < frameCount; frame++) {
            bounds[dir * frameCount + frame] = visibleBounds;
        }
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
    gs.troopHealthBarTexture = (Texture2D){ .id = 2, .width = 113, .height = 5, .mipmaps = 1, .format = 7 };
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

    assert(frame >= 1);
    assert(frame < TROOP_HEALTH_BAR_FRAMES);
}

static void test_status_bars_draw_skips_undamaged_troops(void) {
    GameState gs = make_game_state();
    Entity troop = make_troop(100, 100);
    Camera2D camera = {0};

    gs.battlefield.entities[0] = &troop;
    gs.battlefield.entityCount = 1;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);

    assert(g_drawCalls == 0);
}

static void test_status_bars_draws_damaged_troop_bar(void) {
    GameState gs = make_game_state();
    Entity troop = make_troop(80, 100);
    Camera2D camera = {0};

    gs.battlefield.entities[0] = &troop;
    gs.battlefield.entityCount = 1;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);

    assert(g_drawCalls == 1);
    assert(approx_eq(g_drawSrcs[0].y, 0.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[0].x, 85.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[0].width, 13.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[0].height, 5.0f, 0.001f));
    assert(approx_eq(g_drawDsts[0].width, 42.0f, 0.001f));
    assert(approx_eq(g_drawDsts[0].height, 16.0f, 0.001f));
}

static void test_status_bars_reverses_troop_bar_for_flipped_viewport(void) {
    GameState gs = make_game_state();
    Entity troop = make_troop(80, 100);
    Camera2D camera = {0};

    gs.battlefield.entities[0] = &troop;
    gs.battlefield.entityCount = 1;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);

    Rectangle forwardSrc = g_drawSrcs[0];
    Rectangle forwardDst = g_drawDsts[0];

    reset_draw_state();
    status_bars_draw_screen(&gs, camera, 90.0f, 270.0f, true);

    assert(g_drawCalls == 1);
    assert(approx_eq(g_drawDsts[0].x, forwardDst.x, 0.001f));
    assert(approx_eq(g_drawDsts[0].y, forwardDst.y, 0.001f));
    assert(approx_eq(g_drawDsts[0].width, forwardDst.width, 0.001f));
    assert(approx_eq(g_drawDsts[0].height, forwardDst.height, 0.001f));

    assert(approx_eq(g_drawSrcs[0].x, forwardSrc.x, 0.001f));
    assert(approx_eq(g_drawSrcs[0].y, forwardSrc.y, 0.001f));
    assert(approx_eq(g_drawSrcs[0].width, -forwardSrc.width, 0.001f));
    assert(approx_eq(g_drawSrcs[0].height, forwardSrc.height, 0.001f));
}

static void test_damaged_troop_uses_fallback_when_troop_texture_missing(void) {
    GameState gs = make_game_state();
    Entity troop = make_troop(80, 100);
    Camera2D camera = {0};

    gs.troopHealthBarTexture.id = 0;
    gs.battlefield.entities[0] = &troop;
    gs.battlefield.entityCount = 1;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);

    assert(g_drawCalls == 0);
    assert(g_rectCalls == 3);
}

static void test_fallback_troop_bar_reverses_for_flipped_viewport(void) {
    GameState gs = make_game_state();
    Entity troop = make_troop(80, 100);
    Camera2D camera = {0};

    gs.troopHealthBarTexture.id = 0;
    gs.battlefield.entities[0] = &troop;
    gs.battlefield.entityCount = 1;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);
    float forwardShellY = g_rectDsts[1].y;
    float forwardFillY = g_rectDsts[2].y;

    reset_draw_state();
    status_bars_draw_screen(&gs, camera, 90.0f, 270.0f, true);
    float reversedShellY = g_rectDsts[1].y;
    float reversedFillY = g_rectDsts[2].y;

    assert(approx_eq(forwardShellY, reversedShellY, 0.001f));
    assert(forwardFillY < forwardShellY);
    assert(reversedFillY > reversedShellY);
}

/* Base bars: 4500/5000 HP (0.9) and 7/10 energy.
 *
 * Expected draws per bar:
 *   - Health: 1 empty shell (92x9 src -> 188x20 dst) + 1 scaled fill overlay
 *   - Energy: 1 empty shell + 7 full pip overlays + 1 ghost next-pip overlay
 *
 * Each label emits 2 DrawTextPro calls (shadow + main). Order: hp, LVL,
 * energy, regen. */
static void test_base_bars_health_fill_and_energy_pips(void) {
    GameState gs = make_game_state();
    Entity base = make_base(4500, 5000);
    Camera2D camera = {0};

    gs.players[0].base = &base;
    gs.players[0].energy = 7.0f;
    gs.players[0].energyRegenRate = 1.0f;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);

    /* 1 health shell + 1 health fill + 1 energy shell + 7 energy pips +
     * 1 ghost regen pip = 11. */
    assert(g_drawCalls == 11);
    /* 4 labels (hp, LVL, energy, regen) * (shadow + main) = 8 text draws. */
    assert(g_textCalls == 8);

    float expectedHealthFillSrcWidth = 0.9f * STATUS_BAR_HEALTH_FILL_SRC_WIDTH;
    float expectedHealthFillDstWidth =
        expectedHealthFillSrcWidth * STATUS_BAR_BASE_DRAW_SCALE_X;

    /* Health shell: empty cell at (0, 0, 92, 9). */
    assert(approx_eq(g_drawSrcs[0].x, 0.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[0].y, 0.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[0].width, STATUS_BAR_BASE_SRC_CELL_WIDTH, 0.001f));
    assert(approx_eq(g_drawSrcs[0].height, STATUS_BAR_BASE_SRC_CELL_HEIGHT, 0.001f));

    /* Health fill: full cell interior at (94, 2, 80.1, 5), scaled on screen. */
    assert(approx_eq(g_drawSrcs[1].x,
                     STATUS_BAR_HEALTH_FULL_CELL_X + STATUS_BAR_HEALTH_FILL_SRC_LEFT_INSET,
                     0.001f));
    assert(approx_eq(g_drawSrcs[1].y,
                     STATUS_BAR_HEALTH_FULL_CELL_Y + STATUS_BAR_HEALTH_FILL_SRC_TOP_INSET,
                     0.001f));
    assert(approx_eq(g_drawSrcs[1].width, expectedHealthFillSrcWidth, 0.001f));
    assert(approx_eq(g_drawSrcs[1].height, STATUS_BAR_HEALTH_FILL_SRC_HEIGHT, 0.001f));
    assert(approx_eq(g_drawDsts[1].width, expectedHealthFillDstWidth, 0.001f));
    assert(approx_eq(g_drawDsts[1].height,
                     STATUS_BAR_HEALTH_FILL_SRC_HEIGHT * STATUS_BAR_BASE_DRAW_SCALE_Y,
                     0.001f));

    /* Energy shell: empty cell at (0, 9, 92, 9). */
    assert(approx_eq(g_drawSrcs[2].x, 0.0f, 0.001f));
    assert(approx_eq(g_drawSrcs[2].y, STATUS_BAR_ENERGY_EMPTY_CELL_Y, 0.001f));
    assert(approx_eq(g_drawSrcs[2].width, STATUS_BAR_BASE_SRC_CELL_WIDTH, 0.001f));
    assert(approx_eq(g_drawSrcs[2].height, STATUS_BAR_BASE_SRC_CELL_HEIGHT, 0.001f));

    /* Energy pips: source X = 94 + 9*i, Y = 11, 8x5. */
    for (int i = 0; i < 7; i++) {
        int idx = 3 + i;
        float expectedX = STATUS_BAR_ENERGY_FULL_CELL_X +
                          STATUS_BAR_ENERGY_PIP_SRC_LEFT_INSET +
                          STATUS_BAR_ENERGY_PIP_STRIDE * (float)i;
        assert(approx_eq(g_drawSrcs[idx].x, expectedX, 0.001f));
        assert(approx_eq(g_drawSrcs[idx].y,
                         STATUS_BAR_ENERGY_FULL_CELL_Y + STATUS_BAR_ENERGY_PIP_TOP_INSET,
                         0.001f));
        assert(approx_eq(g_drawSrcs[idx].width, STATUS_BAR_ENERGY_PIP_WIDTH, 0.001f));
        assert(approx_eq(g_drawSrcs[idx].height, STATUS_BAR_ENERGY_PIP_HEIGHT, 0.001f));
    }

    /* Integer energy still shows the next charging pip as a faint ghost. */
    assert(approx_eq(g_drawSrcs[10].x,
                     STATUS_BAR_ENERGY_FULL_CELL_X + STATUS_BAR_ENERGY_PIP_SRC_LEFT_INSET +
                         STATUS_BAR_ENERGY_PIP_STRIDE * 7.0f,
                     0.001f));
    assert(approx_eq(g_drawSrcs[10].width, STATUS_BAR_ENERGY_PIP_WIDTH, 0.001f));
    assert(g_drawTints[10].a == STATUS_BAR_REGEN_GHOST_ALPHA);
}

/* Full bar: health fill spans the full scaled interior, energy draws all
 * 10 pips. */
static void test_base_bar_full_health_and_energy(void) {
    GameState gs = make_game_state();
    Entity base = make_base(5000, 5000);
    Camera2D camera = {0};

    gs.players[0].base = &base;
    gs.players[0].energy = 10.0f;
    gs.players[0].energyRegenRate = 1.0f;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);

    /* Full energy suppresses the regen cue: 1 health shell + 1 health fill +
     * 1 energy shell + 10 pips = 13. */
    assert(g_drawCalls == 13);

    /* Health fill spans the full 89-pixel source band and scaled draw width. */
    assert(approx_eq(g_drawSrcs[1].width, STATUS_BAR_HEALTH_FILL_SRC_WIDTH, 0.001f));
    assert(approx_eq(g_drawDsts[1].width,
                     STATUS_BAR_HEALTH_FILL_SRC_WIDTH * STATUS_BAR_BASE_DRAW_SCALE_X,
                     0.001f));

    /* Energy pip 0 source at (94, 11); pip 9 source at (94 + 9*9, 11). */
    assert(approx_eq(g_drawSrcs[3].x,
                     STATUS_BAR_ENERGY_FULL_CELL_X + STATUS_BAR_ENERGY_PIP_SRC_LEFT_INSET,
                     0.001f));
    assert(approx_eq(g_drawSrcs[12].x,
                     STATUS_BAR_ENERGY_FULL_CELL_X + STATUS_BAR_ENERGY_PIP_SRC_LEFT_INSET +
                         STATUS_BAR_ENERGY_PIP_STRIDE * 9.0f,
                     0.001f));
}

/* Near-empty health still draws a tiny continuous fill sliver; energy at 0
 * draws only the shell. */
static void test_base_bar_near_empty_health_draws_tiny_fill(void) {
    GameState gs = make_game_state();
    Entity base = make_base(0, 5000);
    Camera2D camera = {0};

    /* hp=0 marks base as dead in the filter; tiny HP keeps it rendered. */
    base.hp = 1;

    gs.players[0].base = &base;
    gs.players[0].energy = 0.0f;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);

    /* Health shell + tiny health fill + energy shell. */
    assert(g_drawCalls == 3);
    assert(approx_eq(g_drawSrcs[0].y, 0.0f, 0.001f));    /* health shell */
    assert(approx_eq(g_drawSrcs[1].y,
                     STATUS_BAR_HEALTH_FULL_CELL_Y + STATUS_BAR_HEALTH_FILL_SRC_TOP_INSET,
                     0.001f));                           /* health fill */
    assert(approx_eq(g_drawSrcs[2].y, STATUS_BAR_ENERGY_EMPTY_CELL_Y, 0.001f));
    assert(g_drawDsts[1].width > 0.0f);
    assert(g_drawDsts[1].width < 0.1f);
}

/* Core motivating requirement: small HP damage must visibly move the bar.
 * 28 HP out of 5000 is 0.56% — the scaled destination fill width still moves
 * by just over one screen pixel. */
static void test_base_bar_granularity_moves_on_small_hit(void) {
    GameState gs = make_game_state();
    Camera2D camera = {0};
    gs.players[0].energy = 10.0f;

    /* Sample 1: full. */
    Entity baseFull = make_base(5000, 5000);
    gs.players[0].base = &baseFull;
    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);
    float fullFillWidth = g_drawDsts[1].width;

    /* Sample 2: 4972/5000. */
    reset_draw_state();
    Entity baseHit = make_base(4972, 5000);
    gs.players[0].base = &baseHit;
    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);
    float hitFillWidth = g_drawDsts[1].width;

    assert(approx_eq(fullFillWidth,
                     STATUS_BAR_HEALTH_FILL_SRC_WIDTH * STATUS_BAR_BASE_DRAW_SCALE_X,
                     0.001f));
    assert(approx_eq(hitFillWidth,
                     (4972.0f / 5000.0f) * STATUS_BAR_HEALTH_FILL_SRC_WIDTH *
                         STATUS_BAR_BASE_DRAW_SCALE_X,
                     0.001f));
    assert(fullFillWidth - hitFillWidth >= 1.0f);
}

static void test_base_bar_centers_stay_fixed_when_base_anim_changes(void) {
    CharacterSprite sprite = {0};
    Rectangle idleBounds[DIR_COUNT] = {0};
    Rectangle attackBounds[DIR_COUNT] = {0};
    Entity base = make_base(4500, 5000);
    Camera2D camera = {0};
    Vector2 idleHealthCenter, idleEnergyCenter;
    Vector2 attackHealthCenter, attackEnergyCenter;

    set_sheet_visible_bounds(&sprite.anims[ANIM_IDLE], idleBounds,
                             1, 64, 64,
                             (Rectangle){12.0f, 8.0f, 40.0f, 40.0f});
    set_sheet_visible_bounds(&sprite.anims[ANIM_ATTACK], attackBounds,
                             1, 64, 64,
                             (Rectangle){4.0f, 0.0f, 56.0f, 56.0f});

    base.sprite = &sprite;
    base.spriteScale = 1.0f;
    base.spriteRotationDegrees = 0.0f;
    base.anim.anim = ANIM_IDLE;

    base_bar_centers(&base, camera, &idleHealthCenter, &idleEnergyCenter);

    base.anim.anim = ANIM_ATTACK;
    base_bar_centers(&base, camera, &attackHealthCenter, &attackEnergyCenter);

    assert(approx_eq(idleHealthCenter.x, attackHealthCenter.x, 0.001f));
    assert(approx_eq(idleHealthCenter.y, attackHealthCenter.y, 0.001f));
    assert(approx_eq(idleEnergyCenter.x, attackEnergyCenter.x, 0.001f));
    assert(approx_eq(idleEnergyCenter.y, attackEnergyCenter.y, 0.001f));
}

static void test_base_bar_draw_positions_stay_fixed_when_base_attacks(void) {
    GameState gs = make_game_state();
    CharacterSprite sprite = {0};
    Rectangle idleBounds[DIR_COUNT] = {0};
    Rectangle attackBounds[DIR_COUNT] = {0};
    Entity base = make_base(4500, 5000);
    Camera2D camera = {0};
    Rectangle idleHealthShell, idleEnergyShell;

    set_sheet_visible_bounds(&sprite.anims[ANIM_IDLE], idleBounds,
                             1, 64, 64,
                             (Rectangle){12.0f, 8.0f, 40.0f, 40.0f});
    set_sheet_visible_bounds(&sprite.anims[ANIM_ATTACK], attackBounds,
                             1, 64, 64,
                             (Rectangle){4.0f, 0.0f, 56.0f, 56.0f});

    base.sprite = &sprite;
    base.spriteScale = 1.0f;
    base.spriteRotationDegrees = 0.0f;
    gs.players[0].base = &base;
    gs.players[0].energy = 7.0f;
    gs.players[0].energyRegenRate = 1.0f;

    base.anim.anim = ANIM_IDLE;
    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);
    idleHealthShell = g_drawDsts[0];
    idleEnergyShell = g_drawDsts[2];

    reset_draw_state();
    base.anim.anim = ANIM_ATTACK;
    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);

    assert(approx_eq(g_drawDsts[0].x, idleHealthShell.x, 0.001f));
    assert(approx_eq(g_drawDsts[0].y, idleHealthShell.y, 0.001f));
    assert(approx_eq(g_drawDsts[2].x, idleEnergyShell.x, 0.001f));
    assert(approx_eq(g_drawDsts[2].y, idleEnergyShell.y, 0.001f));
}

/* Fractional energy quantizes down to whole pips: 6.9 → 6 drawn + "6/10"
 * label text. Prevents the previous 7/10 round-up from showing energy the
 * player cannot yet spend. */
static void test_fractional_energy_shows_whole_pips(void) {
    GameState gs = make_game_state();
    Entity base = make_base(5000, 5000);
    Camera2D camera = {0};

    gs.players[0].base = &base;
    gs.players[0].energy = 6.9f;
    gs.players[0].energyRegenRate = 1.0f;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);

    /* 1 health shell + 1 health fill + 1 energy shell + 6 pips +
     * ghost + fractional progress = 11. */
    assert(g_drawCalls == 11);

    /* Draw order: hp (0,1), LVL (2,3), energy (4,5), regen (6,7). */
    assert(strcmp(g_textStrings[0], "5000/5000") == 0);
    assert(strcmp(g_textStrings[4], "6/10") == 0);
    assert(strcmp(g_textStrings[5], "6/10") == 0);
}

/* Fractional energy now drives a visual-first cue: the next pip is ghosted
 * and partially filled toward the next whole energy. */
static void test_fractional_energy_draws_regen_ghost_and_progress(void) {
    GameState gs = make_game_state();
    Entity base = make_base(5000, 5000);
    Camera2D camera = {0};

    gs.players[0].base = &base;
    gs.players[0].energy = 6.9f;
    gs.players[0].energyRegenRate = 1.0f;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);

    /* After the shell and 6 full pips, draw 1 ghost pip then 1 clipped
     * progress overlay in the same slot. */
    assert(g_drawCalls == 11);
    assert(approx_eq(g_drawSrcs[9].x,
                     STATUS_BAR_ENERGY_FULL_CELL_X + STATUS_BAR_ENERGY_PIP_SRC_LEFT_INSET +
                         STATUS_BAR_ENERGY_PIP_STRIDE * 6.0f,
                     0.001f));
    assert(approx_eq(g_drawSrcs[9].width, STATUS_BAR_ENERGY_PIP_WIDTH, 0.001f));
    assert(g_drawTints[9].a == STATUS_BAR_REGEN_GHOST_ALPHA);

    assert(approx_eq(g_drawSrcs[10].x, g_drawSrcs[9].x, 0.001f));
    assert(approx_eq(g_drawSrcs[10].width, 0.9f * STATUS_BAR_ENERGY_PIP_WIDTH, 0.001f));
    assert(approx_eq(g_drawDsts[10].width,
                     0.9f * STATUS_BAR_ENERGY_PIP_WIDTH * STATUS_BAR_BASE_DRAW_SCALE_X,
                     0.001f));
    assert(g_drawTints[10].a == STATUS_BAR_REGEN_PROGRESS_ALPHA);
}

/* The flipped P2 fill direction should charge the same next pip, but the
 * clipped fractional overlay must grow from the already-filled side. */
static void test_fractional_regen_cue_reverses_for_p2(void) {
    GameState gs = make_game_state();
    Entity base = make_base(5000, 5000);
    Camera2D camera = {0};

    gs.players[0].base = &base;
    gs.players[0].energy = 1.25f;
    gs.players[0].energyRegenRate = 1.0f;

    status_bars_draw_screen(&gs, camera, 90.0f, 270.0f, true);

    /* shell + 1 full pip + ghost + progress */
    assert(g_drawCalls == 6);

    /* Full pip lands in the reversed slot 9; the charging pip is slot 8. */
    assert(approx_eq(g_drawSrcs[3].x,
                     STATUS_BAR_ENERGY_FULL_CELL_X + STATUS_BAR_ENERGY_PIP_SRC_LEFT_INSET +
                         STATUS_BAR_ENERGY_PIP_STRIDE * 9.0f,
                     0.001f));
    assert(approx_eq(g_drawSrcs[4].x,
                     STATUS_BAR_ENERGY_FULL_CELL_X + STATUS_BAR_ENERGY_PIP_SRC_LEFT_INSET +
                         STATUS_BAR_ENERGY_PIP_STRIDE * 8.0f,
                     0.001f));
    /* Reverse clipping trims from the left edge of the authored slot. */
    assert(g_drawSrcs[5].x > g_drawSrcs[4].x);
    assert(approx_eq(g_drawSrcs[5].width, 0.25f * STATUS_BAR_ENERGY_PIP_WIDTH, 0.001f));
    assert(g_drawTints[4].a == STATUS_BAR_REGEN_GHOST_ALPHA);
    assert(g_drawTints[5].a == STATUS_BAR_REGEN_PROGRESS_ALPHA);
}

/* Per-bar label placement: health label sits at the bar center; energy
 * label is pushed OUTSIDE along the bar's length axis. */
static void test_label_placement_health_inside_energy_outside(void) {
    GameState gs = make_game_state();
    Entity base = make_base(4500, 5000);
    Camera2D camera = {0};

    gs.players[0].base = &base;
    gs.players[0].energy = 7.0f;
    gs.players[0].energyRegenRate = 1.0f;

    /* Use rotation 0 so OUTSIDE offset shows up purely in X. */
    status_bars_draw_screen(&gs, camera, 0.0f, 0.0f, false);

    /* Draw order: hp (0,1), LVL (2,3), energy (4,5), regen (6,7). */
    assert(g_textCalls == 8);

    /* Health label shadow is at (healthCenter + (1,1)); main is at
     * healthCenter. The shadow/main pair sit within 2px of each other. */
    float healthDx = g_textPositions[1].x - g_textPositions[0].x;
    float healthDy = g_textPositions[1].y - g_textPositions[0].y;
    assert(approx_eq(healthDx, -1.0f, 0.001f));
    assert(approx_eq(healthDy, -1.0f, 0.001f));

    /* Energy and health labels sit on separate bars stacked along the base's
     * head direction. At rotation 0 the head direction is -Y, so the two
     * bar centers differ in Y by (cellHeight + stackGap) = 20 + 6 = 26. */
    float energyY = g_textPositions[5].y;
    float healthY = g_textPositions[1].y;
    assert(fabsf(energyY - healthY) > 20.0f);

    /* LVL label lives on the health bar (same Y as hp label) and is pushed
     * OUTSIDE on the same authored side as the energy label (-X for P1). */
    assert(strcmp(g_textStrings[3], "LVL 1") == 0);
    float lvlDy = g_textPositions[3].y - g_textPositions[1].y;
    assert(fabsf(lvlDy) < 1.0f);
    float lvlOffsetX = g_textPositions[3].x - g_textPositions[1].x;
    assert(lvlOffsetX < -50.0f);

    /* For the non-flipped P1 path, the energy label sits on the left side of
     * the bar, so the offset is along -X. */
    float energyOffsetX = g_textPositions[5].x - g_textPositions[1].x;
    assert(energyOffsetX < -100.0f);

    /* Regen remains a secondary stacked label, but it should stay centered to
     * the energy label even though the regen string is longer. */
    assert(strcmp(g_textStrings[7], "+1.0/sec") == 0);
    float regenDy = g_textPositions[7].y - g_textPositions[5].y;
    float regenDx = g_textPositions[7].x - g_textPositions[5].x;
    assert(regenDy > 8.0f);
    assert(fabsf(regenDx) < 10.0f);
}

/* Live P1 path: bars rotate 90 and labels rotate 90, with the energy label
 * placed on the left-side/"before" end of the bar. In raw screen-space that
 * is -Y for the non-flipped viewport. */
static void test_live_p1_energy_label_uses_bar_axis_for_outside_offset(void) {
    GameState gs = make_game_state();
    Entity base = make_base(4500, 5000);
    Camera2D camera = {0};

    gs.players[0].base = &base;
    gs.players[0].energy = 7.0f;
    gs.players[0].energyRegenRate = 1.0f;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);

    Vector2 healthCenter, energyCenter;
    base_bar_centers(&base, camera, &healthCenter, &energyCenter);

    /* Energy main label is draw index 5 (hp=0,1; LVL=2,3; energy=4,5;
     * regen=6,7). */
    Vector2 energyLabel = g_textPositions[5];
    float dx = energyLabel.x - energyCenter.x;
    float dy = energyLabel.y - energyCenter.y;

    assert(approx_eq(g_textRotations[1], 90.0f, 0.001f));
    assert(approx_eq(g_textRotations[5], 90.0f, 0.001f));
    assert(fabsf(dx) < 0.01f);
    assert(dy < -100.0f);

    /* Regen becomes a stacked secondary label: same general bar end, but
     * shifted off-axis to the left so the cluster reads less crowded. */
    Vector2 regenLabel = g_textPositions[7];
    float regenDx = regenLabel.x - energyCenter.x;
    float regenDy = regenLabel.y - energyCenter.y;
    assert(approx_eq(g_textRotations[7], 90.0f, 0.001f));
    assert(regenDx < -8.0f);
    assert(regenDy < -90.0f);
}

/* Live P2 path: bars still rotate 90, but labels rotate 270. Outside offset
 * must still follow the bar axis (+Y in RT space), not the text rotation. */
static void test_live_p2_energy_label_uses_bar_axis_not_text_rotation(void) {
    GameState gs = make_game_state();
    Entity base = make_base(4500, 5000);
    Camera2D camera = {0};

    gs.players[0].base = &base;
    gs.players[0].energy = 7.0f;
    gs.players[0].energyRegenRate = 1.0f;

    status_bars_draw_screen(&gs, camera, 90.0f, 270.0f, true);

    Vector2 healthCenter, energyCenter;
    base_bar_centers(&base, camera, &healthCenter, &energyCenter);

    Vector2 healthLabel = g_textPositions[1];
    Vector2 energyLabel = g_textPositions[5]; /* hp,LVL,energy,regen — energy at 4,5 */
    float energyDx = energyLabel.x - energyCenter.x;
    float energyDy = energyLabel.y - energyCenter.y;
    float healthDx = healthLabel.x - healthCenter.x;
    float healthDy = healthLabel.y - healthCenter.y;

    assert(approx_eq(g_textRotations[1], 270.0f, 0.001f));
    assert(approx_eq(g_textRotations[5], 270.0f, 0.001f));
    assert(fabsf(healthDx) < 0.01f);
    assert(fabsf(healthDy) < 0.01f);
    assert(fabsf(energyDx) < 0.01f);
    assert(energyDy > 100.0f);

    /* Regen keeps the same overall end of the bar, but shifts right off-axis
     * so it stacks under the energy label on the flipped P2 path. */
    Vector2 regenLabel = g_textPositions[7];
    float regenDx = regenLabel.x - energyCenter.x;
    float regenDy = regenLabel.y - energyCenter.y;
    assert(approx_eq(g_textRotations[7], 270.0f, 0.001f));
    assert(regenDx > 8.0f);
    assert(regenDy > 90.0f);
}

/* The primary stat labels (LVL and energy) should keep the same bar-to-label
 * spacing on the mirrored P1 and P2 live HUD paths. Only the sign flips. */
static void test_live_primary_label_spacing_is_mirrored_between_p1_and_p2(void) {
    GameState gs = make_game_state();
    Entity base = make_base(4500, 5000);
    Camera2D camera = {0};

    gs.players[0].base = &base;
    gs.players[0].energy = 7.0f;
    gs.players[0].energyRegenRate = 1.0f;

    Vector2 healthCenter, energyCenter;
    base_bar_centers(&base, camera, &healthCenter, &energyCenter);

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);
    float p1LevelDx = g_textPositions[3].x - healthCenter.x;
    float p1LevelDy = g_textPositions[3].y - healthCenter.y;
    float p1EnergyDx = g_textPositions[5].x - energyCenter.x;
    float p1EnergyDy = g_textPositions[5].y - energyCenter.y;

    reset_draw_state();
    status_bars_draw_screen(&gs, camera, 90.0f, 270.0f, true);
    float p2LevelDx = g_textPositions[3].x - healthCenter.x;
    float p2LevelDy = g_textPositions[3].y - healthCenter.y;
    float p2EnergyDx = g_textPositions[5].x - energyCenter.x;
    float p2EnergyDy = g_textPositions[5].y - energyCenter.y;

    assert(fabsf(p1LevelDx) < 0.01f);
    assert(fabsf(p2LevelDx) < 0.01f);
    assert(fabsf(p1EnergyDx) < 0.01f);
    assert(fabsf(p2EnergyDx) < 0.01f);

    assert(p1LevelDy < 0.0f);
    assert(p2LevelDy > 0.0f);
    assert(p1EnergyDy < 0.0f);
    assert(p2EnergyDy > 0.0f);

    assert(approx_eq(fabsf(p1LevelDy), fabsf(p2LevelDy), 0.001f));
    assert(approx_eq(fabsf(p1EnergyDy), fabsf(p2EnergyDy), 0.001f));
}

/* Regen label reflects Player.energyRegenRate with %.1f/s formatting.
 * Values sampled from the actual progression curve in src/systems/
 * progression.c (level 1..10 ninths between 1.0 and 2.0). */
static void test_regen_label_values(void) {
    GameState gs = make_game_state();
    Entity base = make_base(5000, 5000);
    Camera2D camera = {0};
    gs.players[0].base = &base;
    gs.players[0].energy = 5.0f;

    /* Level 1 → +1.0/sec. */
    reset_draw_state();
    gs.players[0].energyRegenRate = 1.0f;
    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);
    assert(strcmp(g_textStrings[6], "+1.0/sec") == 0);
    assert(strcmp(g_textStrings[7], "+1.0/sec") == 0);

    /* Level 4 → 1.333.../s → +1.3/sec. */
    reset_draw_state();
    gs.players[0].energyRegenRate = 1.0f + 3.0f / 9.0f;
    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);
    assert(strcmp(g_textStrings[7], "+1.3/sec") == 0);

    /* Level 7 → 1.666.../s → +1.7/sec. */
    reset_draw_state();
    gs.players[0].energyRegenRate = 1.0f + 6.0f / 9.0f;
    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);
    assert(strcmp(g_textStrings[7], "+1.7/sec") == 0);

    /* Level 10 → +2.0/sec. */
    reset_draw_state();
    gs.players[0].energyRegenRate = 2.0f;
    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);
    assert(strcmp(g_textStrings[7], "+2.0/sec") == 0);
}

/* Atlas load failure → fallback path draws both base bars plus labels.
 * Health fallback = shell (border + bg) + 1 fill rect = 3 rects.
 * Energy fallback = shell (border + bg) + filledPips pip rects + ghost cue.
 * For 7 energy: 3 + 2 + 7 + 1 = 13 rect draws total. */
static void test_fallback_renders_bars_and_labels(void) {
    GameState gs = make_game_state();
    gs.statusBarsTexture.id = 0;
    Entity base = make_base(3420, 5000);
    Camera2D camera = {0};

    gs.players[0].base = &base;
    gs.players[0].energy = 7.0f;
    gs.players[0].energyRegenRate = 1.0f;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);

    /* No texture draws in the fallback path. */
    assert(g_drawCalls == 0);

    /* Health shell (2 rects) + health fill (1 rect) + energy shell (2 rects)
     * + 7 pip rects + ghost cue = 13. */
    assert(g_rectCalls == 13);

    /* Health shell bg: unchanged 188x20 draw footprint. */
    assert(approx_eq(g_rectDsts[1].width, STATUS_BAR_BASE_DRAW_WIDTH, 0.001f));
    assert(approx_eq(g_rectDsts[1].height, STATUS_BAR_BASE_DRAW_HEIGHT, 0.001f));

    /* Health fill rect: scaled from the 89x5 authored fill band. */
    assert(approx_eq(g_rectDsts[2].width,
                     (3420.0f / 5000.0f) * STATUS_BAR_HEALTH_FILL_SRC_WIDTH *
                         STATUS_BAR_BASE_DRAW_SCALE_X,
                     0.001f));
    assert(approx_eq(g_rectDsts[2].height,
                     STATUS_BAR_HEALTH_FILL_SRC_HEIGHT * STATUS_BAR_BASE_DRAW_SCALE_Y,
                     0.001f));

    /* Energy shell bg: unchanged 188x20 draw footprint. */
    assert(approx_eq(g_rectDsts[4].width, STATUS_BAR_BASE_DRAW_WIDTH, 0.001f));
    assert(approx_eq(g_rectDsts[4].height, STATUS_BAR_BASE_DRAW_HEIGHT, 0.001f));

    /* First energy pip rect: scaled 8x5 authored pip. */
    assert(approx_eq(g_rectDsts[5].width,
                     STATUS_BAR_ENERGY_PIP_WIDTH * STATUS_BAR_BASE_DRAW_SCALE_X,
                     0.001f));
    assert(approx_eq(g_rectDsts[5].height,
                     STATUS_BAR_ENERGY_PIP_HEIGHT * STATUS_BAR_BASE_DRAW_SCALE_Y,
                     0.001f));

    /* Fallback colors track the new authored sheet palette. */
    assert(color_eq(g_rectColors[0], BAR_BORDER_COLOR));
    assert(color_eq(g_rectColors[1], BAR_EMPTY_COLOR));
    assert(color_eq(g_rectColors[2], HP_BAR_FILL_COLOR));
    assert(color_eq(g_rectColors[3], BAR_BORDER_COLOR));
    assert(color_eq(g_rectColors[4], BAR_EMPTY_COLOR));
    assert(color_eq(g_rectColors[5], ENERGY_BAR_FILL_COLOR));
    assert(color_eq(g_rectColors[12], ENERGY_BAR_REGEN_GHOST_COLOR));

    /* Labels still render: 4 labels (hp, LVL, energy, regen) *
     * (shadow + main) = 8. */
    assert(g_textCalls == 8);
    assert(strcmp(g_textStrings[0], "3420/5000") == 0);
    assert(strcmp(g_textStrings[2], "LVL 1") == 0);
    assert(strcmp(g_textStrings[4], "7/10") == 0);
    assert(strcmp(g_textStrings[6], "+1.0/sec") == 0);
}

static void test_fallback_fractional_energy_draws_regen_cue(void) {
    GameState gs = make_game_state();
    gs.statusBarsTexture.id = 0;
    Entity base = make_base(5000, 5000);
    Camera2D camera = {0};

    gs.players[0].base = &base;
    gs.players[0].energy = 6.25f;
    gs.players[0].energyRegenRate = 1.0f;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);

    /* 3 health rects + 2 energy shell rects + 6 full pips + ghost + progress. */
    assert(g_rectCalls == 13);
    assert(color_eq(g_rectColors[11], ENERGY_BAR_REGEN_GHOST_COLOR));
    assert(color_eq(g_rectColors[12], ENERGY_BAR_REGEN_PROGRESS_COLOR));
    assert(approx_eq(g_rectDsts[12].width,
                     0.25f * STATUS_BAR_ENERGY_PIP_WIDTH * STATUS_BAR_BASE_DRAW_SCALE_X,
                     0.001f));
}

/* Zero-energy fallback draws only the shell (2 rects): no pip rects. */
static void test_fallback_zero_energy_draws_shell_only(void) {
    GameState gs = make_game_state();
    gs.statusBarsTexture.id = 0;
    Entity base = make_base(5000, 5000);
    Camera2D camera = {0};

    gs.players[0].base = &base;
    gs.players[0].energy = 0.0f;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);

    /* Health: border + bg + fill (178 wide) = 3. Energy: border + bg = 2.
     * Total = 5. */
    assert(g_rectCalls == 5);
}

/* Flipped P2 viewport draws filled pips in reverse raw RT order so the final
 * vertically flipped composite reads left-to-right on screen. */
static void test_reversed_energy_fill_direction_for_flipped_viewport(void) {
    GameState gs = make_game_state();
    Entity base = make_base(5000, 5000);
    Camera2D camera = {0};

    gs.players[0].base = &base;
    gs.players[0].energy = 1.0f;

    status_bars_draw_screen(&gs, camera, 90.0f, 90.0f, false);
    float forwardShellY = g_drawDsts[2].y;
    float forwardPipY = g_drawDsts[3].y;

    reset_draw_state();
    status_bars_draw_screen(&gs, camera, 90.0f, 270.0f, true);
    float reversedShellY = g_drawDsts[2].y;
    float reversedPipY = g_drawDsts[3].y;

    assert(approx_eq(forwardShellY, reversedShellY, 0.001f));
    assert(forwardPipY < forwardShellY);
    assert(reversedPipY > reversedShellY);
}

int main(void) {
    printf("Running status bar tests...\n");

    RUN_TEST(test_full_health_troop_frame_hidden);
    RUN_TEST(test_damaged_troop_frame_visible);
    RUN_TEST(test_status_bars_draw_skips_undamaged_troops);
    RUN_TEST(test_status_bars_draws_damaged_troop_bar);
    RUN_TEST(test_status_bars_reverses_troop_bar_for_flipped_viewport);
    RUN_TEST(test_damaged_troop_uses_fallback_when_troop_texture_missing);
    RUN_TEST(test_fallback_troop_bar_reverses_for_flipped_viewport);
    RUN_TEST(test_base_bars_health_fill_and_energy_pips);
    RUN_TEST(test_base_bar_full_health_and_energy);
    RUN_TEST(test_base_bar_near_empty_health_draws_tiny_fill);
    RUN_TEST(test_base_bar_granularity_moves_on_small_hit);
    RUN_TEST(test_base_bar_centers_stay_fixed_when_base_anim_changes);
    RUN_TEST(test_base_bar_draw_positions_stay_fixed_when_base_attacks);
    RUN_TEST(test_fractional_energy_shows_whole_pips);
    RUN_TEST(test_fractional_energy_draws_regen_ghost_and_progress);
    RUN_TEST(test_fractional_regen_cue_reverses_for_p2);
    RUN_TEST(test_label_placement_health_inside_energy_outside);
    RUN_TEST(test_live_p1_energy_label_uses_bar_axis_for_outside_offset);
    RUN_TEST(test_live_p2_energy_label_uses_bar_axis_not_text_rotation);
    RUN_TEST(test_live_primary_label_spacing_is_mirrored_between_p1_and_p2);
    RUN_TEST(test_regen_label_values);
    RUN_TEST(test_fallback_renders_bars_and_labels);
    RUN_TEST(test_fallback_fractional_energy_draws_regen_cue);
    RUN_TEST(test_fallback_zero_energy_draws_shell_only);
    RUN_TEST(test_reversed_energy_fill_direction_for_flipped_viewport);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return 0;
}
