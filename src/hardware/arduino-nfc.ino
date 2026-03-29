// arduino-nfc.ino — Production NFC sketch for NFC-cardgame.
//
// Hardware: Arduino Nano + TCA9548A I2C multiplexer + 3× PN532 NFC readers
//   TCA9548A address: 0x70
//   PN532 IRQ: pin 2, RESET: pin 3
//   Baud: 115200
//
// Protocol: binary packets emitted on Serial when a card is detected.
//   | START_BYTE (0xAA) | reader_idx | uid_len | uid_byte_0...N | checksum |
//   checksum = XOR of all preceding bytes (START_BYTE through last UID byte)
//
// Human-readable output is only emitted during setup() for serial monitor use.

#include <Wire.h>
#include <Adafruit_PN532.h>

#define TCAADDR    0x70
#define NUM_READERS 3
#define PN532_IRQ   2
#define PN532_RESET 3
#define START_BYTE  0xAA

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

void tcaSelect(uint8_t channel) {
    if (channel > 7) return;
    Wire.beginTransmission(TCAADDR);
    Wire.write(1 << channel);
    Wire.endTransmission();
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
        } else {
            Serial.print("OK - v");
            Serial.print((ver >> 24) & 0xFF);
            Serial.print(".");
            Serial.println((ver >> 16) & 0xFF);
            nfc.SAMConfig();
        }
    }

    Serial.println("\n========================================");
    Serial.println("Ready — emitting binary packets");
    Serial.println("========================================\n");
}

void loop() {
    for (int reader = 0; reader < NUM_READERS; reader++) {
        tcaSelect(reader);
        delay(5);

        uint8_t uid[7];
        uint8_t uidLen = 0;

        if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 50)) {
            // Build and emit binary packet
            uint8_t checksum = START_BYTE ^ (uint8_t) reader ^ uidLen;
            for (int i = 0; i < uidLen; i++) checksum ^= uid[i];

            Serial.write(START_BYTE);
            Serial.write((uint8_t) reader);
            Serial.write(uidLen);
            Serial.write(uid, uidLen);
            Serial.write(checksum);
        }
    }

    delay(200);
}