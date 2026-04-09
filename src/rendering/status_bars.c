//
// World-anchored, screen-aligned status bars for troops and bases.
//

#include "status_bars.h"
#include "sprite_renderer.h"
#include "../core/config.h"
#include <math.h>
#include <stdio.h>

#define STATUS_BAR_ROW_HEIGHT             16.0f
#define STATUS_BAR_CELL_STRIDE           144.0f
#define STATUS_BAR_FRAME_X_OFFSET          3.0f
#define STATUS_BAR_TROOP_WIDTH            42.0f
#define STATUS_BAR_BASE_SRC_WIDTH        138.0f
#define STATUS_BAR_BASE_DRAW_WIDTH       180.0f
#define STATUS_BAR_BASE_DRAW_HEIGHT       24.0f
#define STATUS_BAR_TROOP_FRAMES           20
#define STATUS_BAR_BASE_FRAMES            24
#define STATUS_BAR_TROOP_ROW_Y             0.0f
#define STATUS_BAR_BASE_HEALTH_ROW_Y      16.0f
#define STATUS_BAR_BASE_ENERGY_ROW_Y      32.0f
#define STATUS_BAR_TOP_GAP                 6.0f
#define STATUS_BAR_BASE_TOP_GAP            8.0f
#define STATUS_BAR_BASE_STACK_GAP          6.0f

// Label placement mode for base-bar numeric labels.
// INSIDE  — overlay label centered on the bar (compact, may be cramped).
// OUTSIDE — place label past the far end of the bar in its length direction.
// Toggle and rebuild to compare during playtest.
#define STATUS_BAR_LABEL_MODE_INSIDE  0
#define STATUS_BAR_LABEL_MODE_OUTSIDE 1
#define STATUS_BAR_LABEL_MODE         STATUS_BAR_LABEL_MODE_INSIDE

#define STATUS_BAR_LABEL_SPACING           1.0f
#define STATUS_BAR_LABEL_GAP               2.0f
#define STATUS_BAR_BASE_LABEL_FONT_SIZE   16.0f

// Colors matched to health_energy_bars.png by pixel sampling at load time.
// Used directly by the fallback renderer when the atlas fails to load, and as
// a source-of-truth for any future code that needs to match the atlas palette.
static const Color HP_BAR_FILL_COLOR     = { 255,  10,  18, 255 };
static const Color ENERGY_BAR_FILL_COLOR = { 255, 157,  24, 255 };
static const Color BAR_EMPTY_COLOR       = {  53,  53,  53, 255 };
static const Color BAR_BORDER_COLOR      = {   0,   0,   0, 255 };

static float clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static Rectangle status_bar_src(int frameIndex, float rowY, float width) {
    return (Rectangle){
        STATUS_BAR_FRAME_X_OFFSET + STATUS_BAR_CELL_STRIDE * (float)frameIndex,
        rowY,
        width,
        STATUS_BAR_ROW_HEIGHT
    };
}

static int troop_health_frame(const Entity *troop) {
    if (!troop || troop->hp <= 0 || troop->maxHP <= 0) return -1;

    // Troop bars stay hidden until the troop is missing health.
    if (troop->hp >= troop->maxHP) return -1;

    float ratio = clamp01((float)troop->hp / (float)troop->maxHP);

    int frame = (int)floorf((1.0f - ratio) * (float)STATUS_BAR_TROOP_FRAMES);
    if (frame < 0) frame = 0;
    if (frame >= STATUS_BAR_TROOP_FRAMES) frame = STATUS_BAR_TROOP_FRAMES - 1;
    return frame;
}

