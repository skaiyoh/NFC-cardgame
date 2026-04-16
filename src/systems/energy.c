//
// Created by Nathan Davis on 2/16/26.
//

#include "energy.h"
#include "../core/types.h"

void energy_init(Player *p, float maxEnergy, float regenRate) {
    p->maxEnergy = maxEnergy;
    p->energyRegenRate = regenRate;
    p->energy = maxEnergy;
}

void energy_update(Player *p, float deltaTime) {
    if (p->energy < p->maxEnergy) {
        p->energy += p->energyRegenRate * deltaTime;
        if (p->energy > p->maxEnergy)
            p->energy = p->maxEnergy;
    }
}

bool energy_can_afford(const Player *p, int cost) {
    return p->energy >= (float) cost;
}

bool energy_consume(Player *p, int cost) {
    if (!energy_can_afford(p, cost)) return false;
    p->energy -= (float) cost;
    return true;
}

void energy_restore(Player *p, float amount) {
    p->energy += amount;
    if (p->energy > p->maxEnergy)
        p->energy = p->maxEnergy;
}

void energy_set_regen_rate(Player *p, float newRate) {
    p->energyRegenRate = newRate;
}
