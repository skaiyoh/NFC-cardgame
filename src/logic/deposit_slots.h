//
// Deposit slot reservation for farmers returning to base.
//
// Each base exposes a front-facing arc of primary slots plus a wider
// secondary queue ring. Farmers reserve a slot and walk to the slot's
// world position instead of targeting base->position, which is inside
// the base's hard nav blocker. Mirrors the sustenance_claim API style.
//

#ifndef NFC_CARDGAME_DEPOSIT_SLOTS_H
#define NFC_CARDGAME_DEPOSIT_SLOTS_H

#include "../core/types.h"

// Populate both slot rings on a freshly-created base entity. Requires
// base->position, base->navRadius, and base->presentationSide to already
// be set. Slot geometry is centered on the base interaction anchor, not the
// raw render pivot at base->position. Called once from building_create_base.
void deposit_slots_build_for_base(Entity *base);

// Reserve the closest free primary slot to `fromPos`. If no primary is free,
// fall back to the closest free queue slot. Returns the kind of slot that
// was reserved, or DEPOSIT_SLOT_NONE if all 12 slots are taken. On success,
// writes the claimed slot index (0..count-1) into *outSlotIndex. Any previous
// reservation held by entityId is released before acquiring a new one.
DepositSlotKind deposit_slots_reserve_for(Entity *base, int entityId,
                                          Vector2 fromPos, int *outSlotIndex);

// Atomic queue->primary promotion. If a free primary slot exists, claim the
// one closest to the caller's current queue position, release the queue slot,
// and write the new primary index into *outPrimarySlotIndex. Returns true on
// promotion. Fails silently (returns false) if the queue reservation does not
// match entityId or no primary is free.
bool deposit_slots_try_promote(Entity *base, int entityId, int queueSlotIndex,
                               int *outPrimarySlotIndex);

// Release a specific reservation. Idempotent -- if the slot is not claimed
// by entityId (or kind/index is invalid), does nothing.
void deposit_slots_release(Entity *base, DepositSlotKind kind, int slotIndex,
                           int entityId);

// Release every reservation held by entityId across both rings. Idempotent.
// Call from farmer_on_death and farmer state reverts.
void deposit_slots_release_for_entity(Entity *base, int entityId);

// Look up the world position of a reserved slot. Returns (0,0) when kind or
// slotIndex is out of range, so callers should only use this for reservations
// they have previously acquired via deposit_slots_reserve_for / _try_promote.
Vector2 deposit_slots_get_position(const Entity *base, DepositSlotKind kind,
                                   int slotIndex);

// Read-only accessors for iteration (debug overlay, tests).
int deposit_slots_primary_count(const Entity *base);
int deposit_slots_queue_count(const Entity *base);
const DepositSlot *deposit_slots_primary_at(const Entity *base, int slotIndex);
const DepositSlot *deposit_slots_queue_at(const Entity *base, int slotIndex);

#endif //NFC_CARDGAME_DEPOSIT_SLOTS_H