static Rectangle entity_visible_world_bounds(const Entity *e) {
    if (!e) return (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};

    if (e->sprite) {
        Rectangle visible = sprite_visible_bounds(e->sprite, &e->anim,
                                                  e->position, e->spriteScale,
                                                  e->spriteRotationDegrees);
        if (visible.width > 0.0f && visible.height > 0.0f) {
            return visible;
        }

        const SpriteSheet *sheet = sprite_sheet_get(e->sprite, e->anim.anim);
        if (sheet && sheet->frameWidth > 0 && sheet->frameHeight > 0) {
            float width = (float)sheet->frameWidth * e->spriteScale;
            float height = (float)sheet->frameHeight * e->spriteScale;
            return (Rectangle){
                e->position.x - width * 0.5f,
                e->position.y - height * 0.5f,
                width,
                height
            };
        }
    }

    return (Rectangle){e->position.x, e->position.y, 0.0f, 0.0f};
}

static Rectangle entity_stable_visible_world_bounds(const Entity *e) {
    if (!e || !e->sprite) {
        return entity_visible_world_bounds(e);
    }

    const SpriteSheet *sheet = sprite_sheet_get(e->sprite, e->anim.anim);
    if (!sheet || sheet->frameWidth <= 0 || sheet->frameHeight <= 0) {
        return entity_visible_world_bounds(e);
    }

    float rad = e->spriteRotationDegrees * (PI_F / 180.0f);
    float cosA = cosf(rad);
    float sinA = sinf(rad);
    float fw = (float)sheet->frameWidth * e->spriteScale;
    float fh = (float)sheet->frameHeight * e->spriteScale;
    float cx = fw * 0.5f;
    float cy = fh * 0.5f;
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
    bool foundVisible = false;

    for (int dir = 0; dir < DIR_COUNT; dir++) {
        for (int frame = 0; frame < sheet->frameCount; frame++) {
            Rectangle bounds;
            if (sheet->visibleBounds) {
                bounds = sheet->visibleBounds[dir * sheet->frameCount + frame];
                if (bounds.width <= 0.0f || bounds.height <= 0.0f) continue;
            } else {
                bounds = (Rectangle){0.0f, 0.0f, (float)sheet->frameWidth, (float)sheet->frameHeight};
            }

            float vx = bounds.x * e->spriteScale;
            float vy = bounds.y * e->spriteScale;
            float vw = bounds.width * e->spriteScale;
            float vh = bounds.height * e->spriteScale;
            if (e->anim.flipH) {
                vx = fw - (vx + vw);
            }

            float corners[4][2] = {
                { vx - cx,      vy - cy },
                { vx + vw - cx, vy - cy },
                { vx + vw - cx, vy + vh - cy },
                { vx - cx,      vy + vh - cy },
            };

            for (int i = 0; i < 4; i++) {
                float rx = corners[i][0] * cosA - corners[i][1] * sinA;
                float ry = corners[i][0] * sinA + corners[i][1] * cosA;
                float wx = e->position.x + rx;
                float wy = e->position.y + ry;
                if (!foundVisible || wx < minX) minX = wx;
                if (!foundVisible || wy < minY) minY = wy;
                if (!foundVisible || wx > maxX) maxX = wx;
                if (!foundVisible || wy > maxY) maxY = wy;
                foundVisible = true;
            }
        }
    }

    if (!foundVisible) {
        return entity_visible_world_bounds(e);
    }

    return (Rectangle){ minX, minY, maxX - minX, maxY - minY };
}

static Rectangle world_rect_to_screen(Rectangle worldRect, Camera2D camera) {
    Vector2 corners[4] = {
        {worldRect.x, worldRect.y},
        {worldRect.x + worldRect.width, worldRect.y},
        {worldRect.x + worldRect.width, worldRect.y + worldRect.height},
        {worldRect.x, worldRect.y + worldRect.height}
    };

    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    for (int i = 0; i < 4; i++) {
        Vector2 screen = GetWorldToScreen2D(corners[i], camera);
        if (i == 0 || screen.x < minX) minX = screen.x;
        if (i == 0 || screen.y < minY) minY = screen.y;
        if (i == 0 || screen.x > maxX) maxX = screen.x;
        if (i == 0 || screen.y > maxY) maxY = screen.y;
    }

    return (Rectangle){minX, minY, maxX - minX, maxY - minY};
}

