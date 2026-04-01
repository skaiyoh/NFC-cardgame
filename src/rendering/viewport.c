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

    Rectangle p1Screen = { 0, 0, gs->halfWidth, SCREEN_HEIGHT };
    Rectangle p2Screen = { gs->halfWidth, 0, gs->halfWidth, SCREEN_HEIGHT };

    // Both cameras use rot=-90 so that higher world Y maps to lower screen X.
    // This places the seam (y=960) at the inner screen edge (x=960) for both.
    // "Facing each other" is handled by sprite directions, not camera rotation.
    // P1 target = center of bottom territory (540, 1440)
    // P2 target = center of top territory (540, 480)
    player_init(&gs->players[0], 0, SIDE_BOTTOM, p1Screen, -90.0f, bf);
    player_init(&gs->players[1], 1, SIDE_TOP, p2Screen, -90.0f, bf);

    printf("Split-screen viewports initialized (canonical)\n");
}

void viewport_begin(Player *p) {
    BeginScissorMode(
        (int) p->screenArea.x,
        (int) p->screenArea.y,
        (int) p->screenArea.width,
        (int) p->screenArea.height
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
    tilemap_draw(&t->tilemap, t->tileDefs);
    tilemap_draw_details(&t->tilemap, t->detailDefs);
    tilemap_draw_biome_layers(&t->tilemap, t->biomeDef);
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
