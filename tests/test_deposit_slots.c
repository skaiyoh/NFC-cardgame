/*
 * Unit tests for src/logic/deposit_slots.c
 *
 * Self-contained: redefines minimal type stubs and includes
 * deposit_slots.c directly to avoid the heavy types.h include chain
 * (same style as tests/test_spawn_placement.c).
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ---- Prevent heavy-header pull-in ---- */
#define NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_CONFIG_H
#define NFC_CARDGAME_DEPOSIT_SLOTS_H

/* ---- Config defines (mirror src/core/config.h) ---- */
#define BOARD_WIDTH                        1080
#define BOARD_HEIGHT                       1920
#define BASE_NAV_RADIUS                    56.0f
#define BASE_DEPOSIT_PRIMARY_SLOT_COUNT    4
#define BASE_DEPOSIT_QUEUE_SLOT_COUNT      6
#define BASE_DEPOSIT_TOTAL_SLOT_COUNT      (BASE_DEPOSIT_PRIMARY_SLOT_COUNT + BASE_DEPOSIT_QUEUE_SLOT_COUNT)
#define BASE_DEPOSIT_PRIMARY_ARC_DEGREES   160.0f
#define BASE_DEPOSIT_QUEUE_ARC_DEGREES     170.0f
#define BASE_DEPOSIT_SLOT_GAP              40.0f
#define BASE_DEPOSIT_QUEUE_RADIAL_OFFSET   80.0f
#define FARMER_DEFAULT_BODY_RADIUS         14.0f

/* ---- Minimal type stubs (mirror src/core/types.h) ---- */
typedef struct { float x; float y; } Vector2;

typedef enum { SIDE_BOTTOM, SIDE_TOP } BattleSide;

typedef enum {
    NAV_PROFILE_LANE = 0,
    NAV_PROFILE_FREE_GOAL,
    NAV_PROFILE_STATIC
} UnitNavProfile;

typedef enum {
    DEPOSIT_SLOT_NONE = 0,
    DEPOSIT_SLOT_PRIMARY,
    DEPOSIT_SLOT_QUEUE
} DepositSlotKind;

typedef struct {
    Vector2 worldPos;
    int     claimedByEntityId;
} DepositSlot;

typedef struct {
    DepositSlot primary[BASE_DEPOSIT_PRIMARY_SLOT_COUNT];
    DepositSlot queue  [BASE_DEPOSIT_QUEUE_SLOT_COUNT];
    bool initialized;
} DepositSlotRing;

typedef struct Entity {
    Vector2 position;
    float bodyRadius;
    float navRadius;
    BattleSide presentationSide;
    DepositSlotRing depositSlots;
} Entity;

/* ---- Include production code under test ---- */
#include "../src/logic/deposit_slots.c"

/* ---- Test helpers ---- */
static Entity make_base(Vector2 position, BattleSide side) {
    Entity base;
    memset(&base, 0, sizeof(base));
    base.position = position;
    base.bodyRadius = 16.0f; /* legacy default; unused because navRadius is authored */
    base.navRadius = BASE_NAV_RADIUS;
    base.presentationSide = side;
    deposit_slots_build_for_base(&base);
    return base;
}

/* ---- Tests ---- */

/* Derived ring radii -- keep in sync with deposit_slots.c geometry:
 * primary = BASE_NAV_RADIUS + FARMER_DEFAULT_BODY_RADIUS + BASE_DEPOSIT_SLOT_GAP
 * queue   = primary + BASE_DEPOSIT_QUEUE_RADIAL_OFFSET
 */
#define EXPECTED_PRIMARY_RADIUS (BASE_NAV_RADIUS + FARMER_DEFAULT_BODY_RADIUS + BASE_DEPOSIT_SLOT_GAP)
#define EXPECTED_QUEUE_RADIUS   (EXPECTED_PRIMARY_RADIUS + BASE_DEPOSIT_QUEUE_RADIAL_OFFSET)