static Vector2 entity_screen_head_direction(const Entity *e, Camera2D camera) {
    if (!e) return (Vector2){0.0f, -1.0f};

    float rad = e->spriteRotationDegrees * (PI_F / 180.0f);
    Vector2 worldHeadDir = { sinf(rad), -cosf(rad) };
    Vector2 center = GetWorldToScreen2D(e->position, camera);
    Vector2 head = GetWorldToScreen2D(
        (Vector2){
            e->position.x + worldHeadDir.x * 32.0f,
            e->position.y + worldHeadDir.y * 32.0f
        },
        camera
    );
    Vector2 dir = { head.x - center.x, head.y - center.y };
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (len <= 0.001f) {
        return (Vector2){0.0f, -1.0f};
    }
    return (Vector2){ dir.x / len, dir.y / len };
}

static Vector2 screen_anchor_from_bounds(Rectangle screenBounds, const Entity *e,
                                         Camera2D camera, Vector2 headDirection) {
    Vector2 center = {
        screenBounds.x + screenBounds.width * 0.5f,
        screenBounds.y + screenBounds.height * 0.5f
    };

    if (screenBounds.width <= 0.0f || screenBounds.height <= 0.0f) {
        Vector2 fallback = GetWorldToScreen2D(e->position, camera);
        float fallbackGap = 40.0f;
        return (Vector2){
            fallback.x + headDirection.x * fallbackGap,
            fallback.y + headDirection.y * fallbackGap
        };
    }

    if (fabsf(headDirection.x) >= fabsf(headDirection.y)) {
        center.x = (headDirection.x >= 0.0f)
            ? (screenBounds.x + screenBounds.width)
            : screenBounds.x;
    } else {
        center.y = (headDirection.y >= 0.0f)
            ? (screenBounds.y + screenBounds.height)
            : screenBounds.y;
    }

    return center;
}

static Vector2 entity_screen_anchor(const Entity *e, Camera2D camera, Vector2 headDirection) {
    Rectangle screenBounds = world_rect_to_screen(entity_visible_world_bounds(e), camera);
    return screen_anchor_from_bounds(screenBounds, e, camera, headDirection);
}

static void draw_status_bar(Texture2D texture, Rectangle src, Vector2 screenCenter,
                            float dstWidth, float dstHeight,
                            float rotationDegrees) {
    if (texture.id == 0) return;

    Rectangle dst = {
        screenCenter.x,
        screenCenter.y,
        dstWidth,
        dstHeight
    };
    Vector2 origin = { dst.width * 0.5f, dst.height * 0.5f };

    DrawTexturePro(texture, src, dst, origin, rotationDegrees, WHITE);
}

static void draw_troop_health_bar(const GameState *gs, const Entity *troop,
                                  Camera2D camera,
                                  float rotationDegrees) {
    int frame = troop_health_frame(troop);
    if (frame < 0) return;

    Vector2 headDirection = entity_screen_head_direction(troop, camera);
    Rectangle troopScreenBounds = world_rect_to_screen(
        entity_stable_visible_world_bounds(troop), camera
    );
    Vector2 anchor = screen_anchor_from_bounds(
        troopScreenBounds, troop, camera, headDirection
    );
    Rectangle src = status_bar_src(frame, STATUS_BAR_TROOP_ROW_Y, STATUS_BAR_TROOP_WIDTH);
    Vector2 center = {
        anchor.x + headDirection.x * (STATUS_BAR_TOP_GAP + src.height * 0.5f),
        anchor.y + headDirection.y * (STATUS_BAR_TOP_GAP + src.height * 0.5f)
    };

    draw_status_bar(gs->statusBarsTexture, src, center,
                    src.width, src.height, rotationDegrees);
}

