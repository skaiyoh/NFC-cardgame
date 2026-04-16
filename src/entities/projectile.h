//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_PROJECTILE_H
#define NFC_CARDGAME_PROJECTILE_H

#include "../core/types.h"

void projectile_assets_init(ProjectileAssets *assets);
void projectile_assets_cleanup(ProjectileAssets *assets);

void projectile_system_init(ProjectileSystem *system);
void projectile_system_update(GameState *gs, float dt);
void projectile_system_draw(const GameState *gs);

int projectile_reserve_slot(GameState *gs);
void projectile_release_slot(GameState *gs, int slotIndex);
bool projectile_activate_reserved_attack(GameState *gs, int slotIndex,
                                         const Entity *attacker,
                                         const Entity *target);

bool projectile_spawn_for_attack(GameState *gs, const Entity *attacker,
                                 const Entity *target);

#endif //NFC_CARDGAME_PROJECTILE_H
