//
// World-anchored, screen-aligned status bars for troops and bases.
//

#include "status_bars.h"
#include "sprite_renderer.h"
#include "../core/config.h"
#include <math.h>
#include <stdio.h>

#define STATUS_BAR_TROOP_DRAW_WIDTH       42.0f
#define STATUS_BAR_TROOP_DRAW_HEIGHT      16.0f
#define STATUS_BAR_TOP_GAP                 6.0f
#define STATUS_BAR_BASE_TOP_GAP            8.0f
#define STATUS_BAR_BASE_STACK_GAP          6.0f
#define TROOP_HEALTH_BAR_FRAME_X_OFFSET    1.0f
#define TROOP_HEALTH_BAR_CELL_STRIDE      14.0f
#define TROOP_HEALTH_BAR_SRC_WIDTH        13.0f
#define TROOP_HEALTH_BAR_SRC_HEIGHT        5.0f
#define TROOP_HEALTH_BAR_FRAMES             8

// Base-bar atlas layout: 2x2 grid of 92x9 source cells on a 185x18 sheet.
// Column 0 = empty shell, column 1 = full shell. Row 0 = health, row 1 = energy.
// Keep the previous on-screen footprint by drawing these smaller source cells
// into the existing 188x20 destination shell size.
#define STATUS_BAR_TEXTURE_WIDTH             185
#define STATUS_BAR_TEXTURE_HEIGHT             18
#define STATUS_BAR_BASE_SRC_CELL_WIDTH      92.0f
#define STATUS_BAR_BASE_SRC_CELL_HEIGHT      9.0f
#define STATUS_BAR_BASE_DRAW_WIDTH         188.0f
#define STATUS_BAR_BASE_DRAW_HEIGHT         20.0f
#define STATUS_BAR_HEALTH_EMPTY_CELL_X       0.0f
#define STATUS_BAR_HEALTH_EMPTY_CELL_Y       0.0f
#define STATUS_BAR_HEALTH_FULL_CELL_X       93.0f
#define STATUS_BAR_HEALTH_FULL_CELL_Y        0.0f
#define STATUS_BAR_ENERGY_EMPTY_CELL_X       0.0f
#define STATUS_BAR_ENERGY_EMPTY_CELL_Y       9.0f
#define STATUS_BAR_ENERGY_FULL_CELL_X       93.0f
#define STATUS_BAR_ENERGY_FULL_CELL_Y        9.0f

// Empty health shell interior within the 92x9 cell: x=2..90, y=2..6.
// Full-cell fill source band is shifted left by 1px: x=1..89, y=2..6.
#define STATUS_BAR_HEALTH_EMPTY_INTERIOR_LEFT_INSET  2.0f
#define STATUS_BAR_HEALTH_EMPTY_INTERIOR_TOP_INSET   2.0f
#define STATUS_BAR_HEALTH_FILL_SRC_LEFT_INSET        1.0f
#define STATUS_BAR_HEALTH_FILL_SRC_TOP_INSET         2.0f
#define STATUS_BAR_HEALTH_FILL_SRC_WIDTH            89.0f
#define STATUS_BAR_HEALTH_FILL_SRC_HEIGHT            5.0f

// Energy pip geometry within each 92x9 cell: 10 pips, 8x5 each, 9px stride.
// Empty-shell pip windows start at local x=2; full-state pip interiors are
// shifted left by 1px and start at local x=1.
#define STATUS_BAR_ENERGY_PIP_COUNT           10
#define STATUS_BAR_ENERGY_EMPTY_PIP_LEFT_INSET  2.0f
#define STATUS_BAR_ENERGY_PIP_SRC_LEFT_INSET    1.0f
#define STATUS_BAR_ENERGY_PIP_WIDTH             8.0f
#define STATUS_BAR_ENERGY_PIP_HEIGHT            5.0f
#define STATUS_BAR_ENERGY_PIP_STRIDE            9.0f
#define STATUS_BAR_ENERGY_PIP_TOP_INSET         2.0f
#define STATUS_BAR_BASE_DRAW_SCALE_X \
    (STATUS_BAR_BASE_DRAW_WIDTH / STATUS_BAR_BASE_SRC_CELL_WIDTH)
#define STATUS_BAR_BASE_DRAW_SCALE_Y \
    (STATUS_BAR_BASE_DRAW_HEIGHT / STATUS_BAR_BASE_SRC_CELL_HEIGHT)

#define STATUS_BAR_LABEL_SPACING           1.0f
#define STATUS_BAR_LABEL_GAP               2.0f
#define STATUS_BAR_BASE_LABEL_FONT_SIZE   16.0f
#define STATUS_BAR_REGEN_LABEL_FONT_SIZE  12.0f
#define STATUS_BAR_REGEN_LABEL_STACK_OFFSET 12.0f
#define STATUS_BAR_REGEN_GHOST_ALPHA       72
#define STATUS_BAR_REGEN_PROGRESS_ALPHA   176

