//
// Hand UI implementation.
//

#include "hand_ui.h"
#include "../core/config.h"
#include <stdio.h>
#include <string.h>

// Opaque dark neutral fill for the hand-bar background. Deliberately darker
// than the tilemap so the hand strip reads as non-playfield chrome.
static const Color HAND_BAR_BG = { 20, 20, 24, 255 };

// Shared hand occupancy helper lives in player.c; declare only the renderer-
// facing entry point here to keep hand_ui tests lightweight.
int player_hand_occupied_count(const Player *p);

static const int HAND_CARD_ANIMATION_SEQUENCE[] = {0, 1, 2, 3, 4, 0};

static Texture2D hand_ui_load_texture_checked(const char *path, int expectedWidth, int expectedHeight,
                                              const char *label) {
    Texture2D t = LoadTexture(path);
    if (t.id == 0) {
        fprintf(stderr, "[HandUI] Failed to load %s: %s\n", label, path);
        return t;
    }

    if (t.width != expectedWidth || t.height != expectedHeight) {
        fprintf(stderr,
                "[HandUI] Invalid %s texture: %s "
                "(expected %dx%d, got %dx%d)\n",
                label, path, expectedWidth, expectedHeight, t.width, t.height);
        UnloadTexture(t);
        return (Texture2D){0};
    }

    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    return t;
}

Texture2D hand_ui_load_card_sheet(void) {
    return hand_ui_load_texture_checked(HAND_CARD_SHEET_PATH,
                                        HAND_CARD_WIDTH * HAND_CARD_FRAME_COUNT,
                                        HAND_CARD_HEIGHT * HAND_CARD_SHEET_ROWS,
                                        "hand card sheet");
}

Texture2D hand_ui_load_bar_background(void) {
    return hand_ui_load_texture_checked(HAND_BAR_BG_PATH,
                                        HAND_BAR_BG_WIDTH,
                                        HAND_BAR_BG_HEIGHT,
                                        "hand bar background");
}

void hand_ui_unload_texture(Texture2D texture) {
    if (texture.id != 0) {
        UnloadTexture(texture);
    }
}

static int hand_ui_sheet_row_for_card(const Card *card) {
    if (!card || !card->card_id) return 0;

    if (strcmp(card->card_id, "KNIGHT_01") == 0) return 0;
    if (strcmp(card->card_id, "HEALER_01") == 0) return 1;
    if (strcmp(card->card_id, "ASSASSIN_01") == 0) return 2;
    if (strcmp(card->card_id, "FARMER_01") == 0) return 3;
    if (strcmp(card->card_id, "BRUTE_01") == 0) return 4;
    // BIRD/FISHFING/KING reuse existing rows until card_sheet.png is redrawn.
    if (strcmp(card->card_id, "BIRD_01") == 0) return 0;
    if (strcmp(card->card_id, "FISHFING_01") == 0) return 2;
    if (strcmp(card->card_id, "KING_01") == 0) return 4;

    // Unimplemented or unknown cards reuse the knight art row.
    return 0;
}

static float hand_ui_play_animation_duration(void) {
    return HAND_CARD_FRAME_TIME * (float)(HAND_CARD_FRAME_COUNT + 1);
}

static float hand_ui_play_lift_scale(float elapsedSeconds) {
    const float duration = hand_ui_play_animation_duration();
    const float peakScale = HAND_CARD_PLAY_LIFT_PEAK_SCALE;
    const float baseScale = 1.0f;
    const float peakT = 0.25f;

    if (elapsedSeconds <= 0.0f || elapsedSeconds >= duration) {
        return baseScale;
    }

    const float t = elapsedSeconds / duration;
    if (t <= peakT) {
        return baseScale + (peakScale - baseScale) * (t / peakT);
    }

    return peakScale - (peakScale - baseScale) * ((t - peakT) / (1.0f - peakT));
}

