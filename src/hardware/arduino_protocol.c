//
// Created by Nathan Davis on 2/16/26.
//

#include "arduino_protocol.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// 5-state packet parser. One instance per open file descriptor.
typedef enum {
    PS_WAIT_START,
    PS_READ_READER,
    PS_READ_UID_LEN,
    PS_READ_UID,
    PS_READ_CHECKSUM,
} ParseState;

#define MAX_TRACKED_FDS 2

typedef struct {
    int fd;
    ParseState state;
    uint8_t reader_index;
    uint8_t uid_len;
    uint8_t uid[ARDUINO_MAX_UID_LEN];
    uint8_t uid_bytes_read;
    uint8_t checksum_accum; // running XOR of start..last UID byte
} ParserCtx;

static ParserCtx parsers[MAX_TRACKED_FDS];
static int parser_count = 0;

static ParserCtx *get_parser(int fd) {
    for (int i = 0; i < parser_count; i++) {
        if (parsers[i].fd == fd) return &parsers[i];
    }
    if (parser_count >= MAX_TRACKED_FDS) return NULL;
    ParserCtx *p = &parsers[parser_count++];
    memset(p, 0, sizeof(*p));
    p->fd = fd;
    p->state = PS_WAIT_START;
    return p;
}

bool arduino_read_packet(int fd, ArduinoPacket *out) {
    ParserCtx *p = get_parser(fd);
    if (!p) return false;

    uint8_t byte;
    while (read(fd, &byte, 1) == 1) {
        switch (p->state) {
            case PS_WAIT_START:
                if (byte == ARDUINO_START_BYTE) {
                    p->checksum_accum = ARDUINO_START_BYTE;
                    p->state = PS_READ_READER;
                }
                break;

            case PS_READ_READER:
                if (byte > 2) {
                    // Invalid reader index — resync
                    p->state = PS_WAIT_START;
                } else {
                    p->reader_index = byte;
                    p->checksum_accum ^= byte;
                    p->state = PS_READ_UID_LEN;
                }
                break;

            case PS_READ_UID_LEN:
                if (byte < 1 || byte > ARDUINO_MAX_UID_LEN) {
                    p->state = PS_WAIT_START;
                } else {
                    p->uid_len = byte;
                    p->uid_bytes_read = 0;
                    p->checksum_accum ^= byte;
                    p->state = PS_READ_UID;
                }
                break;

            case PS_READ_UID:
                p->uid[p->uid_bytes_read++] = byte;
                p->checksum_accum ^= byte;
                if (p->uid_bytes_read == p->uid_len) {
                    p->state = PS_READ_CHECKSUM;
                }
                break;

            case PS_READ_CHECKSUM:
                if (byte != p->checksum_accum) {
                    printf("[arduino_protocol] checksum mismatch (got 0x%02X, expected 0x%02X)\n",
                           byte, p->checksum_accum);
                    p->state = PS_WAIT_START;
                } else {
                    out->reader_index = p->reader_index;
                    out->uid_len = p->uid_len;
                    memcpy(out->uid, p->uid, p->uid_len);
                    p->state = PS_WAIT_START;
                    return true;
                }
                break;
        }
    }

    // No complete packet yet (EAGAIN / EWOULDBLOCK from non-blocking fd is normal)
    return false;
}

void arduino_uid_to_string(const uint8_t *uid, int uid_len, char *out) {
    for (int i = 0; i < uid_len; i++) {
        // Two hex chars per byte, uppercase
        out[i * 2] = "0123456789ABCDEF"[(uid[i] >> 4) & 0xF];
        out[i * 2 + 1] = "0123456789ABCDEF"[uid[i] & 0xF];
    }
    out[uid_len * 2] = '\0';
}