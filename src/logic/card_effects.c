//
// Created by Nathan Davis on 2/16/26.
//

#include "card_effects.h"
#include "../entities/troop.h"
#include "../entities/entities.h"
#include "../systems/player.h"
#include "../systems/energy.h"
#include "../systems/spawn.h"
#include "../core/battlefield.h"
#include "../core/config.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>

// TODO: MAX_HANDLERS is hardcoded to 32. If new card types are ever added beyond this limit,
// TODO: card_action_register silently drops them. Consider a dynamic array or at least an assertion.
#define MAX_HANDLERS 32

typedef struct {
    // TODO: handlers[i].type stores a raw string pointer — safe only if called with string literals.
    // TODO: If ever called with a stack buffer or heap string, this becomes a dangling pointer.
    const char *type;
    CardPlayFn play;
} CardHandler;

static CardHandler handlers[MAX_HANDLERS];
static int handler_count = 0;

void card_action_register(const char *type, CardPlayFn fn) {
    if (handler_count >= MAX_HANDLERS) {
        fprintf(stderr, "[card_action] handler registry full, cannot register '%s'\n", type);
        return;
    }
    // TODO: No duplicate-type check — registering the same type twice silently adds a second entry.
    // TODO: Only the first match in card_action_play will fire, making the second registration dead.
    handlers[handler_count].type = type;
    handlers[handler_count].play = fn;
    handler_count++;
}

bool card_action_play(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    if (!card || !card->type) return false;

    for (int i = 0; i < handler_count; i++) {
        if (strcmp(handlers[i].type, card->type) == 0) {
            handlers[i].play(card, state, playerIndex, slotIndex);
            return true;
        }
    }

    printf("[PLAY] Unknown card type '%s' for card '%s'\n", card->type, card->name);
    return false;
}

// Helper: spawn a troop from a card for the given player into the given slot.
// Uses canonical Battlefield spawn positions (per D-05, D-06).
static void spawn_troop_from_card(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    if (!state) {
        printf("[%s] %s (cost %d): no game state, skipping spawn\n",
               card->type, card->name, card->cost);
        return;
    }

    Player *player = &state->players[playerIndex];
    CardSlot *slot = player_get_slot(player, slotIndex);

    if (!slot || slot->cooldownTimer > 0.0f) {
        printf("[%s] slot %d unavailable for player %d\n", card->type, slotIndex, playerIndex);
        return;
    }

    if (!energy_consume(player, card->cost)) {
        printf("[PLAY] Not enough energy for '%s' (need %d, have %.1f)\n",
               card->name, card->cost, player->energy);
        return;
    }

    // Canonical spawn position from Battlefield (per D-05, D-06, D-08)
    BattleSide side = bf_side_for_player(playerIndex);
    int canonicalLane = bf_slot_to_lane(side, slotIndex);
    CanonicalPos spawnCanonical = bf_spawn_pos(&state->battlefield, side, slotIndex);
    Vector2 spawnPos = spawnCanonical.v;

    TroopData data = troop_create_data_from_card(card);
    Entity *e = troop_spawn(player, &data, spawnPos, &state->spriteAtlas);
    if (e) {
        CanonicalPos spawnCheck = { e->position };
        BF_ASSERT_IN_BOUNDS(spawnCheck, BOARD_WIDTH, BOARD_HEIGHT);
        // Farmers use direct movement, not lane pathing — skip lane assignment
        if (e->unitRole != UNIT_ROLE_FARMER) {
            e->lane = canonicalLane;  // canonical lane index (per D-07)
            e->waypointIndex = 1; // Skip waypoint[0] (== spawn pos) to avoid zero-distance pause
        }
        spawn_register_entity(state, e, SPAWN_FX_SMOKE);
        player_hand_restart_animation_for_card(player, card);
    }
}

// TODO: play_spell only prints to the console — it has no in-game effect.
// TODO: Implement actual spell logic: apply damage to targeted entities, trigger AOE effects, etc.
static void play_spell(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    (void) slotIndex;
    if (state && !energy_consume(&state->players[playerIndex], card->cost)) {
        printf("[PLAY] Not enough energy for spell '%s' (need %d)\n", card->name, card->cost);
        return;
    }
    printf("[SPELL] %s (cost %d): ", card->name, card->cost);

    if (!card->data) {
        printf("no data\n");
        return;
    }

    cJSON *root = cJSON_Parse(card->data);
    if (!root) {
        printf("invalid data\n");
        return;
    }

    cJSON *damage = cJSON_GetObjectItem(root, "damage");
    cJSON *element = cJSON_GetObjectItem(root, "element");
    cJSON *targets = cJSON_GetObjectItem(root, "targets");

    if (damage && cJSON_IsNumber(damage)) {
        printf("would deal %d", damage->valueint);
        if (element && cJSON_IsString(element))
            printf(" %s", element->valuestring);
        printf(" damage");
    }

    if (targets && cJSON_IsArray(targets)) {
        printf(" to [");
        cJSON *t = NULL;
        int first = 1;
        cJSON_ArrayForEach(t, targets) {
            if (cJSON_IsString(t)) {
                printf("%s%s", first ? "" : ", ", t->valuestring);
                first = 0;
            }
        }
        printf("]");
    }

    printf("\n");
    cJSON_Delete(root);
}

static void play_knight(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    spawn_troop_from_card(card, state, playerIndex, slotIndex);
}

// TODO: play_healer has no unique healing behavior — it spawns a troop identically to play_knight.
// TODO: Add healer-specific logic: passive HP regen aura, heal-on-attack, or targeted heal ability.
static void play_healer(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    spawn_troop_from_card(card, state, playerIndex, slotIndex);
}

// TODO: play_assassin has no unique stealth/burst behavior — it spawns identically to play_knight.
// TODO: Add assassin-specific logic: target-priority override, crit chance, or spawn-behind-lines.
static void play_assassin(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    spawn_troop_from_card(card, state, playerIndex, slotIndex);
}

// TODO: play_brute has no unique tanking behavior — it spawns identically to play_knight.
// TODO: Add brute-specific logic: taunt/aggro nearby enemies, damage reduction, or AoE cleave.
static void play_brute(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    spawn_troop_from_card(card, state, playerIndex, slotIndex);
}

// TODO: play_farmer has no unique behavior — it spawns identically to play_knight.
// TODO: Add farmer-specific logic: resource generation, structure building, or passive energy bonus.
static void play_farmer(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    spawn_troop_from_card(card, state, playerIndex, slotIndex);
}

void card_action_init(void) {
    handler_count = 0;
    card_action_register("spell", play_spell);
    card_action_register("knight", play_knight);
    card_action_register("healer", play_healer);
    card_action_register("assassin", play_assassin);
    card_action_register("brute", play_brute);
    card_action_register("farmer", play_farmer);
}
