/*
 * Unit tests for src/logic/card_effects.c.
 *
 * Self-contained: blocks heavy headers, defines minimal local stubs, and
 * includes the production translation unit directly.
 *
 * Focus: king card dispatch, slot gating, and animation-only base trigger.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ---- Prevent heavy include chains ---- */
#define NFC_CARDGAME_CARD_EFFECTS_H
#define NFC_CARDGAME_TROOP_H
#define NFC_CARDGAME_ENTITIES_H
#define NFC_CARDGAME_PLAYER_H
#define NFC_CARDGAME_ENERGY_H
#define NFC_CARDGAME_PROGRESSION_H
#define NFC_CARDGAME_SPAWN_H
#define NFC_CARDGAME_SPAWN_PLACEMENT_H
#define NFC_CARDGAME_BATTLEFIELD_H
#define NFC_CARDGAME_CONFIG_H

/* ---- Minimal config ---- */
#define NUM_CARD_SLOTS 3
#define BOARD_WIDTH 1080.0f
#define BOARD_HEIGHT 1920.0f
#define BF_ASSERT_IN_BOUNDS(pos, width, height) ((void)(pos), (void)(width), (void)(height))

/* ---- Minimal type stubs ---- */
typedef struct { float x; float y; } Vector2;
typedef struct { Vector2 v; } CanonicalPos;

typedef enum { SIDE_BOTTOM, SIDE_TOP } BattleSide;
typedef enum { TARGET_NEAREST, TARGET_BUILDING, TARGET_SPECIFIC_TYPE } TargetingMode;
typedef enum {
    SPRITE_TYPE_KNIGHT,
    SPRITE_TYPE_HEALER,
    SPRITE_TYPE_ASSASSIN,
    SPRITE_TYPE_BRUTE,
    SPRITE_TYPE_FARMER,
    SPRITE_TYPE_FARMER_FULL,
    SPRITE_TYPE_BIRD,
    SPRITE_TYPE_FISHFING,
    SPRITE_TYPE_BASE,
    SPRITE_TYPE_COUNT
} SpriteType;
typedef enum { UNIT_ROLE_COMBAT, UNIT_ROLE_FARMER } UnitRole;
typedef enum { ESTATE_IDLE, ESTATE_WALKING, ESTATE_ATTACKING, ESTATE_DEAD } EntityState;

typedef struct Card {
    char *card_id;
    char *name;
    int cost;
    char *type;
    char *rules_text;
    char *data;
} Card;

typedef struct SpriteAtlas {
    int unused;
} SpriteAtlas;

typedef struct Entity {
    int id;
    Vector2 position;
    int lane;
    float laneProgress;
    int waypointIndex;
    UnitRole unitRole;
    bool alive;
    bool markedForRemoval;
    EntityState state;
    int attackTargetId;
    int baseLevel;
    bool basePendingKingBurst;
    int basePendingKingBurstDamage;
} Entity;

typedef struct {
    Vector2 worldPos;
    Card *activeCard;
    float cooldownTimer;
} CardSlot;

typedef struct Player {
    float energy;
    float maxEnergy;
    float energyRegenRate;
    Entity *base;
    CardSlot slots[NUM_CARD_SLOTS];
} Player;

typedef struct GameState {
    Player players[2];
    SpriteAtlas spriteAtlas;
} GameState;

typedef struct {
    const char *name;
    int hp, maxHP;
    int attack;
    int healAmount;
    float attackSpeed;
    float attackRange;
    float moveSpeed;
    TargetingMode targeting;
    const char *targetType;
    SpriteType spriteType;
    float bodyRadius;
} TroopData;

typedef enum {
    SPAWN_FX_NONE = 0,
    SPAWN_FX_SMOKE = 1,
} SpawnFxKind;

typedef void (*CardPlayFn)(const Card *card, GameState *state, int playerIndex, int slotIndex);

/* ---- Test observers ---- */
static int g_entity_set_state_calls = 0;
static int g_entity_restart_clip_calls = 0;
static int g_player_hand_restart_calls = 0;
static int g_spawn_find_calls = 0;
static int g_troop_create_data_calls = 0;
static int g_troop_spawn_calls = 0;
static int g_spawn_register_calls = 0;
static const Card *g_last_hand_restart_card = NULL;
static Player *g_last_hand_restart_player = NULL;