static void test_build_for_side_bottom_front_arc(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);

    assert(base.depositSlots.initialized);

    for (int i = 0; i < BASE_DEPOSIT_PRIMARY_SLOT_COUNT; i++) {
        DepositSlot slot = base.depositSlots.primary[i];
        float dx = slot.worldPos.x - 540.0f;
        float dy = slot.worldPos.y - 1616.0f;
        float dist = sqrtf(dx * dx + dy * dy);
        assert(fabsf(dist - EXPECTED_PRIMARY_RADIUS) < 0.1f);
        /* Front-facing arc for SIDE_BOTTOM: every slot sits ahead (y < base.y) */
        assert(slot.worldPos.y < 1616.0f);
        assert(slot.claimedByEntityId == -1);
    }

    for (int i = 0; i < BASE_DEPOSIT_QUEUE_SLOT_COUNT; i++) {
        DepositSlot slot = base.depositSlots.queue[i];
        float dx = slot.worldPos.x - 540.0f;
        float dy = slot.worldPos.y - 1616.0f;
        float dist = sqrtf(dx * dx + dy * dy);
        assert(fabsf(dist - EXPECTED_QUEUE_RADIUS) < 0.1f);
        assert(slot.worldPos.y < 1616.0f);
        assert(slot.claimedByEntityId == -1);
    }
}

static void test_build_for_side_top_front_arc(void) {
    Entity base = make_base((Vector2){ 540.0f, 304.0f }, SIDE_TOP);

    assert(base.depositSlots.initialized);
    for (int i = 0; i < BASE_DEPOSIT_PRIMARY_SLOT_COUNT; i++) {
        DepositSlot slot = base.depositSlots.primary[i];
        float dx = slot.worldPos.x - 540.0f;
        float dy = slot.worldPos.y - 304.0f;
        float dist = sqrtf(dx * dx + dy * dy);
        assert(fabsf(dist - EXPECTED_PRIMARY_RADIUS) < 0.1f);
        /* Front arc for SIDE_TOP faces +Y */
        assert(slot.worldPos.y > 304.0f);
    }
    for (int i = 0; i < BASE_DEPOSIT_QUEUE_SLOT_COUNT; i++) {
        DepositSlot slot = base.depositSlots.queue[i];
        assert(slot.worldPos.y > 304.0f);
    }
}

static void test_reserve_claims_closest_free_primary(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);

    /* Farmer standing slightly in front and to the left of the base. */
    Vector2 fromPos = { 500.0f, 1560.0f };
    int idx = -1;
    DepositSlotKind kind = deposit_slots_reserve_for(&base, 101, fromPos, &idx);
    assert(kind == DEPOSIT_SLOT_PRIMARY);
    assert(idx >= 0 && idx < BASE_DEPOSIT_PRIMARY_SLOT_COUNT);
    assert(base.depositSlots.primary[idx].claimedByEntityId == 101);

    /* The claimed slot must be the nearest unclaimed primary slot to fromPos. */
    float bestDistSq = INFINITY;
    int expectedIdx = -1;
    for (int i = 0; i < BASE_DEPOSIT_PRIMARY_SLOT_COUNT; i++) {
        Vector2 p = base.depositSlots.primary[i].worldPos;
        float d = (p.x - fromPos.x) * (p.x - fromPos.x) +
                  (p.y - fromPos.y) * (p.y - fromPos.y);
        if (d < bestDistSq) {
            bestDistSq = d;
            expectedIdx = i;
        }
    }
    assert(idx == expectedIdx);
}

static void test_reserve_falls_back_to_queue_when_primary_full(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);

    /* Claim every primary slot with phantom ids. */
    for (int i = 0; i < BASE_DEPOSIT_PRIMARY_SLOT_COUNT; i++) {
        base.depositSlots.primary[i].claimedByEntityId = 1000 + i;
    }

    Vector2 fromPos = { 540.0f, 1560.0f };
    int idx = -1;
    DepositSlotKind kind = deposit_slots_reserve_for(&base, 200, fromPos, &idx);
    assert(kind == DEPOSIT_SLOT_QUEUE);
    assert(idx >= 0 && idx < BASE_DEPOSIT_QUEUE_SLOT_COUNT);
    assert(base.depositSlots.queue[idx].claimedByEntityId == 200);
}

