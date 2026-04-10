/*
 * Unit tests for src/logic/assault_slots.c
 *
 * Self-contained: redefines minimal type stubs and includes
 * assault_slots.c directly to avoid the heavy types.h include chain.
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_CONFIG_H
#define NFC_CARDGAME_ASSAULT_SLOTS_H

#define BOARD_WIDTH                        1080
#define BOARD_HEIGHT                       1920
#define BASE_NAV_RADIUS                    56.0f
#define DEFAULT_MELEE_BODY_RADIUS          14.0f
#define BASE_ASSAULT_PRIMARY_SLOT_COUNT    8
#define BASE_ASSAULT_QUEUE_SLOT_COUNT      8
#define BASE_ASSAULT_TOTAL_SLOT_COUNT      (BASE_ASSAULT_PRIMARY_SLOT_COUNT + BASE_ASSAULT_QUEUE_SLOT_COUNT)
#define BASE_ASSAULT_PRIMARY_ARC_DEGREES   150.0f
#define BASE_ASSAULT_QUEUE_ARC_DEGREES     170.0f
#define BASE_ASSAULT_SLOT_GAP              2.0f
#define BASE_ASSAULT_QUEUE_RADIAL_OFFSET   22.0f
#define COMBAT_BUILDING_MELEE_INSET        30.0f

typedef struct { float x; float y; } Vector2;

typedef enum { SIDE_BOTTOM, SIDE_TOP } BattleSide;

typedef enum {
    ASSAULT_SLOT_NONE = 0,
    ASSAULT_SLOT_PRIMARY,
    ASSAULT_SLOT_QUEUE
} AssaultSlotKind;

typedef struct {
    Vector2 worldPos;
    int claimedByEntityId;
} AssaultSlot;

typedef struct {
    AssaultSlot primary[BASE_ASSAULT_PRIMARY_SLOT_COUNT];
    AssaultSlot queue[BASE_ASSAULT_QUEUE_SLOT_COUNT];
    bool initialized;
} AssaultSlotRing;

typedef struct Entity {
    Vector2 position;
    float bodyRadius;
    float navRadius;
    BattleSide presentationSide;
    AssaultSlotRing assaultSlots;
} Entity;

#include "../src/logic/assault_slots.c"

static Entity make_base(Vector2 position, BattleSide side) {
    Entity base;
    memset(&base, 0, sizeof(base));
    base.position = position;
    base.bodyRadius = 16.0f;
    base.navRadius = BASE_NAV_RADIUS;
    base.presentationSide = side;
    assault_slots_build_for_base(&base);
    return base;
}

#define EXPECTED_CONTACT_RADIUS fmaxf(16.0f, BASE_NAV_RADIUS - COMBAT_BUILDING_MELEE_INSET)
#define EXPECTED_PRIMARY_RADIUS (EXPECTED_CONTACT_RADIUS + DEFAULT_MELEE_BODY_RADIUS + BASE_ASSAULT_SLOT_GAP)
#define EXPECTED_QUEUE_RADIUS   (EXPECTED_PRIMARY_RADIUS + BASE_ASSAULT_QUEUE_RADIAL_OFFSET)

static void test_build_for_side_bottom_front_arc(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);

    assert(base.assaultSlots.initialized);
    for (int i = 0; i < BASE_ASSAULT_PRIMARY_SLOT_COUNT; i++) {
        AssaultSlot slot = base.assaultSlots.primary[i];
        float dx = slot.worldPos.x - 540.0f;
        float dy = slot.worldPos.y - 1616.0f;
        float dist = sqrtf(dx * dx + dy * dy);
        assert(fabsf(dist - EXPECTED_PRIMARY_RADIUS) < 0.1f);
        assert(slot.worldPos.y < 1616.0f);
        assert(slot.claimedByEntityId == -1);
    }
}

static void test_build_for_side_top_front_arc(void) {
    Entity base = make_base((Vector2){ 540.0f, 304.0f }, SIDE_TOP);

    assert(base.assaultSlots.initialized);
    for (int i = 0; i < BASE_ASSAULT_QUEUE_SLOT_COUNT; i++) {
        AssaultSlot slot = base.assaultSlots.queue[i];
        float dx = slot.worldPos.x - 540.0f;
        float dy = slot.worldPos.y - 304.0f;
        float dist = sqrtf(dx * dx + dy * dy);
        assert(fabsf(dist - EXPECTED_QUEUE_RADIUS) < 0.1f);
        assert(slot.worldPos.y > 304.0f);
        assert(slot.claimedByEntityId == -1);
    }
}

static void test_first_attackers_get_unique_primary_slots(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);
    int claimedIdx[BASE_ASSAULT_PRIMARY_SLOT_COUNT];

    for (int i = 0; i < BASE_ASSAULT_PRIMARY_SLOT_COUNT; i++) {
        int idx = -1;
        Vector2 fromPos = { 420.0f + (float)i * 32.0f, 1540.0f };
        AssaultSlotKind kind = assault_slots_reserve_for(&base, 100 + i, fromPos, &idx);
        assert(kind == ASSAULT_SLOT_PRIMARY);
        claimedIdx[i] = idx;
    }

    for (int i = 0; i < BASE_ASSAULT_PRIMARY_SLOT_COUNT; i++) {
        for (int j = i + 1; j < BASE_ASSAULT_PRIMARY_SLOT_COUNT; j++) {
            assert(claimedIdx[i] != claimedIdx[j]);
        }
    }
}

static void test_overflow_falls_back_to_queue_ring(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);

    for (int i = 0; i < BASE_ASSAULT_PRIMARY_SLOT_COUNT; i++) {
        base.assaultSlots.primary[i].claimedByEntityId = 1000 + i;
    }

    int idx = -1;
    AssaultSlotKind kind = assault_slots_reserve_for(&base, 200, (Vector2){ 540.0f, 1540.0f }, &idx);
    assert(kind == ASSAULT_SLOT_QUEUE);
    assert(idx >= 0 && idx < BASE_ASSAULT_QUEUE_SLOT_COUNT);
    assert(base.assaultSlots.queue[idx].claimedByEntityId == 200);
}

static void test_queue_promotes_when_primary_frees(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);

    for (int i = 0; i < BASE_ASSAULT_PRIMARY_SLOT_COUNT; i++) {
        base.assaultSlots.primary[i].claimedByEntityId = 3000 + i;
    }
    base.assaultSlots.primary[3].claimedByEntityId = -1;
    base.assaultSlots.queue[5].claimedByEntityId = 500;

    int newIdx = -1;
    bool promoted = assault_slots_try_promote(&base, 500, 5, &newIdx);
    assert(promoted);
    assert(newIdx >= 0 && newIdx < BASE_ASSAULT_PRIMARY_SLOT_COUNT);
    assert(base.assaultSlots.primary[newIdx].claimedByEntityId == 500);
    assert(base.assaultSlots.queue[5].claimedByEntityId == -1);
}

static void test_full_ring_drains_through_promotions(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);
    AssaultSlotKind kindByEntity[BASE_ASSAULT_TOTAL_SLOT_COUNT];
    int primaryIdx[BASE_ASSAULT_TOTAL_SLOT_COUNT];
    int queueIdx[BASE_ASSAULT_TOTAL_SLOT_COUNT];
    bool released[BASE_ASSAULT_TOTAL_SLOT_COUNT];

    for (int i = 0; i < BASE_ASSAULT_TOTAL_SLOT_COUNT; i++) {
        kindByEntity[i] = ASSAULT_SLOT_NONE;
        primaryIdx[i] = -1;
        queueIdx[i] = -1;
        released[i] = false;
    }

    for (int i = 0; i < BASE_ASSAULT_TOTAL_SLOT_COUNT; i++) {
        int idx = -1;
        AssaultSlotKind kind = assault_slots_reserve_for(&base, 700 + i,
                                                         (Vector2){ 540.0f, 1540.0f }, &idx);
        assert(kind != ASSAULT_SLOT_NONE);
        kindByEntity[i] = kind;
        if (kind == ASSAULT_SLOT_PRIMARY) primaryIdx[i] = idx;
        if (kind == ASSAULT_SLOT_QUEUE) queueIdx[i] = idx;
    }

    int releasedCount = 0;
    for (int tick = 0; tick < 64 && releasedCount < BASE_ASSAULT_TOTAL_SLOT_COUNT; tick++) {
        for (int i = 0; i < BASE_ASSAULT_TOTAL_SLOT_COUNT; i++) {
            if (kindByEntity[i] != ASSAULT_SLOT_PRIMARY || released[i]) continue;
            assault_slots_release(&base, ASSAULT_SLOT_PRIMARY, primaryIdx[i], 700 + i);
            kindByEntity[i] = ASSAULT_SLOT_NONE;
            released[i] = true;
            releasedCount++;
        }
        for (int i = 0; i < BASE_ASSAULT_TOTAL_SLOT_COUNT; i++) {
            if (kindByEntity[i] != ASSAULT_SLOT_QUEUE) continue;
            int newIdx = -1;
            if (assault_slots_try_promote(&base, 700 + i, queueIdx[i], &newIdx)) {
                kindByEntity[i] = ASSAULT_SLOT_PRIMARY;
                primaryIdx[i] = newIdx;
                queueIdx[i] = -1;
            }
        }
    }

    for (int i = 0; i < BASE_ASSAULT_TOTAL_SLOT_COUNT; i++) {
        assert(released[i]);
    }
    for (int i = 0; i < BASE_ASSAULT_PRIMARY_SLOT_COUNT; i++) {
        assert(base.assaultSlots.primary[i].claimedByEntityId == -1);
    }
    for (int i = 0; i < BASE_ASSAULT_QUEUE_SLOT_COUNT; i++) {
        assert(base.assaultSlots.queue[i].claimedByEntityId == -1);
    }
}

static void test_slot_positions_stay_on_front_arc(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);
    for (int i = 0; i < BASE_ASSAULT_PRIMARY_SLOT_COUNT; i++) {
        assert(base.assaultSlots.primary[i].worldPos.y < base.position.y);
    }
    for (int i = 0; i < BASE_ASSAULT_QUEUE_SLOT_COUNT; i++) {
        assert(base.assaultSlots.queue[i].worldPos.y < base.position.y);
    }
}

static void test_release_for_entity_clears_both_rings(void) {
    Entity base = make_base((Vector2){ 540.0f, 1616.0f }, SIDE_BOTTOM);
    base.assaultSlots.primary[1].claimedByEntityId = 900;
    base.assaultSlots.queue[4].claimedByEntityId = 900;
    base.assaultSlots.queue[0].claimedByEntityId = 901;

    assault_slots_release_for_entity(&base, 900);

    assert(base.assaultSlots.primary[1].claimedByEntityId == -1);
    assert(base.assaultSlots.queue[4].claimedByEntityId == -1);
    assert(base.assaultSlots.queue[0].claimedByEntityId == 901);
}

int main(void) {
    printf("Running assault_slots tests...\n");
    test_build_for_side_bottom_front_arc();
    printf("  PASS: test_build_for_side_bottom_front_arc\n");
    test_build_for_side_top_front_arc();
    printf("  PASS: test_build_for_side_top_front_arc\n");
    test_first_attackers_get_unique_primary_slots();
    printf("  PASS: test_first_attackers_get_unique_primary_slots\n");
    test_overflow_falls_back_to_queue_ring();
    printf("  PASS: test_overflow_falls_back_to_queue_ring\n");
    test_queue_promotes_when_primary_frees();
    printf("  PASS: test_queue_promotes_when_primary_frees\n");
    test_full_ring_drains_through_promotions();
    printf("  PASS: test_full_ring_drains_through_promotions\n");
    test_slot_positions_stay_on_front_arc();
    printf("  PASS: test_slot_positions_stay_on_front_arc\n");
    test_release_for_entity_clears_both_rings();
    printf("  PASS: test_release_for_entity_clears_both_rings\n");
    printf("\nAll 8 tests passed!\n");
    return 0;
}
