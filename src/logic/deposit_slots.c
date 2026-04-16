//
// Deposit slot reservation for farmers returning to base.
// See deposit_slots.h for the public contract.
//

#include "deposit_slots.h"
#include "base_geometry.h"
#include "../core/config.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>

/* Forward declarations for intra-file calls. In production builds these are
 * redundant with deposit_slots.h, but tests include this .c directly with
 * the header guard set and otherwise hit implicit-declaration warnings. */
void deposit_slots_release_for_entity(Entity *base, int entityId);

#define DEPOSIT_SLOTS_PI 3.14159265358979323846f

static Vector2 deposit_slots_rotate(Vector2 v, float degrees) {
    float rad = degrees * DEPOSIT_SLOTS_PI / 180.0f;
    float c = cosf(rad);
    float s = sinf(rad);
    return (Vector2){ v.x * c - v.y * s, v.x * s + v.y * c };
}

static float deposit_slots_dist_sq(Vector2 a, Vector2 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static void deposit_slots_build_arc(DepositSlot *slots, int count,
                                    Vector2 origin, Vector2 forward,
                                    float radius, float arcDegrees) {
    if (count <= 0) return;
    float half = arcDegrees * 0.5f;
    for (int i = 0; i < count; i++) {
        float t = (count == 1)
            ? 0.0f
            : ((float)i / (float)(count - 1)) * 2.0f - 1.0f;
        float angle = t * half;
        Vector2 dir = deposit_slots_rotate(forward, angle);
        slots[i].worldPos.x = origin.x + dir.x * radius;
        slots[i].worldPos.y = origin.y + dir.y * radius;
        slots[i].claimedByEntityId = -1;
    }
}

void deposit_slots_build_for_base(Entity *base) {
    if (!base) return;

    DepositSlotRing *ring = &base->depositSlots;
    Vector2 anchor = base_interaction_anchor(base);

    float navR = (base->navRadius > 0.0f) ? base->navRadius : base->bodyRadius;
    float primaryRadius = navR + FARMER_DEFAULT_BODY_RADIUS + BASE_DEPOSIT_SLOT_GAP;
    float queueRadius   = primaryRadius + BASE_DEPOSIT_QUEUE_RADIAL_OFFSET;

    // Forward points into the battlefield interior. A SIDE_BOTTOM base sits
    // at large Y and faces -Y; a SIDE_TOP base faces +Y.
    Vector2 forward = (base->presentationSide == SIDE_TOP)
        ? (Vector2){ 0.0f,  1.0f }
        : (Vector2){ 0.0f, -1.0f };

    deposit_slots_build_arc(ring->primary, BASE_DEPOSIT_PRIMARY_SLOT_COUNT,
                            anchor, forward, primaryRadius,
                            BASE_DEPOSIT_PRIMARY_ARC_DEGREES);
    deposit_slots_build_arc(ring->queue, BASE_DEPOSIT_QUEUE_SLOT_COUNT,
                            anchor, forward, queueRadius,
                            BASE_DEPOSIT_QUEUE_ARC_DEGREES);

    ring->initialized = true;

    // Paranoia: every slot must stay inside the canonical board. The plan's
    // geometry sanity check verified this for the authored base spawn
    // coordinates; the assert catches future drift if BASE_NAV_RADIUS, the
    // queue offset, or the base spawn gap are ever retuned.
    for (int i = 0; i < BASE_DEPOSIT_PRIMARY_SLOT_COUNT; i++) {
        assert(ring->primary[i].worldPos.x >= 0.0f);
        assert(ring->primary[i].worldPos.x <= (float)BOARD_WIDTH);
        assert(ring->primary[i].worldPos.y >= 0.0f);
        assert(ring->primary[i].worldPos.y <= (float)BOARD_HEIGHT);
    }
    for (int i = 0; i < BASE_DEPOSIT_QUEUE_SLOT_COUNT; i++) {
        assert(ring->queue[i].worldPos.x >= 0.0f);
        assert(ring->queue[i].worldPos.x <= (float)BOARD_WIDTH);
        assert(ring->queue[i].worldPos.y >= 0.0f);
        assert(ring->queue[i].worldPos.y <= (float)BOARD_HEIGHT);
    }
}

static int deposit_slots_closest_free_index(const DepositSlot *slots, int count,
                                             Vector2 fromPos) {
    int bestIdx = -1;
    float bestDistSq = INFINITY;
    for (int i = 0; i < count; i++) {
        if (slots[i].claimedByEntityId != -1) continue;
        float d = deposit_slots_dist_sq(slots[i].worldPos, fromPos);
        if (d < bestDistSq) {
            bestDistSq = d;
            bestIdx = i;
        }
    }
    return bestIdx;
}

DepositSlotKind deposit_slots_reserve_for(Entity *base, int entityId,
                                          Vector2 fromPos, int *outSlotIndex) {
    if (outSlotIndex) *outSlotIndex = -1;
    if (!base || !base->depositSlots.initialized || !outSlotIndex) {
        return DEPOSIT_SLOT_NONE;
    }

    // Release any stale reservation held by entityId first so a farmer that
    // re-enters FARMER_RETURNING does not double-book slots.
    deposit_slots_release_for_entity(base, entityId);

    DepositSlotRing *ring = &base->depositSlots;

    int idx = deposit_slots_closest_free_index(
        ring->primary, BASE_DEPOSIT_PRIMARY_SLOT_COUNT, fromPos);
    if (idx >= 0) {
        ring->primary[idx].claimedByEntityId = entityId;
        *outSlotIndex = idx;
        return DEPOSIT_SLOT_PRIMARY;
    }

    idx = deposit_slots_closest_free_index(
        ring->queue, BASE_DEPOSIT_QUEUE_SLOT_COUNT, fromPos);
    if (idx >= 0) {
        ring->queue[idx].claimedByEntityId = entityId;
        *outSlotIndex = idx;
        return DEPOSIT_SLOT_QUEUE;
    }

    return DEPOSIT_SLOT_NONE;
}

bool deposit_slots_try_promote(Entity *base, int entityId, int queueSlotIndex,
                               int *outPrimarySlotIndex) {
    if (!base || !base->depositSlots.initialized || !outPrimarySlotIndex) {
        return false;
    }
    if (queueSlotIndex < 0 || queueSlotIndex >= BASE_DEPOSIT_QUEUE_SLOT_COUNT) {
        return false;
    }

    DepositSlotRing *ring = &base->depositSlots;
    if (ring->queue[queueSlotIndex].claimedByEntityId != entityId) {
        return false;
    }

    // Promote to the primary slot closest to the farmer's current queue
    // position so the follow-up walk is as short as possible.
    Vector2 queuePos = ring->queue[queueSlotIndex].worldPos;
    int idx = deposit_slots_closest_free_index(
        ring->primary, BASE_DEPOSIT_PRIMARY_SLOT_COUNT, queuePos);
    if (idx < 0) return false;

    ring->primary[idx].claimedByEntityId = entityId;
    ring->queue[queueSlotIndex].claimedByEntityId = -1;
    *outPrimarySlotIndex = idx;
    return true;
}

void deposit_slots_release(Entity *base, DepositSlotKind kind, int slotIndex,
                           int entityId) {
    if (!base || !base->depositSlots.initialized) return;

    DepositSlotRing *ring = &base->depositSlots;

    if (kind == DEPOSIT_SLOT_PRIMARY &&
        slotIndex >= 0 && slotIndex < BASE_DEPOSIT_PRIMARY_SLOT_COUNT) {
        if (ring->primary[slotIndex].claimedByEntityId == entityId) {
            ring->primary[slotIndex].claimedByEntityId = -1;
        }
        return;
    }
    if (kind == DEPOSIT_SLOT_QUEUE &&
        slotIndex >= 0 && slotIndex < BASE_DEPOSIT_QUEUE_SLOT_COUNT) {
        if (ring->queue[slotIndex].claimedByEntityId == entityId) {
            ring->queue[slotIndex].claimedByEntityId = -1;
        }
    }
}

void deposit_slots_release_for_entity(Entity *base, int entityId) {
    if (!base || !base->depositSlots.initialized) return;

    DepositSlotRing *ring = &base->depositSlots;
    for (int i = 0; i < BASE_DEPOSIT_PRIMARY_SLOT_COUNT; i++) {
        if (ring->primary[i].claimedByEntityId == entityId) {
            ring->primary[i].claimedByEntityId = -1;
        }
    }
    for (int i = 0; i < BASE_DEPOSIT_QUEUE_SLOT_COUNT; i++) {
        if (ring->queue[i].claimedByEntityId == entityId) {
            ring->queue[i].claimedByEntityId = -1;
        }
    }
}

Vector2 deposit_slots_get_position(const Entity *base, DepositSlotKind kind,
                                   int slotIndex) {
    Vector2 zero = { 0.0f, 0.0f };
    if (!base || !base->depositSlots.initialized) return zero;

    const DepositSlotRing *ring = &base->depositSlots;

    if (kind == DEPOSIT_SLOT_PRIMARY &&
        slotIndex >= 0 && slotIndex < BASE_DEPOSIT_PRIMARY_SLOT_COUNT) {
        return ring->primary[slotIndex].worldPos;
    }
    if (kind == DEPOSIT_SLOT_QUEUE &&
        slotIndex >= 0 && slotIndex < BASE_DEPOSIT_QUEUE_SLOT_COUNT) {
        return ring->queue[slotIndex].worldPos;
    }
    return zero;
}

int deposit_slots_primary_count(const Entity *base) {
    (void)base;
    return BASE_DEPOSIT_PRIMARY_SLOT_COUNT;
}

int deposit_slots_queue_count(const Entity *base) {
    (void)base;
    return BASE_DEPOSIT_QUEUE_SLOT_COUNT;
}

const DepositSlot *deposit_slots_primary_at(const Entity *base, int slotIndex) {
    if (!base || slotIndex < 0 || slotIndex >= BASE_DEPOSIT_PRIMARY_SLOT_COUNT) {
        return NULL;
    }
    return &base->depositSlots.primary[slotIndex];
}

const DepositSlot *deposit_slots_queue_at(const Entity *base, int slotIndex) {
    if (!base || slotIndex < 0 || slotIndex >= BASE_DEPOSIT_QUEUE_SLOT_COUNT) {
        return NULL;
    }
    return &base->depositSlots.queue[slotIndex];
}