static int hand_ui_frame_for_elapsed(float elapsedSeconds) {
    const float duration = hand_ui_play_animation_duration();
    if (elapsedSeconds <= 0.0f || elapsedSeconds >= duration) {
        return 0;
    }

    int step = (int)(elapsedSeconds / HAND_CARD_FRAME_TIME);
    if (step < 0) step = 0;
    if (step >= (int)(sizeof(HAND_CARD_ANIMATION_SEQUENCE) / sizeof(HAND_CARD_ANIMATION_SEQUENCE[0]))) {
        return 0;
    }
    return HAND_CARD_ANIMATION_SEQUENCE[step];
}

static Rectangle hand_ui_card_src_rect(const Card *card, float elapsedSeconds) {
    const int frameIndex = hand_ui_frame_for_elapsed(elapsedSeconds);
    const int rowIndex = hand_ui_sheet_row_for_card(card);
    return (Rectangle){
        (float)(frameIndex * HAND_CARD_WIDTH),
        (float)(rowIndex * HAND_CARD_HEIGHT),
        (float)HAND_CARD_WIDTH,
        (float)HAND_CARD_HEIGHT
    };
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

// Rotation per seat so hand chrome and cards read upright to each player.
// P1 sits at the bottom edge (SIDE_BOTTOM), P2 sits at the top (SIDE_TOP).
static float hand_ui_side_rotation(BattleSide side) {
    return (side == SIDE_BOTTOM) ? 90.0f : 270.0f;
}

void hand_ui_draw(const Player *p, Texture2D handBarTexture, Texture2D cardSheet) {
    // 1. Opaque background fill for the entire hand-bar strip.
    DrawRectangleRec(p->handArea, HAND_BAR_BG);

    if (handBarTexture.id != 0) {
        Rectangle srcRect = {
            0.0f,
            0.0f,
            (float)HAND_BAR_BG_WIDTH,
            (float)HAND_BAR_BG_HEIGHT
        };
        Rectangle dstRect = {
            p->handArea.x + p->handArea.width * 0.5f,
            p->handArea.y + p->handArea.height * 0.5f,
            p->handArea.height,
            p->handArea.width
        };
        Vector2 origin = { dstRect.width * 0.5f, dstRect.height * 0.5f };
        DrawTexturePro(handBarTexture, srcRect, dstRect, origin, hand_ui_side_rotation(p->side), WHITE);
    }

    const int visibleCardCount = player_hand_occupied_count(p);
    if (visibleCardCount <= 0) return;

    const float rotation = hand_ui_side_rotation(p->side);

    int visibleIndex = 0;
    for (int i = 0; i < HAND_MAX_CARDS; i++) {
        const Card *card = p->handCards[i];
        if (!card) continue;

        if (cardSheet.id == 0) {
            visibleIndex++;
            continue;
        }

        Rectangle srcRect = hand_ui_card_src_rect(card, p->handCardAnimElapsed[i]);
        Vector2 center = hand_ui_card_center_for_index(p->handArea, visibleCardCount, visibleIndex);
        const float liftScale = hand_ui_play_lift_scale(p->handCardAnimElapsed[i]);
        const float drawWidth = (float)HAND_CARD_WIDTH * liftScale;
        const float drawHeight = (float)HAND_CARD_HEIGHT * liftScale;

        // Destination rect is native-size 128x160 centered on the computed
        // center, with origin at the rect's center so rotation pivots around
        // it. DrawTexturePro interprets the origin as offset from the rect's
        // position (top-left), so origin = { width/2, height/2 } with
        // position at the center value draws centered.
        Rectangle dstRect = {
            center.x,
            center.y,
            drawWidth,
            drawHeight
        };
        Vector2 origin = { drawWidth * 0.5f, drawHeight * 0.5f };
        DrawTexturePro(cardSheet, srcRect, dstRect, origin, rotation, WHITE);
        visibleIndex++;
    }
}
