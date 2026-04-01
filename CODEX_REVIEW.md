# Codex Review: Viewport Seam Clipping Bug

## Bug Summary

In a split-screen 2D game (Raylib, C), entities crossing from one player's territory into the other's are eventually rendered in the opponent's viewport. As those crossed entities walk deeper into enemy territory, they approach the **center screen seam** (x=960) from within the opponent's viewport and get scissor-clipped. The sprite is abruptly cut off at x=960.

Two prior fix attempts have been made:
1. **Expanded scissor** — widened the scissor past x=960 during entity drawing. Fixed the clipping but introduced a **mirrored sprite** artifact: the bleed pixels were rendered with the wrong camera rotation (180° off), making the sprite appear flipped in the bleed zone.
2. **Center-seam bleed pass** — re-draws the bleed portion using the other player's camera with adjusted direction (DIR_UP↔DIR_DOWN, flipH toggled). The direction adjustment formulas may be incorrect, or there may be a subtlety with how Raylib's camera rotation interacts with sprite sheet row selection.

The bug is still not fully resolved. The task for Codex: figure out the correct approach and implement it.

## Game Architecture

This is a Clash Royale-style NFC card game. Two players sit on opposite sides of a table. The screen (1920x1080, landscape) is split vertically into two 960x1080 halves. Each player's **world space** is 1080 wide x 960 tall, and a `Camera2D` rotation of +90 / -90 degrees maps it onto their screen half so it appears right-side-up from their seating position.

Troops spawn at each player's base and walk toward the enemy. When they cross the border (world y < 0), they appear in the opponent's viewport via a coordinate mapping function.

## Screen Layout & Camera Math

```
Screen: 1920 x 1080

+------ 960 ------+------ 960 ------+
|                  |                  |
|   Player 1       |   Player 2       |
|   Viewport       |   Viewport       |
|                  |                  |
|  Camera rot: 90  | Camera rot: -90  |
|                  |                  |
|  Scissor:        |  Scissor:        |
|  x=0,w=960      |  x=960,w=960     |
|                  |                  |
+------ base ------+------ base ------+
         ^--- center seam (x=960) ---^

CRITICAL INSIGHT — the center seam is at the BASE/SPAWN end:

P1 camera (rot=+90°, target=(540,480), offset=(480,540)):
  world y=960 → screen x=960  (center seam — spawn/base end)
  world y=0   → screen x=0    (left screen edge — enemy border)

P2 camera (rot=-90°, target=(1500,480), offset=(1440,540)):
  world y=960 → screen x=960  (center seam — spawn/base end)
  world y=0   → screen x=1920 (right screen edge — enemy border)

Entities walk from HIGH Y (spawn, near center seam) toward LOW Y (enemy).
On screen, they walk AWAY from the center seam toward the outer edges.

When entities cross into enemy territory (y < 0), they are drawn in the
opponent's viewport via game_map_crossed_world_point(). In the opponent's
viewport, they walk FROM the outer screen edge TOWARD the center seam.
The clipping happens when they REACH the center seam from within the
opponent's viewport.
```

## Camera Transform Formulas

```
P1 camera (rot=90°):
  screen_x = world_y                    (world Y maps directly to screen X)
  screen_y = 1080 - world_x

P2 camera (rot=-90°):
  screen_x = 1920 - world_y             (world Y maps inversely to screen X)
  screen_y = world_x - 960

Position matching between cameras (same screen pixel from different world coords):
  p1_world_y = 1920 - p2_world_y
  p1_world_x = 2040 - p2_world_x
  (These only produce identical screen pixels at y=960, i.e., exactly at the seam)
```

## The Clipping Problem in Detail

When a P1 entity crosses into P2's territory and walks deep enough:

```
P1 entity at owner position (540, -900):
  → In P1's viewport: screen x = -900 (off-screen left, harmless)
  → Mapped to P2 space: (1500, 900) via game_map_crossed_world_point
  → In P2's viewport: screen x = 1920 - 900 = 1020
  → Sprite extends from screen x = 1020-79 = 941 to 1020+79 = 1099
  → P2 scissor clips at x=960, so screen x 941..959 is LOST (19px clipped)

P1 entity at owner position (540, -940):
  → Mapped to P2 space: (1500, 940)
  → P2 screen x = 1920 - 940 = 980
  → Sprite extends: 980-79 = 901 to 980+79 = 1059
  → Clipped portion: 901..959 (59px lost — significant visual artifact)
```

