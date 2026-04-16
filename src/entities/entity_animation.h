//
// Animation policy: per-sprite clip specs, stat-driven cycle durations.
//

#ifndef NFC_CARDGAME_ENTITY_ANIMATION_H
#define NFC_CARDGAME_ENTITY_ANIMATION_H

#include "../rendering/sprite_renderer.h"

typedef struct {
    AnimationType anim;
    AnimPlayMode mode;
    float cycleSeconds;     // default duration; overridden by stat-driven helpers
    float hitNormalized;    // 0.0-1.0 for when damage fires; -1.0 if N/A
    bool lockFacing;
    bool removeOnFinish;
    int visualLoops;        // 0/1 => one traversal per cycle; >1 repeats within the same cycle
    float idleHoldMinSeconds; // only used by ANIM_PLAY_IDLE_BURST
    float idleHoldMaxSeconds; // only used by ANIM_PLAY_IDLE_BURST
    float idleInitialPhaseNormalized; // negative => randomized seeded offset into first hold+burst cycle
} EntityAnimSpec;

// Walk tuning: world pixels covered per full walk cycle (8 frames * ~8px stride)
#define WALK_PIXELS_PER_CYCLE 64.0f

// Clamp bounds for stat-derived cycle durations
#define CYCLE_MIN_SECONDS 0.3f
#define CYCLE_MAX_SECONDS 3.0f

// Get the default animation spec for a sprite type + animation type pair.
// Returns a pointer to a static table entry; never NULL.
const EntityAnimSpec *anim_spec_get(SpriteType spriteType, AnimationType animType);

// Compute walk cycle duration from moveSpeed.
// Returns cycleDuration clamped to [CYCLE_MIN_SECONDS, CYCLE_MAX_SECONDS].
float anim_walk_cycle_seconds(float moveSpeed, float pixelsPerCycle);

// Compute attack cycle duration from attackSpeed.
// Returns 1.0f / attackSpeed, clamped to [CYCLE_MIN_SECONDS, CYCLE_MAX_SECONDS].
float anim_attack_cycle_seconds(float attackSpeed);

#endif //NFC_CARDGAME_ENTITY_ANIMATION_H
