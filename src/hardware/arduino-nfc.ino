// arduino-nfc.ino — Production NFC sketch for NFC-cardgame.
//
// Hardware: Arduino Nano + TCA9548A I2C multiplexer + 3× PN532 NFC readers
//   TCA9548A address: 0x70
//   PN532 IRQ: pin 2, RESET: pin 3
//   Baud: 115200
//
// Protocol: binary packets emitted on Serial when a new card placement is detected.
//   | START_BYTE (0xAA) | reader_idx | uid_len | uid_byte_0...N | checksum |
//   checksum = XOR of all preceding bytes (START_BYTE through last UID byte)
//
// Human-readable output is only emitted during setup() for serial monitor use.

#include <Wire.h>
#include <Adafruit_PN532.h>
#include <string.h>

#define TCAADDR    0x70
#define NUM_READERS 3
#define PN532_IRQ   2
#define PN532_RESET 3
#define START_BYTE  0xAA
#define MAX_UID_LEN 7
#define READER_SETTLE_DELAY_MS 1
#define READ_TIMEOUT_MS 12
#define RELEASE_MISS_THRESHOLD 3

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

struct ReaderState {
    bool enabled;
    bool latched;
    uint8_t uid[MAX_UID_LEN];
    uint8_t uidLen;
    uint8_t missCount;
};

static ReaderState g_readerStates[NUM_READERS];

void tcaSelect(uint8_t channel) {
    if (channel > 7) return;
    Wire.beginTransmission(TCAADDR);
    Wire.write(1 << channel);
    Wire.endTransmission();
}

static bool readerUidMatches(const ReaderState &state, const uint8_t *uid, uint8_t uidLen) {
    return state.latched && state.uidLen == uidLen && memcmp(state.uid, uid, uidLen) == 0;
}

static void readerLatch(const int reader, const uint8_t *uid, uint8_t uidLen) {
    ReaderState &state = g_readerStates[reader];
    memcpy(state.uid, uid, uidLen);
    state.uidLen = uidLen;
    state.latched = true;
    state.missCount = 0;
}

static void readerRelease(const int reader) {
    ReaderState &state = g_readerStates[reader];
    state.latched = false;
    state.uidLen = 0;
    state.missCount = 0;
}

static void emitPacket(uint8_t reader, const uint8_t *uid, uint8_t uidLen) {
    if (uidLen < 1 || uidLen > MAX_UID_LEN) return;

    uint8_t checksum = START_BYTE ^ reader ^ uidLen;
    for (uint8_t i = 0; i < uidLen; i++) checksum ^= uid[i];

    Serial.write(START_BYTE);
    Serial.write(reader);
    Serial.write(uidLen);
    Serial.write(uid, uidLen);
    Serial.write(checksum);
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println("\n========================================");
    Serial.println("NFC Card Game - Binary Protocol Mode");
    Serial.println("========================================\n");

    Wire.begin();

    Wire.beginTransmission(TCAADDR);
    if (Wire.endTransmission() == 0) {
        Serial.println("TCA9548A multiplexer found");
    } else {
        Serial.println("ERROR: TCA9548A NOT found!");
        while (1);
    }

    Serial.println("\nInitializing readers...");
    for (int i = 0; i < NUM_READERS; i++) {
        Serial.print("Reader ");
        Serial.print(i + 1);
        Serial.print(" (Channel ");
        Serial.print(i);
        Serial.print("): ");

        tcaSelect(i);
        delay(10);

        nfc.begin();
        uint32_t ver = nfc.getFirmwareVersion();

        if (!ver) {
            Serial.println("NOT FOUND!");
            Serial.print("  Check Reader ");
            Serial.print(i + 1);
            Serial.println(" wiring");
            g_readerStates[i].enabled = false;
        } else {
            Serial.print("OK - v");
            Serial.print((ver >> 24) & 0xFF);
            Serial.print(".");
            Serial.println((ver >> 16) & 0xFF);
            nfc.SAMConfig();
            g_readerStates[i].enabled = true;
        }
    }

    Serial.println("\n========================================");
    Serial.println("Ready — emitting binary packets");
    Serial.println("========================================\n");
}

void loop() {
    for (int reader = 0; reader < NUM_READERS; reader++) {
        ReaderState &state = g_readerStates[reader];
        if (!state.enabled) continue;

        tcaSelect(reader);
        delay(READER_SETTLE_DELAY_MS);

        uint8_t uid[MAX_UID_LEN];
        uint8_t uidLen = 0;

        if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, READ_TIMEOUT_MS)) {
            if (uidLen < 1 || uidLen > MAX_UID_LEN) {
                continue;
            }

            state.missCount = 0;

            if (!readerUidMatches(state, uid, uidLen)) {
                emitPacket((uint8_t) reader, uid, uidLen);
                readerLatch(reader, uid, uidLen);
            }
            continue;
        }

        if (!state.latched) continue;

        if (state.missCount < 0xFF) state.missCount++;
        if (state.missCount >= RELEASE_MISS_THRESHOLD) {
            readerRelease(reader);
        }
    }
}
