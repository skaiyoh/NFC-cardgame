//
// Created by Nathan Davis on 2/16/26.
//

#include "sprite_renderer.h"
#include "../core/config.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
    const char *path;
    int frameCount;
    const Rectangle *visibleBounds;
} SpriteSheetAtlasEntry;

#include "sprite_frame_atlas.h"

static int sheet_bounds_index(const SpriteSheet *sheet, SpriteDirection dir, int frame) {
    return dir * sheet->frameCount + frame;
}

static Rectangle compute_visible_bounds(const Color *pixels, int imageWidth,
                                        int frameX, int frameY,
                                        int frameWidth, int frameHeight) {
    int minX = frameWidth;
    int minY = frameHeight;
    int maxX = -1;
    int maxY = -1;

    for (int y = 0; y < frameHeight; y++) {
        for (int x = 0; x < frameWidth; x++) {
            Color px = pixels[(frameY + y) * imageWidth + (frameX + x)];
            if (px.a == 0) continue;

            if (x < minX) minX = x;
            if (y < minY) minY = y;
            if (x > maxX) maxX = x;
            if (y > maxY) maxY = y;
        }
    }

    if (maxX < minX || maxY < minY) {
        return (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};
    }

    return (Rectangle){
        (float) minX,
        (float) minY,
        (float) (maxX - minX + 1),
        (float) (maxY - minY + 1)
    };
}

static const SpriteSheetAtlasEntry *find_sheet_atlas_entry(const char *path, int frameCount) {
    for (int i = 0; i < kSpriteSheetAtlasCount; i++) {
        const SpriteSheetAtlasEntry *entry = &kSpriteSheetAtlas[i];
        if (entry->frameCount == frameCount && strcmp(entry->path, path) == 0) {
            return entry;
        }
    }
    return NULL;
}

static Rectangle *alloc_visible_bounds(int frameCount) {
    return calloc((size_t) (frameCount * DIR_COUNT), sizeof(Rectangle));
}

static void populate_visible_bounds_from_image(SpriteSheet *sheet, const char *path) {
    if (!sheet->visibleBounds) return;

    Image image = LoadImage(path);
    if (!image.data) return;

    Color *pixels = LoadImageColors(image);
    if (pixels) {
        for (int dir = 0; dir < DIR_COUNT; dir++) {
            for (int frame = 0; frame < sheet->frameCount; frame++) {
                int frameX = frame * sheet->frameWidth;
                int frameY = dir * sheet->frameHeight;
                sheet->visibleBounds[sheet_bounds_index(sheet, (SpriteDirection) dir, frame)] =
                    compute_visible_bounds(pixels, image.width, frameX, frameY,
                                           sheet->frameWidth, sheet->frameHeight);
            }
        }
        UnloadImageColors(pixels);
    }

    UnloadImage(image);
}

// Helper: load one animation sheet and compute frame dimensions
static SpriteSheet load_sheet(const char *path, int frameCount) {
    SpriteSheet s = {0};
    s.texture = LoadTexture(path);
    if (s.texture.id == 0) return s;

    SetTextureFilter(s.texture, TEXTURE_FILTER_POINT);

    s.frameCount = frameCount;
    s.frameWidth = s.texture.width / frameCount;
    // TODO: frameHeight assumes exactly DIR_COUNT (3) rows in the sheet (SIDE, DOWN, UP).
    // TODO: If any sprite sheet has a different row layout this silently produces wrong frame heights.
    // TODO: Validate texture.height % DIR_COUNT == 0 and log a warning if not.
    s.frameHeight = s.texture.height / DIR_COUNT;
    s.visibleBounds = alloc_visible_bounds(s.frameCount);

    const SpriteSheetAtlasEntry *entry = find_sheet_atlas_entry(path, frameCount);
    if (s.visibleBounds && entry) {
        memcpy(s.visibleBounds, entry->visibleBounds,
               (size_t) (s.frameCount * DIR_COUNT) * sizeof(Rectangle));
    } else {
        populate_visible_bounds_from_image(&s, path);
    }

    return s;
}