static void test_reserve_returns_none_when_all_full(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);

    for (int i = 0; i < BASE_DEPOSIT_PRIMARY_SLOT_COUNT; i++) {
        base.depositSlots.primary[i].claimedByEntityId = 1000 + i;
    }
    for (int i = 0; i < BASE_DEPOSIT_QUEUE_SLOT_COUNT; i++) {
        base.depositSlots.queue[i].claimedByEntityId = 2000 + i;
    }

    Vector2 fromPos = { 540.0f, 1560.0f };
    int idx = 42; /* sentinel to detect overwrite */
    DepositSlotKind kind = deposit_slots_reserve_for(&base, 300, fromPos, &idx);
    assert(kind == DEPOSIT_SLOT_NONE);
    assert(idx == -1);
}

static void test_release_for_entity_clears_both_rings(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);

    base.depositSlots.primary[2].claimedByEntityId = 777;
    base.depositSlots.queue[4].claimedByEntityId = 777;
    base.depositSlots.queue[0].claimedByEntityId = 999; /* not ours -- must survive */

    deposit_slots_release_for_entity(&base, 777);

    assert(base.depositSlots.primary[2].claimedByEntityId == -1);
    assert(base.depositSlots.queue[4].claimedByEntityId == -1);
    assert(base.depositSlots.queue[0].claimedByEntityId == 999);
}

static void test_try_promote_swaps_queue_to_primary(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);

    /* Occupy all primaries, then release one so promotion has a target. */
    for (int i = 0; i < BASE_DEPOSIT_PRIMARY_SLOT_COUNT; i++) {
        base.depositSlots.primary[i].claimedByEntityId = 1000 + i;
    }
    base.depositSlots.primary[3].claimedByEntityId = -1;

    /* Farmer 500 is already holding queue slot 2. */
    base.depositSlots.queue[2].claimedByEntityId = 500;

    int newIdx = -1;
    bool promoted = deposit_slots_try_promote(&base, 500, 2, &newIdx);
    assert(promoted);
    assert(newIdx >= 0 && newIdx < BASE_DEPOSIT_PRIMARY_SLOT_COUNT);
    assert(base.depositSlots.primary[newIdx].claimedByEntityId == 500);
    assert(base.depositSlots.queue[2].claimedByEntityId == -1);
}

static void test_try_promote_fails_when_no_primary_free(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);

    for (int i = 0; i < BASE_DEPOSIT_PRIMARY_SLOT_COUNT; i++) {
        base.depositSlots.primary[i].claimedByEntityId = 1000 + i;
    }
    base.depositSlots.queue[0].claimedByEntityId = 600;

    int newIdx = 42;
    bool promoted = deposit_slots_try_promote(&base, 600, 0, &newIdx);
    assert(!promoted);
    assert(base.depositSlots.queue[0].claimedByEntityId == 600);
    assert(newIdx == 42); /* unchanged on failure */
}

static void test_reservations_idempotent(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);

    Vector2 fromPos = { 540.0f, 1560.0f };
    int idx = -1;
    DepositSlotKind kind = deposit_slots_reserve_for(&base, 101, fromPos, &idx);
    assert(kind == DEPOSIT_SLOT_PRIMARY);

    /* Release twice -- second release is a no-op. */
    deposit_slots_release(&base, DEPOSIT_SLOT_PRIMARY, idx, 101);
    assert(base.depositSlots.primary[idx].claimedByEntityId == -1);
    deposit_slots_release(&base, DEPOSIT_SLOT_PRIMARY, idx, 101);
    assert(base.depositSlots.primary[idx].claimedByEntityId == -1);

    /* Releasing a slot not claimed by the caller is a no-op (no corruption). */
    base.depositSlots.primary[idx].claimedByEntityId = 999;
    deposit_slots_release(&base, DEPOSIT_SLOT_PRIMARY, idx, 101);
    assert(base.depositSlots.primary[idx].claimedByEntityId == 999);
}