// Continuous base-bar fill via a two-frame blend.
//
// The atlas was pre-built by rendering 24 discrete cut points over a fill
// interior that is flat-shaded in the full frame (frame 0) and flat gray in
// the empty frame (frame N-1). The top/bottom shell rows and the left/right
// border columns are provably identical across every frame (verified by
// pixel sampling at planning time), so cutting frame 0 on the left and
// frame N-1 on the right at any integer pixel boundary produces the same
// visual output as a hypothetical frame with that exact fill level.
//
// Granularity is 1 atlas pixel: for a 138-pixel-wide bar that is 139
// levels (vs the previous 24), which is effectively continuous for this
// bar size.
static void draw_base_bar_continuous(Texture2D texture, float rowY, float ratio,
                                     Vector2 screenCenter, float rotationDegrees) {
    if (texture.id == 0) return;

    float clamped = clamp01(ratio);
    int fillPixels = (int)roundf(clamped * STATUS_BAR_BASE_SRC_WIDTH);
    if (fillPixels < 0) fillPixels = 0;
    if (fillPixels > (int)STATUS_BAR_BASE_SRC_WIDTH) fillPixels = (int)STATUS_BAR_BASE_SRC_WIDTH;

    // Degenerate: single frame (avoids zero-width DrawTexturePro calls).
    if (fillPixels == 0) {
        Rectangle src = status_bar_src(STATUS_BAR_BASE_FRAMES - 1, rowY,
                                       STATUS_BAR_BASE_SRC_WIDTH);
        draw_status_bar(texture, src, screenCenter,
                        STATUS_BAR_BASE_DRAW_WIDTH, STATUS_BAR_BASE_DRAW_HEIGHT,
                        rotationDegrees);
        return;
    }
    if (fillPixels == (int)STATUS_BAR_BASE_SRC_WIDTH) {
        Rectangle src = status_bar_src(0, rowY, STATUS_BAR_BASE_SRC_WIDTH);
        draw_status_bar(texture, src, screenCenter,
                        STATUS_BAR_BASE_DRAW_WIDTH, STATUS_BAR_BASE_DRAW_HEIGHT,
                        rotationDegrees);
        return;
    }

    // Blended case: draw two halves, each rotating around its own center.
    // Offsets are along the bar's local +X axis, which in screen space is
    // (cos(rot), sin(rot)).
    float rad = rotationDegrees * (PI_F / 180.0f);
    float cosA = cosf(rad);
    float sinA = sinf(rad);

    float fillRatio = (float)fillPixels / STATUS_BAR_BASE_SRC_WIDTH;
    float leftWidth = STATUS_BAR_BASE_DRAW_WIDTH * fillRatio;
    float rightWidth = STATUS_BAR_BASE_DRAW_WIDTH - leftWidth;
    float leftOffset  = (leftWidth - STATUS_BAR_BASE_DRAW_WIDTH) * 0.5f;
    float rightOffset = leftWidth * 0.5f;

    // Left half: frame 0, source columns [0, fillPixels).
    Rectangle leftSrc = {
        STATUS_BAR_FRAME_X_OFFSET,
        rowY,
        (float)fillPixels,
        STATUS_BAR_ROW_HEIGHT
    };
    Rectangle leftDst = {
        screenCenter.x + cosA * leftOffset,
        screenCenter.y + sinA * leftOffset,
        leftWidth,
        STATUS_BAR_BASE_DRAW_HEIGHT
    };
    Vector2 leftOrigin = { leftWidth * 0.5f, STATUS_BAR_BASE_DRAW_HEIGHT * 0.5f };
    DrawTexturePro(texture, leftSrc, leftDst, leftOrigin, rotationDegrees, WHITE);

    // Right half: frame N-1, source columns [fillPixels, BASE_WIDTH).
    Rectangle rightSrc = {
        STATUS_BAR_FRAME_X_OFFSET +
            STATUS_BAR_CELL_STRIDE * (float)(STATUS_BAR_BASE_FRAMES - 1) +
            (float)fillPixels,
        rowY,
        STATUS_BAR_BASE_SRC_WIDTH - (float)fillPixels,
        STATUS_BAR_ROW_HEIGHT
    };
    Rectangle rightDst = {
        screenCenter.x + cosA * rightOffset,
        screenCenter.y + sinA * rightOffset,
        rightWidth,
        STATUS_BAR_BASE_DRAW_HEIGHT
    };
    Vector2 rightOrigin = { rightWidth * 0.5f, STATUS_BAR_BASE_DRAW_HEIGHT * 0.5f };
    DrawTexturePro(texture, rightSrc, rightDst, rightOrigin, rotationDegrees, WHITE);
}

