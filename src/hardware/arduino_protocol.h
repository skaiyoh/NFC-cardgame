//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_ARDUINO_PROTOCOL_H
#define NFC_CARDGAME_ARDUINO_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#define ARDUINO_START_BYTE  0xAA
#define ARDUINO_MAX_UID_LEN 7

// Packet decoded from the Arduino binary wire format:
//   | 0xAA | reader_idx | uid_len | uid_byte_0 ... uid_byte_N | checksum |
// checksum = XOR of all preceding bytes (start byte through last UID byte)
typedef struct {
    uint8_t reader_index; // 0, 1, or 2 (TCA multiplexer channel)
    uint8_t uid_len; // 4 or 7
    uint8_t uid[ARDUINO_MAX_UID_LEN]; // raw UID bytes
} ArduinoPacket;

// Non-blocking read from fd. Returns true and fills *out if a complete, valid
// packet was received. Returns false if no full packet is available or the
// checksum is bad. Maintains per-fd parser state internally via a static table
// keyed on fd (max 2 fds).
bool arduino_read_packet(int fd, ArduinoPacket *out);

// Convert raw UID bytes to uppercase hex string (e.g. "04A1B2C3").
// out must have space for at least uid_len * 2 + 1 bytes.
void arduino_uid_to_string(const uint8_t *uid, int uid_len, char *out);

#endif //NFC_CARDGAME_ARDUINO_PROTOCOL_H