The clipped portion falls in P1's screen area (x < 960). To show it, we must draw those pixels using P1's camera. But P1's camera has +90° rotation while P2's has -90° — a 180° difference. A sprite drawn with the wrong camera rotation appears mirrored/flipped.

## Why This Is Hard

The fundamental constraint: the two cameras rotate the world in opposite directions. A sprite straddling the center seam at x=960 cannot look correct for both cameras simultaneously. Any approach must handle the rotation difference.

## Approaches Considered

| Approach | Status | Problem |
|----------|--------|---------|
| Expanded scissor (no camera change) | Tried | Sprite direction is mirrored in bleed zone — wrong camera rotation |
| Center-seam bleed with direction swap | Tried | Direction adjustment (UP↔DOWN, flipH toggle) may be incorrect; hard to validate without visual testing |
| RenderTexture per viewport | Not tried | Render entities to texture (captures correct rotation), composite to screen. Clean but adds GPU resources and init/cleanup code |
| Screen-space entity drawing | Not tried | Compute screen position, draw sprite directly without camera transform. Requires manual rotation math for each sprite |
| Accept limitation | Possible | The canonical world-space refactor (in progress) eliminates this problem permanently. The seam clipping could be accepted as a known limitation for now |

## Current Code

### src/core/game.c — Coordinate Mapping

```c
// Maps an entity position from owner's world space to opponent's world space
static Vector2 game_map_crossed_world_point(
        const Player *owner, const Player *opponent, Vector2 worldPos) {
    float lateral = (worldPos.x - owner->playArea.x) / owner->playArea.width;
    float mirroredLateral = 1.0f - lateral;
    float depth = owner->playArea.y - worldPos.y;
    return (Vector2){
        opponent->playArea.x + mirroredLateral * opponent->playArea.width,
        opponent->playArea.y + depth
    };
}

static void game_apply_crossed_direction(const Entity *e, const Player *owner,
                                         const Player *opponent, AnimState *crossed) {
    if (e->lane < 0 || e->lane >= 3) return;
    if (e->waypointIndex < 0 || e->waypointIndex >= LANE_WAYPOINT_COUNT) return;

    Vector2 mappedPos = game_map_crossed_world_point(owner, opponent, e->position);
    Vector2 target = owner->laneWaypoints[e->lane][e->waypointIndex];
    Vector2 mappedTarget = game_map_crossed_world_point(owner, opponent, target);
    Vector2 diff = {
        mappedTarget.x - mappedPos.x,
        mappedTarget.y - mappedPos.y
    };

    pathfind_apply_direction(crossed, diff);
}
```

### src/core/game.c — Entity Drawing (Pass 1)

```c
static void game_draw_entities_for_viewport(GameState *g, const Player *viewportPlayer) {
    for (int pid = 0; pid < 2; pid++) {
        const Player *owner = &g->players[pid];
        const Player *opponent = &g->players[1 - pid];

        for (int i = 0; i < owner->entityCount; i++) {
            const Entity *e = owner->entities[i];
            float spriteHalfH = (SPRITE_FRAME_SIZE * e->spriteScale) * 0.5f;
            float seamBorder = owner->playArea.y;

            if (viewportPlayer == owner) {
                entity_draw(e);
            } else {
                bool crossed = (e->position.y < seamBorder);
                bool spriteOverlapsSeam = (e->position.y - spriteHalfH < seamBorder) &&
                                          (e->position.y + spriteHalfH > seamBorder);

                if (crossed || spriteOverlapsSeam) {
                    Vector2 mappedPos = game_map_crossed_world_point(owner, opponent, e->position);
                    AnimState crossedAnim = e->anim;
                    game_apply_crossed_direction(e, owner, opponent, &crossedAnim);
                    sprite_draw(e->sprite, &crossedAnim, mappedPos, e->spriteScale);
                }
            }
        }
    }
}
```

### src/core/game.c — Center-Seam Bleed (Pass 2, current attempt)

