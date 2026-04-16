#include "debug_overlay.h"
#include <math.h>

static bool debug_overlay_point_in_rect(Vector2 p, Rectangle r) {
    return p.x >= r.x && p.x < r.x + r.width &&
           p.y >= r.y && p.y < r.y + r.height;
}

bool debug_overlay_toggle_key(DebugOverlayFlags *flags, int key) {
    if (!flags) return false;

    switch (key) {
        case KEY_F2:  flags->attackBars = !flags->attackBars; return true;
        case KEY_F3:  flags->targetLines = !flags->targetLines; return true;
        case KEY_F4:  flags->eventFlashes = !flags->eventFlashes; return true;
        case KEY_F5:  flags->rangeCirlces = !flags->rangeCirlces; return true;
        case KEY_F6:  flags->sustenanceNodes = !flags->sustenanceNodes; return true;
        case KEY_F7:  flags->sustenancePlacement = !flags->sustenancePlacement; return true;
        case KEY_F8:  flags->navOverlay = !flags->navOverlay; return true;
        case KEY_F9:  flags->depositSlots = !flags->depositSlots; return true;
        case KEY_F10: flags->crowdShells = !flags->crowdShells; return true;
        default: return false;
    }
}

DebugNavOverlayState debug_overlay_resolve_nav_state(const Battlefield *bf,
                                                     Rectangle battlefieldArea,
                                                     Camera2D camera,
                                                     Vector2 mouseScreen,
                                                     bool useLocalCoords) {
    DebugNavOverlayState state = {
        .hasFocus = false,
        .focusEntityId = -1,
        .mouseWorld = { 0.0f, 0.0f }
    };
    if (!bf) return state;
    if (!debug_overlay_point_in_rect(mouseScreen, battlefieldArea)) return state;

    state.hasFocus = true;
    if (useLocalCoords) {
        Vector2 local = {
            mouseScreen.x - battlefieldArea.x,
            mouseScreen.y - battlefieldArea.y
        };
        // P2 is rendered into a viewport-sized render texture with RT-local
        // camera coordinates. The negative source-height composite corrects
        // the framebuffer's upside-down storage; it does not require an extra
        // input-space mirror here.
        state.mouseWorld = GetScreenToWorld2D(local, camera);
    } else {
        state.mouseWorld = GetScreenToWorld2D(mouseScreen, camera);
    }

    const float maxFocusDistSq = 48.0f * 48.0f;
    float bestDistSq = maxFocusDistSq;
    for (int i = 0; i < bf->entityCount; ++i) {
        const Entity *e = bf->entities[i];
        if (!e || !e->alive || e->markedForRemoval) continue;

        float dx = e->position.x - state.mouseWorld.x;
        float dy = e->position.y - state.mouseWorld.y;
        float distSq = dx * dx + dy * dy;
        if (distSq > maxFocusDistSq) continue;
        if (state.focusEntityId < 0 ||
            distSq < bestDistSq - 0.001f ||
            (fabsf(distSq - bestDistSq) <= 0.001f && e->id < state.focusEntityId)) {
            bestDistSq = distSq;
            state.focusEntityId = e->id;
        }
    }

    return state;
}
