//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_BUILDING_H
#define NFC_CARDGAME_BUILDING_H

#include "../core/types.h"

Entity *building_create_base(Player *owner, Vector2 position, const SpriteAtlas *atlas);

void building_take_damage(Entity *building, int damage, GameState *gs);

#endif //NFC_CARDGAME_BUILDING_H