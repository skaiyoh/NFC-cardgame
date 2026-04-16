//
// Base level progression driven by lifetime Player.sustenanceCollected.
//

#include "progression.h"

static int clamp_level(int level) {
    if (level < 1) return 1;
    if (level > PROGRESSION_MAX_LEVEL) return PROGRESSION_MAX_LEVEL;
    return level;
}

int progression_level_from_sustenance(int sustenance) {
    if (sustenance < 0) sustenance = 0;
    int level = 1 + sustenance / PROGRESSION_SUSTENANCE_PER_LEVEL;
    return clamp_level(level);
}

float progression_regen_rate_for_level(int level) {
    level = clamp_level(level);
    float span = PROGRESSION_REGEN_LEVEL_MAX - PROGRESSION_REGEN_LEVEL1;
    float t = (float)(level - 1) / (float)(PROGRESSION_MAX_LEVEL - 1);
    return PROGRESSION_REGEN_LEVEL1 + span * t;
}

int progression_king_burst_damage_for_level(int level) {
    level = clamp_level(level);
    int span = PROGRESSION_KING_DMG_LEVEL_MAX - PROGRESSION_KING_DMG_LEVEL1;
    // span/(max-1) is exact for 10→9 levels (27/9=3), so integer math suffices.
    return PROGRESSION_KING_DMG_LEVEL1 +
           (span * (level - 1)) / (PROGRESSION_MAX_LEVEL - 1);
}

void progression_sync_player(GameState *gs, int playerIndex) {
    if (!gs || playerIndex < 0 || playerIndex > 1) return;
    Player *player = &gs->players[playerIndex];

    int level = progression_level_from_sustenance(player->sustenanceCollected);
    player->energyRegenRate = progression_regen_rate_for_level(level);

    Entity *base = player->base;
    if (base && base->alive && !base->markedForRemoval) {
        base->baseLevel = level;
    }
}
