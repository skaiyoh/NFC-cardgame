//
// Hand UI -- per-seat card strip along each player's outer edge.
//
// The hand bar paints an opaque background on the player's outer-edge strip
// and draws one placeholder card per occupied hand entry, rotated so cards
// read upright to each player. Hand capacity stays fixed, but rendering only
// uses occupied slots so the visible hand can grow naturally over time.
//

#ifndef NFC_CARDGAME_HAND_UI_H
#define NFC_CARDGAME_HAND_UI_H

#include "../core/types.h"

// Load the shared hand placeholder texture once at startup.
// Safe to call before the window exists only if raylib is initialized.
Texture2D hand_ui_load_placeholder(void);

// Load the shared knight card sheet once at startup.
Texture2D hand_ui_load_knight_sheet(void);

// Unload the shared placeholder texture. No-op if the texture id is zero.
void hand_ui_unload_placeholder(Texture2D texture);

// Draw the hand bar (background fill + one rotated placeholder card per
// occupied hand entry) inside the player's handArea. Must be called outside
// any scissor or camera scope (i.e. at screen-space draw time).
void hand_ui_draw(const Player *p, Texture2D placeholder, Texture2D knightSheet);

// Pure-math helper: compute the world/screen-space center of the visibleIndex-th
// card in a run of `visibleCardCount` cards inside `handArea`. The stacking axis
// is the handArea's long axis (height on a 180x1080 strip). Exposed for unit testing.
Vector2 hand_ui_card_center_for_index(Rectangle handArea, int visibleCardCount, int visibleIndex);

#endif //NFC_CARDGAME_HAND_UI_H
