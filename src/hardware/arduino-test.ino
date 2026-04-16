#include <Wire.h>
#include <Adafruit_PN532.h>
#include <string.h>

#define TCAADDR 0x70
#define NUM_READERS 3
#define PN532_IRQ 2
#define PN532_RESET 3
#define MAX_UID_LEN 7
#define READER_SETTLE_DELAY_MS 5
#define READ_TIMEOUT_MS 50
#define RELEASE_MISS_THRESHOLD 3
#define SUMMARY_INTERVAL_MS 1000

struct ReaderState {
    bool enabled;
    bool present;
    uint8_t uid[MAX_UID_LEN];
    uint8_t uidLen;
    uint8_t missCount;
    uint32_t hitCountWindow;
    uint32_t missCountWindow;
    uint32_t readCountWindow;
    uint32_t readMicrosTotalWindow;
    uint32_t maxReadMicrosWindow;
};

Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

static ReaderState g_readerStates[NUM_READERS];
static uint32_t g_summaryWindowStartMs = 0;
static uint32_t g_sweepCountWindow = 0;
static uint32_t g_sweepMicrosTotalWindow = 0;
static uint32_t g_maxSweepMicrosWindow = 0;

void tcaSelect(uint8_t channel) {
    if (channel > 7) return;
    Wire.beginTransmission(TCAADDR);
    Wire.write(1 << channel);
    Wire.endTransmission();
}

static bool readerUidMatches(const ReaderState &state, const uint8_t *uid, uint8_t uidLen) {
    return state.present && state.uidLen == uidLen && memcmp(state.uid, uid, uidLen) == 0;
}

static void setReaderUid(ReaderState &state, const uint8_t *uid, uint8_t uidLen) {
    memcpy(state.uid, uid, uidLen);
    state.uidLen = uidLen;
    state.present = true;
    state.missCount = 0;
}

static void clearReader(ReaderState &state) {
    state.present = false;
    state.uidLen = 0;
    state.missCount = 0;
}

static void printUid(const uint8_t *uid, uint8_t uidLen) {
    for (uint8_t i = 0; i < uidLen; i++) {
        if (uid[i] < 0x10) Serial.print("0");
        Serial.print(uid[i], HEX);
        if (i + 1 < uidLen) Serial.print(" ");
    }
}

static void logDetect(int reader, const uint8_t *uid, uint8_t uidLen, uint32_t readMicros) {
    Serial.print("DETECT ms=");
    Serial.print(millis());
    Serial.print(" reader=");
    Serial.print(reader + 1);
    Serial.print(" uid=");
    printUid(uid, uidLen);
    Serial.print(" read_us=");
    Serial.println(readMicros);
}

static void logRelease(int reader) {
    Serial.print("RELEASE ms=");
    Serial.print(millis());
    Serial.print(" reader=");
    Serial.println(reader + 1);
}

static void printFloat2(float value) {
    Serial.print(value, 2);
}

static void resetSummaryWindow() {
    g_summaryWindowStartMs = millis();
    g_sweepCountWindow = 0;
    g_sweepMicrosTotalWindow = 0;
    g_maxSweepMicrosWindow = 0;

    for (int i = 0; i < NUM_READERS; i++) {
        ReaderState &state = g_readerStates[i];
        state.hitCountWindow = 0;
        state.missCountWindow = 0;
        state.readCountWindow = 0;
        state.readMicrosTotalWindow = 0;
        state.maxReadMicrosWindow = 0;
    }
}

