//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_SPAWN_H
#define NFC_CARDGAME_SPAWN_H

#include "../core/types.h"

typedef enum {
    SPAWN_FX_NONE = 0,
    SPAWN_FX_SMOKE = 1,
} SpawnFxKind;

void spawn_register_entity(GameState *state, Entity *entity, SpawnFxKind fx);

#endif //NFC_CARDGAME_SPAWN_H