/* N concurrent farmers reserve -- each gets a unique primary slot and no
 * two farmers share the same claim. This is the happy case where slot
 * reservation alone resolves the deadlock. */
static void test_n_farmers_get_unique_primary_slots(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);

    int claimedIdx[BASE_DEPOSIT_PRIMARY_SLOT_COUNT];
    for (int f = 0; f < BASE_DEPOSIT_PRIMARY_SLOT_COUNT; f++) {
        int idx = -1;
        /* Spread farmers across a shallow arc so closest-slot is varied. */
        Vector2 fromPos = { 420.0f + (float)f * 40.0f, 1555.0f };
        DepositSlotKind kind = deposit_slots_reserve_for(&base, 100 + f, fromPos, &idx);
        assert(kind == DEPOSIT_SLOT_PRIMARY);
        claimedIdx[f] = idx;
    }

    /* Every claimedIdx must be unique. */
    for (int i = 0; i < BASE_DEPOSIT_PRIMARY_SLOT_COUNT; i++) {
        for (int j = i + 1; j < BASE_DEPOSIT_PRIMARY_SLOT_COUNT; j++) {
            assert(claimedIdx[i] != claimedIdx[j]);
        }
    }

    /* All primary slots must now be claimed by the farmer ids we passed. */
    for (int i = 0; i < BASE_DEPOSIT_PRIMARY_SLOT_COUNT; i++) {
        int claimant = base.depositSlots.primary[i].claimedByEntityId;
        assert(claimant >= 100 && claimant < 100 + BASE_DEPOSIT_PRIMARY_SLOT_COUNT);
    }
}

/* Full drain cycle: primary + queue farmers. As primaries release, queue
 * farmers promote and eventually every farmer reaches a primary slot, fully
 * draining the ring. Exercises try_promote under contention. */