```c
static void game_draw_center_seam_bleed(GameState *g) {
    int bleed = (int)((SPRITE_FRAME_SIZE * 2.0f) * 0.5f) + 1;

    for (int vp = 0; vp < 2; vp++) {
        const Player *viewportPlayer = &g->players[vp];
        const Player *otherPlayer    = &g->players[1 - vp];

        int bleedX, bleedW;
        if (vp == 0) {
            bleedX = (int)otherPlayer->screenArea.x;       // 960
            bleedW = bleed;                                 // 80
        } else {
            bleedX = (int)viewportPlayer->screenArea.x - bleed; // 880
            bleedW = bleed;
        }

        BeginScissorMode(bleedX, 0, bleedW, SCREEN_HEIGHT);
        BeginMode2D(otherPlayer->camera);

        for (int pid = 0; pid < 2; pid++) {
            const Player *owner    = &g->players[pid];
            const Player *opponent = &g->players[1 - pid];

            if (viewportPlayer == owner) continue;

            for (int i = 0; i < owner->entityCount; i++) {
                const Entity *e = owner->entities[i];
                if (e->position.y >= owner->playArea.y) continue;

                float spriteHalfH = (SPRITE_FRAME_SIZE * e->spriteScale) * 0.5f;
                Vector2 mappedPos = game_map_crossed_world_point(owner, opponent, e->position);
                float centerSeamY = viewportPlayer->playArea.y + viewportPlayer->playArea.height;

                if (mappedPos.y + spriteHalfH <= centerSeamY) continue;

                // Position in otherPlayer's world that maps to the same screen pixel
                Vector2 bleedPos = {
                    (float)(SCREEN_HEIGHT + g->halfWidth) - mappedPos.x,
                    (float)SCREEN_WIDTH - mappedPos.y
                };

                // Crossed animation, then adjust for 180° camera rotation difference
                AnimState bleedAnim = e->anim;
                game_apply_crossed_direction(e, owner, opponent, &bleedAnim);
                if (bleedAnim.dir == DIR_UP)        bleedAnim.dir = DIR_DOWN;
                else if (bleedAnim.dir == DIR_DOWN)  bleedAnim.dir = DIR_UP;
                bleedAnim.flipH = !bleedAnim.flipH;

                sprite_draw(e->sprite, &bleedAnim, bleedPos, e->spriteScale);
            }
        }

        EndMode2D();
        EndScissorMode();
    }
}
```

### src/core/game.c — Render Pipeline

```c
void game_render(GameState *g) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Pass 1: Tilemaps, text, and entities with tight scissors
    viewport_begin(&g->players[0]);
    viewport_draw_tilemap(&g->players[0]);
    game_draw_entities_for_viewport(g, &g->players[0]);
    DrawText("PLAYER 1", ...);
    viewport_end();

    viewport_begin(&g->players[1]);
    viewport_draw_tilemap(&g->players[1]);
    game_draw_entities_for_viewport(g, &g->players[1]);
    DrawText("PLAYER 2", ...);
    viewport_end();

    // Pass 2: Center-seam bleed for crossed entities
    game_draw_center_seam_bleed(g);

    // HUD, debug overlay...
    EndDrawing();
}
```

### src/rendering/viewport.c — Viewport Setup

```c
void viewport_init_split_screen(GameState *gs) {
    gs->halfWidth = SCREEN_WIDTH / 2;  // 960

    // Player 1: Left half of screen, rotated 90 degrees
    Rectangle p1PlayArea = { .x = 0, .y = 0, .width = SCREEN_HEIGHT, .height = gs->halfWidth };
    // = { 0, 0, 1080, 960 }
    Rectangle p1ScreenArea = { .x = 0, .y = 0, .width = gs->halfWidth, .height = SCREEN_HEIGHT };
    // = { 0, 0, 960, 1080 }

    // Player 2: Right half of screen, rotated -90 degrees
    Rectangle p2PlayArea = { .x = gs->halfWidth, .y = 0, .width = SCREEN_HEIGHT, .height = gs->halfWidth };
    // = { 960, 0, 1080, 960 }
    Rectangle p2ScreenArea = { .x = gs->halfWidth, .y = 0, .width = gs->halfWidth, .height = SCREEN_HEIGHT };
    // = { 960, 0, 960, 1080 }

    player_init(&gs->players[0], 0, p1PlayArea, p1ScreenArea, 90.0f, ...);
    player_init(&gs->players[1], 1, p2PlayArea, p2ScreenArea, -90.0f, ...);
}

void viewport_begin(Player *p) {
    BeginScissorMode(
        (int) p->screenArea.x, (int) p->screenArea.y,
        (int) p->screenArea.width, (int) p->screenArea.height
    );
    BeginMode2D(p->camera);
}

void viewport_end(void) {
    EndMode2D();
    EndScissorMode();
}
```

