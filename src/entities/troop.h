//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_TROOP_H
#define NFC_CARDGAME_TROOP_H

#include "../core/types.h"
#include "../data/cards.h"

typedef struct {
    const char *name;
    int hp, maxHP;
    int attack;
    int healAmount;
    float attackSpeed;
    float attackRange;
    float moveSpeed;
    TargetingMode targeting;
    const char *targetType;
    SpriteType spriteType;
    float bodyRadius;
    EntityRenderLayer renderLayer;
    CombatProfileId combatProfileId;
    AttackEngagementMode engagementMode;
    AttackDeliveryMode deliveryMode;
    ProjectileVisualType projectileVisualType;
    float projectileSpeed;
    float projectileHitRadius;
    float projectileSplashRadius;
    float projectileRenderScale;
    Vector2 projectileLaunchOffset;
} TroopData;

// Default collision footprint radius for a given sprite type.
// Used for both troop spawn and the home-base building so all live entities
// participate in the same overlap / aggro geometry.
float troop_default_body_radius(SpriteType type);

// Create a TroopData from a card's JSON data field (with sensible defaults)
TroopData troop_create_data_from_card(const Card *card);

// Spawn a troop entity owned by a player at a position
Entity *troop_spawn(Player *owner, const TroopData *data, Vector2 position,
                    const SpriteAtlas *atlas);

#endif //NFC_CARDGAME_TROOP_H