typedef enum {
    LABEL_INSIDE,
    LABEL_OUTSIDE
} LabelPlacement;

// Colors sampled from health_energy_bars_sheet.png. The fallback renderer is
// intentionally flat-shaded, so these are representative shell/fill tones from
// the authored art rather than every highlight/shadow color in the sheet.
static const Color HP_BAR_FILL_COLOR     = { 232,  74,  66, 255 };
static const Color ENERGY_BAR_FILL_COLOR = { 255, 197,  75, 255 };
static const Color BAR_EMPTY_COLOR       = {  44,  27,  43, 255 };
static const Color BAR_BORDER_COLOR      = {  34,  12,  33, 255 };
static const Color ENERGY_BAR_REGEN_GHOST_TINT =
    { 255, 255, 255, STATUS_BAR_REGEN_GHOST_ALPHA };
static const Color ENERGY_BAR_REGEN_PROGRESS_TINT =
    { 255, 255, 255, STATUS_BAR_REGEN_PROGRESS_ALPHA };
static const Color ENERGY_BAR_REGEN_GHOST_COLOR =
    { 255, 197,  75, STATUS_BAR_REGEN_GHOST_ALPHA };
static const Color ENERGY_BAR_REGEN_PROGRESS_COLOR =
    { 255, 197,  75, STATUS_BAR_REGEN_PROGRESS_ALPHA };

static float clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static Rectangle troop_health_bar_src(int frameIndex,
                                      bool reverseFillDirection) {
    float x = TROOP_HEALTH_BAR_FRAME_X_OFFSET +
              TROOP_HEALTH_BAR_CELL_STRIDE * (float)frameIndex;
    float width = TROOP_HEALTH_BAR_SRC_WIDTH;
    if (reverseFillDirection) {
        width = -width;
    }

    return (Rectangle){
        x,
        0.0f,
        width,
        TROOP_HEALTH_BAR_SRC_HEIGHT
    };
}

static int troop_health_frame(const Entity *troop) {
    if (!troop || troop->hp <= 0 || troop->maxHP <= 0) return -1;

    // Troop bars stay hidden until the troop is missing health.
    if (troop->hp >= troop->maxHP) return -1;

    float ratio = clamp01((float)troop->hp / (float)troop->maxHP);
    int frame = (int)ceilf(ratio * (float)(TROOP_HEALTH_BAR_FRAMES - 1));
    if (frame < 1) frame = 1;
    if (frame >= TROOP_HEALTH_BAR_FRAMES) frame = TROOP_HEALTH_BAR_FRAMES - 1;
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
                                  float rotationDegrees,
                                  bool reverseFillDirection) {
    int frame = troop_health_frame(troop);
    if (frame < 0) return;

    Vector2 headDirection = entity_screen_head_direction(troop, camera);
    Rectangle troopScreenBounds = world_rect_to_screen(
        entity_stable_visible_world_bounds(troop), camera
    );
    Vector2 anchor = screen_anchor_from_bounds(
        troopScreenBounds, troop, camera, headDirection
    );
    Rectangle src = troop_health_bar_src(frame, reverseFillDirection);
    Vector2 center = {
        anchor.x + headDirection.x * (STATUS_BAR_TOP_GAP + STATUS_BAR_TROOP_DRAW_HEIGHT * 0.5f),
        anchor.y + headDirection.y * (STATUS_BAR_TOP_GAP + STATUS_BAR_TROOP_DRAW_HEIGHT * 0.5f)
    };

    draw_status_bar(gs->troopHealthBarTexture, src, center,
                    STATUS_BAR_TROOP_DRAW_WIDTH,
                    STATUS_BAR_TROOP_DRAW_HEIGHT,
                    rotationDegrees);
}

// Draw a sub-rect overlay at a local (dx, dy) offset from a rotated bar center.
// Local +X is the bar's length axis; +Y is its thickness axis, both measured
// in bar-local (pre-rotation) pixels.
static Rectangle base_overlay_dst_rect(Vector2 screenCenter, float dx, float dy,
                                       float dstWidth, float dstHeight,
                                       float rotationDegrees) {
    float rad = rotationDegrees * (PI_F / 180.0f);
    float cosA = cosf(rad);
    float sinA = sinf(rad);

    return (Rectangle){
        screenCenter.x + dx * cosA - dy * sinA,
        screenCenter.y + dx * sinA + dy * cosA,
        dstWidth,
        dstHeight
    };
}

static void draw_base_overlay_tinted(Texture2D texture, Rectangle src,
                                     Vector2 screenCenter, float dx, float dy,
                                     float dstWidth, float dstHeight,
                                     float rotationDegrees, Color tint) {
    if (texture.id == 0) return;

    Rectangle dst = base_overlay_dst_rect(screenCenter, dx, dy,
                                          dstWidth, dstHeight,
                                          rotationDegrees);
    Vector2 origin = { dstWidth * 0.5f, dstHeight * 0.5f };
    DrawTexturePro(texture, src, dst, origin, rotationDegrees, tint);
}

