//
// Hand UI implementation.
//

#include "hand_ui.h"
#include "../core/config.h"
#include <stdio.h>

// Opaque dark neutral fill for the hand-bar background. Deliberately darker
// than the tilemap so the hand strip reads as non-playfield chrome.
static const Color HAND_BAR_BG = { 20, 20, 24, 255 };

// Shared hand occupancy helpers live in player.c; declare only the two
// renderer-facing entry points here to keep hand_ui tests lightweight.
bool player_hand_slot_is_occupied(const Player *p, int handIndex);
int player_hand_occupied_count(const Player *p);

Texture2D hand_ui_load_placeholder(void) {
    Texture2D t = LoadTexture(HAND_CARD_PLACEHOLDER_PATH);
    if (t.id == 0) {
        fprintf(stderr, "[HandUI] Failed to load placeholder card: %s\n",
                HAND_CARD_PLACEHOLDER_PATH);
        return t;
    }

    if (t.width != HAND_CARD_WIDTH || t.height != HAND_CARD_HEIGHT) {
        fprintf(stderr,
                "[HandUI] Invalid placeholder card texture: %s "
                "(expected %dx%d, got %dx%d)\n",
                HAND_CARD_PLACEHOLDER_PATH,
                HAND_CARD_WIDTH, HAND_CARD_HEIGHT,
                t.width, t.height);
        UnloadTexture(t);
        return (Texture2D){0};
    }

    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    return t;
}

void hand_ui_unload_placeholder(Texture2D texture) {
    if (texture.id != 0) {
        UnloadTexture(texture);
    }
}

Vector2 hand_ui_card_center_for_index(Rectangle handArea, int visibleCardCount, int visibleIndex) {
    if (visibleCardCount <= 0) {
        return (Vector2){
            handArea.x + handArea.width * 0.5f,
            handArea.y + handArea.height * 0.5f
        };
    }

    // Cards rotate 90/270°, so the visual "width" of each card along the
    // stacking (long) axis is HAND_CARD_WIDTH (128), and gaps sit between
    // adjacent cards on that same axis.
    const float stride = (float)HAND_CARD_WIDTH + (float)HAND_CARD_GAP;
    const float runLength = (float)visibleCardCount * (float)HAND_CARD_WIDTH
                          + (float)(visibleCardCount - 1) * (float)HAND_CARD_GAP;

    // Center the entire run inside the handArea's long axis (height).
    const float startY = handArea.y + (handArea.height - runLength) * 0.5f
                       + (float)HAND_CARD_WIDTH * 0.5f;

    Vector2 center;
    center.x = handArea.x + handArea.width * 0.5f;
    center.y = startY + (float)visibleIndex * stride;
    return center;
}

// Rotation per seat so cards read upright to each player.
// P1 sits at the bottom edge (SIDE_BOTTOM), P2 sits at the top (SIDE_TOP).
static float hand_ui_card_rotation(BattleSide side) {
    return (side == SIDE_BOTTOM) ? 90.0f : 270.0f;
}

void hand_ui_draw(const Player *p, Texture2D placeholder) {
    // 1. Opaque background fill for the entire hand-bar strip.
    DrawRectangleRec(p->handArea, HAND_BAR_BG);

    // 2. Bail out gracefully if the texture failed to load: the dark fill
    //    still reserves the space so layout assumptions hold.
    if (placeholder.id == 0) return;

    const int visibleCardCount = player_hand_occupied_count(p);
    if (visibleCardCount <= 0) return;

    const float rotation = hand_ui_card_rotation(p->side);
    const Rectangle srcRect = { 0.0f, 0.0f,
                                (float)placeholder.width,
                                (float)placeholder.height };

    int visibleIndex = 0;
    for (int i = 0; i < HAND_MAX_CARDS; i++) {
        if (!player_hand_slot_is_occupied(p, i)) continue;

        Vector2 center = hand_ui_card_center_for_index(p->handArea, visibleCardCount, visibleIndex);

        // Destination rect is native-size 128x160 centered on the computed
        // center, with origin at the rect's center so rotation pivots around
        // it. DrawTexturePro interprets the origin as offset from the rect's
        // position (top-left), so origin = { width/2, height/2 } with
        // position at the center value draws centered.
        Rectangle dstRect = {
            center.x,
            center.y,
            (float)HAND_CARD_WIDTH,
            (float)HAND_CARD_HEIGHT
        };
        Vector2 origin = { (float)HAND_CARD_WIDTH * 0.5f,
                           (float)HAND_CARD_HEIGHT * 0.5f };
        DrawTexturePro(placeholder, srcRect, dstRect, origin, rotation, WHITE);
        visibleIndex++;
    }
}
