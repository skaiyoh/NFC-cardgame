#ifndef NFC_CARDGAME_CARD_CATALOG_H
#define NFC_CARDGAME_CARD_CATALOG_H

#include <stddef.h>
#include <string.h>

// Requires Card to be defined before inclusion.
typedef struct {
    const char *card_id;
    const char *type;
    int hand_sheet_row; // -1 when the card remains playable but has no hand-sheet art.
} CardCatalogEntry;

// Source row order inside card_sheet.png. Hidden sustenance cards remain
// playable but have no row in the shared hand sheet.
static const CardCatalogEntry CARD_CATALOG_ENTRIES[] = {
    {"BIRD_01", "bird", 0},
    {"HEALER_01", "healer", 1},
    {"KNIGHT_01", "knight", 2},
    {"FARMER_01", "farmer", 3},
    {"FISHFING_01", "fishfing", 4},
    {"KING_01", "king", -1},
    {"MEGA_BARF_01", "mega_barf", -1},
    {"ROTTEN_ROAST_01", "rotten_roast", -1},
    {"BRUTE_01", "brute", 5},
    {"ASSASSIN_01", "assassin", 6},
};

// Presentation order for debug/demo bindings and playable hand contents. Cards
// without hand-sheet art can still stay in this order for hidden bindings, but
// only the visible 8-card hand loop lives here.
static const char *const CARD_HAND_PRESENTATION_ORDER[] = {
    "FARMER_01",
    "HEALER_01",
    "KNIGHT_01",
    "ASSASSIN_01",
    "BIRD_01",
    "FISHFING_01",
    "BRUTE_01",
    "KING_01",
};

static inline int card_catalog_count(void) {
    return (int)(sizeof(CARD_CATALOG_ENTRIES) / sizeof(CARD_CATALOG_ENTRIES[0]));
}

static inline int card_catalog_presentation_count(void) {
    return (int)(sizeof(CARD_HAND_PRESENTATION_ORDER) / sizeof(CARD_HAND_PRESENTATION_ORDER[0]));
}

static inline const CardCatalogEntry *card_catalog_entry_for_id(const char *card_id) {
    if (!card_id) return NULL;

    for (int i = 0; i < card_catalog_count(); i++) {
        if (strcmp(CARD_CATALOG_ENTRIES[i].card_id, card_id) == 0) {
            return &CARD_CATALOG_ENTRIES[i];
        }
    }

    return NULL;
}

static inline const CardCatalogEntry *card_catalog_entry_for_card(const Card *card) {
    if (!card) return NULL;
    return card_catalog_entry_for_id(card->card_id);
}

static inline const char *card_catalog_card_id_for_row(int rowIndex) {
    if (rowIndex < 0) return NULL;

    for (int i = 0; i < card_catalog_count(); i++) {
        if (CARD_CATALOG_ENTRIES[i].hand_sheet_row == rowIndex) {
            return CARD_CATALOG_ENTRIES[i].card_id;
        }
    }

    return NULL;
}

static inline const char *card_catalog_card_id_for_presentation_index(int presentationIndex) {
    if (presentationIndex < 0 || presentationIndex >= card_catalog_presentation_count()) return NULL;
    return CARD_HAND_PRESENTATION_ORDER[presentationIndex];
}

static inline const char *card_catalog_resolved_type(const Card *card) {
    const CardCatalogEntry *entry = card_catalog_entry_for_card(card);
    if (entry) return entry->type;
    return (card) ? card->type : NULL;
}

static inline int card_catalog_hand_presentation_rank_for_id(const char *card_id) {
    if (!card_id) return -1;

    for (int i = 0; i < card_catalog_presentation_count(); i++) {
        if (strcmp(CARD_HAND_PRESENTATION_ORDER[i], card_id) == 0) {
            return i;
        }
    }

    return -1;
}

static inline int card_catalog_hand_presentation_rank_for_card(const Card *card) {
    return card ? card_catalog_hand_presentation_rank_for_id(card->card_id) : -1;
}

static inline int card_catalog_hand_sheet_row_for_card(const Card *card) {
    const CardCatalogEntry *entry = card_catalog_entry_for_card(card);
    return entry ? entry->hand_sheet_row : -1;
}

#endif // NFC_CARDGAME_CARD_CATALOG_H