/* ---- Minimal dependency stubs ---- */
CardSlot *player_get_slot(Player *p, int slotIndex) {
    if (!p || slotIndex < 0 || slotIndex >= NUM_CARD_SLOTS) return NULL;
    return &p->slots[slotIndex];
}

void player_hand_restart_animation_for_card(Player *p, const Card *card) {
    g_player_hand_restart_calls++;
    g_last_hand_restart_player = p;
    g_last_hand_restart_card = card;
}

bool energy_can_afford(Player *p, int cost) {
    return p && p->energy >= (float)cost;
}

bool energy_consume(Player *p, int cost) {
    if (!energy_can_afford(p, cost)) return false;
    p->energy -= (float)cost;
    return true;
}

BattleSide bf_side_for_player(int playerIndex) {
    return (playerIndex == 0) ? SIDE_BOTTOM : SIDE_TOP;
}

int bf_slot_to_lane(BattleSide side, int slotIndex) {
    (void)side;
    return slotIndex;
}

bool spawn_find_free_anchor(GameState *gs, BattleSide side, int slotIndex,
                            float bodyRadius, Vector2 *outPos) {
    (void)gs;
    (void)side;
    (void)slotIndex;
    (void)bodyRadius;
    g_spawn_find_calls++;
    if (outPos) *outPos = (Vector2){0.0f, 0.0f};
    return true;
}

TroopData troop_create_data_from_card(const Card *card) {
    g_troop_create_data_calls++;
    TroopData data = {0};
    data.name = card ? card->name : NULL;
    data.spriteType = SPRITE_TYPE_KNIGHT;
    data.bodyRadius = 14.0f;
    return data;
}

Entity *troop_spawn(Player *owner, const TroopData *data, Vector2 position,
                    const SpriteAtlas *atlas) {
    (void)owner;
    (void)data;
    (void)position;
    (void)atlas;
    static Entity spawned = {0};
    g_troop_spawn_calls++;
    spawned.alive = true;
    spawned.unitRole = UNIT_ROLE_COMBAT;
    return &spawned;
}

void spawn_register_entity(GameState *state, Entity *entity, SpawnFxKind fx) {
    (void)state;
    (void)entity;
    (void)fx;
    g_spawn_register_calls++;
}

void entity_set_state(Entity *e, EntityState newState) {
    g_entity_set_state_calls++;
    if (e) e->state = newState;
}

void entity_restart_clip(Entity *e) {
    (void)e;
    g_entity_restart_clip_calls++;
}

int progression_king_burst_damage_for_level(int level) {
    if (level < 1) level = 1;
    if (level > 10) level = 10;
    return 28 + (level - 1) * 3;
}

/* ---- Production code under test ---- */
#include "../src/logic/card_effects.c"

/* ---- Helpers ---- */
static void reset_observers(void) {
    g_entity_set_state_calls = 0;
    g_entity_restart_clip_calls = 0;
    g_player_hand_restart_calls = 0;
    g_spawn_find_calls = 0;
    g_troop_create_data_calls = 0;
    g_troop_spawn_calls = 0;
    g_spawn_register_calls = 0;
    g_last_hand_restart_card = NULL;
    g_last_hand_restart_player = NULL;
}

static GameState make_game_state(void) {
    GameState gs;
    memset(&gs, 0, sizeof(gs));
    gs.players[0].energy = 10.0f;
    gs.players[0].maxEnergy = 10.0f;
    gs.players[1].energy = 10.0f;
    gs.players[1].maxEnergy = 10.0f;
    return gs;
}

static Entity make_base(EntityState state) {
    Entity base;
    memset(&base, 0, sizeof(base));
    base.alive = true;
    base.state = state;
    base.attackTargetId = 77;
    base.baseLevel = 1;
    return base;
}

static Card make_king_card(void) {
    Card card = {0};
    card.card_id = "KING_01";
    card.name = "King";
    card.cost = 4;
    card.type = "king";
    return card;
}

#define RUN_TEST(fn) do { \
    reset_observers(); \
    card_action_init(); \
    printf("  "); \
    fn(); \
    printf("PASS: %s\n", #fn); \
} while (0)

