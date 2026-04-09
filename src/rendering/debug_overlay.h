//
// Visual debug overlay: attack progress bars, target lock lines, event flashes.
// Drawn in world space inside Camera2D. Each layer has its own toggle.
//

#ifndef NFC_CARDGAME_DEBUG_OVERLAY_H
#define NFC_CARDGAME_DEBUG_OVERLAY_H

#include "../core/types.h"

// Independent toggle flags for each debug layer
typedef struct {
    bool attackBars;    // F2: attack progress bars + hit marker
    bool targetLines;   // F3: attacker → target lock lines
    bool eventFlashes;  // F4: state/hit/death event rings
    bool rangeCirlces;  // F5: attack range circles
    bool sustenanceNodes;      // F6: sustenance node state markers + claim lines
    bool sustenancePlacement;  // F7: placement-diagnostics grid
} DebugOverlayFlags;

// Draw all enabled debug layers.
void debug_overlay_draw(const Battlefield *bf, const GameState *gs,
                        DebugOverlayFlags flags);

#endif //NFC_CARDGAME_DEBUG_OVERLAY_H