// TODO: ANIM_RUN sheets are loaded for every character type but no code ever sets ANIM_RUN on
// TODO: an entity. These textures are loaded into VRAM and never used. Remove the load_sheet calls
// TODO: for ANIM_RUN or add run-state transitions to entity_update / entity_set_state.
void sprite_atlas_init(SpriteAtlas *atlas) {
    CharacterSprite *b = &atlas->base;
    b->anims[ANIM_IDLE] = load_sheet(CHAR_BASE_PATH "Basic/idle.png", 4);
    b->anims[ANIM_RUN] = load_sheet(CHAR_BASE_PATH "Basic/run.png", 8);
    b->anims[ANIM_WALK] = load_sheet(CHAR_BASE_PATH "Basic/walk.png", 8);
    b->anims[ANIM_HURT] = load_sheet(CHAR_BASE_PATH "Basic/hurt.png", 4);
    b->anims[ANIM_DEATH] = load_sheet(CHAR_BASE_PATH "Basic/death.png", 6);
    b->anims[ANIM_ATTACK] = load_sheet(CHAR_BASE_PATH "Attack/sword.png", 6);

    CharacterSprite *knight = &atlas->types[SPRITE_TYPE_KNIGHT];
    knight->anims[ANIM_IDLE] = load_sheet(CHAR_KNIGHT_PATH "idle.png", 4);
    knight->anims[ANIM_WALK] = load_sheet(CHAR_KNIGHT_PATH "walk.png", 8);
    knight->anims[ANIM_RUN] = load_sheet(CHAR_KNIGHT_PATH "run.png", 8);
    knight->anims[ANIM_HURT] = load_sheet(CHAR_KNIGHT_PATH "hurt.png", 4);
    knight->anims[ANIM_DEATH] = load_sheet(CHAR_KNIGHT_PATH "death.png", 6);
    knight->anims[ANIM_ATTACK] = load_sheet(CHAR_KNIGHT_PATH "sword.png", 6);
    atlas->typeLoaded[SPRITE_TYPE_KNIGHT] = true;

    CharacterSprite *healer = &atlas->types[SPRITE_TYPE_HEALER];
    healer->anims[ANIM_IDLE] = load_sheet(CHAR_HEALER_PATH "idle.png", 4);
    healer->anims[ANIM_WALK] = load_sheet(CHAR_HEALER_PATH "walk.png", 8);
    healer->anims[ANIM_RUN] = load_sheet(CHAR_HEALER_PATH "run.png", 8);
    healer->anims[ANIM_HURT] = load_sheet(CHAR_HEALER_PATH "hurt.png", 4);
    healer->anims[ANIM_DEATH] = load_sheet(CHAR_HEALER_PATH "death.png", 6);
    healer->anims[ANIM_ATTACK] = load_sheet(CHAR_HEALER_PATH "staff.png", 10);
    atlas->typeLoaded[SPRITE_TYPE_HEALER] = true;

    CharacterSprite *assassin = &atlas->types[SPRITE_TYPE_ASSASSIN];
    assassin->anims[ANIM_IDLE] = load_sheet(CHAR_ASSASSIN_PATH "idle.png", 4);
    assassin->anims[ANIM_WALK] = load_sheet(CHAR_ASSASSIN_PATH "walk.png", 8);
    assassin->anims[ANIM_RUN] = load_sheet(CHAR_ASSASSIN_PATH "run.png", 8);
    assassin->anims[ANIM_HURT] = load_sheet(CHAR_ASSASSIN_PATH "hurt.png", 4);
    assassin->anims[ANIM_DEATH] = load_sheet(CHAR_ASSASSIN_PATH "death.png", 6);
    assassin->anims[ANIM_ATTACK] = load_sheet(CHAR_ASSASSIN_PATH "sword 2.png", 6);
    atlas->typeLoaded[SPRITE_TYPE_ASSASSIN] = true;

    CharacterSprite *brute = &atlas->types[SPRITE_TYPE_BRUTE];
    brute->anims[ANIM_IDLE] = load_sheet(CHAR_BRUTE_PATH "idle.png", 4);
    brute->anims[ANIM_WALK] = load_sheet(CHAR_BRUTE_PATH "walk.png", 8);
    brute->anims[ANIM_RUN] = load_sheet(CHAR_BRUTE_PATH "run.png", 8);
    brute->anims[ANIM_HURT] = load_sheet(CHAR_BRUTE_PATH "hurt.png", 4);
    brute->anims[ANIM_DEATH] = load_sheet(CHAR_BRUTE_PATH "death.png", 6);
    // TODO: Brute uses "block.png" (4 frames) as its ANIM_ATTACK — likely a "block" animation
    // TODO: repurposed as attack. Verify this is intentional or replace with an actual attack sheet.
    brute->anims[ANIM_ATTACK] = load_sheet(CHAR_BRUTE_PATH "block.png", 4);
    atlas->typeLoaded[SPRITE_TYPE_BRUTE] = true;

    CharacterSprite *farmer = &atlas->types[SPRITE_TYPE_FARMER];
    farmer->anims[ANIM_IDLE] = load_sheet(CHAR_FARMER_PATH "idle.png", 4);
    farmer->anims[ANIM_WALK] = load_sheet(CHAR_FARMER_PATH "walk.png", 8);
    farmer->anims[ANIM_RUN] = load_sheet(CHAR_FARMER_PATH "run.png", 8);
    farmer->anims[ANIM_HURT] = load_sheet(CHAR_FARMER_PATH "hurt.png", 4);
    farmer->anims[ANIM_DEATH] = load_sheet(CHAR_FARMER_PATH "death.png", 6);
    farmer->anims[ANIM_ATTACK] = load_sheet(CHAR_FARMER_PATH "pickaxe.png", 6);
    atlas->typeLoaded[SPRITE_TYPE_FARMER] = true;
}