static void draw_base_overlay(Texture2D texture, Rectangle src,
                              Vector2 screenCenter, float dx, float dy,
                              float dstWidth, float dstHeight,
                              float rotationDegrees) {
    draw_base_overlay_tinted(texture, src, screenCenter, dx, dy,
                             dstWidth, dstHeight, rotationDegrees, WHITE);
}

// Quantize a continuous energy value into whole-pip units. Partial-pip fills
// are out of scope; energy displays as 0..10 discrete segments matching the
// authored pip geometry on the atlas.
static int base_energy_filled_pips(float energy) {
    if (energy <= 0.0f) return 0;
    int pips = (int)floorf(energy);
    if (pips < 0) pips = 0;
    if (pips > STATUS_BAR_ENERGY_PIP_COUNT) pips = STATUS_BAR_ENERGY_PIP_COUNT;
    return pips;
}

static bool energy_regen_cue_visible(float energy, float maxEnergy,
                                     float energyRegenRate) {
    return energy < maxEnergy &&
           energyRegenRate > 0.0f &&
           base_energy_filled_pips(energy) < STATUS_BAR_ENERGY_PIP_COUNT;
}

static float energy_regen_cue_progress(float energy) {
    return clamp01(energy - floorf(energy));
}

static int energy_regen_cue_slot_index(float energy, bool reverseFillDirection) {
    int filled = base_energy_filled_pips(energy);
    return reverseFillDirection
        ? (STATUS_BAR_ENERGY_PIP_COUNT - 1 - filled)
        : filled;
}

static Rectangle energy_pip_src_rect(int slotIndex) {
    return (Rectangle){
        STATUS_BAR_ENERGY_FULL_CELL_X + STATUS_BAR_ENERGY_PIP_SRC_LEFT_INSET
            + STATUS_BAR_ENERGY_PIP_STRIDE * (float)slotIndex,
        STATUS_BAR_ENERGY_FULL_CELL_Y + STATUS_BAR_ENERGY_PIP_TOP_INSET,
        STATUS_BAR_ENERGY_PIP_WIDTH,
        STATUS_BAR_ENERGY_PIP_HEIGHT
    };
}

static float energy_pip_center_dx(int slotIndex, float pipWidthDst) {
    return -(STATUS_BAR_BASE_DRAW_WIDTH * 0.5f
             - STATUS_BAR_ENERGY_EMPTY_PIP_LEFT_INSET * STATUS_BAR_BASE_DRAW_SCALE_X)
           + pipWidthDst * 0.5f
           + STATUS_BAR_ENERGY_PIP_STRIDE * STATUS_BAR_BASE_DRAW_SCALE_X
                 * (float)slotIndex;
}

// Health bar: empty shell + continuous fill overlay sourced from the full-shell
// interior band. The authored fill band is only 89 source pixels wide, so draw
// it proportionally into the larger on-screen destination footprint.
static void draw_base_health_continuous(Texture2D texture, float ratio,
                                        Vector2 screenCenter, float rotationDegrees) {
    if (texture.id == 0) return;

    Rectangle shellSrc = {
        STATUS_BAR_HEALTH_EMPTY_CELL_X, STATUS_BAR_HEALTH_EMPTY_CELL_Y,
        STATUS_BAR_BASE_SRC_CELL_WIDTH, STATUS_BAR_BASE_SRC_CELL_HEIGHT
    };
    draw_status_bar(texture, shellSrc, screenCenter,
                    STATUS_BAR_BASE_DRAW_WIDTH, STATUS_BAR_BASE_DRAW_HEIGHT,
                    rotationDegrees);

    float clamped = clamp01(ratio);
    float fillWidthSrc = clamped * STATUS_BAR_HEALTH_FILL_SRC_WIDTH;
    if (fillWidthSrc <= 0.0f) return;

    Rectangle fillSrc = {
        STATUS_BAR_HEALTH_FULL_CELL_X + STATUS_BAR_HEALTH_FILL_SRC_LEFT_INSET,
        STATUS_BAR_HEALTH_FULL_CELL_Y + STATUS_BAR_HEALTH_FILL_SRC_TOP_INSET,
        fillWidthSrc,
        STATUS_BAR_HEALTH_FILL_SRC_HEIGHT
    };
    float fillWidthDst = fillWidthSrc * STATUS_BAR_BASE_DRAW_SCALE_X;
    float fillHeightDst = STATUS_BAR_HEALTH_FILL_SRC_HEIGHT * STATUS_BAR_BASE_DRAW_SCALE_Y;
    float dx = -(STATUS_BAR_BASE_DRAW_WIDTH * 0.5f
                 - STATUS_BAR_HEALTH_EMPTY_INTERIOR_LEFT_INSET * STATUS_BAR_BASE_DRAW_SCALE_X)
               + fillWidthDst * 0.5f;
    float dy = STATUS_BAR_HEALTH_EMPTY_INTERIOR_TOP_INSET * STATUS_BAR_BASE_DRAW_SCALE_Y
               + fillHeightDst * 0.5f
               - STATUS_BAR_BASE_DRAW_HEIGHT * 0.5f;
    draw_base_overlay(texture, fillSrc, screenCenter, dx, dy,
                      fillWidthDst, fillHeightDst,
                      rotationDegrees);
}

