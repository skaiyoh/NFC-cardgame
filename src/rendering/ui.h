//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_UI_H
#define NFC_CARDGAME_UI_H

#include "../core/types.h"

// Load the buff icon strip used by the HUD. Returns a zero-id texture on
// load or dimension-validation failure; callers should treat that as
// "draw timer without icon" rather than a fatal error.
Texture2D ui_load_buff_icons(void);

// Draws the player's current spendable sustenance bank anchored to a corner
// of `viewport` and rotated for the player's orientation. Uses the bitmap
// font sheet when available; falls back to the default font with a
// player-derived color if the texture has zero id. Color is never provided
// by the caller.
void ui_draw_sustenance_counter(const Player *p, Rectangle viewport,
                                float rotation, Texture2D letteringTexture,
                                Texture2D buffIconsTexture);

// Draws a match-result overlay ("DRAW", "VICTORY", or "DEFEAT") centered on
// the player's battlefield area. Uses the bitmap font sheet when available;
// falls back to the default font with a text-derived color if the texture
// has zero id.
void ui_draw_match_result(const Player *p, const char *text, float rotation,
                          Texture2D letteringTexture);

// Draws a translucent full-screen black backdrop behind the match-result
// labels so VICTORY/DEFEAT/DRAW text reads clearly over the battlefield.
// Call once per frame, before the per-player result labels.
void ui_draw_match_result_backdrop(void);

#endif //NFC_CARDGAME_UI_H
