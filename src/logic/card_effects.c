//
// Created by Nathan Davis on 2/16/26.
//

#include "card_effects.h"
#include "../data/card_catalog.h"
#include "../entities/troop.h"
#include "../entities/entities.h"
#include "../systems/player.h"
#include "../systems/energy.h"
#include "../systems/progression.h"
#include "../systems/spawn.h"
#include "../systems/spawn_placement.h"
#include "../core/battlefield.h"
#include "../core/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: MAX_HANDLERS is hardcoded to 32. If new card types are ever added beyond this limit,
// TODO: card_action_register silently drops them. Consider a dynamic array or at least an assertion.
#define MAX_HANDLERS 32

static const float MEGA_BARF_REGEN_MULTIPLIER = 2.0f;
static const float MEGA_BARF_DURATION_SECONDS = 15.0f;
static const int ROTTEN_ROAST_BASE_HP_BONUS = 350;

typedef struct {
    // TODO: handlers[i].type stores a raw string pointer — safe only if called with string literals.
    // TODO: If ever called with a stack buffer or heap string, this becomes a dangling pointer.
    const char *type;
    CardPlayFn play;
} CardHandler;

static CardHandler handlers[MAX_HANDLERS];
static int handler_count = 0;

static const char *card_cost_label(const Card *card) {
    if (!card) return card_cost_resource_name(CARD_COST_RESOURCE_ENERGY);
    return card_cost_resource_name(card->costResource);
}

static bool card_try_pay_cost(Player *player, const Card *card) {
    if (!player || !card) return false;

    if (!player_can_afford_cost(player, card->cost, card->costResource)) {
        if (card->costResource == CARD_COST_RESOURCE_SUSTENANCE) {
            printf("[PLAY] Not enough sustenance for '%s' (need %d, have %d)\n",
                   card->name, card->cost, player->sustenanceBank);
        } else {
            printf("[PLAY] Not enough energy for '%s' (need %d, have %.1f)\n",
                   card->name, card->cost, player->energy);
        }
        return false;
    }

    return player_consume_cost(player, card->cost, card->costResource);
}

static Entity *card_resolve_live_base(Player *player, int playerIndex, const char *logTag) {
    if (!player) return NULL;

    Entity *base = player->base;
    if (!base || !base->alive || base->markedForRemoval) {
        printf("[%s] no live base for player %d, skipping\n", logTag, playerIndex);
        return NULL;
    }

    return base;
}

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
    const char *cardType = card_catalog_resolved_type(card);
    if (!card || !cardType) return false;

    for (int i = 0; i < handler_count; i++) {
        if (strcmp(handlers[i].type, cardType) == 0) {
            handlers[i].play(card, state, playerIndex, slotIndex);
            return true;
        }
    }

    printf("[PLAY] Unknown card type '%s' for card '%s'\n", cardType, card->name);
    return false;
}

// Helper: spawn a troop from a card for the given player into the given slot.
// Uses canonical Battlefield spawn positions (per D-05, D-06).
//
// Ordering matters here: the free-slot search runs before energy_consume()
// so a cancelled spawn is a clean early-exit with no refund rollback. If
// we consumed energy first and then failed to find a slot, we'd have to
// call energy_restore() -- which could interact badly with other energy
// systems mid-frame (regen tick, future multi-cost spells, etc.).
static void spawn_troop_from_card(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    if (!state) {
        printf("[%s] %s (cost %d %s): no game state, skipping spawn\n",
               card->type, card->name, card->cost, card_cost_label(card));
        return;
    }

    Player *player = &state->players[playerIndex];
    CardSlot *slot = player_get_slot(player, slotIndex);

    if (!slot || slot->cooldownTimer > 0.0f) {
        printf("[%s] slot %d unavailable for player %d\n", card->type, slotIndex, playerIndex);
        return;
    }

    // 1. Resource affordability check (no consume yet).
    if (!player_can_afford_cost(player, card->cost, card->costResource)) {
        if (card->costResource == CARD_COST_RESOURCE_SUSTENANCE) {
            printf("[PLAY] Not enough sustenance for '%s' (need %d, have %d)\n",
                   card->name, card->cost, player->sustenanceBank);
        } else {
            printf("[PLAY] Not enough energy for '%s' (need %d, have %.1f)\n",
                   card->name, card->cost, player->energy);
        }
        return;
    }

    // 2. Build troop data so we know the body radius for the placement search.
    TroopData data = troop_create_data_from_card(card);

    // 3. Find a non-overlapping spawn anchor. Farmers use the lane slot anchor
    //    as a starting point too -- they become blockers and need a valid
    //    initial position even though they walk freely after.
    BattleSide side = bf_side_for_player(playerIndex);
    int canonicalLane = bf_slot_to_lane(side, slotIndex);
    Vector2 spawnPos;
    if (!spawn_find_free_anchor(state, side, slotIndex, data.bodyRadius, &spawnPos)) {
        printf("[PLAY] '%s' cancelled: no free spawn position at slot %d for player %d\n",
               card->name, slotIndex, playerIndex);
        if (data.targetType) free((char *)data.targetType);
        return;
    }

    // phase 4. Commit: consume the resolved resource, spawn, register.
    if (!player_consume_cost(player, card->cost, card->costResource)) {
        // Defensive: affordability was just checked -- this should never fire,
        // but guard against concurrent resource drains or future side effects.
        printf("[PLAY] %s consume failed unexpectedly for '%s'\n",
               card_cost_label(card), card->name);
        if (data.targetType) free((char *)data.targetType);
        return;
    }

    Entity *e = troop_spawn(player, &data, spawnPos, &state->spriteAtlas);
    if (e) {
        CanonicalPos spawnCheck = { e->position };
        BF_ASSERT_IN_BOUNDS(spawnCheck, BOARD_WIDTH, BOARD_HEIGHT);
        // Farmers use direct movement, not lane pathing — skip lane assignment
        if (e->unitRole != UNIT_ROLE_FARMER) {
            e->lane = canonicalLane;  // canonical lane index (per D-07)
            e->laneProgress = 0.0f;
            e->waypointIndex = 1; // Skip waypoint[0] (== spawn pos) to avoid zero-distance pause
        }
        spawn_register_entity(state, e, SPAWN_FX_SMOKE);
        player_hand_restart_animation_for_card(player, card);
    }
}