/* ---- Tests ---- */
static void test_king_dispatch_consumes_energy_and_enters_attack(void) {
    GameState gs = make_game_state();
    Entity base = make_base(ESTATE_IDLE);
    Card king = make_king_card();
    gs.players[0].base = &base;

    bool ok = card_action_play(&king, &gs, 0, 0);

    assert(ok);
    assert(gs.players[0].energy == 6.0f);
    assert(base.state == ESTATE_ATTACKING);
    assert(base.attackTargetId == -1);
    /* Level 1 base queues the level-1 burst (28 damage) but does not apply
     * it yet — the hit-marker branch in entities.c resolves damage later. */
    assert(base.basePendingKingBurst == true);
    assert(base.basePendingKingBurstDamage == 28);
    assert(g_entity_set_state_calls == 1);
    assert(g_entity_restart_clip_calls == 0);
    assert(g_player_hand_restart_calls == 1);
    assert(g_last_hand_restart_player == &gs.players[0]);
    assert(g_last_hand_restart_card == &king);
    assert(g_spawn_find_calls == 0);
    assert(g_troop_create_data_calls == 0);
    assert(g_troop_spawn_calls == 0);
    assert(g_spawn_register_calls == 0);
}

static void test_king_burst_damage_scales_with_base_level(void) {
    GameState gs = make_game_state();
    Entity base = make_base(ESTATE_IDLE);
    Card king = make_king_card();
    base.baseLevel = 10;
    gs.players[0].base = &base;

    bool ok = card_action_play(&king, &gs, 0, 0);

    assert(ok);
    assert(base.basePendingKingBurst == true);
    assert(base.basePendingKingBurstDamage == 55);
}

static void test_king_burst_overwrites_pending_on_replay(void) {
    GameState gs = make_game_state();
    Entity base = make_base(ESTATE_ATTACKING);
    Card king = make_king_card();
    base.basePendingKingBurst = true;
    base.basePendingKingBurstDamage = 999;  /* stale value from a prior frame */
    gs.players[0].base = &base;

    bool ok = card_action_play(&king, &gs, 0, 0);

    assert(ok);
    assert(base.basePendingKingBurst == true);
    assert(base.basePendingKingBurstDamage == 28);  /* overwritten, not stacked */
    assert(g_entity_restart_clip_calls == 1);
}

static void test_king_gating_leaves_pending_burst_untouched(void) {
    GameState gs = make_game_state();
    Entity base = make_base(ESTATE_IDLE);
    Card king = make_king_card();
    gs.players[0].base = &base;
    gs.players[0].energy = 1.0f;  /* cannot afford cost 4 */

    bool ok = card_action_play(&king, &gs, 0, 0);

    assert(ok);
    assert(gs.players[0].energy == 1.0f);
    assert(base.basePendingKingBurst == false);
    assert(base.basePendingKingBurstDamage == 0);
    assert(base.state == ESTATE_IDLE);
}

static void test_king_slot_cooldown_blocks_without_side_effects(void) {
    GameState gs = make_game_state();
    Entity base = make_base(ESTATE_IDLE);
    Card king = make_king_card();
    gs.players[0].base = &base;
    gs.players[0].slots[0].cooldownTimer = 1.0f;

    bool ok = card_action_play(&king, &gs, 0, 0);

    assert(ok);
    assert(gs.players[0].energy == 10.0f);
    assert(base.state == ESTATE_IDLE);
    assert(base.attackTargetId == 77);
    assert(g_entity_set_state_calls == 0);
    assert(g_entity_restart_clip_calls == 0);
    assert(g_player_hand_restart_calls == 0);
    assert(g_spawn_find_calls == 0);
    assert(g_troop_create_data_calls == 0);
    assert(g_troop_spawn_calls == 0);
    assert(g_spawn_register_calls == 0);
}

static void test_king_invalid_slot_blocks_without_side_effects(void) {
    GameState gs = make_game_state();
    Entity base = make_base(ESTATE_IDLE);
    Card king = make_king_card();
    gs.players[0].base = &base;

    bool ok = card_action_play(&king, &gs, 0, 99);

    assert(ok);
    assert(gs.players[0].energy == 10.0f);
    assert(base.state == ESTATE_IDLE);
    assert(base.attackTargetId == 77);
    assert(g_entity_set_state_calls == 0);
    assert(g_entity_restart_clip_calls == 0);
    assert(g_player_hand_restart_calls == 0);
    assert(g_spawn_find_calls == 0);
    assert(g_troop_create_data_calls == 0);
    assert(g_troop_spawn_calls == 0);
    assert(g_spawn_register_calls == 0);
}

