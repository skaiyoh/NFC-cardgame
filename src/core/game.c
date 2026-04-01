//
// Created by Nathan Davis on 2/16/26.
//

#include "game.h"
#include "config.h"
#include "battlefield.h"
#include "../logic/card_effects.h"
#include "../rendering/viewport.h"
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

    // Load NFC UID -> card_id mappings (non-fatal if table is empty or missing)
    cards_load_nfc_map(&g->deck, &g->db);

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "NFC Card Game");
    SetTargetFPS(60);

    // Initialize card system
    card_action_init();
    card_atlas_init(&g->cardAtlas);

    // Initialize biome definitions (loads textures, builds tile defs)
    biome_init_all(g->biomeDefs);

    // Initialize canonical Battlefield (authoritative world model per D-11)
    float tileSize = DEFAULT_TILE_SIZE * DEFAULT_TILE_SCALE;
    bf_init(&g->battlefield, g->biomeDefs,
            BIOME_GRASS, BIOME_GRASS,  // bottom/top biome (matches current setup)
            tileSize, 42, 99);         // seeds match current hardcoded values

    // Initialize character sprite atlas
    sprite_atlas_init(&g->spriteAtlas);

    // Initialize split-screen viewports and players
    viewport_init_split_screen(g);

    // Initialize NFC serial ports (optional -- game works with keyboard input only if unset)
    g->nfc.fds[0] = -1;
    g->nfc.fds[1] = -1;
    const char *single_port = getenv("NFC_PORT");
    const char *port0 = getenv("NFC_PORT_P1");
    const char *port1 = getenv("NFC_PORT_P2");

    if (single_port) {
        if (!nfc_init_single(&g->nfc, single_port)) {
            printf("[NFC] Warning: failed to open test port -- NFC disabled\n");
        }
    } else if (port0 && port1) {
        if (!nfc_init(&g->nfc, port0, port1)) {
            printf("[NFC] Warning: failed to open serial ports -- NFC disabled\n");
        }
    } else {
        printf("[NFC] No NFC port env vars set -- NFC disabled\n");
    }

    return true;
}

// Simulate a knight card play through the full production code path:
// cards_find -> card_action_play -> play_knight -> spawn_troop_from_card -> troop_spawn
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

// Draw all Battlefield entities visible in the current viewport.
// Entities are in canonical world space; the active Camera2D handles
// projection. No ownership branching, no remap. (per D-19)
static void game_draw_canonical_entities(const Battlefield *bf) {
    for (int i = 0; i < bf->entityCount; i++) {
        const Entity *e = bf->entities[i];
        if (!e || !e->alive || !e->sprite) continue;
        entity_draw(e);
    }
}

void game_render(GameState *g) {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    Battlefield *bf = &g->battlefield;

    // --- Player 1 viewport (SIDE_BOTTOM) ---
    viewport_begin(&g->players[0]);
    // Draw BOTH territories' tilemaps so the full board is visible
    viewport_draw_battlefield_tilemap(bf, SIDE_BOTTOM);
    viewport_draw_battlefield_tilemap(bf, SIDE_TOP);
    game_draw_canonical_entities(bf);
    DrawText("PLAYER 1",
             (int)(bf->territories[SIDE_BOTTOM].bounds.x + 40),
             (int)(bf->territories[SIDE_BOTTOM].bounds.y + 40),
             40, DARKGREEN);
    viewport_end();

    // --- Player 2 viewport (SIDE_TOP) ---
    viewport_begin(&g->players[1]);
    viewport_draw_battlefield_tilemap(bf, SIDE_BOTTOM);
    viewport_draw_battlefield_tilemap(bf, SIDE_TOP);
    game_draw_canonical_entities(bf);
    DrawText("PLAYER 2",
             (int)(bf->territories[SIDE_TOP].bounds.x + 40),
             (int)(bf->territories[SIDE_TOP].bounds.y + 40),
             40, MAROON);
    viewport_end();

    // Debug lane overlay
    if (s_showLaneDebug) {
        debug_draw_lane_paths_screen(bf, SIDE_BOTTOM, g->players[0].camera);
        debug_draw_lane_paths_screen(bf, SIDE_TOP, g->players[1].camera);
    }

    // HUD -- screen space, drawn after all viewports
    ui_draw_energy_bar(&g->players[0], 0, SCREEN_WIDTH / 2);
    ui_draw_energy_bar(&g->players[1], 960, SCREEN_WIDTH / 2);

    EndDrawing();
}

void game_cleanup(GameState *g) {
    nfc_shutdown(&g->nfc);

    // Cleanup players (frees tilemaps)
    player_cleanup(&g->players[0]);
    player_cleanup(&g->players[1]);

    // Cleanup Battlefield (must be before biome_free_all since tilemaps reference biome textures)
    bf_cleanup(&g->battlefield);

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
