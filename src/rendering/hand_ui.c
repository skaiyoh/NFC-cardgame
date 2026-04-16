//
// Hand UI implementation.
//

#include "hand_ui.h"
#include "../core/config.h"
#include "../data/card_catalog.h"
#include <stdio.h>

// Opaque dark neutral fill for the hand-bar background. Deliberately darker
// than the tilemap so the hand strip reads as non-playfield chrome.
static const Color HAND_BAR_BG = { 20, 20, 24, 255 };

// Shared hand occupancy helper lives in player.c; declare only the renderer-
// facing entry point here to keep hand_ui tests lightweight.
int player_hand_occupied_count(const Player *p);

typedef struct {
    const Card *card;
    int handSlotIndex;
    int sortKey;
} HandVisibleCard;

static const int HAND_CARD_ANIMATION_SEQUENCE[] = {0, 1, 2, 3, 4, 0};
// Measured from card_sheet.png alpha bounds. These are source-space offsets
// from the nominal 128x160 frame center to the visible sprite center.
static const Vector2 HAND_CARD_VISUAL_CENTER_OFFSETS_ROW_ZERO[HAND_CARD_FRAME_COUNT] = {
    {-3.0f, -3.0f}, {-3.0f, -3.0f}, {-3.0f, -3.0f},
    {-3.0f, -3.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}
};
static const Vector2 HAND_CARD_VISUAL_CENTER_OFFSETS_STANDARD[HAND_CARD_FRAME_COUNT] = {
    {-3.0f, -9.0f}, {-3.0f, -9.0f}, {-3.0f, -9.0f},
    {-3.0f, -9.0f}, {0.0f, -6.0f}, {0.0f, -6.0f}
};

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
    return card_catalog_hand_sheet_row_for_card(card);
}

static int hand_ui_collect_visible_cards(const Player *p, HandVisibleCard *outCards) {
    if (!p || !outCards) return 0;

    int count = 0;
    for (int i = 0; i < HAND_MAX_CARDS; i++) {
        const Card *card = p->handCards[i];
        if (!card) continue;

        int sortKey = card_catalog_hand_presentation_rank_for_card(card);
        if (sortKey < 0) {
            sortKey = card_catalog_presentation_count() + i;
        }

        outCards[count].card = card;
        outCards[count].handSlotIndex = i;
        outCards[count].sortKey = sortKey;
        count++;
    }

    // Stable insertion sort: known cards follow presentation order, and ties
    // (for duplicates/unknowns) preserve the original hand slot order.
    for (int i = 1; i < count; i++) {
        HandVisibleCard current = outCards[i];
        int j = i - 1;
        while (j >= 0 && outCards[j].sortKey > current.sortKey) {
            outCards[j + 1] = outCards[j];
            j--;
        }
        outCards[j + 1] = current;
    }

    return count;
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

static Rectangle hand_ui_card_src_rect(int rowIndex, int frameIndex) {
    return (Rectangle){
        (float)(frameIndex * HAND_CARD_WIDTH),
        (float)(rowIndex * HAND_CARD_HEIGHT),
        (float)HAND_CARD_WIDTH,
        (float)HAND_CARD_HEIGHT
    };
}

static Vector2 hand_ui_card_visual_center_offset(int rowIndex, int frameIndex) {
    if (rowIndex < 0 || rowIndex >= HAND_CARD_SHEET_ROWS
        || frameIndex < 0 || frameIndex >= HAND_CARD_FRAME_COUNT) {
        return (Vector2){0.0f, 0.0f};
    }

    if (rowIndex == 0) {
        return HAND_CARD_VISUAL_CENTER_OFFSETS_ROW_ZERO[frameIndex];
    }

    return HAND_CARD_VISUAL_CENTER_OFFSETS_STANDARD[frameIndex];
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

static Vector2 hand_ui_card_visual_draw_correction(BattleSide side, int rowIndex,
                                                   int frameIndex, float drawScale) {
    Vector2 sourceOffset = hand_ui_card_visual_center_offset(rowIndex, frameIndex);
    Vector2 correction = {0.0f, 0.0f};

    if (side == SIDE_BOTTOM) {
        correction.x = sourceOffset.y * drawScale;
        correction.y = -sourceOffset.x * drawScale;
    } else {
        correction.x = -sourceOffset.y * drawScale;
        correction.y = sourceOffset.x * drawScale;
    }

    return correction;
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

    HandVisibleCard visibleCards[HAND_MAX_CARDS];
    const int visibleCardCount = hand_ui_collect_visible_cards(p, visibleCards);
    if (visibleCardCount <= 0) return;

    const float rotation = hand_ui_side_rotation(p->side);

    for (int visibleIndex = 0; visibleIndex < visibleCardCount; visibleIndex++) {
        const HandVisibleCard *entry = &visibleCards[visibleIndex];
        const Card *card = entry->card;
        const int handSlotIndex = entry->handSlotIndex;

        if (cardSheet.id == 0) {
            continue;
        }

        const int rowIndex = hand_ui_sheet_row_for_card(card);
        const int frameIndex = hand_ui_frame_for_elapsed(p->handCardAnimElapsed[handSlotIndex]);
        Rectangle srcRect = hand_ui_card_src_rect(rowIndex, frameIndex);
        Vector2 center = hand_ui_card_center_for_index(p->handArea, visibleCardCount, visibleIndex);
        const float liftScale = hand_ui_play_lift_scale(p->handCardAnimElapsed[handSlotIndex]);
        // The measured visual-center correction is frame-specific and scales
        // with the play-lift pulse, so cards will "step" slightly as the clip
        // moves between frames with different opaque-bounds centers.
        Vector2 visualCorrection =
            hand_ui_card_visual_draw_correction(p->side, rowIndex, frameIndex, liftScale);
        center.x += visualCorrection.x;
        center.y += visualCorrection.y;
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
    }
}
