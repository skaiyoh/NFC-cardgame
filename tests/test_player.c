/*
 * Unit tests for src/systems/player.c resource helpers.
 *
 * Self-contained: stubs only the pieces player.c needs so we can verify
 * sustenance banking behavior without pulling in the full runtime.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ---- Prevent heavy include chains ---- */
#define NFC_CARDGAME_PLAYER_H
#define NFC_CARDGAME_ENERGY_H
#define NFC_CARDGAME_PROGRESSION_H
#define NFC_CARDGAME_BATTLEFIELD_H

/* ---- Minimal config ---- */
#define HAND_CARD_FRAME_TIME 0.05f
#define HAND_CARD_FRAME_COUNT 6
#define NUM_CARD_SLOTS 3
#define HAND_MAX_CARDS 8
#define PROGRESSION_REGEN_LEVEL1 1.0f

/* ---- Minimal type stubs ---- */
typedef struct { float x; float y; } Vector2;
typedef struct { float x; float y; float width; float height; } Rectangle;
typedef struct {
    Vector2 offset;
    Vector2 target;
    float rotation;
    float zoom;
} Camera2D;

typedef enum { SIDE_BOTTOM = 0, SIDE_TOP = 1 } BattleSide;
typedef enum {
    CARD_COST_RESOURCE_ENERGY = 0,
    CARD_COST_RESOURCE_SUSTENANCE
} CardCostResource;

typedef struct Card { int unused; } Card;
typedef struct Entity { int unused; } Entity;
typedef struct Battlefield Battlefield;

typedef struct { Vector2 v; } CanonicalPos;

typedef struct {
    Vector2 worldPos;
    Card *activeCard;
    float cooldownTimer;
} CardSlot;

typedef struct Player {
    int id;
    int sustenanceBank;
    int sustenanceCollected;
    BattleSide side;
    Rectangle screenArea;
    Rectangle battlefieldArea;
    Rectangle handArea;
    Camera2D camera;
    float cameraRotation;
    CardSlot slots[NUM_CARD_SLOTS];
    Card *handCards[HAND_MAX_CARDS];
    bool handCardAnimating[HAND_MAX_CARDS];
    float handCardAnimElapsed[HAND_MAX_CARDS];
    float energy;
    float maxEnergy;
    float energyRegenRate;
    Entity *base;
} Player;

typedef struct GameState {
    Player players[2];
} GameState;

/* ---- Dependency stubs ---- */
static int g_progression_sync_calls = 0;
static int g_last_synced_player = -1;

Rectangle bf_play_bounds(const Battlefield *bf, BattleSide side) {
    (void)bf;
    (void)side;
    return (Rectangle){0.0f, 0.0f, 100.0f, 100.0f};
}

CanonicalPos bf_spawn_pos(const Battlefield *bf, BattleSide side, int slotIndex) {
    (void)bf;
    (void)side;
    (void)slotIndex;
    return (CanonicalPos){ .v = { 0.0f, 0.0f } };
}

void energy_init(Player *p, float maxEnergy, float regenRate) {
    p->maxEnergy = maxEnergy;
    p->energyRegenRate = regenRate;
    p->energy = maxEnergy;
}

void energy_update(Player *p, float deltaTime) {
    (void)p;
    (void)deltaTime;
}

bool energy_can_afford(const Player *p, int cost) {
    return p && p->energy >= (float)cost;
}

bool energy_consume(Player *p, int cost) {
    if (!energy_can_afford(p, cost)) return false;
    p->energy -= (float)cost;
    return true;
}

void progression_sync_player(GameState *gs, int playerIndex) {
    (void)gs;
    g_progression_sync_calls++;
    g_last_synced_player = playerIndex;
}

/* ---- API declarations skipped by include guards ---- */
void player_init(Player *p, int id, BattleSide side,
                 Rectangle screenArea, Rectangle battlefieldArea, Rectangle handArea,
                 float cameraRotation, const Battlefield *bf);
void player_update(Player *p, float deltaTime);
void player_cleanup(Player *p);
CardSlot *player_get_slot(Player *p, int slotIndex);
bool player_slot_is_available(Player *p, int slotIndex);
void player_hand_set_card(Player *p, int handIndex, Card *card);
void player_hand_clear_card(Player *p, int handIndex);
Card *player_hand_get_card(const Player *p, int handIndex);
bool player_hand_slot_is_occupied(const Player *p, int handIndex);
int player_hand_occupied_count(const Player *p);
void player_hand_restart_animation_for_card(Player *p, const Card *card);
bool player_can_afford_cost(const Player *p, int amount, CardCostResource resource);
bool player_consume_cost(Player *p, int amount, CardCostResource resource);
void player_award_sustenance(GameState *gs, int playerIndex, int amount);

/* ---- Production code under test ---- */
#include "../src/systems/player.c"

/* ---- Helpers ---- */
static void reset_observers(void) {
    g_progression_sync_calls = 0;
    g_last_synced_player = -1;
}

#define RUN_TEST(fn) do { \
    reset_observers(); \
    printf("  "); \
    fn(); \
    printf("PASS: %s\n", #fn); \
} while (0)

/* ---- Tests ---- */
static void test_award_sustenance_updates_bank_lifetime_and_progression(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    gs.players[0].sustenanceBank = 2;
    gs.players[0].sustenanceCollected = 5;

    player_award_sustenance(&gs, 0, 3);

    assert(gs.players[0].sustenanceBank == 5);
    assert(gs.players[0].sustenanceCollected == 8);
    assert(g_progression_sync_calls == 1);
    assert(g_last_synced_player == 0);
}

static void test_consume_energy_cost_leaves_sustenance_bank_untouched(void) {
    Player p;
    memset(&p, 0, sizeof(p));
    p.energy = 7.0f;
    p.sustenanceBank = 4;

    assert(player_consume_cost(&p, 5, CARD_COST_RESOURCE_ENERGY));
    assert(p.energy == 2.0f);
    assert(p.sustenanceBank == 4);
}

static void test_consume_sustenance_cost_leaves_energy_untouched(void) {
    Player p;
    memset(&p, 0, sizeof(p));
    p.energy = 7.0f;
    p.sustenanceBank = 4;

    assert(player_consume_cost(&p, 3, CARD_COST_RESOURCE_SUSTENANCE));
    assert(p.energy == 7.0f);
    assert(p.sustenanceBank == 1);
}

static void test_cannot_afford_sustenance_cost_from_lifetime_only(void) {
    Player p;
    memset(&p, 0, sizeof(p));
    p.energy = 10.0f;
    p.sustenanceCollected = 12;
    p.sustenanceBank = 1;

    assert(!player_can_afford_cost(&p, 2, CARD_COST_RESOURCE_SUSTENANCE));
    assert(!player_consume_cost(&p, 2, CARD_COST_RESOURCE_SUSTENANCE));
    assert(p.energy == 10.0f);
    assert(p.sustenanceBank == 1);
    assert(p.sustenanceCollected == 12);
}

int main(void) {
    printf("Running player tests...\n");
    RUN_TEST(test_award_sustenance_updates_bank_lifetime_and_progression);
    RUN_TEST(test_consume_energy_cost_leaves_sustenance_bank_untouched);
    RUN_TEST(test_consume_sustenance_cost_leaves_energy_untouched);
    RUN_TEST(test_cannot_afford_sustenance_cost_from_lifetime_only);
    printf("\nAll player tests passed!\n");
    return 0;
}
