/*
 * Unit tests for src/systems/progression.c.
 *
 * Self-contained: stubs just enough GameState/Player/Entity to cover
 * progression_sync_player without pulling in the full types.h include chain.
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ---- Prevent heavy include chains ---- */
#define NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_PROGRESSION_H

/* ---- Minimal type stubs ---- */
typedef struct { float x; float y; } Vector2;

typedef struct Entity {
    int baseLevel;
    bool alive;
    bool markedForRemoval;
} Entity;

typedef struct Player {
    float energyRegenRate;
    Entity *base;
    int sustenanceBank;
    int sustenanceCollected;
} Player;

typedef struct GameState {
    Player players[2];
} GameState;

/* Pull the constants and function prototypes from the header without using
 * the include guard (we already set NFC_CARDGAME_PROGRESSION_H above). */
#define PROGRESSION_MAX_LEVEL            10
#define PROGRESSION_SUSTENANCE_PER_LEVEL 10
#define PROGRESSION_REGEN_LEVEL1         1.0f
#define PROGRESSION_REGEN_LEVEL_MAX      2.0f
#define PROGRESSION_KING_DMG_LEVEL1      28
#define PROGRESSION_KING_DMG_LEVEL_MAX   55
#define PROGRESSION_KING_BURST_RADIUS    160.0f

int   progression_level_from_sustenance(int sustenance);
float progression_regen_rate_for_level(int level);
int   progression_king_burst_damage_for_level(int level);
void  progression_sync_player(GameState *gs, int playerIndex);

/* ---- Production code under test ---- */
#include "../src/systems/progression.c"

/* ---- Helpers ---- */
static bool approx_eq(float a, float b, float eps) {
    return fabsf(a - b) < eps;
}

#define RUN_TEST(fn) do { \
    printf("  "); \
    fn(); \
    printf("PASS: %s\n", #fn); \
} while (0)

/* ---- Tests ---- */
static void test_level_thresholds(void) {
    assert(progression_level_from_sustenance(0) == 1);
    assert(progression_level_from_sustenance(9) == 1);
    assert(progression_level_from_sustenance(10) == 2);
    assert(progression_level_from_sustenance(19) == 2);
    assert(progression_level_from_sustenance(20) == 3);
    assert(progression_level_from_sustenance(89) == 9);
    assert(progression_level_from_sustenance(90) == 10);
    assert(progression_level_from_sustenance(9999) == 10); /* clamp */
    assert(progression_level_from_sustenance(-50) == 1);   /* defensive */
}

static void test_regen_rate_curve(void) {
    assert(approx_eq(progression_regen_rate_for_level(1), 1.0f, 0.0001f));
    assert(approx_eq(progression_regen_rate_for_level(10), 2.0f, 0.0001f));

    float lv5 = progression_regen_rate_for_level(5);
    assert(lv5 > 1.0f && lv5 < 2.0f);
    /* level 5 sits at (5-1)/9 = 0.4444 of the span. */
    assert(approx_eq(lv5, 1.0f + (4.0f / 9.0f), 0.0001f));

    /* Clamp below/above bounds. */
    assert(approx_eq(progression_regen_rate_for_level(0), 1.0f, 0.0001f));
    assert(approx_eq(progression_regen_rate_for_level(99), 2.0f, 0.0001f));
}

static void test_king_burst_damage_curve(void) {
    int expected[11] = { 0, 28, 31, 34, 37, 40, 43, 46, 49, 52, 55 };
    for (int lv = 1; lv <= 10; lv++) {
        assert(progression_king_burst_damage_for_level(lv) == expected[lv]);
    }
    /* Clamp. */
    assert(progression_king_burst_damage_for_level(0) == 28);
    assert(progression_king_burst_damage_for_level(42) == 55);
}

static void test_sync_player_updates_base_level_and_regen(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    Entity base = { .alive = true, .markedForRemoval = false, .baseLevel = 1 };
    gs.players[0].base = &base;
    gs.players[0].sustenanceCollected = 0;

    progression_sync_player(&gs, 0);
    assert(base.baseLevel == 1);
    assert(approx_eq(gs.players[0].energyRegenRate, 1.0f, 0.0001f));

    gs.players[0].sustenanceCollected = 30;
    progression_sync_player(&gs, 0);
    assert(base.baseLevel == 4);
    assert(approx_eq(gs.players[0].energyRegenRate,
                     1.0f + (3.0f / 9.0f), 0.0001f));

    gs.players[0].sustenanceCollected = 200;
    progression_sync_player(&gs, 0);
    assert(base.baseLevel == 10);
    assert(approx_eq(gs.players[0].energyRegenRate, 2.0f, 0.0001f));
}

static void test_sync_player_ignores_spendable_sustenance_bank(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    Entity base = { .alive = true, .markedForRemoval = false, .baseLevel = 1 };
    gs.players[0].base = &base;
    gs.players[0].sustenanceCollected = 30;
    gs.players[0].sustenanceBank = 0;

    progression_sync_player(&gs, 0);
    assert(base.baseLevel == 4);
    assert(approx_eq(gs.players[0].energyRegenRate,
                     progression_regen_rate_for_level(4), 0.0001f));
}

static void test_sync_player_without_live_base_still_updates_regen(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    gs.players[1].base = NULL;
    gs.players[1].sustenanceCollected = 50;

    progression_sync_player(&gs, 1);
    assert(gs.players[1].base == NULL);
    assert(approx_eq(gs.players[1].energyRegenRate,
                     progression_regen_rate_for_level(6), 0.0001f));
}

static void test_sync_player_skips_dead_base(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    Entity base = { .alive = false, .markedForRemoval = false, .baseLevel = 4 };
    gs.players[0].base = &base;
    gs.players[0].sustenanceCollected = 90;

    progression_sync_player(&gs, 0);
    assert(base.baseLevel == 4);          /* stale level preserved; base is dead */
    /* Regen still reflects current sustenance. */
    assert(approx_eq(gs.players[0].energyRegenRate, 2.0f, 0.0001f));
}

static void test_sync_player_ignores_bad_index(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    gs.players[0].energyRegenRate = 5.0f;
    progression_sync_player(&gs, -1);
    progression_sync_player(&gs, 2);
    assert(approx_eq(gs.players[0].energyRegenRate, 5.0f, 0.0001f));
}

int main(void) {
    printf("Running progression tests...\n");
    RUN_TEST(test_level_thresholds);
    RUN_TEST(test_regen_rate_curve);
    RUN_TEST(test_king_burst_damage_curve);
    RUN_TEST(test_sync_player_updates_base_level_and_regen);
    RUN_TEST(test_sync_player_ignores_spendable_sustenance_bank);
    RUN_TEST(test_sync_player_without_live_base_still_updates_regen);
    RUN_TEST(test_sync_player_skips_dead_base);
    RUN_TEST(test_sync_player_ignores_bad_index);
    printf("\nAll progression tests passed!\n");
    return 0;
}
