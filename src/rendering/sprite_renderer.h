//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_SPRITE_RENDERER_H
#define NFC_CARDGAME_SPRITE_RENDERER_H

#include <raylib.h>
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
    int sourceRowCount; // number of authored directional rows in the source sheet
    Rectangle *visibleBounds; // frame-local opaque bounds, indexed by dir * frameCount + frame
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
    SPRITE_TYPE_FARMER_FULL,
    SPRITE_TYPE_BIRD,
    SPRITE_TYPE_FISHFING,
    SPRITE_TYPE_BASE,
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
    float elapsed;        // seconds since clip start
    float cycleDuration;  // total seconds for one full cycle
    float normalizedTime; // elapsed / cycleDuration, clamped [0,1] for one-shot
    bool oneShot;
    bool finished;        // true when one-shot clip completes
    bool flipH;
} AnimState;

// Playback events returned by anim_state_update
typedef struct {
    float prevNormalized; // normalized time before this tick
    float currNormalized; // normalized time after this tick
    bool finishedThisTick;
    bool loopedThisTick;
} AnimPlaybackEvent;

void sprite_atlas_init(SpriteAtlas *atlas);

void sprite_atlas_free(SpriteAtlas *atlas);

// Returns the authored sheet for anim, or the explicit fallback sheet when a
// semantic clip (for example IDLE/DEATH) is not authored for this sprite set.
const SpriteSheet *sprite_sheet_get(const CharacterSprite *cs, AnimationType anim);

Rectangle sprite_visible_bounds(const CharacterSprite *cs, const AnimState *state,
                                Vector2 pos, float scale, float rotationDegrees);

void sprite_draw(const CharacterSprite *cs, const AnimState *state,
                 Vector2 pos, float scale, float rotationDegrees);

void anim_state_init(AnimState *state, AnimationType anim, SpriteDirection dir,
                     float cycleDuration, bool oneShot);

AnimPlaybackEvent anim_state_update(AnimState *state, float dt);

// Sprite type registry
const CharacterSprite *sprite_atlas_get(const SpriteAtlas *atlas, SpriteType type);

SpriteType sprite_type_from_card(const char *cardType);

#endif //NFC_CARDGAME_SPRITE_RENDERER_H
