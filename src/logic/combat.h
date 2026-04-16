//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_COMBAT_H
#define NFC_CARDGAME_COMBAT_H

#include "../core/types.h"
#include <stdbool.h>

// Returns true if b is within a's attack range (cross-space aware)
bool combat_in_range(const Entity *a, const Entity *b, const GameState *gs);

// Small arrival slack around a melee engagement goal. This is intentionally
// much tighter than raw attackRange so melee units reach readable contact
// positions before swinging.
float combat_melee_reach_distance(const Entity *attacker, const Entity *target);

// Effective combat contact radius for the target, distinct from authored
// pathfinding nav radius. Static targets can expose a smaller melee shell than
// their navigation footprint so attackers read as contacting the sprite.
float combat_target_contact_radius(const Entity *target);

// Center-to-center attack radius for a static target. Any attacker inside this
// shell may apply hits to the base; there is no assault-slot gate.
float combat_static_target_attack_radius(const Entity *attacker, const Entity *target);

// Larger center-to-center occupancy shell used only by local steering. Allied
// same-target assault units inside this radius may compress near a static
// target without hard-blocking each other.
float combat_static_target_occupancy_radius(const Entity *attacker, const Entity *target);

// Stable center-seeking movement direction for a static target with a small
// deterministic tangent bias. Used by local steering to create a compact
// left/right assault fan without reintroducing slot-seeking.
Vector2 combat_static_target_flow_direction(const Entity *attacker, const Entity *target);

// Deterministic per-attacker/target fan angle used by local steering. This is
// stable for the pair and stays within a compact readable range.
float combat_static_target_flow_angle_degrees(const Entity *attacker, const Entity *target);

// Resolve the current steering goal for attacker -> target combat. Static
// targets use the target center plus a radius stop, so multiple same-target
// attackers can compress into the shared assault cloud without ring-seeking.
// Troop targets still use a dynamic perimeter point with a stable tangent
// spread.
bool combat_engagement_goal(const Entity *attacker, const Entity *target,
                            const Battlefield *bf, Vector2 *outGoal,
                            float *outStopRadius);

// Find the best target for attacker among the enemy player's entities
Entity *combat_find_target(Entity *attacker, GameState *gs);

// Enemy-only nearest-valid probe bounded by maxRadius (center-to-center).
// Used by the local steering aggro probe. Unlike combat_find_target, this
// does NOT run the heal-first branch: walking healers do not chase injured
// allies. TARGET_BUILDING priority is still preserved for building-focused
// attackers whose nearest building falls inside maxRadius.
Entity *combat_find_target_within_radius(Entity *attacker, GameState *gs, float maxRadius);

// Apply one attack from attacker to target (respects cooldown).
// Legacy: used before clip-driven attacks. Retained for non-clip entities.
void combat_resolve(Entity *attacker, Entity *target, GameState *gs, float deltaTime);

// Build the effect payload an attacker should deliver to a currently locked
// target. Returns false for strict no-op cases (for example a healer pointed
// at a friendly target that is no longer healable).
bool combat_build_effect_payload(const Entity *attacker, const Entity *target,
                                 CombatEffectPayload *outPayload);

// Apply an already-authored damage/heal payload to a target. Used by both
// direct hits and projectile impacts so kill/heal bookkeeping stays unified.
bool combat_apply_effect_payload(const CombatEffectPayload *payload,
                                 Entity *target, GameState *gs);

// Apply one hit from attacker to target (no cooldown check, immediate damage).
// Called by the animation hit-sync system when the attack clip crosses its hit marker.
// Checks for base kill and latches win condition if applicable.
void combat_apply_hit(Entity *attacker, Entity *target, GameState *gs);

// Enemy-only radial damage burst centered at `center`. Damages every alive
// enemy troop/building within `radius`, skipping projectiles, dead/marked
// entities, and friendlies. Applies the full kill-bookkeeping path.
void combat_apply_enemy_burst(Vector2 center, float radius, int damage,
                              int sourceEntityId, int sourceOwnerId,
                              GameState *gs);

// Area burst centered on a base. Damages every alive enemy troop/building
// within `radius` by `damage`, skipping projectiles, friendlies, and the
// base itself. Applies the full kill-bookkeeping path (farmer drop, win latch).
void combat_apply_king_burst(Entity *base, float radius, int damage, GameState *gs);

// Apply damage to an entity; transitions to ESTATE_DEAD if hp <= 0.
// Returns true if the entity was just killed (was alive, now dead).
bool entity_take_damage(Entity *entity, int damage);

// Returns true if target is a currently valid friendly heal target for attacker.
// Requires a supporter unit, another living friendly troop, and hp < maxHP.
bool combat_can_heal_target(const Entity *attacker, const Entity *target);

// Restore HP to an entity, clamped to maxHP. No-op on dead/marked entities.
// Returns true if any HP was actually restored.
bool entity_apply_heal(Entity *entity, int amount);

#endif //NFC_CARDGAME_COMBAT_H
