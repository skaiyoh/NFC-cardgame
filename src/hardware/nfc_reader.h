//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_NFC_READER_H
#define NFC_CARDGAME_NFC_READER_H

#include <stdbool.h>

#define NFC_NUM_PLAYERS 2
#define NFC_READERS_PER_PLAYER 3   // One TCA channel per card slot

// A single card placement event produced by an Arduino.
typedef struct {
    char uid[32]; // Uppercase hex UID string (e.g. "04A1B2C3")
    int readerIndex; // Which reader slot fired (0–2, maps to card slot / lane)
    int playerIndex; // Which player this reader belongs to (0 or 1)
} NFCEvent;

// One serial file descriptor per Arduino (one per player).
typedef struct {
    int fds[NFC_NUM_PLAYERS]; // serial fd per Arduino (-1 if not open)
} NFCReader;

// Opens serial ports for both Arduinos at 115200 baud, non-blocking.
// port0/port1 e.g. "/dev/ttyACM0", "/dev/ttyACM1".
// Returns false if either port fails to open (both fds are left at -1).
bool nfc_init(NFCReader *r, const char *port0, const char *port1);

// Single-Arduino test mode: opens one port, all reader events → Player 0. fd[1] stays -1.
bool nfc_init_single(NFCReader *r, const char *port);

// Polls both serial ports (non-blocking). Fills events[0..max_events-1].
// Returns the number of placement events produced (0 if no packets were queued).
int nfc_poll(NFCReader *r, NFCEvent *events, int max_events);

// Closes open serial port file descriptors.
void nfc_shutdown(NFCReader *r);

#endif //NFC_CARDGAME_NFC_READER_H