### src/systems/player.c — Camera Setup

```c
void player_init(Player *p, int id, Rectangle playArea, Rectangle screenArea,
                 float cameraRotation, ...) {
    p->camera = (Camera2D){ 0 };
    p->camera.target = rect_center(playArea);
    // P1: (540, 480), P2: (1500, 480)
    p->camera.offset = (Vector2){
        screenArea.x + screenArea.width / 2.0f,
        screenArea.y + screenArea.height / 2.0f
    };
    // P1: (480, 540), P2: (1440, 540)
    p->camera.rotation = cameraRotation;  // P1: 90, P2: -90
    p->camera.zoom = 1.0f;
}
```

### src/rendering/sprite_renderer.c — Sprite Drawing

```c
void sprite_draw(const CharacterSprite *cs, const AnimState *state,
                 Vector2 pos, float scale) {
    const SpriteSheet *sheet = &cs->anims[state->anim];
    if (sheet->texture.id == 0) return;

    int col = state->frame % sheet->frameCount;
    int row = state->dir;     // DIR_SIDE=0, DIR_DOWN=1, DIR_UP=2

    float fw = (float) sheet->frameWidth;   // 79
    float fh = (float) sheet->frameHeight;  // 79

    Rectangle src = {
        (float)(col * sheet->frameWidth),
        (float)(row * sheet->frameHeight),
        state->flipH ? -fw : fw,    // negative width = horizontal flip
        fh
    };

    float dw = fw * scale;  // 79 * 2.0 = 158
    float dh = fh * scale;  // 79 * 2.0 = 158

    Rectangle dst = { pos.x, pos.y, dw, dh };
    Vector2 origin = { dw / 2.0f, dh / 2.0f };  // centered on position

    DrawTexturePro(sheet->texture, src, dst, origin, 0.0f, WHITE);
    // rotation=0.0f — sprite is NOT rotated; camera rotation handles screen transform
}
```

### src/rendering/sprite_renderer.h — Direction Enum

```c
typedef enum {
    DIR_SIDE, // Row 0: side-facing (right by default; flipH for left)
    DIR_DOWN, // Row 1: front-facing (character faces camera)
    DIR_UP,   // Row 2: back-facing (character faces away)
    DIR_COUNT
} SpriteDirection;
```

### src/core/types.h — Key Structs

```c
struct Player {
    int id;                          // 0 or 1
    Rectangle playArea;              // World space: P1={0,0,1080,960}, P2={960,0,1080,960}
    Rectangle screenArea;            // Screen space: P1={0,0,960,1080}, P2={960,0,960,1080}
    Camera2D camera;
    float cameraRotation;            // 90 or -90

    TileMap tilemap;
    Entity *entities[MAX_ENTITIES];
    int entityCount;
    Vector2 laneWaypoints[3][LANE_WAYPOINT_COUNT];
};

struct GameState {
    Player players[2];
    int halfWidth;     // 960 — half of SCREEN_WIDTH
    // ... other fields
};
```

### src/core/config.h — Constants

```c
#define SCREEN_WIDTH  1920
#define SCREEN_HEIGHT 1080
#define SPRITE_FRAME_SIZE 79
```

## Review Request

The center-seam bleed approach (Pass 2) is the most promising but the sprite direction adjustment is wrong or incomplete. The visual result is a mirrored/flipped sprite in the bleed zone.

Please:
1. Verify the position-matching formula: `bleedPos = (2040 - mapped_x, 1920 - mapped_y)` — does this produce the correct screen pixel alignment between the two cameras?
2. Verify the direction adjustment: swapping DIR_UP↔DIR_DOWN and toggling flipH — does this correctly compensate for the 180° camera rotation difference? Work through a concrete example with a specific sprite direction.
3. If the bleed approach is fundamentally flawed, propose an alternative (RenderTexture, screen-space drawing, etc.) with concrete Raylib API calls.
4. Implement the fix in `game_draw_center_seam_bleed()` or replace it entirely.

## Constraints

- C language, Raylib rendering library
- No game engine — all rendering is manual Raylib calls
- The dual-camera split-screen setup (90°/-90° rotation) cannot change
- A full canonical world-space refactor is in progress (Phase 11) that will eliminate this problem permanently. This fix is an interim solution.
- Keep changes minimal — only modify `src/core/game.c`
