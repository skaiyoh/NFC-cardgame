//
// Created by Nathan Davis on 2/16/26.
//

#include "viewport.h"
#include "../core/battlefield.h"
#include "../systems/player.h"
#include <stdio.h>

void viewport_init_split_screen(GameState *gs) {
    gs->halfWidth = SCREEN_WIDTH / 2;
    Battlefield *bf = &gs->battlefield;

    // Each player owns a full 960x1080 half, split into an outer hand-bar
    // strip (HAND_UI_DEPTH_PX deep) and an inner battlefield sub-rect that
    // hosts all world-space rendering. The seam (canonical y=SEAM_Y) is
    // anchored to the inner edge of each battlefield sub-rect.
    //
    //   P1 (left):  hand { 0, 0, 180, 1080 }   bf { 180, 0, 780, 1080 }
    //   P2 (right): bf   { 960, 0, 780, 1080 } hand { 1740, 0, 180, 1080 }
    const float handDepth = (float)HAND_UI_DEPTH_PX;
    const float bfWidth = (float)gs->halfWidth - handDepth;

    Rectangle p1Screen = { 0.0f, 0.0f, (float)gs->halfWidth, (float)SCREEN_HEIGHT };
    Rectangle p1Hand   = { 0.0f, 0.0f, handDepth, (float)SCREEN_HEIGHT };
    Rectangle p1Bf     = { handDepth, 0.0f, bfWidth, (float)SCREEN_HEIGHT };

    Rectangle p2Screen = { (float)gs->halfWidth, 0.0f, (float)gs->halfWidth, (float)SCREEN_HEIGHT };
    Rectangle p2Bf     = { (float)gs->halfWidth, 0.0f, bfWidth, (float)SCREEN_HEIGHT };
    Rectangle p2Hand   = { (float)gs->halfWidth + bfWidth, 0.0f, handDepth, (float)SCREEN_HEIGHT };

    // Both cameras use rot=+90 so that the seam (y=960) maps to the inner
    // battlefield edge (screen x=960) for both viewports.  P2's viewport is
    // rendered to a RenderTexture and flipped vertically when composited,
    // which reverses the world-X → screen-Y mapping to give P2 the opposite
    // perspective (across-the-table).
    player_init(&gs->players[0], 0, SIDE_BOTTOM, p1Screen, p1Bf, p1Hand, 90.0f, bf);
    player_init(&gs->players[1], 1, SIDE_TOP,    p2Screen, p2Bf, p2Hand, 90.0f, bf);

    printf("Split-screen viewports initialized (canonical, with hand bars)\n");
}

void viewport_begin(Player *p) {
    BeginScissorMode(
        (int) p->battlefieldArea.x,
        (int) p->battlefieldArea.y,
        (int) p->battlefieldArea.width,
        (int) p->battlefieldArea.height
    );
    BeginMode2D(p->camera);
}

void viewport_end(void) {
    EndMode2D();
    EndScissorMode();
}

Vector2 viewport_world_to_screen(Player *p, Vector2 worldPos) {
    return GetWorldToScreen2D(worldPos, p->camera);
}

Vector2 viewport_screen_to_world(Player *p, Vector2 screenPos) {
    return GetScreenToWorld2D(screenPos, p->camera);
}

void viewport_draw_battlefield_tilemap(const Battlefield *bf, BattleSide side) {
    Territory *t = bf_territory_for_side((Battlefield *)bf, side);
    float rotationDegrees = (side == SIDE_TOP) ? 180.0f : 0.0f;
    tilemap_draw_oriented(&t->tilemap, t->tileDefs, rotationDegrees);
    tilemap_draw_details_oriented(&t->tilemap, t->detailDefs, rotationDegrees);
    tilemap_draw_biome_layers_oriented(&t->tilemap, t->biomeDef, rotationDegrees);
}

void viewport_draw_card_slots_debug(Player *p) {
    // Draw debug circles at card slot positions
    for (int i = 0; i < NUM_CARD_SLOTS; i++) {
        CardSlot *slot = &p->slots[i];
        Color slotColor = GREEN;

        // Draw slot indicator
        DrawCircleV(slot->worldPos, 20.0f, slotColor);
        DrawCircleLines(slot->worldPos.x, slot->worldPos.y, 25.0f, WHITE);

        // Draw slot number
        char slotNum[2];
        slotNum[0] = '1' + i;
        slotNum[1] = '\0';
        DrawText(slotNum, slot->worldPos.x - 5, slot->worldPos.y - 8, 16, WHITE);
    }
}

void debug_draw_lane_paths_screen(const Battlefield *bf, BattleSide side, Camera2D cam) {
    // Colors per lane: left=BLUE, center=GREEN, right=RED
    const Color laneColors[3] = {BLUE, GREEN, RED};

    for (int lane = 0; lane < 3; lane++) {
        Color c = laneColors[lane];

        for (int i = 0; i < LANE_WAYPOINT_COUNT; i++) {
            // Get canonical waypoint and convert to screen-space
            CanonicalPos wp = bf_waypoint(bf, side, lane, i);
            Vector2 sp = GetWorldToScreen2D(wp.v, cam);

            // Draw waypoint dot in screen space
            DrawCircleV(sp, 4.0f, c);

            // Draw line segment to next waypoint in screen space
            if (i < LANE_WAYPOINT_COUNT - 1) {
                CanonicalPos wpNext = bf_waypoint(bf, side, lane, i + 1);
                Vector2 spNext = GetWorldToScreen2D(wpNext.v, cam);
                DrawLineV(sp, spNext, c);
            }
        }
    }
}
