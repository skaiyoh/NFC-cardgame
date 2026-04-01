//
// Created by Nathan Davis on 2/16/26.
//

#include "game.h"
#include "config.h"
#include "../logic/card_effects.h"
#include "../logic/pathfinding.h"
#include "../rendering/viewport.h"
#include "../rendering/sprite_renderer.h"
#include "../rendering/ui.h"
#include "../systems/player.h"
#include "../entities/entities.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

static bool s_showLaneDebug = false;

bool game_init(GameState *g) {
    srand((unsigned int) time(NULL));

    const char *db_path = getenv("DB_PATH");
    if (!db_path) db_path = "cardgame.db";
    if (!db_init(&g->db, db_path)) {
        printf("db_init failed — ensure %s exists (run: make init-db)\n", db_path);
        return false;
    }

    if (!cards_load(&g->deck, &g->db)) {
        db_close(&g->db);
        return false;
    }

    // Load NFC UID → card_id mappings (non-fatal if table is empty or missing)
    cards_load_nfc_map(&g->deck, &g->db);

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "NFC Card Game");
    SetTargetFPS(60);

    // Initialize card system
    card_action_init();
    card_atlas_init(&g->cardAtlas);

    // Initialize biome definitions (loads textures, builds tile defs)
    biome_init_all(g->biomeDefs);

    // Initialize character sprite atlas
    sprite_atlas_init(&g->spriteAtlas);

    // Initialize split-screen viewports and players
    viewport_init_split_screen(g);

    // Initialize NFC serial ports (optional — game works with keyboard input only if unset)
    g->nfc.fds[0] = -1;
    g->nfc.fds[1] = -1;
    const char *single_port = getenv("NFC_PORT");
    const char *port0 = getenv("NFC_PORT_P1");
    const char *port1 = getenv("NFC_PORT_P2");

    if (single_port) {
        if (!nfc_init_single(&g->nfc, single_port)) {
            printf("[NFC] Warning: failed to open test port — NFC disabled\n");
        }
    } else if (port0 && port1) {
        if (!nfc_init(&g->nfc, port0, port1)) {
            printf("[NFC] Warning: failed to open serial ports — NFC disabled\n");
        }
    } else {
        printf("[NFC] No NFC port env vars set — NFC disabled\n");
    }

    return true;
}

// Simulate a knight card play through the full production code path:
// cards_find → card_action_play → play_knight → spawn_troop_from_card → troop_spawn
static void game_test_play_knight(GameState *g, int playerIndex, int slotIndex) {
    Card *card = cards_find(&g->deck, "KNIGHT_01");
    if (!card) {
        printf("[TEST] KNIGHT_01 not found in deck\n");
        return;
    }
    card_action_play(card, g, playerIndex, slotIndex);
}

static void game_handle_nfc_events(GameState *g) {
    NFCEvent events[6];
    int count = nfc_poll(&g->nfc, events, 6);
    for (int i = 0; i < count; i++) {
        const Card *card = cards_find_by_uid(&g->deck, events[i].uid);
        if (!card) {
            printf("[NFC] Unknown UID: %s\n", events[i].uid);
            continue;
        }
        card_action_play(card, g, events[i].playerIndex, events[i].readerIndex);
    }
}

static void game_handle_test_input(GameState *g) {
    // Toggle lane debug overlay
    if (IsKeyPressed(KEY_F1)) s_showLaneDebug = !s_showLaneDebug;

    // Player 1: key 1
    if (IsKeyPressed(KEY_ONE)) game_test_play_knight(g, 0, 0);
    if (IsKeyPressed(KEY_TWO)) game_test_play_knight(g, 0, 1);
    if (IsKeyPressed(KEY_THREE)) game_test_play_knight(g, 0, 2);

    // Player 2: key Q
    if (IsKeyPressed(KEY_Q)) game_test_play_knight(g, 1, 0);
    if (IsKeyPressed(KEY_W)) game_test_play_knight(g, 1, 1);
    if (IsKeyPressed(KEY_E)) game_test_play_knight(g, 1, 2);
}

void game_update(GameState *g) {
    float deltaTime = fminf(GetFrameTime(), 1.0f / 20.0f);

    game_handle_nfc_events(g);
    game_handle_test_input(g);

    // Update both players
    player_update(&g->players[0], deltaTime);
    player_update(&g->players[1], deltaTime);

    // Update entities for both players
    player_update_entities(&g->players[0], g, deltaTime);
    player_update_entities(&g->players[1], g, deltaTime);
}

// Draw entities for a viewport. Owner's entities draw normally; opponent's
// crossed entities appear at a mirrored position with mirrored waypoint facing.
// TODO: Iterates all entities from both players per viewport — O(2 × totalEntities) draw calls per frame.
// TODO: At MAX_ENTITIES=64 per player (128 entities × 2 passes = 256 iterations), this is fine now,
// TODO: but consider a spatial cull if entity counts grow.
static Vector2 game_map_crossed_world_point(const Player *owner, const Player *opponent, Vector2 worldPos) {
    float lateral = (worldPos.x - owner->playArea.x) / owner->playArea.width;
    float mirroredLateral = 1.0f - lateral;
    float depth = owner->playArea.y - worldPos.y;
    return (Vector2){
        opponent->playArea.x + mirroredLateral * opponent->playArea.width,
        opponent->playArea.y + depth
    };
}