// Draws a numeric label anchored to a bar center, rotated to match the bar.
// Uses the default Raylib font plus a one-pixel black drop shadow so the
// label stays readable over both the red/orange fill and the dark empty
// region of the bar.
static void draw_bar_numeric_label(const char *text, Vector2 barCenter,
                                   float barWidth, float fontSize,
                                   float rotationDegrees) {
    if (!text || text[0] == '\0') return;
    (void)barWidth;

    Font font = GetFontDefault();
    Vector2 textSize = MeasureTextEx(font, text, fontSize, STATUS_BAR_LABEL_SPACING);

    Vector2 center = barCenter;

#if STATUS_BAR_LABEL_MODE == STATUS_BAR_LABEL_MODE_OUTSIDE
    // Push the label past the far end of the bar in its length direction.
    // In the rotated player view this reads as "to the side of" the bar.
    float rad = rotationDegrees * (PI_F / 180.0f);
    float cosA = cosf(rad);
    float sinA = sinf(rad);
    float outsideOffset =
        barWidth * 0.5f + STATUS_BAR_LABEL_GAP + textSize.x * 0.5f;
    center.x += cosA * outsideOffset;
    center.y += sinA * outsideOffset;
#endif

    Vector2 origin = { textSize.x * 0.5f, textSize.y * 0.5f };

    // Drop shadow (+1,+1 in screen space) then main text.
    DrawTextPro(font, text,
                (Vector2){ center.x + 1.0f, center.y + 1.0f },
                origin, rotationDegrees,
                fontSize,
                STATUS_BAR_LABEL_SPACING, BLACK);
    DrawTextPro(font, text, center, origin, rotationDegrees,
                fontSize,
                STATUS_BAR_LABEL_SPACING, WHITE);
}

// Fallback bar rendering used when the atlas fails to load. Matches the
// atlas palette and preserves continuous fill, so missing-texture bars are
// still trustworthy gameplay feedback.
static void draw_bar_rect_fallback(Vector2 screenCenter, float barWidth,
                                   float barHeight,
                                   float ratio, Color fillColor,
                                   float rotationDegrees) {
    // Border — draw a slightly oversized black rect underneath the bar.
    {
        Rectangle borderDst = {
            screenCenter.x, screenCenter.y,
            barWidth + 2.0f, barHeight + 2.0f
        };
        Vector2 borderOrigin = {
            borderDst.width * 0.5f, borderDst.height * 0.5f
        };
        DrawRectanglePro(borderDst, borderOrigin, rotationDegrees,
                         BAR_BORDER_COLOR);
    }

    // Empty interior.
    {
        Rectangle bgDst = {
            screenCenter.x, screenCenter.y, barWidth, barHeight
        };
        Vector2 bgOrigin = { bgDst.width * 0.5f, bgDst.height * 0.5f };
        DrawRectanglePro(bgDst, bgOrigin, rotationDegrees, BAR_EMPTY_COLOR);
    }

    // Partial fill, offset along the bar's length direction.
    float clamped = clamp01(ratio);
    int fillPixels = (int)roundf(clamped * barWidth);
    if (fillPixels <= 0) return;
    if (fillPixels > (int)barWidth) fillPixels = (int)barWidth;

    float rad = rotationDegrees * (PI_F / 180.0f);
    float cosA = cosf(rad);
    float sinA = sinf(rad);
    float leftOffset = ((float)fillPixels - barWidth) * 0.5f;

    Rectangle fillDst = {
        screenCenter.x + cosA * leftOffset,
        screenCenter.y + sinA * leftOffset,
        (float)fillPixels,
        barHeight
    };
    Vector2 fillOrigin = { fillDst.width * 0.5f, fillDst.height * 0.5f };
    DrawRectanglePro(fillDst, fillOrigin, rotationDegrees, fillColor);
}

