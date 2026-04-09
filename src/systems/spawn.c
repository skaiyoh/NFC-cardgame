//
// Created by Nathan Davis on 2/16/26.
//

#include "spawn.h"
#include "../core/battlefield.h"
#include "../rendering/spawn_fx.h"

void spawn_register_entity(GameState *state, Entity *entity, SpawnFxKind fx) {
    if (!state || !entity) return;

    if (fx == SPAWN_FX_SMOKE) {
        spawn_fx_emit_smoke(&state->spawnFx, entity->position, entity->spriteScale);
    }

    bf_add_entity(&state->battlefield, entity);
}