static void printSummary() {
    const uint32_t nowMs = millis();
    uint32_t elapsedMs = nowMs - g_summaryWindowStartMs;
    if (elapsedMs == 0) elapsedMs = 1;

    const float sweepHz = (1000.0f * (float) g_sweepCountWindow) / (float) elapsedMs;
    const float avgSweepMs = (g_sweepCountWindow > 0)
        ? ((float) g_sweepMicrosTotalWindow / (float) g_sweepCountWindow) / 1000.0f
        : 0.0f;
    const float maxSweepMs = (float) g_maxSweepMicrosWindow / 1000.0f;

    Serial.print("SUMMARY ms=");
    Serial.print(nowMs);
    Serial.print(" sweep_hz=");
    printFloat2(sweepHz);
    Serial.print(" avg_sweep_ms=");
    printFloat2(avgSweepMs);
    Serial.print(" max_sweep_ms=");
    printFloat2(maxSweepMs);

    for (int i = 0; i < NUM_READERS; i++) {
        const ReaderState &state = g_readerStates[i];
        const float avgReadUs = (state.readCountWindow > 0)
            ? (float) state.readMicrosTotalWindow / (float) state.readCountWindow
            : 0.0f;

        Serial.print(" r");
        Serial.print(i + 1);
        Serial.print("_avg_us=");
        printFloat2(avgReadUs);
        Serial.print(" r");
        Serial.print(i + 1);
        Serial.print("_max_us=");
        Serial.print(state.maxReadMicrosWindow);
        Serial.print(" r");
        Serial.print(i + 1);
        Serial.print("_hits=");
        Serial.print(state.hitCountWindow);
        Serial.print(" r");
        Serial.print(i + 1);
        Serial.print("_misses=");
        Serial.print(state.missCountWindow);
    }

    Serial.println();
    resetSummaryWindow();
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Serial.println("\n========================================");
    Serial.println("3-Reader Timing Debug");
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

    Serial.println("\nTiming config:");
    Serial.print("  READER_SETTLE_DELAY_MS=");
    Serial.println(READER_SETTLE_DELAY_MS);
    Serial.print("  READ_TIMEOUT_MS=");
    Serial.println(READ_TIMEOUT_MS);
    Serial.print("  RELEASE_MISS_THRESHOLD=");
    Serial.println(RELEASE_MISS_THRESHOLD);
    Serial.print("  SUMMARY_INTERVAL_MS=");
    Serial.println(SUMMARY_INTERVAL_MS);

    Serial.println("\nEvent format:");
    Serial.println("  DETECT ms=<t> reader=<n> uid=<hex> read_us=<us>");
    Serial.println("  RELEASE ms=<t> reader=<n>");
    Serial.println("  SUMMARY ms=<t> sweep_hz=<hz> avg_sweep_ms=<ms> max_sweep_ms=<ms> ...");
    Serial.println("\n========================================");
    Serial.println("Ready! Measuring reader timing...");
    Serial.println("========================================\n");

    resetSummaryWindow();
}

void loop() {
    const uint32_t sweepStartMicros = micros();

    for (int reader = 0; reader < NUM_READERS; reader++) {
        ReaderState &state = g_readerStates[reader];
        if (!state.enabled) continue;

        tcaSelect(reader);
        delay(READER_SETTLE_DELAY_MS);

        uint8_t uid[MAX_UID_LEN];
        uint8_t uidLen = 0;

        const uint32_t readStartMicros = micros();
        const bool found = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, READ_TIMEOUT_MS);
        const uint32_t readMicros = micros() - readStartMicros;

        state.readCountWindow++;
        state.readMicrosTotalWindow += readMicros;
        if (readMicros > state.maxReadMicrosWindow) {
            state.maxReadMicrosWindow = readMicros;
        }

        if (found && uidLen >= 1 && uidLen <= MAX_UID_LEN) {
            state.hitCountWindow++;

            if (!readerUidMatches(state, uid, uidLen)) {
                logDetect(reader, uid, uidLen, readMicros);
                setReaderUid(state, uid, uidLen);
            } else {
                state.missCount = 0;
            }
            continue;
        }

        state.missCountWindow++;

        if (!state.present) continue;

        if (state.missCount < 0xFF) state.missCount++;
        if (state.missCount >= RELEASE_MISS_THRESHOLD) {
            logRelease(reader);
            clearReader(state);
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
