//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_TROOP_H
#define NFC_CARDGAME_TROOP_H

#include "../core/types.h"
#include "../data/cards.h"

typedef enum {
    TARGET_NEAREST,
    TARGET_BUILDING,
    TARGET_SPECIFIC_TYPE
} TargetingMode;

typedef struct {
    const char *name;
    int hp, maxHP;
    int attack;
    float attackSpeed;
    float attackRange;
    float moveSpeed;
    TargetingMode targeting;
    const char *targetType;
    SpriteType spriteType;
} TroopData;

// Create a TroopData from a card's JSON data field (with sensible defaults)
TroopData troop_create_data_from_card(const Card *card);

// Spawn a troop entity owned by a player at a position
Entity *troop_spawn(Player *owner, const TroopData *data, Vector2 position,
                    const SpriteAtlas *atlas);

#endif //NFC_CARDGAME_TROOP_H