static void draw_troop_health_bar_fallback(const Entity *troop, Camera2D camera,
                                           float rotationDegrees) {
    if (!troop || troop->hp <= 0 || troop->maxHP <= 0) return;
    // Match the atlas renderer: no troop bar at full HP.
    if (troop->hp >= troop->maxHP) return;

    Vector2 headDirection = entity_screen_head_direction(troop, camera);
    Rectangle troopScreenBounds = world_rect_to_screen(
        entity_stable_visible_world_bounds(troop), camera
    );
    Vector2 anchor = screen_anchor_from_bounds(
        troopScreenBounds, troop, camera, headDirection
    );
    float offset = STATUS_BAR_TOP_GAP + STATUS_BAR_ROW_HEIGHT * 0.5f;
    Vector2 center = {
        anchor.x + headDirection.x * offset,
        anchor.y + headDirection.y * offset
    };

    float ratio = (float)troop->hp / (float)troop->maxHP;
    draw_bar_rect_fallback(center, STATUS_BAR_TROOP_WIDTH, STATUS_BAR_ROW_HEIGHT, ratio,
                           HP_BAR_FILL_COLOR, rotationDegrees);
}

// Compute the two stacked base-bar centers for a given base entity.
// Shared by the atlas and fallback renderers so placement stays in sync.
static void base_bar_centers(const Entity *base, Camera2D camera,
                             Vector2 *outHealth, Vector2 *outEnergy) {
    Vector2 headDirection = entity_screen_head_direction(base, camera);
    Rectangle baseScreenBounds = world_rect_to_screen(
        entity_stable_visible_world_bounds(base), camera
    );
    Vector2 anchor;
    if (baseScreenBounds.width <= 0.0f || baseScreenBounds.height <= 0.0f) {
        anchor = entity_screen_anchor(base, camera, headDirection);
    } else {
        anchor = screen_anchor_from_bounds(baseScreenBounds, base, camera, headDirection);
    }

    float firstOffset  = STATUS_BAR_BASE_TOP_GAP + STATUS_BAR_BASE_DRAW_HEIGHT * 0.5f;
    float secondOffset = STATUS_BAR_BASE_TOP_GAP + STATUS_BAR_BASE_DRAW_HEIGHT +
                         STATUS_BAR_BASE_STACK_GAP + STATUS_BAR_BASE_DRAW_HEIGHT * 0.5f;
    outHealth->x = anchor.x + headDirection.x * firstOffset;
    outHealth->y = anchor.y + headDirection.y * firstOffset;
    outEnergy->x = anchor.x + headDirection.x * secondOffset;
    outEnergy->y = anchor.y + headDirection.y * secondOffset;
}

// Format an integer "cur/max" label into the caller's buffer.
static void format_bar_label(char *buf, size_t bufSize, int cur, int max) {
    if (cur < 0) cur = 0;
    if (max < 0) max = 0;
    snprintf(buf, bufSize, "%d/%d", cur, max);
}

static void draw_base_bars(const GameState *gs, const Entity *base, const Player *owner,
                           Camera2D camera,
                           float rotationDegrees,
                           float labelRotationDegrees) {
    if (!base || !owner) return;

    Vector2 healthCenter, energyCenter;
    base_bar_centers(base, camera, &healthCenter, &energyCenter);

    float hpRatio = (base->maxHP > 0)
        ? ((float)base->hp / (float)base->maxHP)
        : 0.0f;
    float energyRatio = (owner->maxEnergy > 0.0f)
        ? (owner->energy / owner->maxEnergy)
        : 0.0f;

    draw_base_bar_continuous(gs->statusBarsTexture, STATUS_BAR_BASE_HEALTH_ROW_Y,
                             hpRatio, healthCenter, rotationDegrees);
    draw_base_bar_continuous(gs->statusBarsTexture, STATUS_BAR_BASE_ENERGY_ROW_Y,
                             energyRatio, energyCenter, rotationDegrees);

    char hpLabel[32];
    char energyLabel[32];
    format_bar_label(hpLabel, sizeof(hpLabel), base->hp, base->maxHP);
    format_bar_label(energyLabel, sizeof(energyLabel),
                     (int)roundf(owner->energy),
                     (int)roundf(owner->maxEnergy));

    draw_bar_numeric_label(hpLabel, healthCenter, STATUS_BAR_BASE_DRAW_WIDTH,
                           STATUS_BAR_BASE_LABEL_FONT_SIZE, labelRotationDegrees);
    draw_bar_numeric_label(energyLabel, energyCenter, STATUS_BAR_BASE_DRAW_WIDTH,
                           STATUS_BAR_BASE_LABEL_FONT_SIZE, labelRotationDegrees);
}

