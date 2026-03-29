//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_SPRITE_RENDERER_H
#define NFC_CARDGAME_SPRITE_RENDERER_H

#include "../../lib/raylib.h"
#include <stdbool.h>

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
    DIR_SIDE, // Row 0: side-facing (right by default; flipH for left)
    DIR_DOWN, // Row 1: front-facing (character faces camera)
    DIR_UP, // Row 2: back-facing (character faces away)
    DIR_COUNT
} SpriteDirection;

// A single animation sheet (e.g. "idle.png")
typedef struct {
    Texture2D texture;
    int frameWidth;
    int frameHeight;
    int frameCount; // number of columns (frames per direction)
} SpriteSheet;

// All animations for one character type
typedef struct {
    SpriteSheet anims[ANIM_COUNT];
} CharacterSprite;

// Sprite type mapping (card type → character sprite set)
typedef enum {
    SPRITE_TYPE_KNIGHT,
    SPRITE_TYPE_HEALER,
    SPRITE_TYPE_ASSASSIN,
    SPRITE_TYPE_BRUTE,
    SPRITE_TYPE_FARMER,
    SPRITE_TYPE_COUNT
} SpriteType;

// Shared atlas — one per GameState, holds all character types
typedef struct {
    CharacterSprite base; // Default fallback
    CharacterSprite types[SPRITE_TYPE_COUNT]; // Per-type sprites
    bool typeLoaded[SPRITE_TYPE_COUNT]; // Whether unique assets exist
} SpriteAtlas;

// Per-entity animation state (lightweight, no texture data)
typedef struct {
    AnimationType anim;
    SpriteDirection dir;
    int frame;
    float timer;
    float fps;
    bool flipH;
} AnimState;

void sprite_atlas_init(SpriteAtlas *atlas);

void sprite_atlas_free(SpriteAtlas *atlas);

void sprite_draw(const CharacterSprite *cs, const AnimState *state,
                 Vector2 pos, float scale);

void anim_state_init(AnimState *state, AnimationType anim, SpriteDirection dir, float fps);

void anim_state_update(AnimState *state, float dt);

// Sprite type registry
const CharacterSprite *sprite_atlas_get(const SpriteAtlas *atlas, SpriteType type);

SpriteType sprite_type_from_card(const char *cardType);

#endif //NFC_CARDGAME_SPRITE_RENDERER_H