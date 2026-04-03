//
// card_enroll — NFC card enrollment tool
// Maps physical NFC card UIDs to game card IDs in the nfc_tags table.
//
// Usage: NFC_PORT=/dev/ttyACM0 DB_CONNECTION="host=localhost ..." ./card_enroll
//

#include "../src/hardware/nfc_reader.h"
#include "../src/hardware/arduino_protocol.h"
#include "../src/data/db.h"
#include "../src/data/cards.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void print_card_list(const Deck *deck) {
    for (int i = 0; i < deck->count; i++) {
        const Card *c = &deck->cards[i];
        printf("  %2d. %-20s %-16s (%s, cost %d)\n",
               i + 1, c->card_id, c->name, c->type, c->cost);
    }
    printf("\n");
}

int main(void) {
    const char *nfc_port = getenv("NFC_PORT");
    const char *db_path  = getenv("DB_PATH");
    if (!db_path) db_path = "cardgame.db";

    if (!nfc_port) {
        fprintf(stderr, "Usage: NFC_PORT=/dev/ttyACM0 [DB_PATH=cardgame.db] ./card_enroll\n");
        return 1;
    }

    NFCReader r;
    if (!nfc_init_single(&r, nfc_port)) {
        fprintf(stderr, "[ERROR] Failed to open serial port: %s\n", nfc_port);
        return 1;
    }

    DB db;
    if (!db_init(&db, db_path)) {
        fprintf(stderr, "[ERROR] Failed to connect to database: %s\n", db_error(&db));
        nfc_shutdown(&r);
        return 1;
    }

    Deck deck = {0};
    if (!cards_load(&deck, &db)) {
        fprintf(stderr, "[ERROR] Failed to load cards\n");
        db_close(&db);
        nfc_shutdown(&r);
        return 1;
    }

    cards_load_nfc_map(&deck, &db);

    printf("Connected to database. %d cards loaded, %d UIDs already enrolled.\n\n",
           deck.count, deck.uid_map_count);
    print_card_list(&deck);

    for (;;) {
        printf("Waiting for NFC scan (Ctrl+C to quit)...\n");
        fflush(stdout);

        ArduinoPacket pkt;
        for (;;) {
            while (!arduino_read_packet(r.fds[0], &pkt)) {
                usleep(1000);
            }
            if (pkt.reader_index == 0) break;
        }

        char uid_str[32];
        arduino_uid_to_string(pkt.uid, pkt.uid_len, uid_str);

        const Card *existing = cards_find_by_uid(&deck, uid_str);
        if (existing) {
            printf("[SCAN] UID: %s  (currently → %s)\n", uid_str, existing->card_id);
        } else {
            printf("[SCAN] UID: %s  (not enrolled)\n", uid_str);
        }
        print_card_list(&deck);

        printf("Enter number or card_id: ");
        fflush(stdout);

        char buf[128];
        if (!fgets(buf, sizeof(buf), stdin)) break;
        buf[strcspn(buf, "\n")] = '\0';

        // Try parsing as 1-based index first, then as raw card_id.
        char *card_id = NULL;
        char *endptr;
        long n = strtol(buf, &endptr, 10);
        if (endptr != buf && *endptr == '\0' && n >= 1 && n <= (long)deck.count) {
            card_id = deck.cards[n - 1].card_id;
        } else {
            Card *found = cards_find(&deck, buf);
            if (!found) {
                printf("[ERROR] Unknown card_id: %s\n\n", buf);
                continue;
            }
            card_id = found->card_id;
        }

        const char *params[2] = { uid_str, card_id };
        DBResult *res = db_query_params(&db,
            "INSERT INTO nfc_tags (uid, card_id) VALUES (?1, ?2) "
            "ON CONFLICT (uid) DO UPDATE SET card_id = EXCLUDED.card_id",
            2, params);

        if (!res) {
            fprintf(stderr, "[ERROR] DB write failed: %s\n", db_error(&db));
        } else {
            db_result_free(res);
            printf("[ENROLLED] %s → %s\n\n", uid_str, card_id);

            cards_free_nfc_map(&deck);
            cards_load_nfc_map(&deck, &db);
        }
    }

    cards_free(&deck);
    db_close(&db);
    nfc_shutdown(&r);
    return 0;
}