// Energy bar: empty shell + one full-pip overlay per filled unit, sampled from
// the authored pip geometry so the 9 separators are preserved and never
// painted over.
static void draw_base_energy_pips(Texture2D texture, float energy,
                                  float maxEnergy, float energyRegenRate,
                                  Vector2 screenCenter, float rotationDegrees,
                                  bool reverseFillDirection) {
    if (texture.id == 0) return;

    Rectangle shellSrc = {
        STATUS_BAR_ENERGY_EMPTY_CELL_X, STATUS_BAR_ENERGY_EMPTY_CELL_Y,
        STATUS_BAR_BASE_SRC_CELL_WIDTH, STATUS_BAR_BASE_SRC_CELL_HEIGHT
    };
    draw_status_bar(texture, shellSrc, screenCenter,
                    STATUS_BAR_BASE_DRAW_WIDTH, STATUS_BAR_BASE_DRAW_HEIGHT,
                    rotationDegrees);

    int filled = base_energy_filled_pips(energy);
    float pipWidthDst = STATUS_BAR_ENERGY_PIP_WIDTH * STATUS_BAR_BASE_DRAW_SCALE_X;
    float pipHeightDst = STATUS_BAR_ENERGY_PIP_HEIGHT * STATUS_BAR_BASE_DRAW_SCALE_Y;
    float dyPip = STATUS_BAR_ENERGY_PIP_TOP_INSET * STATUS_BAR_BASE_DRAW_SCALE_Y
                  + pipHeightDst * 0.5f
                  - STATUS_BAR_BASE_DRAW_HEIGHT * 0.5f;
    for (int i = 0; i < filled; i++) {
        int slotIndex = reverseFillDirection
            ? (STATUS_BAR_ENERGY_PIP_COUNT - 1 - i)
            : i;
        Rectangle pipSrc = energy_pip_src_rect(slotIndex);
        float dx = energy_pip_center_dx(slotIndex, pipWidthDst);
        draw_base_overlay(texture, pipSrc, screenCenter, dx, dyPip,
                          pipWidthDst,
                          pipHeightDst,
                          rotationDegrees);
    }

    if (!energy_regen_cue_visible(energy, maxEnergy, energyRegenRate)) return;

    int regenSlotIndex = energy_regen_cue_slot_index(energy, reverseFillDirection);
    Rectangle regenSrc = energy_pip_src_rect(regenSlotIndex);
    float regenDx = energy_pip_center_dx(regenSlotIndex, pipWidthDst);
    draw_base_overlay_tinted(texture, regenSrc, screenCenter, regenDx, dyPip,
                             pipWidthDst, pipHeightDst, rotationDegrees,
                             ENERGY_BAR_REGEN_GHOST_TINT);

    float progress = energy_regen_cue_progress(energy);
    if (progress <= 0.0f) return;

    Rectangle progressSrc = regenSrc;
    float progressWidthSrc = progressSrc.width * progress;
    float progressWidthDst = pipWidthDst * progress;
    if (reverseFillDirection) {
        progressSrc.x += progressSrc.width - progressWidthSrc;
    }
    progressSrc.width = progressWidthSrc;
    float progressDx = regenDx +
        (reverseFillDirection ? 1.0f : -1.0f) * (pipWidthDst - progressWidthDst) * 0.5f;
    draw_base_overlay_tinted(texture, progressSrc, screenCenter, progressDx, dyPip,
                             progressWidthDst, pipHeightDst, rotationDegrees,
                             ENERGY_BAR_REGEN_PROGRESS_TINT);
}