static void test_full_drain_through_promotion(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);

    int primaryIdxByEntity[BASE_DEPOSIT_TOTAL_SLOT_COUNT];
    int queueIdxByEntity[BASE_DEPOSIT_TOTAL_SLOT_COUNT];
    DepositSlotKind kindByEntity[BASE_DEPOSIT_TOTAL_SLOT_COUNT];
    bool depositedByEntity[BASE_DEPOSIT_TOTAL_SLOT_COUNT];

    for (int f = 0; f < BASE_DEPOSIT_TOTAL_SLOT_COUNT; f++) {
        primaryIdxByEntity[f] = -1;
        queueIdxByEntity[f] = -1;
        kindByEntity[f] = DEPOSIT_SLOT_NONE;
        depositedByEntity[f] = false;
    }

    /* All farmers enter FARMER_RETURNING on the same tick and reserve. */
    Vector2 fromPos = { 540.0f, 1560.0f };
    for (int f = 0; f < BASE_DEPOSIT_TOTAL_SLOT_COUNT; f++) {
        int idx = -1;
        DepositSlotKind kind = deposit_slots_reserve_for(&base, 200 + f, fromPos, &idx);
        assert(kind != DEPOSIT_SLOT_NONE);
        kindByEntity[f] = kind;
        if (kind == DEPOSIT_SLOT_PRIMARY) {
            primaryIdxByEntity[f] = idx;
        } else {
            queueIdxByEntity[f] = idx;
        }
    }

    /* Primary and queue counts must match the configured slot counts. */
    int primaryCount = 0;
    int queueCount = 0;
    for (int f = 0; f < BASE_DEPOSIT_TOTAL_SLOT_COUNT; f++) {
        if (kindByEntity[f] == DEPOSIT_SLOT_PRIMARY) primaryCount++;
        if (kindByEntity[f] == DEPOSIT_SLOT_QUEUE) queueCount++;
    }
    assert(primaryCount == BASE_DEPOSIT_PRIMARY_SLOT_COUNT);
    assert(queueCount == BASE_DEPOSIT_QUEUE_SLOT_COUNT);

    /* Drain: each tick, every primary holder "deposits" and releases, then
     * every queue holder tries to promote. Run until every farmer has
     * deposited. Deterministic: serial iteration mirrors the id-sorted
     * update order the live game uses. */
    int maxTicks = 50;
    int deposited = 0;
    for (int tick = 0; tick < maxTicks && deposited < BASE_DEPOSIT_TOTAL_SLOT_COUNT; tick++) {
        /* Primary holders deposit this tick. */
        for (int f = 0; f < BASE_DEPOSIT_TOTAL_SLOT_COUNT; f++) {
            if (kindByEntity[f] != DEPOSIT_SLOT_PRIMARY) continue;
            if (depositedByEntity[f]) continue;
            deposit_slots_release(&base, DEPOSIT_SLOT_PRIMARY,
                                  primaryIdxByEntity[f], 200 + f);
            kindByEntity[f] = DEPOSIT_SLOT_NONE;
            primaryIdxByEntity[f] = -1;
            depositedByEntity[f] = true;
            deposited++;
        }
        /* Queue holders try to promote. */
        for (int f = 0; f < BASE_DEPOSIT_TOTAL_SLOT_COUNT; f++) {
            if (kindByEntity[f] != DEPOSIT_SLOT_QUEUE) continue;
            int newIdx = -1;
            if (deposit_slots_try_promote(&base, 200 + f,
                                          queueIdxByEntity[f], &newIdx)) {
                kindByEntity[f] = DEPOSIT_SLOT_PRIMARY;
                primaryIdxByEntity[f] = newIdx;
                queueIdxByEntity[f] = -1;
            }
        }
    }

    /* Every farmer eventually deposited. */
    for (int f = 0; f < BASE_DEPOSIT_TOTAL_SLOT_COUNT; f++) {
        assert(depositedByEntity[f]);
    }

    /* Rings fully drained at the end. */
    for (int i = 0; i < BASE_DEPOSIT_PRIMARY_SLOT_COUNT; i++) {
        assert(base.depositSlots.primary[i].claimedByEntityId == -1);
    }
    for (int i = 0; i < BASE_DEPOSIT_QUEUE_SLOT_COUNT; i++) {
        assert(base.depositSlots.queue[i].claimedByEntityId == -1);
    }
}

/* Re-reserving for an entity that already holds a slot releases the old
 * reservation before claiming the new one -- no double-booking on state
 * transition bounce (e.g. farmer re-entering FARMER_RETURNING). */
static void test_reserve_releases_stale_claim_for_same_entity(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);

    Vector2 leftFrom = { 460.0f, 1560.0f };
    int firstIdx = -1;
    DepositSlotKind firstKind = deposit_slots_reserve_for(&base, 500,
                                                           leftFrom, &firstIdx);
    assert(firstKind == DEPOSIT_SLOT_PRIMARY);

    Vector2 rightFrom = { 620.0f, 1560.0f };
    int secondIdx = -1;
    DepositSlotKind secondKind = deposit_slots_reserve_for(&base, 500,
                                                            rightFrom, &secondIdx);
    assert(secondKind == DEPOSIT_SLOT_PRIMARY);

    /* First slot must be vacated; second slot must be the only one owned
     * by entity 500 across both rings. */
    int slotsHeldBy500 = 0;
    for (int i = 0; i < BASE_DEPOSIT_PRIMARY_SLOT_COUNT; i++) {
        if (base.depositSlots.primary[i].claimedByEntityId == 500) {
            slotsHeldBy500++;
            assert(i == secondIdx);
        }
    }
    for (int i = 0; i < BASE_DEPOSIT_QUEUE_SLOT_COUNT; i++) {
        assert(base.depositSlots.queue[i].claimedByEntityId != 500);
    }
    assert(slotsHeldBy500 == 1);
}

