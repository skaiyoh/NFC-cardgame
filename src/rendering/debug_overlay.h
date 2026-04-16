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
    bool navOverlay;           // F8: nav blocker mask + focused flow-field preview
    bool depositSlots;         // F9: base deposit slot rings (primary + queue)
    bool crowdShells;          // F10: hard blocker shell vs soft ally shell
} DebugOverlayFlags;

typedef struct {
    bool hasFocus;
    int focusEntityId;
    Vector2 mouseWorld;
} DebugNavOverlayState;

// Toggle the debug flag associated with `key`. Returns true when handled.
bool debug_overlay_toggle_key(DebugOverlayFlags *flags, int key);

// Resolve per-viewport mouse focus for the nav overlay.
DebugNavOverlayState debug_overlay_resolve_nav_state(const Battlefield *bf,
                                                     Rectangle battlefieldArea,
                                                     Camera2D camera,
                                                     Vector2 mouseScreen,
                                                     bool useLocalCoords);

// Draw all enabled debug layers.
void debug_overlay_draw(const Battlefield *bf, const GameState *gs,
                        DebugOverlayFlags flags,
                        const DebugNavOverlayState *navState);

#endif //NFC_CARDGAME_DEBUG_OVERLAY_H
