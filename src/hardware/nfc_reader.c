//
// Created by Nathan Davis on 2/16/26.
//

#include "nfc_reader.h"
#include "arduino_protocol.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

// Open a serial port at 115200 baud in raw, non-blocking mode.
// Returns the fd on success, -1 on failure.
static int open_serial_port(const char *path) {
    int fd = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        perror(path);
        return -1;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        close(fd);
        return -1;
    }

    cfmakeraw(&tty);
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    tty.c_cc[VMIN] = 0; // Non-blocking: return immediately if no data
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        close(fd);
        return -1;
    }

    return fd;
}

bool nfc_init(NFCReader *r, const char *port0, const char *port1) {
    r->fds[0] = -1;
    r->fds[1] = -1;
    memset(r->lastUID, 0, sizeof(r->lastUID));
    memset(r->noPacketFrames, 0, sizeof(r->noPacketFrames));

    r->fds[0] = open_serial_port(port0);
    if (r->fds[0] < 0) {
        printf("[NFC] Failed to open port for Player 1: %s\n", port0);
        return false;
    }

    r->fds[1] = open_serial_port(port1);
    if (r->fds[1] < 0) {
        printf("[NFC] Failed to open port for Player 2: %s\n", port1);
        close(r->fds[0]);
        r->fds[0] = -1;
        return false;
    }

    printf("[NFC] Serial ports opened: %s (P1), %s (P2)\n", port0, port1);
    return true;
}

bool nfc_init_single(NFCReader *r, const char *port) {
    r->fds[0] = -1;
    r->fds[1] = -1;
    memset(r->lastUID, 0, sizeof(r->lastUID));
    memset(r->noPacketFrames, 0, sizeof(r->noPacketFrames));

    r->fds[0] = open_serial_port(port);
    if (r->fds[0] < 0) {
        printf("[NFC] Failed to open single test port: %s\n", port);
        return false;
    }

    printf("[NFC] Single-Arduino test mode: %s (all readers → Player 0)\n", port);
    return true;
}

int nfc_poll(NFCReader *r, NFCEvent *events, int max_events) {
    int count = 0;

    for (int player = 0; player < NFC_NUM_PLAYERS && count < max_events; player++) {
        int fd = r->fds[player];
        if (fd < 0) continue;

        bool seen[NFC_READERS_PER_PLAYER] = {false};

        ArduinoPacket pkt;
        while (count < max_events && arduino_read_packet(fd, &pkt)) {
            int ri = pkt.reader_index;
            if (ri < 0 || ri >= NFC_READERS_PER_PLAYER) continue;
            seen[ri] = true;
            r->noPacketFrames[player][ri] = 0; // card still present, reset counter

            char uid_str[32];
            arduino_uid_to_string(pkt.uid, pkt.uid_len, uid_str);

            // Only emit on rising edge (new card placed)
            if (strcmp(r->lastUID[player][ri], uid_str) == 0) continue;
            memcpy(r->lastUID[player][ri], uid_str, sizeof(r->lastUID[player][ri]));

            NFCEvent *ev = &events[count++];
            ev->playerIndex = player;
            ev->readerIndex = ri;
            memcpy(ev->uid, uid_str, sizeof(ev->uid));
        }

        // Increment silence counter; only clear lastUID after sustained silence (card removed)
        for (int ri = 0; ri < NFC_READERS_PER_PLAYER; ri++) {
            if (!seen[ri]) {
                r->noPacketFrames[player][ri]++;
                if (r->noPacketFrames[player][ri] >= NFC_REMOVAL_TIMEOUT_FRAMES) {
                    r->lastUID[player][ri][0] = '\0';
                    r->noPacketFrames[player][ri] = 0;
                }
            }
        }
    }

    return count;
}

void nfc_shutdown(NFCReader *r) {
    for (int i = 0; i < NFC_NUM_PLAYERS; i++) {
        if (r->fds[i] >= 0) {
            close(r->fds[i]);
            r->fds[i] = -1;
        }
    }
}