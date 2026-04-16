//
// Base level progression driven by lifetime Player.sustenanceCollected.
//

#ifndef NFC_CARDGAME_PROGRESSION_H
#define NFC_CARDGAME_PROGRESSION_H

#include "../core/types.h"

#define PROGRESSION_MAX_LEVEL            10
// Front-loaded progression: early levels arrive quickly, later levels require
// larger sustenance jumps, and level 10 unlocks at 200 total sustenance.
#define PROGRESSION_REGEN_LEVEL1         0.5f
#define PROGRESSION_REGEN_LEVEL_MAX      2.0f
// King burst is tuned to one-shot most troops at low levels and delete even
// Brutes at high levels.
#define PROGRESSION_KING_DMG_LEVEL1      220
#define PROGRESSION_KING_DMG_LEVEL_MAX   355
// Burst radius is measured from the base pivot, so this needs to exceed the
// shifted base interaction anchor used by ranged attackers.
#define PROGRESSION_KING_BURST_RADIUS    280.0f

// Pure helpers. No side effects, no GameState dependency.
int   progression_level_from_sustenance(int sustenance);
float progression_regen_rate_for_level(int level);
int   progression_king_burst_damage_for_level(int level);

// Recompute level/regen for a player from lifetime sustenance gathered,
// assign to the owning base (if live), update Player.baseEnergyRegenRate,
// and refresh Player.energyRegenRate through any active temporary boost.
// Safe to call repeatedly.
void  progression_sync_player(GameState *gs, int playerIndex);

#endif // NFC_CARDGAME_PROGRESSION_H
