//
// Created by Nathan Davis on 2/16/26.
//

#include "player.h"
#include "energy.h"
#include "../core/battlefield.h"
#include <string.h>
#include <stdio.h>

void player_init(Player *p, int id, BattleSide side,
                 Rectangle screenArea, float cameraRotation,
                 const Battlefield *bf) {
    memset(p, 0, sizeof(Player));
    p->id = id;
    p->sustenanceCollected = 0;
    p->side = side;
    p->screenArea = screenArea;
    p->cameraRotation = cameraRotation;

    // Camera targets the center of this player's territory
    const Territory *territory = bf_territory_for_side((Battlefield *)bf, side);
    Vector2 targetCenter = {
        territory->bounds.x + territory->bounds.width / 2.0f,
        territory->bounds.y + territory->bounds.height / 2.0f
    };

    p->camera = (Camera2D){0};
    p->camera.target = targetCenter;
    p->camera.offset = (Vector2){
        screenArea.x + screenArea.width / 2.0f,
        screenArea.y + screenArea.height / 2.0f
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

    // Initialize energy
    energy_init(p, 100.0f, 1.5f);

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
