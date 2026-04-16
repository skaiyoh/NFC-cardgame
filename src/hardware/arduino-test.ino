#include <Wire.h>
#include <Adafruit_PN532.h>

#define TCAADDR 0x70
#define NUM_READERS 3
#define PN532_IRQ 2
#define PN532_RESET 3
#define MAX_UID_LEN 7
#define READER_SETTLE_DELAY_MS 5
#define READ_TIMEOUT_MS 50
#define SUMMARY_INTERVAL_MS 1000

struct ReaderStats {
    bool enabled;
    uint16_t hitCountWindow;
    uint16_t missCountWindow;
    uint16_t maxHitUsWindow;
    uint16_t maxMissUsWindow;
    uint32_t hitMicrosTotalWindow;
    uint32_t missMicrosTotalWindow;
};

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

static ReaderStats g_readerStats[NUM_READERS];
static uint32_t g_summaryWindowStartMs = 0;
static uint16_t g_sweepCountWindow = 0;
static uint32_t g_sweepMicrosTotalWindow = 0;
static uint32_t g_maxSweepMicrosWindow = 0;

void tcaSelect(uint8_t channel) {
    if (channel > 7) return;
    Wire.beginTransmission(TCAADDR);
    Wire.write(1 << channel);
    Wire.endTransmission();
}

static void printFixed2(uint32_t valueTimes100) {
    Serial.print(valueTimes100 / 100);
    Serial.print(".");
    const uint8_t frac = (uint8_t) (valueTimes100 % 100);
    if (frac < 10) Serial.print("0");
    Serial.print(frac);
}

static void resetSummaryWindow() {
    g_summaryWindowStartMs = millis();
    g_sweepCountWindow = 0;
    g_sweepMicrosTotalWindow = 0;
    g_maxSweepMicrosWindow = 0;

    for (int i = 0; i < NUM_READERS; i++) {
        ReaderStats &stats = g_readerStats[i];
        stats.hitCountWindow = 0;
        stats.missCountWindow = 0;
        stats.maxHitUsWindow = 0;
        stats.maxMissUsWindow = 0;
        stats.hitMicrosTotalWindow = 0;
        stats.missMicrosTotalWindow = 0;
    }
}

