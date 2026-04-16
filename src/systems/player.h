//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_PLAYER_H
#define NFC_CARDGAME_PLAYER_H

#include "../core/types.h"

// Forward declaration
typedef struct Battlefield Battlefield;

// Initialize a player with their viewport layout and side on the Battlefield.
// screenArea: full half-screen rect owned by this player (1920/2 x 1080).
// battlefieldArea: inner world-space sub-rect (screen minus the hand bar).
// handArea: outer-edge hand-bar strip (HAND_UI_DEPTH_PX deep).
// The camera centers on bf_play_bounds(side) so the seam still lands on the
// inner battlefield edge after the hand-bar inset.
void player_init(Player *p, int id, BattleSide side,
                 Rectangle screenArea, Rectangle battlefieldArea, Rectangle handArea,
                 float cameraRotation, const Battlefield *bf);

// Update player state (energy regen, slot cooldowns)
void player_update(Player *p, float deltaTime);

// Clean up player resources
void player_cleanup(Player *p);

// Card slot access
CardSlot *player_get_slot(Player *p, int slotIndex);

bool player_slot_is_available(Player *p, int slotIndex);

// Visible hand helpers
void player_hand_set_card(Player *p, int handIndex, Card *card);

void player_hand_clear_card(Player *p, int handIndex);

Card *player_hand_get_card(const Player *p, int handIndex);

bool player_hand_slot_is_occupied(const Player *p, int handIndex);

int player_hand_occupied_count(const Player *p);

void player_hand_restart_animation_for_card(Player *p, const Card *card);

void player_set_base_energy_regen_rate(Player *p, float baseRate);

bool player_energy_regen_boost_is_active(const Player *p);

bool player_try_activate_energy_regen_boost(Player *p, float multiplier, float durationSeconds);

bool player_can_afford_cost(const Player *p, int amount, CardCostResource resource);

bool player_consume_cost(Player *p, int amount, CardCostResource resource);

// Single choke point for all sustenance grants. Adds to the player's counter
// and spendable bank, then re-syncs progression-dependent state.
void player_award_sustenance(GameState *gs, int playerIndex, int amount);

#endif //NFC_CARDGAME_PLAYER_H