static void game_apply_crossed_direction(const Entity *e, const Player *owner,
                                         const Player *opponent, AnimState *crossed) {
    if (e->lane < 0 || e->lane >= 3) return;
    if (e->waypointIndex < 0 || e->waypointIndex >= LANE_WAYPOINT_COUNT) return;

    Vector2 mappedPos = game_map_crossed_world_point(owner, opponent, e->position);
    Vector2 target = owner->laneWaypoints[e->lane][e->waypointIndex];
    Vector2 mappedTarget = game_map_crossed_world_point(owner, opponent, target);
    Vector2 diff = {
        mappedTarget.x - mappedPos.x,
        mappedTarget.y - mappedPos.y
    };

    pathfind_apply_direction(crossed, diff);
}

static void game_draw_entities_for_viewport(GameState *g, const Player *viewportPlayer) {
    for (int pid = 0; pid < 2; pid++) {
        const Player *owner = &g->players[pid];
        const Player *opponent = &g->players[1 - pid];

        for (int i = 0; i < owner->entityCount; i++) {
            const Entity *e = owner->entities[i];
            // Compute sprite half-height for seam overlap detection
            float spriteHalfH = (SPRITE_FRAME_SIZE * e->spriteScale) * 0.5f;
            // The seam boundary is at the top edge of the owner's play area
            float seamBorder = owner->playArea.y;

            if (viewportPlayer == owner) {
                // Draw in owner's viewport — scissor clips at the edge naturally
                // TODO: entity_draw is still submitted when position.y < 0 (entity is off-screen in
                // TODO: owner's space). The scissor clips it, but the draw call still reaches the GPU.
                // TODO: Consider skipping draw when entity is clearly outside the owner's viewport bounds.
                entity_draw(e);
            } else {
                // Opponent viewport: draw if entity has crossed OR if sprite overlaps seam
                bool crossed = (e->position.y < seamBorder);
                bool spriteOverlapsSeam = (e->position.y - spriteHalfH < seamBorder) &&
                                          (e->position.y + spriteHalfH > seamBorder);

                if (crossed || spriteOverlapsSeam) {
                    Vector2 mappedPos = game_map_crossed_world_point(owner, opponent, e->position);
                    AnimState crossedAnim = e->anim;
                    game_apply_crossed_direction(e, owner, opponent, &crossedAnim);
                    sprite_draw(e->sprite, &crossedAnim, mappedPos, e->spriteScale);
                }
            }
        }
    }
}

void game_render(GameState *g) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Render Player 1's viewport
    viewport_begin(&g->players[0]);
    viewport_draw_tilemap(&g->players[0]);
    game_draw_entities_for_viewport(g, &g->players[0]);
    DrawText("PLAYER 1",
             g->players[0].playArea.x + 40,
             g->players[0].playArea.y + 40,
             40, DARKGREEN);
    // TODO: viewport_draw_card_slots_debug is commented out — re-enable or replace with proper card slot UI.
    // viewport_draw_card_slots_debug(&g->players[0]);
    viewport_end();

    // Render Player 2's viewport
    viewport_begin(&g->players[1]);
    viewport_draw_tilemap(&g->players[1]);
    game_draw_entities_for_viewport(g, &g->players[1]);
    DrawText("PLAYER 2",
             g->players[1].playArea.x + 40,
             g->players[1].playArea.y + 40,
             40, MAROON);
    // TODO: viewport_draw_card_slots_debug is commented out — re-enable or replace with proper card slot UI.
    // viewport_draw_card_slots_debug(&g->players[1]);
    viewport_end();

    // Debug lane overlay — screen space, both players' paths overlap
    if (s_showLaneDebug) {
        debug_draw_lane_paths_screen(&g->players[0], g->players[0].camera);
        debug_draw_lane_paths_screen(&g->players[1], g->players[1].camera);
    }

    // HUD — screen space, drawn after all viewports
    ui_draw_energy_bar(&g->players[0], 0, SCREEN_WIDTH / 2);
    ui_draw_energy_bar(&g->players[1], 960, SCREEN_WIDTH / 2);

    // TODO: No visual divider line is drawn between Player 1 and Player 2 viewports. Add a separator.
    // TODO: No in-game card UI — cards are loaded and used for spawning but never rendered to the screen.
    // TODO: No game phase/state machine — the game boots directly into the play loop. Add a GamePhase
    // TODO: enum and dispatch update/render through pregame → playing → postgame for NFC integration.

    EndDrawing();
}

void game_cleanup(GameState *g) {
    nfc_shutdown(&g->nfc);

    // Cleanup players (frees tilemaps)
    player_cleanup(&g->players[0]);
    player_cleanup(&g->players[1]);

    sprite_atlas_free(&g->spriteAtlas);
    card_atlas_free(&g->cardAtlas);
    biome_free_all(g->biomeDefs);
    CloseWindow();
    cards_free_nfc_map(&g->deck);
    cards_free(&g->deck);
    db_close(&g->db);
}

int main(void) {
    GameState game = {0};

    if (!game_init(&game)) {
        return 1;
    }

    while (!WindowShouldClose()) {
        game_update(&game);
        game_render(&game);
    }

    game_cleanup(&game);
    return 0;
}