/* Null-safety guarantees for the public API. */
static void test_null_base_no_crash(void) {
    int idx = 99;
    DepositSlotKind kind = deposit_slots_reserve_for(NULL, 1, (Vector2){0,0}, &idx);
    assert(kind == DEPOSIT_SLOT_NONE);
    assert(idx == -1);

    int newIdx = -1;
    assert(!deposit_slots_try_promote(NULL, 1, 0, &newIdx));

    deposit_slots_release(NULL, DEPOSIT_SLOT_PRIMARY, 0, 1);   /* no crash */
    deposit_slots_release_for_entity(NULL, 1);                  /* no crash */

    Vector2 pos = deposit_slots_get_position(NULL, DEPOSIT_SLOT_PRIMARY, 0);
    assert(pos.x == 0.0f && pos.y == 0.0f);

    assert(deposit_slots_primary_at(NULL, 0) == NULL);
    assert(deposit_slots_queue_at(NULL, 0) == NULL);
}

/* Out-of-range slot indices return safely. */
static void test_out_of_range_indices_safe(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);

    assert(deposit_slots_primary_at(&base, -1) == NULL);
    assert(deposit_slots_primary_at(&base, BASE_DEPOSIT_PRIMARY_SLOT_COUNT) == NULL);
    assert(deposit_slots_queue_at(&base, -1) == NULL);
    assert(deposit_slots_queue_at(&base, BASE_DEPOSIT_QUEUE_SLOT_COUNT) == NULL);

    Vector2 bad1 = deposit_slots_get_position(&base, DEPOSIT_SLOT_PRIMARY, -1);
    assert(bad1.x == 0.0f && bad1.y == 0.0f);
    Vector2 bad2 = deposit_slots_get_position(&base, DEPOSIT_SLOT_QUEUE,
                                               BASE_DEPOSIT_QUEUE_SLOT_COUNT);
    assert(bad2.x == 0.0f && bad2.y == 0.0f);

    /* Release on out-of-range slot is a silent no-op. */
    deposit_slots_release(&base, DEPOSIT_SLOT_PRIMARY, -1, 42);
    deposit_slots_release(&base, DEPOSIT_SLOT_QUEUE, 99, 42);
}

int main(void) {
    printf("Running deposit_slots tests...\n");
    test_build_for_side_bottom_front_arc();
    printf("  PASS: test_build_for_side_bottom_front_arc\n");
    test_build_for_side_top_front_arc();
    printf("  PASS: test_build_for_side_top_front_arc\n");
    test_reserve_claims_closest_free_primary();
    printf("  PASS: test_reserve_claims_closest_free_primary\n");
    test_reserve_falls_back_to_queue_when_primary_full();
    printf("  PASS: test_reserve_falls_back_to_queue_when_primary_full\n");
    test_reserve_returns_none_when_all_full();
    printf("  PASS: test_reserve_returns_none_when_all_full\n");
    test_release_for_entity_clears_both_rings();
    printf("  PASS: test_release_for_entity_clears_both_rings\n");
    test_try_promote_swaps_queue_to_primary();
    printf("  PASS: test_try_promote_swaps_queue_to_primary\n");
    test_try_promote_fails_when_no_primary_free();
    printf("  PASS: test_try_promote_fails_when_no_primary_free\n");
    test_reservations_idempotent();
    printf("  PASS: test_reservations_idempotent\n");
    test_n_farmers_get_unique_primary_slots();
    printf("  PASS: test_n_farmers_get_unique_primary_slots\n");
    test_full_drain_through_promotion();
    printf("  PASS: test_full_drain_through_promotion\n");
    test_reserve_releases_stale_claim_for_same_entity();
    printf("  PASS: test_reserve_releases_stale_claim_for_same_entity\n");
    test_null_base_no_crash();
    printf("  PASS: test_null_base_no_crash\n");
    test_out_of_range_indices_safe();
    printf("  PASS: test_out_of_range_indices_safe\n");
    printf("\nAll 14 tests passed!\n");
    return 0;
}