static void play_knight(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    spawn_troop_from_card(card, state, playerIndex, slotIndex);
}

static void play_healer(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    spawn_troop_from_card(card, state, playerIndex, slotIndex);
}

static void play_assassin(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    spawn_troop_from_card(card, state, playerIndex, slotIndex);
}

static void play_brute(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    spawn_troop_from_card(card, state, playerIndex, slotIndex);
}

static void play_farmer(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    spawn_troop_from_card(card, state, playerIndex, slotIndex);
}

static void play_bird(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    spawn_troop_from_card(card, state, playerIndex, slotIndex);
}

static void play_fishfing(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    spawn_troop_from_card(card, state, playerIndex, slotIndex);
}

// King queues an area burst on the owning base. The swing animation drives
// damage resolution at the spec's hit marker, so activation only consumes
// the configured card resource, restarts the clip, and stages pending-burst
// state on the base.
static void play_king(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    if (!state) return;

    Player *player = &state->players[playerIndex];
    CardSlot *slot = player_get_slot(player, slotIndex);
    if (!slot || slot->cooldownTimer > 0.0f) {
        printf("[%s] slot %d unavailable for player %d\n", card->type, slotIndex, playerIndex);
        return;
    }

    Entity *base = card_resolve_live_base(player, playerIndex, "KING");
    if (!base) return;

    if (!card_try_pay_cost(player, card)) return;

    int level = (base->baseLevel > 0) ? base->baseLevel : 1;
    base->basePendingKingBurst = true;
    base->basePendingKingBurstDamage =
        progression_king_burst_damage_for_level(level);

    if (base->state == ESTATE_ATTACKING) {
        entity_restart_clip(base);
    } else {
        entity_set_state(base, ESTATE_ATTACKING);
    }
    base->attackTargetId = -1;

    player_hand_restart_animation_for_card(player, card);
    printf("[PLAY] king '%s' activated base burst for player %d (level %d, dmg %d, paid %d %s)\n",
           card->name, playerIndex, level, base->basePendingKingBurstDamage,
           card->cost, card_cost_label(card));
}

static void play_mega_barf(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    if (!state) return;

    Player *player = &state->players[playerIndex];
    CardSlot *slot = player_get_slot(player, slotIndex);
    if (!slot || slot->cooldownTimer > 0.0f) {
        printf("[%s] slot %d unavailable for player %d\n", card->type, slotIndex, playerIndex);
        return;
    }

    if (!card_resolve_live_base(player, playerIndex, "MEGA_BARF")) return;

    if (player_energy_regen_boost_is_active(player)) {
        printf("[MEGA_BARF] regen boost already active for player %d, ignoring '%s'\n",
               playerIndex, card->name);
        return;
    }

    if (!card_try_pay_cost(player, card)) return;
    if (!player_try_activate_energy_regen_boost(player,
                                                MEGA_BARF_REGEN_MULTIPLIER,
                                                MEGA_BARF_DURATION_SECONDS)) {
        if (card->costResource == CARD_COST_RESOURCE_SUSTENANCE) {
            player->sustenanceBank += card->cost;
        } else {
            energy_restore(player, (float)card->cost);
        }
        printf("[MEGA_BARF] failed to activate regen boost for player %d\n", playerIndex);
        return;
    }

    player_hand_restart_animation_for_card(player, card);
    printf("[PLAY] mega_barf '%s' doubled regen for player %d for %.1fs (paid %d %s)\n",
           card->name, playerIndex, MEGA_BARF_DURATION_SECONDS,
           card->cost, card_cost_label(card));
}

static void play_rotten_roast(const Card *card, GameState *state, int playerIndex, int slotIndex) {
    if (!state) return;

    Player *player = &state->players[playerIndex];
    CardSlot *slot = player_get_slot(player, slotIndex);
    if (!slot || slot->cooldownTimer > 0.0f) {
        printf("[%s] slot %d unavailable for player %d\n", card->type, slotIndex, playerIndex);
        return;
    }

    Entity *base = card_resolve_live_base(player, playerIndex, "ROTTEN_ROAST");
    if (!base) return;

    if (!card_try_pay_cost(player, card)) return;

    base->maxHP += ROTTEN_ROAST_BASE_HP_BONUS;
    base->hp += ROTTEN_ROAST_BASE_HP_BONUS;

    player_hand_restart_animation_for_card(player, card);
    printf("[PLAY] rotten_roast '%s' fortified player %d base to %d/%d (paid %d %s)\n",
           card->name, playerIndex, base->hp, base->maxHP,
           card->cost, card_cost_label(card));
}

void card_action_init(void) {
    handler_count = 0;
    card_action_register("knight", play_knight);
    card_action_register("healer", play_healer);
    card_action_register("assassin", play_assassin);
    card_action_register("brute", play_brute);
    card_action_register("farmer", play_farmer);
    card_action_register("bird", play_bird);
    card_action_register("fishfing", play_fishfing);
    card_action_register("king", play_king);
    card_action_register("mega_barf", play_mega_barf);
    card_action_register("rotten_roast", play_rotten_roast);
}