static void test_king_missing_dead_or_marked_base_blocks_without_energy_spend(void) {
    Card king = make_king_card();

    {
        GameState gs = make_game_state();
        bool ok = card_action_play(&king, &gs, 0, 0);
        assert(ok);
        assert(gs.players[0].energy == 10.0f);
    }

    reset_observers();
    card_action_init();
    {
        GameState gs = make_game_state();
        Entity base = make_base(ESTATE_IDLE);
        base.alive = false;
        gs.players[0].base = &base;
        bool ok = card_action_play(&king, &gs, 0, 0);
        assert(ok);
        assert(gs.players[0].energy == 10.0f);
    }

    reset_observers();
    card_action_init();
    {
        GameState gs = make_game_state();
        Entity base = make_base(ESTATE_IDLE);
        base.markedForRemoval = true;
        gs.players[0].base = &base;
        bool ok = card_action_play(&king, &gs, 0, 0);
        assert(ok);
        assert(gs.players[0].energy == 10.0f);
    }

    assert(g_entity_set_state_calls == 0);
    assert(g_entity_restart_clip_calls == 0);
    assert(g_player_hand_restart_calls == 0);
    assert(g_spawn_find_calls == 0);
    assert(g_troop_create_data_calls == 0);
    assert(g_troop_spawn_calls == 0);
    assert(g_spawn_register_calls == 0);
}

static void test_king_restarts_clip_when_base_already_attacking(void) {
    GameState gs = make_game_state();
    Entity base = make_base(ESTATE_ATTACKING);
    Card king = make_king_card();
    gs.players[0].base = &base;

    bool ok = card_action_play(&king, &gs, 0, 0);

    assert(ok);
    assert(gs.players[0].energy == 6.0f);
    assert(base.state == ESTATE_ATTACKING);
    assert(base.attackTargetId == -1);
    assert(g_entity_set_state_calls == 0);
    assert(g_entity_restart_clip_calls == 1);
    assert(g_player_hand_restart_calls == 1);
    assert(g_spawn_find_calls == 0);
    assert(g_troop_create_data_calls == 0);
    assert(g_troop_spawn_calls == 0);
    assert(g_spawn_register_calls == 0);
}

static void test_catalog_type_resolution_allows_known_card_id_without_db_type(void) {
    GameState gs = make_game_state();
    Entity base = make_base(ESTATE_IDLE);
    Card king = make_king_card();
    king.type = NULL;
    gs.players[0].base = &base;

    bool ok = card_action_play(&king, &gs, 0, 0);

    assert(ok);
    assert(gs.players[0].energy == 6.0f);
    assert(base.state == ESTATE_ATTACKING);
    assert(base.attackTargetId == -1);
    assert(g_entity_set_state_calls == 1);
    assert(g_entity_restart_clip_calls == 0);
    assert(g_player_hand_restart_calls == 1);
    assert(g_troop_create_data_calls == 0);
    assert(g_troop_spawn_calls == 0);
    assert(g_spawn_register_calls == 0);
}

int main(void) {
    printf("Running card_effects tests...\n");
    RUN_TEST(test_king_dispatch_consumes_energy_and_enters_attack);
    RUN_TEST(test_king_burst_damage_scales_with_base_level);
    RUN_TEST(test_king_burst_overwrites_pending_on_replay);
    RUN_TEST(test_king_gating_leaves_pending_burst_untouched);
    RUN_TEST(test_king_slot_cooldown_blocks_without_side_effects);
    RUN_TEST(test_king_invalid_slot_blocks_without_side_effects);
    RUN_TEST(test_king_missing_dead_or_marked_base_blocks_without_energy_spend);
    RUN_TEST(test_king_restarts_clip_when_base_already_attacking);
    RUN_TEST(test_catalog_type_resolution_allows_known_card_id_without_db_type);
    printf("\nAll card_effects tests passed!\n");
    return 0;
}