static void printSummary() {
    const uint32_t nowMs = millis();
    uint32_t elapsedMs = nowMs - g_summaryWindowStartMs;
    if (elapsedMs == 0) elapsedMs = 1;

    const uint32_t sweepHzTimes100 =
        ((uint32_t) g_sweepCountWindow * 100000UL + (elapsedMs / 2)) / elapsedMs;
    const uint32_t avgSweepUs = (g_sweepCountWindow > 0)
        ? (g_sweepMicrosTotalWindow / g_sweepCountWindow)
        : 0;

    Serial.print(F("SUMMARY ms="));
    Serial.print(nowMs);
    Serial.print(F(" sweep_hz="));
    printFixed2(sweepHzTimes100);
    Serial.print(F(" avg_sweep_us="));
    Serial.print(avgSweepUs);
    Serial.print(F(" max_sweep_us="));
    Serial.print(g_maxSweepMicrosWindow);

    for (int i = 0; i < NUM_READERS; i++) {
        const ReaderStats &stats = g_readerStats[i];
        const uint32_t avgHitUs = (stats.hitCountWindow > 0)
            ? (stats.hitMicrosTotalWindow / stats.hitCountWindow)
            : 0;
        const uint32_t avgMissUs = (stats.missCountWindow > 0)
            ? (stats.missMicrosTotalWindow / stats.missCountWindow)
            : 0;

        Serial.print(F(" r"));
        Serial.print(i + 1);
        Serial.print(F("_hits="));
        Serial.print(stats.hitCountWindow);
        Serial.print(F(" r"));
        Serial.print(i + 1);
        Serial.print(F("_avg_hit_us="));
        Serial.print(avgHitUs);
        Serial.print(F(" r"));
        Serial.print(i + 1);
        Serial.print(F("_max_hit_us="));
        Serial.print(stats.maxHitUsWindow);
        Serial.print(F(" r"));
        Serial.print(i + 1);
        Serial.print(F("_misses="));
        Serial.print(stats.missCountWindow);
        Serial.print(F(" r"));
        Serial.print(i + 1);
        Serial.print(F("_avg_miss_us="));
        Serial.print(avgMissUs);
        Serial.print(F(" r"));
        Serial.print(i + 1);
        Serial.print(F("_max_miss_us="));
        Serial.print(stats.maxMissUsWindow);
    }

    Serial.println();
    resetSummaryWindow();
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println(F("\n=============================="));
    Serial.println(F("3-Reader Timing Debug"));
    Serial.println(F("==============================\n"));

    Wire.begin();

    Wire.beginTransmission(TCAADDR);
    if (Wire.endTransmission() == 0) {
        Serial.println(F("TCA9548A multiplexer found"));
    } else {
        Serial.println(F("ERROR: TCA9548A NOT found!"));
        while (1);
    }

    Serial.println(F("\nInitializing readers..."));
    for (int i = 0; i < NUM_READERS; i++) {
        Serial.print(F("Reader "));
        Serial.print(i + 1);
        Serial.print(F(" (Channel "));
        Serial.print(i);
        Serial.print(F("): "));

        tcaSelect(i);
        delay(10);

        nfc.begin();
        uint32_t ver = nfc.getFirmwareVersion();

        if (!ver) {
            Serial.println(F("NOT FOUND!"));
            g_readerStats[i].enabled = false;
        } else {
            Serial.print(F("OK - v"));
            Serial.print((ver >> 24) & 0xFF);
            Serial.print(".");
            Serial.println((ver >> 16) & 0xFF);
            nfc.SAMConfig();
            g_readerStats[i].enabled = true;
        }
    }

    Serial.print(F("\nREADER_SETTLE_DELAY_MS="));
    Serial.println(READER_SETTLE_DELAY_MS);
    Serial.print(F("READ_TIMEOUT_MS="));
    Serial.println(READ_TIMEOUT_MS);
    Serial.print(F("SUMMARY_INTERVAL_MS="));
    Serial.println(SUMMARY_INTERVAL_MS);
    Serial.println(F("\nReady. Hold cards on readers and watch SUMMARY lines.\n"));

    resetSummaryWindow();
}

void loop() {
    const uint32_t sweepStartMicros = micros();

    for (int reader = 0; reader < NUM_READERS; reader++) {
        ReaderStats &stats = g_readerStats[reader];
        if (!stats.enabled) continue;

        tcaSelect(reader);
        delay(READER_SETTLE_DELAY_MS);

        uint8_t uid[MAX_UID_LEN];
        uint8_t uidLen = 0;

        const uint32_t readStartMicros = micros();
        const bool found = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, READ_TIMEOUT_MS);
        const uint16_t readMicros = (uint16_t) (micros() - readStartMicros);

        if (found && uidLen >= 1 && uidLen <= MAX_UID_LEN) {
            stats.hitCountWindow++;
            stats.hitMicrosTotalWindow += readMicros;
            if (readMicros > stats.maxHitUsWindow) {
                stats.maxHitUsWindow = readMicros;
            }
            continue;
        }

        stats.missCountWindow++;
        stats.missMicrosTotalWindow += readMicros;
        if (readMicros > stats.maxMissUsWindow) {
            stats.maxMissUsWindow = readMicros;
        }
    }

    const uint32_t sweepMicros = micros() - sweepStartMicros;
    g_sweepCountWindow++;
    g_sweepMicrosTotalWindow += sweepMicros;
    if (sweepMicros > g_maxSweepMicrosWindow) {
        g_maxSweepMicrosWindow = sweepMicros;
    }

    if (millis() - g_summaryWindowStartMs >= SUMMARY_INTERVAL_MS) {
        printSummary();
    }
}
