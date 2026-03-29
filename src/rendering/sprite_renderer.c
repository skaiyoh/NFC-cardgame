//
// Created by Nathan Davis on 2/16/26.
//

#include "sprite_renderer.h"
#include "../core/config.h"
#include <string.h>

// Helper: load one animation sheet and compute frame dimensions
static SpriteSheet load_sheet(const char *path, int frameCount) {
    SpriteSheet s = {0};
    // TODO: LoadTexture returns a texture with id==0 on failure (file missing / wrong path).
    // TODO: No error is logged here — init continues silently with a broken texture.
    // TODO: Check s.texture.id == 0 after load and log the failing path so asset issues are visible.
    s.texture = LoadTexture(path);
    SetTextureFilter(s.texture, TEXTURE_FILTER_POINT);
    s.frameCount = frameCount;
    s.frameWidth = s.texture.width / frameCount;
    // TODO: frameHeight assumes exactly DIR_COUNT (3) rows in the sheet (DOWN, SIDE, UP).
    // TODO: If any sprite sheet has a different row layout this silently produces wrong frame heights.
    // TODO: Validate texture.height % DIR_COUNT == 0 and log a warning if not.
    s.frameHeight = s.texture.height / DIR_COUNT;
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
        if (b->anims[i].texture.id > 0) {
            UnloadTexture(b->anims[i].texture);
        }
    }

    // Free per-type sprites
    for (int t = 0; t < SPRITE_TYPE_COUNT; t++) {
        if (!atlas->typeLoaded[t]) continue;
        for (int i = 0; i < ANIM_COUNT; i++) {
            if (atlas->types[t].anims[i].texture.id > 0) {
                UnloadTexture(atlas->types[t].anims[i].texture);
            }
        }
    }
}

void sprite_draw(const CharacterSprite *cs, const AnimState *state,
                 Vector2 pos, float scale) {
    const SpriteSheet *sheet = &cs->anims[state->anim];
    // TODO: When LoadTexture fails, texture.id == 0 and we silently return without drawing.
    // TODO: This is safe but gives no indication of why nothing appears. Log a warning at load time.
    if (sheet->texture.id == 0) return;

    int col = state->frame % sheet->frameCount;
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

    DrawTexturePro(sheet->texture, src, dst, origin, 0.0f, WHITE);
}


void anim_state_init(AnimState *state, AnimationType anim, SpriteDirection dir, float fps) {
    state->anim = anim;
    state->dir = dir;
    state->frame = 0;
    state->timer = 0.0f;
    state->fps = fps;
    state->flipH = false;
}

void anim_state_update(AnimState *state, float dt) {
    const float frameDuration = 1.0f / state->fps;
    state->timer += dt;
    while (state->timer >= frameDuration) {
        state->timer -= frameDuration;
        // TODO: state->frame grows unboundedly and is never reset. At 60fps it overflows int after
        // TODO: ~2.1 billion increments (~1 year of runtime). Wrap it here:
        // TODO:   state->frame = (state->frame + 1) % sheet->frameCount;
        // TODO: Currently wrapping happens in sprite_draw via modulo — safe but indirect.
        state->frame++;
    }
    // Frame wrapping is handled in sprite_draw via modulo
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