void sprite_atlas_free(SpriteAtlas *atlas) {
    // Free base sprites
    CharacterSprite *b = &atlas->base;
    for (int i = 0; i < ANIM_COUNT; i++) {
        free(b->anims[i].visibleBounds);
        b->anims[i].visibleBounds = NULL;
        if (b->anims[i].texture.id > 0) {
            UnloadTexture(b->anims[i].texture);
        }
    }

    // Free per-type sprites
    for (int t = 0; t < SPRITE_TYPE_COUNT; t++) {
        if (!atlas->typeLoaded[t]) continue;
        for (int i = 0; i < ANIM_COUNT; i++) {
            free(atlas->types[t].anims[i].visibleBounds);
            atlas->types[t].anims[i].visibleBounds = NULL;
            if (atlas->types[t].anims[i].texture.id > 0) {
                UnloadTexture(atlas->types[t].anims[i].texture);
            }
        }
    }
}

const SpriteSheet *sprite_sheet_get(const CharacterSprite *cs, AnimationType anim) {
    if (!cs || anim < 0 || anim >= ANIM_COUNT) return NULL;
    return &cs->anims[anim];
}

Rectangle sprite_visible_bounds(const CharacterSprite *cs, const AnimState *state,
                                Vector2 pos, float scale) {
    if (!state) return (Rectangle){pos.x, pos.y, 0.0f, 0.0f};

    const SpriteSheet *sheet = sprite_sheet_get(cs, state->anim);
    if (!sheet) return (Rectangle){pos.x, pos.y, 0.0f, 0.0f};

    int frame = (int)(state->normalizedTime * (float)sheet->frameCount);
    if (frame >= sheet->frameCount) frame = sheet->frameCount - 1;
    if (frame < 0) frame = 0;
    Rectangle bounds = {0.0f, 0.0f, (float) sheet->frameWidth, (float) sheet->frameHeight};
    if (sheet->visibleBounds) {
        bounds = sheet->visibleBounds[sheet_bounds_index(sheet, state->dir, frame)];
    }
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return (Rectangle){pos.x, pos.y, 0.0f, 0.0f};
    }

    float frameLeft = pos.x - (float) sheet->frameWidth * scale * 0.5f;
    float frameTop = pos.y - (float) sheet->frameHeight * scale * 0.5f;
    float visibleX = frameLeft + bounds.x * scale;

    if (state->flipH) {
        visibleX = frameLeft + ((float) sheet->frameWidth - (bounds.x + bounds.width)) * scale;
    }

    return (Rectangle){
        visibleX,
        frameTop + bounds.y * scale,
        bounds.width * scale,
        bounds.height * scale
    };
}