// Draws a numeric label anchored to a bar center, rotated to match the bar.
// Uses the default Raylib font plus a one-pixel black drop shadow so the
// label stays readable over both the red/orange fill and the dark empty
// region of the bar.
static void draw_bar_numeric_label(const char *text, Vector2 barCenter,
                                   float barWidth, float fontSize,
                                   float textRotationDegrees,
                                   float axisRotationDegrees,
                                   LabelPlacement placement,
                                   float outsideDirectionSign,
                                   float extraAxialOffset,
                                   float extraNormalOffset) {
    if (!text || text[0] == '\0') return;

    Font font = GetFontDefault();
    Vector2 textSize = MeasureTextEx(font, text, fontSize, STATUS_BAR_LABEL_SPACING);

    Vector2 center = barCenter;

    if (placement == LABEL_OUTSIDE) {
        // Push the label past the far end of the bar in its length direction.
        // Keep the offset aligned to the bar's axis, even when the glyphs use a
        // different rotation (P2 labels are rotated 270 while bars are drawn 90).
        float rad = axisRotationDegrees * (PI_F / 180.0f);
        float cosA = cosf(rad);
        float sinA = sinf(rad);
        float outsideOffset =
            barWidth * 0.5f + STATUS_BAR_LABEL_GAP + textSize.x * 0.5f
            + extraAxialOffset;
        center.x += cosA * outsideOffset * outsideDirectionSign;
        center.y += sinA * outsideOffset * outsideDirectionSign;

        // Optional offset in the screen-space normal of the bar axis. Useful
        // for stacking secondary labels near the same bar end without turning
        // them into one long crowded run.
        center.x += -sinA * extraNormalOffset;
        center.y +=  cosA * extraNormalOffset;
    }

    Vector2 origin = { textSize.x * 0.5f, textSize.y * 0.5f };

    // Drop shadow (+1,+1 in screen space) then main text.
    DrawTextPro(font, text,
                (Vector2){ center.x + 1.0f, center.y + 1.0f },
                origin, textRotationDegrees,
                fontSize,
                STATUS_BAR_LABEL_SPACING, BLACK);
    DrawTextPro(font, text, center, origin, textRotationDegrees,
                fontSize,
                STATUS_BAR_LABEL_SPACING, WHITE);
}

