//
// Created by Nathan Davis on 2/16/26.
//

#include "player.h"
#include "energy.h"
#include "progression.h"
#include "../core/battlefield.h"
#include <string.h>
#include <stdio.h>

static float player_hand_animation_duration(void) {
    return HAND_CARD_FRAME_TIME * (float)(HAND_CARD_FRAME_COUNT + 1);
}

static bool player_energy_regen_boost_is_active_internal(const Player *p) {
    return p &&
           p->energyRegenBoostRemaining > 0.0f &&
           p->energyRegenBoostMultiplier > 1.0f;
}

static void player_refresh_energy_regen_rate(Player *p) {
    if (!p) return;

    float multiplier = player_energy_regen_boost_is_active_internal(p)
        ? p->energyRegenBoostMultiplier
        : 1.0f;
    p->energyRegenRate = p->baseEnergyRegenRate * multiplier;
}

static void player_clear_energy_regen_boost(Player *p) {
    if (!p) return;

    p->energyRegenBoostRemaining = 0.0f;
    p->energyRegenBoostMultiplier = 1.0f;
    player_refresh_energy_regen_rate(p);
}

void player_init(Player *p, int id, BattleSide side,
                 Rectangle screenArea, Rectangle battlefieldArea, Rectangle handArea,
                 float cameraRotation, const Battlefield *bf) {
    memset(p, 0, sizeof(Player));
    p->id = id;
    p->sustenanceBank = 0;
    p->sustenanceCollected = 0;
    p->side = side;
    p->screenArea = screenArea;
    p->battlefieldArea = battlefieldArea;
    p->handArea = handArea;
    p->cameraRotation = cameraRotation;

    // Camera targets the center of this side's shortened playable rect,
    // so the seam (world y=SEAM_Y) continues to project onto the inner
    // edge of the battlefield sub-rect after the hand-bar inset.
    Rectangle play = bf_play_bounds(bf, side);
    Vector2 targetCenter = {
        play.x + play.width / 2.0f,
        play.y + play.height / 2.0f
    };

    p->camera = (Camera2D){0};
    p->camera.target = targetCenter;
    p->camera.offset = (Vector2){
        battlefieldArea.x + battlefieldArea.width / 2.0f,
        battlefieldArea.y + battlefieldArea.height / 2.0f
    };
    p->camera.rotation = cameraRotation;
    p->camera.zoom = 1.0f;

    // Initialize card slots with canonical spawn positions from Battlefield
    for (int i = 0; i < NUM_CARD_SLOTS; i++) {
        CanonicalPos spawnPos = bf_spawn_pos(bf, side, i);
        p->slots[i].worldPos = spawnPos.v;
        p->slots[i].activeCard = NULL;
        p->slots[i].cooldownTimer = 0.0f;
    }

    // Initialize energy at the level-1 regen rate; progression_sync_player
    // re-asserts this after the base is created.
    energy_init(p, 10.0f, PROGRESSION_REGEN_LEVEL1);
    p->baseEnergyRegenRate = PROGRESSION_REGEN_LEVEL1;
    p->energyRegenBoostMultiplier = 1.0f;
    p->energyRegenBoostRemaining = 0.0f;
    player_refresh_energy_regen_rate(p);

    printf("Player %d (side %s) initialized\n", id,
           side == SIDE_BOTTOM ? "BOTTOM" : "TOP");
}

void player_update(Player *p, float deltaTime) {
    // Update energy regeneration. Temporary boosts can expire mid-frame, so
    // split the tick if needed to avoid granting extra boosted regen time.
    float remainingDelta = deltaTime;
    if (player_energy_regen_boost_is_active_internal(p)) {
        float boostedDelta = remainingDelta;
        if (boostedDelta > p->energyRegenBoostRemaining) {
            boostedDelta = p->energyRegenBoostRemaining;
        }

        if (boostedDelta > 0.0f) {
            energy_update(p, boostedDelta);
            remainingDelta -= boostedDelta;
            p->energyRegenBoostRemaining -= boostedDelta;
        }

        if (p->energyRegenBoostRemaining <= 0.0f) {
            player_clear_energy_regen_boost(p);
        }
    }

    if (remainingDelta > 0.0f) {
        energy_update(p, remainingDelta);
    }

    // Update card slot cooldowns
    for (int i = 0; i < NUM_CARD_SLOTS; i++) {
        if (p->slots[i].cooldownTimer > 0.0f) {
            p->slots[i].cooldownTimer -= deltaTime;
            if (p->slots[i].cooldownTimer < 0.0f) {
                p->slots[i].cooldownTimer = 0.0f;
            }
        }
    }

    const float animationDuration = player_hand_animation_duration();
    for (int i = 0; i < HAND_MAX_CARDS; i++) {
        if (!p->handCardAnimating[i]) continue;

        p->handCardAnimElapsed[i] += deltaTime;
        if (p->handCardAnimElapsed[i] >= animationDuration) {
            p->handCardAnimElapsed[i] = animationDuration;
            p->handCardAnimating[i] = false;
        }
    }
}