void sprite_draw(const CharacterSprite *cs, const AnimState *state,
                 Vector2 pos, float scale, float rotationDegrees) {
    const SpriteSheet *sheet = sprite_sheet_get(cs, state->anim);
    // TODO: When LoadTexture fails, texture.id == 0 and we silently return without drawing.
    // TODO: This is safe but gives no indication of why nothing appears. Log a warning at load time.
    if (!sheet || sheet->texture.id == 0) return;

    int col = (int)(state->normalizedTime * (float)sheet->frameCount);
    if (col >= sheet->frameCount) col = sheet->frameCount - 1;
    if (col < 0) col = 0;
    int row = state->dir;

    float fw = (float) sheet->frameWidth;
    float fh = (float) sheet->frameHeight;

    // Flip source width negative for horizontal mirror
    Rectangle src = {
        (float) (col * sheet->frameWidth),
        (float) (row * sheet->frameHeight),
        state->flipH ? -fw : fw,
        fh
    };

    float dw = fw * scale;
    float dh = fh * scale;

    Rectangle dst = {
        pos.x,
        pos.y,
        dw,
        dh
    };

    // Center the sprite on the position
    Vector2 origin = {dw / 2.0f, dh / 2.0f};

    DrawTexturePro(sheet->texture, src, dst, origin, rotationDegrees, WHITE);
}


void anim_state_init(AnimState *state, AnimationType anim, SpriteDirection dir,
                     float cycleDuration, bool oneShot) {
    state->anim = anim;
    state->dir = dir;
    state->elapsed = 0.0f;
    state->cycleDuration = cycleDuration;
    state->normalizedTime = 0.0f;
    state->oneShot = oneShot;
    state->finished = false;
    state->flipH = false;
}

AnimPlaybackEvent anim_state_update(AnimState *state, float dt) {
    AnimPlaybackEvent evt = {0};

    if (state->cycleDuration <= 0.0f) {
        // Degenerate clip: stay at frame 0, report finished immediately for one-shot
        evt.finishedThisTick = state->oneShot && !state->finished;
        state->finished = state->oneShot;
        return evt;
    }

    evt.prevNormalized = state->normalizedTime;
    state->elapsed += dt;

    if (state->oneShot) {
        if (state->elapsed >= state->cycleDuration) {
            state->elapsed = state->cycleDuration;
            state->normalizedTime = 1.0f;
            if (!state->finished) {
                evt.finishedThisTick = true;
                state->finished = true;
            }
        } else {
            state->normalizedTime = state->elapsed / state->cycleDuration;
        }
    } else {
        // Looping: wrap elapsed and compute normalized [0, 1)
        while (state->elapsed >= state->cycleDuration) {
            state->elapsed -= state->cycleDuration;
            evt.loopedThisTick = true;
        }
        state->normalizedTime = state->elapsed / state->cycleDuration;
    }

    evt.currNormalized = state->normalizedTime;
    return evt;
}

const CharacterSprite *sprite_atlas_get(const SpriteAtlas *atlas, SpriteType type) {
    if (type >= 0 && type < SPRITE_TYPE_COUNT && atlas->typeLoaded[type]) {
        return &atlas->types[type];
    }
    return &atlas->base;
}

SpriteType sprite_type_from_card(const char *cardType) {
    if (!cardType) return SPRITE_TYPE_KNIGHT;
    if (strcmp(cardType, "knight") == 0) return SPRITE_TYPE_KNIGHT;
    if (strcmp(cardType, "healer") == 0) return SPRITE_TYPE_HEALER;
    if (strcmp(cardType, "assassin") == 0) return SPRITE_TYPE_ASSASSIN;
    if (strcmp(cardType, "brute") == 0) return SPRITE_TYPE_BRUTE;
    if (strcmp(cardType, "farmer") == 0) return SPRITE_TYPE_FARMER;
    // TODO: Unknown card types silently fall back to SPRITE_TYPE_KNIGHT. Log a warning so new card
    // TODO: types that are missing a sprite mapping are caught during development.
    return SPRITE_TYPE_KNIGHT; // default fallback
}