// Fallback bar rendering used when the atlas fails to load. Matches the
// atlas palette and preserves continuous fill, so missing-texture bars are
// still trustworthy gameplay feedback.
static void draw_bar_rect_fallback(Vector2 screenCenter, float barWidth,
                                   float barHeight,
                                   float ratio, Color fillColor,
                                   float rotationDegrees,
                                   bool reverseFillDirection) {
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
    float leftOffset = reverseFillDirection
        ? (barWidth - (float)fillPixels) * 0.5f
        : ((float)fillPixels - barWidth) * 0.5f;

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
                                           float rotationDegrees,
                                           bool reverseFillDirection) {
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
    float offset = STATUS_BAR_TOP_GAP + STATUS_BAR_TROOP_DRAW_HEIGHT * 0.5f;
    Vector2 center = {
        anchor.x + headDirection.x * offset,
        anchor.y + headDirection.y * offset
    };

    float ratio = (float)troop->hp / (float)troop->maxHP;
    draw_bar_rect_fallback(center, STATUS_BAR_TROOP_DRAW_WIDTH,
                           STATUS_BAR_TROOP_DRAW_HEIGHT, ratio,
                           HP_BAR_FILL_COLOR, rotationDegrees,
                           reverseFillDirection);
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

static void format_regen_label(char *buf, size_t bufSize, float rate) {
    if (rate < 0.0f) rate = 0.0f;
    snprintf(buf, bufSize, "+%.1f/sec", rate);
}

static float stacked_label_axial_offset(const char *anchorText, float anchorFontSize,
                                        const char *stackedText, float stackedFontSize) {
    Font font = GetFontDefault();
    Vector2 anchorSize = MeasureTextEx(font, anchorText, anchorFontSize,
                                       STATUS_BAR_LABEL_SPACING);
    Vector2 stackedSize = MeasureTextEx(font, stackedText, stackedFontSize,
                                        STATUS_BAR_LABEL_SPACING);
    return (anchorSize.x - stackedSize.x) * 0.5f;
}

static float regen_label_normal_offset(float labelRotationDegrees) {
    int rot = ((int)lroundf(labelRotationDegrees)) % 360;
    if (rot < 0) rot += 360;
    return (rot == 270)
        ? -STATUS_BAR_REGEN_LABEL_STACK_OFFSET
        :  STATUS_BAR_REGEN_LABEL_STACK_OFFSET;
}

static void draw_base_bars(const GameState *gs, const Entity *base, const Player *owner,
                           Camera2D camera,
                           float rotationDegrees,
                           float labelRotationDegrees,
                           bool reverseFillDirection) {
    if (!base || !owner) return;

    Vector2 healthCenter, energyCenter;
    base_bar_centers(base, camera, &healthCenter, &energyCenter);

    float hpRatio = (base->maxHP > 0)
        ? ((float)base->hp / (float)base->maxHP)
        : 0.0f;

    draw_base_health_continuous(gs->statusBarsTexture, hpRatio,
                                healthCenter, rotationDegrees);
    draw_base_energy_pips(gs->statusBarsTexture, owner->energy,
                          owner->maxEnergy, owner->energyRegenRate,
                          energyCenter, rotationDegrees,
                          reverseFillDirection);

    char hpLabel[32];
    char energyLabel[32];
    char levelLabel[32];
    char regenLabel[32];
    int filledPips = base_energy_filled_pips(owner->energy);
    // The top player's viewport is vertically flipped during composite, so its
    // raw RT-space outside offset stays positive while the unflipped P1 path
    // needs the opposite sign to land on the same authored side on screen.
    float energyLabelDirection = reverseFillDirection ? 1.0f : -1.0f;
    int displayLevel = (base->baseLevel > 0) ? base->baseLevel : 1;
    format_bar_label(hpLabel, sizeof(hpLabel), base->hp, base->maxHP);
    format_bar_label(energyLabel, sizeof(energyLabel),
                     filledPips, STATUS_BAR_ENERGY_PIP_COUNT);
    snprintf(levelLabel, sizeof(levelLabel), "LVL %d", displayLevel);
    format_regen_label(regenLabel, sizeof(regenLabel), owner->energyRegenRate);
    float regenNormalOffset = regen_label_normal_offset(labelRotationDegrees);
    float regenAxialOffset = stacked_label_axial_offset(
        energyLabel, STATUS_BAR_BASE_LABEL_FONT_SIZE,
        regenLabel, STATUS_BAR_REGEN_LABEL_FONT_SIZE
    );

    draw_bar_numeric_label(hpLabel, healthCenter, STATUS_BAR_BASE_DRAW_WIDTH,
                           STATUS_BAR_BASE_LABEL_FONT_SIZE,
                           labelRotationDegrees, rotationDegrees, LABEL_INSIDE,
                           1.0f, 0.0f, 0.0f);
    draw_bar_numeric_label(levelLabel, healthCenter, STATUS_BAR_BASE_DRAW_WIDTH,
                           STATUS_BAR_BASE_LABEL_FONT_SIZE,
                           labelRotationDegrees, rotationDegrees, LABEL_OUTSIDE,
                           energyLabelDirection, 0.0f, 0.0f);
    draw_bar_numeric_label(energyLabel, energyCenter, STATUS_BAR_BASE_DRAW_WIDTH,
                           STATUS_BAR_BASE_LABEL_FONT_SIZE,
                           labelRotationDegrees, rotationDegrees, LABEL_OUTSIDE,
                           energyLabelDirection, 0.0f, 0.0f);
    draw_bar_numeric_label(regenLabel, energyCenter, STATUS_BAR_BASE_DRAW_WIDTH,
                           STATUS_BAR_REGEN_LABEL_FONT_SIZE,
                           labelRotationDegrees, rotationDegrees, LABEL_OUTSIDE,
                           energyLabelDirection, regenAxialOffset,
                           regenNormalOffset);
}

// Draw the base-bar shell border + empty interior used by both fallback bars.
// Matches the atlas shell's on-screen footprint so fallback placement stays in
// sync with atlas placement.
static void draw_base_fallback_shell(Vector2 screenCenter, float rotationDegrees) {
    Rectangle borderDst = {
        screenCenter.x, screenCenter.y,
        STATUS_BAR_BASE_DRAW_WIDTH + 2.0f, STATUS_BAR_BASE_DRAW_HEIGHT + 2.0f
    };
    Vector2 borderOrigin = { borderDst.width * 0.5f, borderDst.height * 0.5f };
    DrawRectanglePro(borderDst, borderOrigin, rotationDegrees, BAR_BORDER_COLOR);

    Rectangle bgDst = {
        screenCenter.x, screenCenter.y,
        STATUS_BAR_BASE_DRAW_WIDTH, STATUS_BAR_BASE_DRAW_HEIGHT
    };
    Vector2 bgOrigin = { bgDst.width * 0.5f, bgDst.height * 0.5f };
    DrawRectanglePro(bgDst, bgOrigin, rotationDegrees, BAR_EMPTY_COLOR);
}

// Fallback health: shell + continuous fill scaled to the drawn shell size.
static void draw_base_health_fallback(Vector2 screenCenter, float ratio,
                                      float rotationDegrees) {
    draw_base_fallback_shell(screenCenter, rotationDegrees);

    float clamped = clamp01(ratio);
    float fillWidthDst =
        clamped * STATUS_BAR_HEALTH_FILL_SRC_WIDTH * STATUS_BAR_BASE_DRAW_SCALE_X;
    if (fillWidthDst <= 0.0f) return;

    float rad = rotationDegrees * (PI_F / 180.0f);
    float cosA = cosf(rad);
    float sinA = sinf(rad);
    float fillHeightDst = STATUS_BAR_HEALTH_FILL_SRC_HEIGHT * STATUS_BAR_BASE_DRAW_SCALE_Y;
    float dx = -(STATUS_BAR_BASE_DRAW_WIDTH * 0.5f
                 - STATUS_BAR_HEALTH_EMPTY_INTERIOR_LEFT_INSET * STATUS_BAR_BASE_DRAW_SCALE_X)
               + fillWidthDst * 0.5f;
    float dy = STATUS_BAR_HEALTH_EMPTY_INTERIOR_TOP_INSET * STATUS_BAR_BASE_DRAW_SCALE_Y
               + fillHeightDst * 0.5f
               - STATUS_BAR_BASE_DRAW_HEIGHT * 0.5f;

    Rectangle fillDst = {
        screenCenter.x + dx * cosA - dy * sinA,
        screenCenter.y + dx * sinA + dy * cosA,
        fillWidthDst,
        fillHeightDst
    };
    Vector2 fillOrigin = { fillDst.width * 0.5f, fillDst.height * 0.5f };
    DrawRectanglePro(fillDst, fillOrigin, rotationDegrees, HP_BAR_FILL_COLOR);
}

// Fallback energy: shell + one pip rect per filled unit, scaled from the
// authored 8x5 pip geometry.
static void draw_base_energy_fallback(Vector2 screenCenter, float energy,
                                      float maxEnergy, float energyRegenRate,
                                      float rotationDegrees,
                                      bool reverseFillDirection) {
    draw_base_fallback_shell(screenCenter, rotationDegrees);

    int filled = base_energy_filled_pips(energy);
    float pipWidthDst = STATUS_BAR_ENERGY_PIP_WIDTH * STATUS_BAR_BASE_DRAW_SCALE_X;
    float pipHeightDst = STATUS_BAR_ENERGY_PIP_HEIGHT * STATUS_BAR_BASE_DRAW_SCALE_Y;
    float dy = STATUS_BAR_ENERGY_PIP_TOP_INSET * STATUS_BAR_BASE_DRAW_SCALE_Y
               + pipHeightDst * 0.5f
               - STATUS_BAR_BASE_DRAW_HEIGHT * 0.5f;

    for (int i = 0; i < filled; i++) {
        int slotIndex = reverseFillDirection
            ? (STATUS_BAR_ENERGY_PIP_COUNT - 1 - i)
            : i;
        float dx = energy_pip_center_dx(slotIndex, pipWidthDst);
        Rectangle pipDst = base_overlay_dst_rect(screenCenter, dx, dy,
                                                 pipWidthDst, pipHeightDst,
                                                 rotationDegrees);
        Vector2 pipOrigin = { pipDst.width * 0.5f, pipDst.height * 0.5f };
        DrawRectanglePro(pipDst, pipOrigin, rotationDegrees,
                         ENERGY_BAR_FILL_COLOR);
    }

    if (!energy_regen_cue_visible(energy, maxEnergy, energyRegenRate)) return;

    int regenSlotIndex = energy_regen_cue_slot_index(energy, reverseFillDirection);
    float regenDx = energy_pip_center_dx(regenSlotIndex, pipWidthDst);
    Rectangle ghostDst = base_overlay_dst_rect(screenCenter, regenDx, dy,
                                               pipWidthDst, pipHeightDst,
                                               rotationDegrees);
    Vector2 ghostOrigin = { ghostDst.width * 0.5f, ghostDst.height * 0.5f };
    DrawRectanglePro(ghostDst, ghostOrigin, rotationDegrees,
                     ENERGY_BAR_REGEN_GHOST_COLOR);

    float progress = energy_regen_cue_progress(energy);
    if (progress <= 0.0f) return;

    float progressWidthDst = pipWidthDst * progress;
    float progressDx = regenDx +
        (reverseFillDirection ? 1.0f : -1.0f) * (pipWidthDst - progressWidthDst) * 0.5f;
    Rectangle progressDst = base_overlay_dst_rect(screenCenter, progressDx, dy,
                                                  progressWidthDst, pipHeightDst,
                                                  rotationDegrees);
    Vector2 progressOrigin = { progressDst.width * 0.5f, progressDst.height * 0.5f };
    DrawRectanglePro(progressDst, progressOrigin, rotationDegrees,
                     ENERGY_BAR_REGEN_PROGRESS_COLOR);
}

static void draw_base_bars_fallback(const Entity *base, const Player *owner,
                                    Camera2D camera, float rotationDegrees,
                                    float labelRotationDegrees,
                                    bool reverseFillDirection) {
    if (!base || !owner) return;

    Vector2 healthCenter, energyCenter;
    base_bar_centers(base, camera, &healthCenter, &energyCenter);

    float hpRatio = (base->maxHP > 0)
        ? ((float)base->hp / (float)base->maxHP)
        : 0.0f;

    draw_base_health_fallback(healthCenter, hpRatio, rotationDegrees);
    draw_base_energy_fallback(energyCenter, owner->energy,
                              owner->maxEnergy, owner->energyRegenRate,
                              rotationDegrees,
                              reverseFillDirection);

    char hpLabel[32];
    char energyLabel[32];
    char levelLabel[32];
    char regenLabel[32];
    int filledPips = base_energy_filled_pips(owner->energy);
    // Keep fallback label placement aligned with the textured path.
    float energyLabelDirection = reverseFillDirection ? 1.0f : -1.0f;
    int displayLevel = (base->baseLevel > 0) ? base->baseLevel : 1;
    format_bar_label(hpLabel, sizeof(hpLabel), base->hp, base->maxHP);
    format_bar_label(energyLabel, sizeof(energyLabel),
                     filledPips, STATUS_BAR_ENERGY_PIP_COUNT);
    snprintf(levelLabel, sizeof(levelLabel), "LVL %d", displayLevel);
    format_regen_label(regenLabel, sizeof(regenLabel), owner->energyRegenRate);
    float regenNormalOffset = regen_label_normal_offset(labelRotationDegrees);
    float regenAxialOffset = stacked_label_axial_offset(
        energyLabel, STATUS_BAR_BASE_LABEL_FONT_SIZE,
        regenLabel, STATUS_BAR_REGEN_LABEL_FONT_SIZE
    );

    draw_bar_numeric_label(hpLabel, healthCenter, STATUS_BAR_BASE_DRAW_WIDTH,
                           STATUS_BAR_BASE_LABEL_FONT_SIZE,
                           labelRotationDegrees, rotationDegrees, LABEL_INSIDE,
                           1.0f, 0.0f, 0.0f);
    draw_bar_numeric_label(levelLabel, healthCenter, STATUS_BAR_BASE_DRAW_WIDTH,
                           STATUS_BAR_BASE_LABEL_FONT_SIZE,
                           labelRotationDegrees, rotationDegrees, LABEL_OUTSIDE,
                           energyLabelDirection, 0.0f, 0.0f);
    draw_bar_numeric_label(energyLabel, energyCenter, STATUS_BAR_BASE_DRAW_WIDTH,
                           STATUS_BAR_BASE_LABEL_FONT_SIZE,
                           labelRotationDegrees, rotationDegrees, LABEL_OUTSIDE,
                           energyLabelDirection, 0.0f, 0.0f);
    draw_bar_numeric_label(regenLabel, energyCenter, STATUS_BAR_BASE_DRAW_WIDTH,
                           STATUS_BAR_REGEN_LABEL_FONT_SIZE,
                           labelRotationDegrees, rotationDegrees, LABEL_OUTSIDE,
                           energyLabelDirection, regenAxialOffset,
                           regenNormalOffset);
}

Texture2D status_bars_load(void) {
    Texture2D texture = LoadTexture(STATUS_BARS_PATH);
    if (texture.id == 0) {
        printf("[STATUS_BARS] Failed to load %s\n", STATUS_BARS_PATH);
        return texture;
    }
    if (texture.width != STATUS_BAR_TEXTURE_WIDTH ||
        texture.height != STATUS_BAR_TEXTURE_HEIGHT) {
        printf("[STATUS_BARS] WARNING: expected %s to be %dx%d, got %dx%d\n",
               STATUS_BARS_PATH,
               STATUS_BAR_TEXTURE_WIDTH, STATUS_BAR_TEXTURE_HEIGHT,
               texture.width, texture.height);
    }

    SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    return texture;
}

Texture2D troop_health_bar_load(void) {
    Texture2D texture = LoadTexture(TROOP_HEALTH_BAR_PATH);
    if (texture.id == 0) {
        printf("[STATUS_BARS] Failed to load %s\n", TROOP_HEALTH_BAR_PATH);
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

void troop_health_bar_unload(Texture2D texture) {
    if (texture.id > 0) {
        UnloadTexture(texture);
    }
}

void status_bars_draw_screen(const GameState *gs, Camera2D camera,
                             float rotationDegrees,
                             float labelRotationDegrees,
                             bool reverseFillDirection) {
    if (!gs) return;

    bool hasBaseTexture = (gs->statusBarsTexture.id != 0);
    bool hasTroopTexture = (gs->troopHealthBarTexture.id != 0);
    const Battlefield *bf = &gs->battlefield;

    for (int i = 0; i < bf->entityCount; i++) {
        const Entity *e = bf->entities[i];
        if (!e || e->markedForRemoval || e->type != ENTITY_TROOP) continue;
        if (!e->alive || e->hp <= 0) continue;
        if (hasTroopTexture) {
            draw_troop_health_bar(gs, e, camera, rotationDegrees,
                                  reverseFillDirection);
        } else {
            draw_troop_health_bar_fallback(e, camera, rotationDegrees,
                                           reverseFillDirection);
        }
    }

    for (int i = 0; i < 2; i++) {
        const Player *player = &gs->players[i];
        const Entity *base = player->base;
        if (!base || base->markedForRemoval) continue;
        if (hasBaseTexture) {
            draw_base_bars(gs, base, player, camera,
                           rotationDegrees, labelRotationDegrees,
                           reverseFillDirection);
        } else {
            draw_base_bars_fallback(base, player, camera,
                                    rotationDegrees, labelRotationDegrees,
                                    reverseFillDirection);
        }
    }
}
