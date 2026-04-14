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

void player_init(Player *p, int id, BattleSide side,
                 Rectangle screenArea, Rectangle battlefieldArea, Rectangle handArea,
                 float cameraRotation, const Battlefield *bf) {
    memset(p, 0, sizeof(Player));
    p->id = id;
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

    printf("Player %d (side %s) initialized\n", id,
           side == SIDE_BOTTOM ? "BOTTOM" : "TOP");
}

void player_update(Player *p, float deltaTime) {
    // Update energy regeneration
    energy_update(p, deltaTime);

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

void player_award_sustenance(GameState *gs, int playerIndex, int amount) {
    if (!gs || playerIndex < 0 || playerIndex > 1 || amount <= 0) return;
    gs->players[playerIndex].sustenanceCollected += amount;
    progression_sync_player(gs, playerIndex);
}