static void draw_base_bars_fallback(const Entity *base, const Player *owner,
                                    Camera2D camera, float rotationDegrees,
                                    float labelRotationDegrees) {
    if (!base || !owner) return;

    Vector2 healthCenter, energyCenter;
    base_bar_centers(base, camera, &healthCenter, &energyCenter);

    float hpRatio = (base->maxHP > 0)
        ? ((float)base->hp / (float)base->maxHP)
        : 0.0f;
    float energyRatio = (owner->maxEnergy > 0.0f)
        ? (owner->energy / owner->maxEnergy)
        : 0.0f;

    draw_bar_rect_fallback(healthCenter, STATUS_BAR_BASE_DRAW_WIDTH,
                           STATUS_BAR_BASE_DRAW_HEIGHT, hpRatio,
                           HP_BAR_FILL_COLOR, rotationDegrees);
    draw_bar_rect_fallback(energyCenter, STATUS_BAR_BASE_DRAW_WIDTH,
                           STATUS_BAR_BASE_DRAW_HEIGHT, energyRatio,
                           ENERGY_BAR_FILL_COLOR, rotationDegrees);

    char hpLabel[32];
    char energyLabel[32];
    format_bar_label(hpLabel, sizeof(hpLabel), base->hp, base->maxHP);
    format_bar_label(energyLabel, sizeof(energyLabel),
                     (int)roundf(owner->energy),
                     (int)roundf(owner->maxEnergy));

    draw_bar_numeric_label(hpLabel, healthCenter, STATUS_BAR_BASE_DRAW_WIDTH,
                           STATUS_BAR_BASE_LABEL_FONT_SIZE, labelRotationDegrees);
    draw_bar_numeric_label(energyLabel, energyCenter, STATUS_BAR_BASE_DRAW_WIDTH,
                           STATUS_BAR_BASE_LABEL_FONT_SIZE, labelRotationDegrees);
}

Texture2D status_bars_load(void) {
    Texture2D texture = LoadTexture(STATUS_BARS_PATH);
    if (texture.id == 0) {
        printf("[STATUS_BARS] Failed to load %s\n", STATUS_BARS_PATH);
        return texture;
    }

    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    return texture;
}

void status_bars_unload(Texture2D texture) {
    if (texture.id > 0) {
        UnloadTexture(texture);
    }
}

void status_bars_draw_screen(const GameState *gs, Camera2D camera,
                             float rotationDegrees,
                             float labelRotationDegrees) {
    if (!gs) return;

    bool hasTexture = (gs->statusBarsTexture.id != 0);
    const Battlefield *bf = &gs->battlefield;

    for (int i = 0; i < bf->entityCount; i++) {
        const Entity *e = bf->entities[i];
        if (!e || e->markedForRemoval || e->type != ENTITY_TROOP) continue;
        if (!e->alive || e->hp <= 0) continue;
        if (hasTexture) {
            draw_troop_health_bar(gs, e, camera, rotationDegrees);
        } else {
            draw_troop_health_bar_fallback(e, camera, rotationDegrees);
        }
    }

    for (int i = 0; i < 2; i++) {
        const Player *player = &gs->players[i];
        const Entity *base = player->base;
        if (!base || base->markedForRemoval) continue;
        if (hasTexture) {
            draw_base_bars(gs, base, player, camera,
                           rotationDegrees, labelRotationDegrees);
        } else {
            draw_base_bars_fallback(base, player, camera,
                                    rotationDegrees, labelRotationDegrees);
        }
    }
}
