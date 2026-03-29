#include <Wire.h>
#include <Adafruit_PN532.h>

#define TCAADDR 0x70
#define NUM_READERS 3
#define PN532_IRQ 2
#define PN532_RESET 3

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
    Serial.println("3-Reader Test System");
    Serial.println("========================================\n");

    Wire.begin();

    // Check TCA9548A
    Wire.beginTransmission(TCAADDR);
    if (Wire.endTransmission() == 0) {
        Serial.println("TCA9548A multiplexer found");
    } else {
        Serial.println("ERROR: TCA9548A NOT found!");
        while (1);
    }

    // Test each reader
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
    Serial.println("Ready! Place cards on readers...");
    Serial.println("========================================\n");
}

void loop() {
    // Scan all 3 readers
    for (int reader = 0; reader < NUM_READERS; reader++) {
        tcaSelect(reader);
        delay(5);

        uint8_t uid[7];
        uint8_t uidLen;

        if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 50)) {
            Serial.print("Reader ");
            Serial.print(reader + 1);
            Serial.print(": Card detected - UID: ");

            for (uint8_t i = 0; i < uidLen; i++) {
                if (uid[i] < 0x10) Serial.print("0");
                Serial.print(uid[i], HEX);
                if (i < uidLen - 1) Serial.print(" ");
            }
            Serial.println();
        }
    }

    delay(200);
}