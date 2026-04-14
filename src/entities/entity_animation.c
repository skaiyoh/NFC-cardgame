//
// Animation policy: per-sprite clip specs, stat-driven cycle durations.
//

#include "entity_animation.h"

// Default specs for each (SpriteType, AnimationType) pair.
// Frame counts are NOT stored here — they live in SpriteSheet and are
// resolved at draw time. Only timing and behavior policy is here.
// Some troop sets intentionally lack authored IDLE/RUN/DEATH sheets; the
// renderer resolves those semantic requests onto fallback art when needed.
//
// SYNC REQUIREMENT: hitNormalized values are tuned per-sprite. If a sprite's
// attack sheet changes frame count, hitNormalized may need retuning.
static const EntityAnimSpec s_specs[SPRITE_TYPE_COUNT][ANIM_COUNT] = {
    // SPRITE_TYPE_KNIGHT
    {
        [ANIM_IDLE]   = { ANIM_IDLE,   ANIM_PLAY_LOOP, 0.00f, -1.0f, false, false },
        [ANIM_RUN]    = { ANIM_RUN,    ANIM_PLAY_LOOP, 0.80f, -1.0f, false, false },
        [ANIM_WALK]   = { ANIM_WALK,   ANIM_PLAY_LOOP, 0.80f, -1.0f, false, false },
        [ANIM_HURT]   = { ANIM_HURT,   ANIM_PLAY_ONCE, 0.50f, -1.0f, false, false },
        [ANIM_DEATH]  = { ANIM_DEATH,  ANIM_PLAY_ONCE, 0.50f, -1.0f, false, true  },
        [ANIM_ATTACK] = { ANIM_ATTACK, ANIM_PLAY_ONCE, 0.60f,  0.5f, true,  false }, // 6 frames
    },
    // SPRITE_TYPE_HEALER
    {
        [ANIM_IDLE]   = { ANIM_IDLE,   ANIM_PLAY_LOOP, 0.00f, -1.0f, false, false },
        [ANIM_RUN]    = { ANIM_RUN,    ANIM_PLAY_LOOP, 0.80f, -1.0f, false, false },
        [ANIM_WALK]   = { ANIM_WALK,   ANIM_PLAY_LOOP, 0.80f, -1.0f, false, false },
        [ANIM_HURT]   = { ANIM_HURT,   ANIM_PLAY_ONCE, 0.50f, -1.0f, false, false },
        [ANIM_DEATH]  = { ANIM_DEATH,  ANIM_PLAY_ONCE, 0.50f, -1.0f, false, true  },
        [ANIM_ATTACK] = { ANIM_ATTACK, ANIM_PLAY_ONCE, 1.00f,  0.5f, true,  false }, // 10 frames
    },
    // SPRITE_TYPE_ASSASSIN
    {
        [ANIM_IDLE]   = { ANIM_IDLE,   ANIM_PLAY_LOOP, 0.00f, -1.0f, false, false },
        [ANIM_RUN]    = { ANIM_RUN,    ANIM_PLAY_LOOP, 0.80f, -1.0f, false, false },
        [ANIM_WALK]   = { ANIM_WALK,   ANIM_PLAY_LOOP, 0.80f, -1.0f, false, false },
        [ANIM_HURT]   = { ANIM_HURT,   ANIM_PLAY_ONCE, 0.50f, -1.0f, false, false },
        [ANIM_DEATH]  = { ANIM_DEATH,  ANIM_PLAY_ONCE, 0.50f, -1.0f, false, true  },
        [ANIM_ATTACK] = { ANIM_ATTACK, ANIM_PLAY_ONCE, 0.60f,  0.5f, true,  false }, // 6 frames
    },
    // SPRITE_TYPE_BRUTE
    {
        [ANIM_IDLE]   = { ANIM_IDLE,   ANIM_PLAY_LOOP, 0.00f, -1.0f, false, false },
        [ANIM_RUN]    = { ANIM_RUN,    ANIM_PLAY_LOOP, 0.80f, -1.0f, false, false },
        [ANIM_WALK]   = { ANIM_WALK,   ANIM_PLAY_LOOP, 0.80f, -1.0f, false, false },
        [ANIM_HURT]   = { ANIM_HURT,   ANIM_PLAY_ONCE, 0.50f, -1.0f, false, false },
        [ANIM_DEATH]  = { ANIM_DEATH,  ANIM_PLAY_ONCE, 0.50f, -1.0f, false, true  },
        [ANIM_ATTACK] = { ANIM_ATTACK, ANIM_PLAY_ONCE, 0.40f,  0.6f, true,  false }, // 4 frames, hit later
    },
    // SPRITE_TYPE_FARMER (empty Cheffy: seek/walk-to/gather)
    // ATTACK reuses the authored idle sheet as a one-shot gather animation.
    {
        [ANIM_IDLE]   = { ANIM_IDLE,   ANIM_PLAY_LOOP, 1.00f, -1.0f, false, false },
        [ANIM_RUN]    = { ANIM_RUN,    ANIM_PLAY_LOOP, 0.80f, -1.0f, false, false },
        [ANIM_WALK]   = { ANIM_WALK,   ANIM_PLAY_LOOP, 0.80f, -1.0f, false, false },
        [ANIM_HURT]   = { ANIM_HURT,   ANIM_PLAY_ONCE, 0.50f, -1.0f, false, false },
        [ANIM_DEATH]  = { ANIM_DEATH,  ANIM_PLAY_ONCE, 0.50f, -1.0f, false, true  },
        [ANIM_ATTACK] = { ANIM_ATTACK, ANIM_PLAY_ONCE, 0.80f,  0.5f, true,  false, 2 },
    },
    // SPRITE_TYPE_FARMER_FULL (carrying Cheffy: return/queue-wait/deposit)
    {
        [ANIM_IDLE]   = { ANIM_IDLE,   ANIM_PLAY_LOOP, 1.00f, -1.0f, false, false },
        [ANIM_RUN]    = { ANIM_RUN,    ANIM_PLAY_LOOP, 0.80f, -1.0f, false, false },
        [ANIM_WALK]   = { ANIM_WALK,   ANIM_PLAY_LOOP, 0.80f, -1.0f, false, false },
        [ANIM_HURT]   = { ANIM_HURT,   ANIM_PLAY_ONCE, 0.50f, -1.0f, false, false },
        [ANIM_DEATH]  = { ANIM_DEATH,  ANIM_PLAY_ONCE, 0.50f, -1.0f, false, true  },
        [ANIM_ATTACK] = { ANIM_ATTACK, ANIM_PLAY_ONCE, 0.80f,  0.5f, true,  false, 2 },
    },
    // SPRITE_TYPE_BIRD
    {
        [ANIM_IDLE]   = { ANIM_IDLE,   ANIM_PLAY_LOOP, 0.00f, -1.0f, false, false },
        [ANIM_RUN]    = { ANIM_RUN,    ANIM_PLAY_LOOP, 0.80f, -1.0f, false, false },
        [ANIM_WALK]   = { ANIM_WALK,   ANIM_PLAY_LOOP, 0.80f, -1.0f, false, false },
        [ANIM_HURT]   = { ANIM_HURT,   ANIM_PLAY_ONCE, 0.50f, -1.0f, false, false },
        [ANIM_DEATH]  = { ANIM_DEATH,  ANIM_PLAY_ONCE, 0.50f, -1.0f, false, true  },
        [ANIM_ATTACK] = { ANIM_ATTACK, ANIM_PLAY_ONCE, 0.60f,  0.5f, true,  false }, // 6 frames
    },
    // SPRITE_TYPE_FISHFING
    {
        [ANIM_IDLE]   = { ANIM_IDLE,   ANIM_PLAY_LOOP, 0.00f, -1.0f, false, false },
        [ANIM_RUN]    = { ANIM_RUN,    ANIM_PLAY_LOOP, 0.80f, -1.0f, false, false },
        [ANIM_WALK]   = { ANIM_WALK,   ANIM_PLAY_LOOP, 0.80f, -1.0f, false, false },
        [ANIM_HURT]   = { ANIM_HURT,   ANIM_PLAY_ONCE, 0.50f, -1.0f, false, false },
        [ANIM_DEATH]  = { ANIM_DEATH,  ANIM_PLAY_ONCE, 0.50f, -1.0f, false, true  },
        [ANIM_ATTACK] = { ANIM_ATTACK, ANIM_PLAY_ONCE, 0.60f,  0.5f, true,  false }, // 6 frames
    },
    // SPRITE_TYPE_BASE (King art — attack clip drives King burst at midpoint)
    {
        [ANIM_IDLE]   = { ANIM_IDLE,   ANIM_PLAY_LOOP, 1.00f, -1.0f, false, false },
        [ANIM_RUN]    = { ANIM_RUN,    ANIM_PLAY_LOOP, 1.00f, -1.0f, false, false },
        [ANIM_WALK]   = { ANIM_WALK,   ANIM_PLAY_LOOP, 1.00f, -1.0f, false, false },
        [ANIM_HURT]   = { ANIM_HURT,   ANIM_PLAY_ONCE, 0.50f, -1.0f, false, false },
        [ANIM_DEATH]  = { ANIM_DEATH,  ANIM_PLAY_ONCE, 1.00f, -1.0f, false, true  },
        [ANIM_ATTACK] = { ANIM_ATTACK, ANIM_PLAY_ONCE, 0.60f,  0.5f, true,  false }, // 6 frames
    },
};

