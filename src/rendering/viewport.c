//
// Created by Nathan Davis on 2/16/26.
//

#include "viewport.h"
#include "../systems/player.h"
#include <stdio.h>

void viewport_init_split_screen(GameState *gs) {
    gs->halfWidth = SCREEN_WIDTH / 2;

    // Player 1: Left half of screen, rotated 90 degrees
    Rectangle p1PlayArea = {
        .x = 0,
        .y = 0,
        .width = SCREEN_HEIGHT,
        .height = gs->halfWidth
    };
    Rectangle p1ScreenArea = {
        .x = 0,
        .y = 0,
        .width = gs->halfWidth,
        .height = SCREEN_HEIGHT
    };

    // Player 2: Right half of screen, rotated -90 degrees
    // TODO: P1 playArea (x=0, w=1080) and P2 playArea (x=960, w=1080) overlap by 120px in world X.
    // TODO: Entities in the overlap zone will appear in both viewports before scissoring kicks in.
    // TODO: Document whether this shared center strip is intentional or needs geometry adjustment.
    Rectangle p2PlayArea = {
        .x = gs->halfWidth,
        .y = 0,
        .width = SCREEN_HEIGHT,
        .height = gs->halfWidth
    };
    Rectangle p2ScreenArea = {
        .x = gs->halfWidth,
        .y = 0,
        .width = gs->halfWidth,
        .height = SCREEN_HEIGHT
    };

    float tileSize = DEFAULT_TILE_SIZE * DEFAULT_TILE_SCALE;

    // TODO: Seeds (42 and 99) and biome assignments (GRASS / UNDEAD) are hardcoded.
    // TODO: Randomize seeds per match (e.g. from time or match ID) and allow biome selection
    // TODO: during pregame so players can choose or be assigned different biomes.
    player_init(&gs->players[0], 0, p1PlayArea, p1ScreenArea, 90.0f,
                BIOME_GRASS, &gs->biomeDefs[BIOME_GRASS], tileSize, 42);
    player_init(&gs->players[1], 1, p2PlayArea, p2ScreenArea, -90.0f,
                BIOME_GRASS, &gs->biomeDefs[BIOME_GRASS], tileSize, 99);

    printf("Split-screen viewports initialized\n");
}

void viewport_begin(Player *p) {
    BeginScissorMode(
        (int)p->screenArea.x,
        (int)p->screenArea.y,
        (int)p->screenArea.width,
        (int)p->screenArea.height
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

void viewport_draw_tilemap(Player *p) {
    tilemap_draw(&p->tilemap, p->tileDefs);
    tilemap_draw_details(&p->tilemap, p->detailDefs);
    tilemap_draw_biome_layers(&p->tilemap, p->biomeDef);
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

void debug_draw_lane_paths(const Player *p) {
    // Colors per lane: left=BLUE, center=GREEN, right=RED
    const Color laneColors[3] = { BLUE, GREEN, RED };

    for (int lane = 0; lane < 3; lane++) {
        Color c = laneColors[lane];

        for (int i = 0; i < LANE_WAYPOINT_COUNT; i++) {
            // Draw waypoint dot
            DrawCircleV(p->laneWaypoints[lane][i], 4.0f, c);

            // Draw line segment to next waypoint
            if (i < LANE_WAYPOINT_COUNT - 1) {
                DrawLineV(p->laneWaypoints[lane][i],
                          p->laneWaypoints[lane][i + 1], c);
            }
        }
    }
}
