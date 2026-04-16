//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_CARDS_H
#define NFC_CARDGAME_CARDS_H

#include "db.h"
#include <stdbool.h>

typedef enum {
    CARD_COST_RESOURCE_ENERGY = 0,
    CARD_COST_RESOURCE_SUSTENANCE
} CardCostResource;

static inline const char *card_cost_resource_name(CardCostResource resource) {
    switch (resource) {
        case CARD_COST_RESOURCE_SUSTENANCE: return "sustenance";
        case CARD_COST_RESOURCE_ENERGY:
        default: return "energy";
    }
}

typedef struct Card {
    char *card_id;
    char *name;
    int cost;
    CardCostResource costResource;
    char *type;
    char *rules_text;
    char *data;
} Card;

typedef struct {
    char uid[32];
    char card_id[64];
} UIDMapping;

typedef struct {
    Card *cards;
    int count;

    UIDMapping *uid_map;
    int uid_map_count;
} Deck;

bool cards_load(Deck * deck, DB * db);

Card *cards_find(Deck *deck, const char *card_id);

void cards_free(Deck *deck);

// Load nfc_tags table from DB into deck->uid_map. Call after cards_load.
bool cards_load_nfc_map(Deck * deck, DB * db);

// Find a card by NFC UID string (uppercase hex). Returns NULL if not registered.
const Card *cards_find_by_uid(const Deck *deck, const char *uid);

void cards_free_nfc_map(Deck *deck);

#endif //NFC_CARDGAME_CARDS_H
