//
// Base level progression driven by lifetime Player.sustenanceCollected.
//

#ifndef NFC_CARDGAME_PROGRESSION_H
#define NFC_CARDGAME_PROGRESSION_H

#include "../core/types.h"

#define PROGRESSION_MAX_LEVEL            10
// Tuned from playtest pacing: ~400 sustenance by 7 minutes should land around
// mid-progression, with level 10 reachable near the 15-minute mark.
#define PROGRESSION_SUSTENANCE_PER_LEVEL 95
// Start slower so early card throughput stays constrained, but preserve the
// level-10 cap for late-game pacing.
#define PROGRESSION_REGEN_LEVEL1         0.5f
#define PROGRESSION_REGEN_LEVEL_MAX      2.0f
#define PROGRESSION_KING_DMG_LEVEL1      48
#define PROGRESSION_KING_DMG_LEVEL_MAX   75
#define PROGRESSION_KING_BURST_RADIUS    160.0f

// Pure helpers. No side effects, no GameState dependency.
int   progression_level_from_sustenance(int sustenance);
float progression_regen_rate_for_level(int level);
int   progression_king_burst_damage_for_level(int level);

// Recompute level/regen for a player from lifetime sustenance gathered,
// assign to the owning base (if live) and to Player.energyRegenRate.
// Safe to call repeatedly.
void  progression_sync_player(GameState *gs, int playerIndex);

#endif // NFC_CARDGAME_PROGRESSION_H
