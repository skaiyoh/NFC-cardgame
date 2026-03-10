//
// Created by Nathan Davis on 2/16/26.
//

#include "cards.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool cards_load(Deck *deck, DB *db) {
    if (!deck || !db) return false;

    memset(deck, 0, sizeof(Deck));

    DBResult *res = db_query(db,
        "SELECT card_id, name, cost, type, rules_text, data FROM cards");

    if (!res) {
        fprintf(stderr, "Failed to load cards from database\n");
        return false;
    }

    int rows = db_result_rows(res);
    if (rows == 0) {
        db_result_free(res);
        return true;
    }

    deck->cards = calloc(rows, sizeof(Card));
    if (!deck->cards) {
        fprintf(stderr, "Failed to allocate card storage\n");
        db_result_free(res);
        return false;
    }

    deck->count = rows;

    for (int i = 0; i < rows; i++) {
        Card *c = &deck->cards[i];
        c->card_id    = strdup(db_result_value(res, i, 0));
        c->name       = strdup(db_result_value(res, i, 1));
        c->cost       = atoi(db_result_value(res, i, 2));
        c->type       = strdup(db_result_value(res, i, 3));
        c->rules_text = db_result_isnull(res, i, 4) ? NULL : strdup(db_result_value(res, i, 4));
        c->data       = db_result_isnull(res, i, 5) ? NULL : strdup(db_result_value(res, i, 5));
    }

    db_result_free(res);
    printf("Loaded %d cards into memory\n", deck->count);
    return true;
}

// TODO: cards_find() is an O(n) linear scan with strcmp. For a small deck (< 100 cards) this is
// TODO: fine, but consider a hash map keyed on card_id if the deck grows or lookups become frequent.
Card *cards_find(Deck *deck, const char *card_id) {
    if (!deck || !card_id) return NULL;

    for (int i = 0; i < deck->count; i++) {
        if (strcmp(deck->cards[i].card_id, card_id) == 0) {
            return &deck->cards[i];
        }
    }

    return NULL;
}

bool cards_load_nfc_map(Deck *deck, DB *db) {
    if (!deck || !db) return false;

    DBResult *res = db_query(db, "SELECT uid, card_id FROM nfc_tags");
    if (!res) {
        fprintf(stderr, "[NFC] Failed to load nfc_tags from database\n");
        return false;
    }

    int rows = db_result_rows(res);
    if (rows == 0) {
        db_result_free(res);
        return true;
    }

    deck->uid_map = calloc(rows, sizeof(UIDMapping));
    if (!deck->uid_map) {
        fprintf(stderr, "[NFC] Failed to allocate UID map storage\n");
        db_result_free(res);
        return false;
    }

    deck->uid_map_count = rows;
    for (int i = 0; i < rows; i++) {
        strncpy(deck->uid_map[i].uid,     db_result_value(res, i, 0), sizeof(deck->uid_map[i].uid) - 1);
        strncpy(deck->uid_map[i].card_id, db_result_value(res, i, 1), sizeof(deck->uid_map[i].card_id) - 1);
    }

    db_result_free(res);
    printf("[NFC] Loaded %d UID mappings\n", deck->uid_map_count);
    return true;
}

const Card *cards_find_by_uid(const Deck *deck, const char *uid) {
    if (!deck || !uid) return NULL;

    for (int i = 0; i < deck->uid_map_count; i++) {
        if (strcmp(deck->uid_map[i].uid, uid) == 0) {
            // uid_map entry found — now look up the card by card_id.
            // Cast away const for cards_find (which takes non-const Deck*).
            return cards_find((Deck *)deck, deck->uid_map[i].card_id);
        }
    }

    return NULL;
}

void cards_free_nfc_map(Deck *deck) {
    if (!deck) return;
    free(deck->uid_map);
    deck->uid_map = NULL;
    deck->uid_map_count = 0;
}

void cards_free(Deck *deck) {
    if (!deck) return;

    for (int i = 0; i < deck->count; i++) {
        Card *c = &deck->cards[i];
        free(c->card_id);
        free(c->name);
        free(c->type);
        free(c->rules_text);
        free(c->data);
    }

    free(deck->cards);
    memset(deck, 0, sizeof(Deck));
}