// Fallback spec for out-of-bounds lookups
static const EntityAnimSpec s_fallback = {
    ANIM_IDLE, ANIM_PLAY_LOOP, 0.5f, -1.0f, false, false, 1
};

const EntityAnimSpec *anim_spec_get(SpriteType spriteType, AnimationType animType) {
    if (spriteType < 0 || spriteType >= SPRITE_TYPE_COUNT ||
        animType < 0 || animType >= ANIM_COUNT) {
        return &s_fallback;
    }
    return &s_specs[spriteType][animType];
}

static float clamp_cycle(float seconds) {
    if (seconds < CYCLE_MIN_SECONDS) return CYCLE_MIN_SECONDS;
    if (seconds > CYCLE_MAX_SECONDS) return CYCLE_MAX_SECONDS;
    return seconds;
}

float anim_walk_cycle_seconds(float moveSpeed, float pixelsPerCycle) {
    if (moveSpeed <= 0.0f || pixelsPerCycle <= 0.0f) {
        return CYCLE_MAX_SECONDS;
    }
    return clamp_cycle(pixelsPerCycle / moveSpeed);
}

float anim_attack_cycle_seconds(float attackSpeed) {
    if (attackSpeed <= 0.0f) {
        return CYCLE_MAX_SECONDS;
    }
    return clamp_cycle(1.0f / attackSpeed);
}