void player_cleanup(Player *p) {
    // Player no longer owns tilemaps or entities (Battlefield does).
    printf("Player %d cleaned up\n", p->id);
}

CardSlot *player_get_slot(Player *p, int slotIndex) {
    if (slotIndex < 0 || slotIndex >= NUM_CARD_SLOTS) {
        return NULL;
    }
    return &p->slots[slotIndex];
}

bool player_slot_is_available(Player *p, int slotIndex) {
    if (slotIndex < 0 || slotIndex >= NUM_CARD_SLOTS) {
        return false;
    }
    return p->slots[slotIndex].cooldownTimer <= 0.0f;
}

void player_hand_set_card(Player *p, int handIndex, Card *card) {
    if (!p || handIndex < 0 || handIndex >= HAND_MAX_CARDS) {
        return;
    }
    p->handCards[handIndex] = card;
    p->handCardAnimating[handIndex] = false;
    p->handCardAnimElapsed[handIndex] = 0.0f;
}

void player_hand_clear_card(Player *p, int handIndex) {
    player_hand_set_card(p, handIndex, NULL);
}

Card *player_hand_get_card(const Player *p, int handIndex) {
    if (!p || handIndex < 0 || handIndex >= HAND_MAX_CARDS) {
        return NULL;
    }
    return p->handCards[handIndex];
}

bool player_hand_slot_is_occupied(const Player *p, int handIndex) {
    return player_hand_get_card(p, handIndex) != NULL;
}

int player_hand_occupied_count(const Player *p) {
    if (!p) return 0;

    int count = 0;
    for (int i = 0; i < HAND_MAX_CARDS; i++) {
        if (player_hand_slot_is_occupied(p, i)) count++;
    }
    return count;
}

void player_hand_restart_animation_for_card(Player *p, const Card *card) {
    if (!p || !card) return;

    for (int i = 0; i < HAND_MAX_CARDS; i++) {
        if (p->handCards[i] != card) continue;

        p->handCardAnimating[i] = true;
        p->handCardAnimElapsed[i] = 0.0f;
        return;
    }
}

void player_set_base_energy_regen_rate(Player *p, float baseRate) {
    if (!p) return;
    if (baseRate < 0.0f) baseRate = 0.0f;

    p->baseEnergyRegenRate = baseRate;
    player_refresh_energy_regen_rate(p);
}

bool player_energy_regen_boost_is_active(const Player *p) {
    return player_energy_regen_boost_is_active_internal(p);
}

bool player_try_activate_energy_regen_boost(Player *p, float multiplier, float durationSeconds) {
    if (!p || multiplier <= 1.0f || durationSeconds <= 0.0f) return false;
    if (player_energy_regen_boost_is_active_internal(p)) return false;

    p->energyRegenBoostMultiplier = multiplier;
    p->energyRegenBoostRemaining = durationSeconds;
    player_refresh_energy_regen_rate(p);
    return true;
}

bool player_can_afford_cost(const Player *p, int amount, CardCostResource resource) {
    if (!p) return false;
    if (amount <= 0) return true;

    switch (resource) {
        case CARD_COST_RESOURCE_SUSTENANCE:
            return p->sustenanceBank >= amount;
        case CARD_COST_RESOURCE_ENERGY:
        default:
            return energy_can_afford(p, amount);
    }
}

bool player_consume_cost(Player *p, int amount, CardCostResource resource) {
    if (!p) return false;
    if (amount <= 0) return true;
    if (!player_can_afford_cost(p, amount, resource)) return false;

    switch (resource) {
        case CARD_COST_RESOURCE_SUSTENANCE:
            p->sustenanceBank -= amount;
            return true;
        case CARD_COST_RESOURCE_ENERGY:
        default:
            return energy_consume(p, amount);
    }
}

void player_award_sustenance(GameState *gs, int playerIndex, int amount) {
    if (!gs || playerIndex < 0 || playerIndex > 1 || amount <= 0) return;
    gs->players[playerIndex].sustenanceBank += amount;
    gs->players[playerIndex].sustenanceCollected += amount;
    progression_sync_player(gs, playerIndex);
}
