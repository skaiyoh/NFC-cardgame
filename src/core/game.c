//
// Created by Nathan Davis on 2/16/26.
//

#include "game.h"
#include "config.h"
#include "battlefield.h"
#include "sustenance.h"
#include "debug_events.h"
#include "../logic/card_effects.h"
#include "../logic/farmer.h"
#include "../logic/win_condition.h"
#include "../rendering/viewport.h"
#include "../rendering/debug_overlay.h"
#include "../rendering/sustenance_renderer.h"
#include "../rendering/spawn_fx.h"
#include "../rendering/ui.h"
#include "../systems/player.h"
#include "../entities/entities.h"
#include "../entities/building.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

static bool s_showLaneDebug = false;
static DebugOverlayFlags s_debugFlags = {0};

bool game_init(GameState *g) {
    srand((unsigned int) time(NULL));
    // Derive sustenance seed before bf_init: tilemap creation reseeds global rand(),
    // so generating this afterward would lock sustenance placement to deterministic values.
    uint32_t sustenanceSeed = ((uint32_t)rand() << 16) ^ (uint32_t)rand();
    if (sustenanceSeed == 0) sustenanceSeed = 1;

    const char *db_path = getenv("DB_PATH");
    if (!db_path) db_path = "cardgame.db";
    if (!db_init(&g->db, db_path)) {
        printf("db_init failed -- ensure %s exists (run: make init-db)\n", db_path);
        return false;
    }

    if (!cards_load(&g->deck, &g->db)) {
        db_close(&g->db);
        return false;
    }

    // Load NFC UID -> card_id mappings (non-fatal if table is empty or missing)
    cards_load_nfc_map(&g->deck, &g->db);

    SetConfigFlags(FLAG_WINDOW_UNDECORATED);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "NFC Card Game");
    SetWindowPosition(0, 0);
    SetTargetFPS(60);
    HideCursor();

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

    // Initialize sustenance resource nodes (dedicated RNG, after bf_init generates waypoints)
    sustenance_init(&g->battlefield, sustenanceSeed);

    // Load sustenance texture
    g->sustenanceTexture = sustenance_renderer_load();

    // Initialize character sprite atlas
    sprite_atlas_init(&g->spriteAtlas);
    spawn_fx_init(&g->spawnFx);

    // Initialize split-screen viewports and players
    viewport_init_split_screen(g);

    // Spawn home bases behind center-lane spawn points
    for (int i = 0; i < 2; i++) {
        BattleSide side = bf_side_for_player(i);
        CanonicalPos anchor = bf_base_anchor(&g->battlefield, side);
        Entity *base = building_create_base(&g->players[i], anchor.v, &g->spriteAtlas);
        if (base) {
            g->players[i].base = base;
            bf_add_entity(&g->battlefield, base);
        }
    }

    // Match result state
    g->gameOver = false;
    g->winnerID = -1;

    // P2 viewport render target (flipped vertically for across-the-table perspective)
    g->p2RT = LoadRenderTexture(g->halfWidth, SCREEN_HEIGHT);

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

static void game_handle_debug_input(void) {
    if (IsKeyPressed(KEY_F1)) s_showLaneDebug = !s_showLaneDebug;
    if (IsKeyPressed(KEY_F2)) s_debugFlags.attackBars   = !s_debugFlags.attackBars;
    if (IsKeyPressed(KEY_F3)) s_debugFlags.targetLines  = !s_debugFlags.targetLines;
    if (IsKeyPressed(KEY_F4)) s_debugFlags.eventFlashes = !s_debugFlags.eventFlashes;
    if (IsKeyPressed(KEY_F5)) s_debugFlags.rangeCirlces = !s_debugFlags.rangeCirlces;
    if (IsKeyPressed(KEY_F6)) s_debugFlags.sustenanceNodes     = !s_debugFlags.sustenanceNodes;
    if (IsKeyPressed(KEY_F7)) s_debugFlags.sustenancePlacement  = !s_debugFlags.sustenancePlacement;
}

static void game_test_play_farmer(GameState *g, int playerIndex, int slotIndex) {
    Card *card = cards_find(&g->deck, "FARMER_01");
    if (!card) {
        printf("[TEST] FARMER_01 not found in deck\n");
        return;
    }
    card_action_play(card, g, playerIndex, slotIndex);
}

static void game_handle_spawn_input(GameState *g) {
    // Player 1: key 1/2/3 = knight, F = farmer
    if (IsKeyPressed(KEY_ONE)) game_test_play_knight(g, 0, 0);
    if (IsKeyPressed(KEY_TWO)) game_test_play_knight(g, 0, 1);
    if (IsKeyPressed(KEY_THREE)) game_test_play_knight(g, 0, 2);
    if (IsKeyPressed(KEY_F)) game_test_play_farmer(g, 0, 0);

    // Player 2: key Q/W/E = knight, R = farmer
    if (IsKeyPressed(KEY_Q)) game_test_play_knight(g, 1, 0);
    if (IsKeyPressed(KEY_W)) game_test_play_knight(g, 1, 1);
    if (IsKeyPressed(KEY_E)) game_test_play_knight(g, 1, 2);
    if (IsKeyPressed(KEY_R)) game_test_play_farmer(g, 1, 0);
}

void game_update(GameState *g) {
    float deltaTime = fminf(GetFrameTime(), 1.0f / 20.0f);
    spawn_fx_update(&g->spawnFx, deltaTime);

    // Debug toggles always active (even after gameOver)
    game_handle_debug_input();

    // Freeze gameplay once match result is latched
    // (debug event timers still decay so hit flashes fade naturally)
    if (g->gameOver) {
        debug_events_tick(deltaTime);
        return;
    }

    game_handle_nfc_events(g);
    game_handle_spawn_input(g);

    // Update both players (energy regen, slot cooldowns)
    player_update(&g->players[0], deltaTime);
    player_update(&g->players[1], deltaTime);

    // Update all entities from Battlefield registry
    Battlefield *bf = &g->battlefield;
    for (int i = 0; i < bf->entityCount; i++) {
        entity_update(bf->entities[i], g, deltaTime);
        if (g->gameOver) break;  // Win latched mid-loop — stop processing
    }

    // Defensive fallback: catch base deaths from non-combat paths
    win_check(g);

    // Sweep dead/removed entities (runs once on the trigger frame, then frozen)
    for (int i = bf->entityCount - 1; i >= 0; i--) {
        if (bf->entities[i]->markedForRemoval) {
            Entity *dead = bf->entities[i];

            // Farmer death fallback: release claims / award sustenance.
            // farmer_on_death is idempotent — safe if already called from combat.
            if (dead->unitRole == UNIT_ROLE_FARMER) {
                farmer_on_death(dead, g);
            }

            // Clear stale base pointers before freeing memory
            if (dead == g->players[0].base) g->players[0].base = NULL;
            if (dead == g->players[1].base) g->players[1].base = NULL;

            bf->entities[i] = bf->entities[bf->entityCount - 1];
            bf->entityCount--;
            entity_destroy(dead);
        }
    }

    debug_events_tick(deltaTime);
}

// Draw all Battlefield entities visible in the current viewport.
// Entities are in canonical world space; the active Camera2D handles
// projection. No ownership branching, no remap. (per D-19)
static void game_draw_canonical_entities(const Battlefield *bf) {
    for (int i = 0; i < bf->entityCount; i++) {
        const Entity *e = bf->entities[i];
        if (!e || e->markedForRemoval || !e->sprite) continue;
        entity_draw(e);
    }
}

// Debug overlay is now in src/rendering/debug_overlay.c

void game_render(GameState *g) {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    Battlefield *bf = &g->battlefield;

    // --- Player 1 viewport (SIDE_BOTTOM) — direct to screen ---
    viewport_begin(&g->players[0]);
    viewport_draw_battlefield_tilemap(bf, SIDE_BOTTOM);
    viewport_draw_battlefield_tilemap(bf, SIDE_TOP);
    sustenance_renderer_draw(&bf->sustenanceField, SIDE_BOTTOM, g->sustenanceTexture, 0.0f);
    sustenance_renderer_draw(&bf->sustenanceField, SIDE_TOP, g->sustenanceTexture, 0.0f);
    spawn_fx_draw(&g->spawnFx, 180.0f);
    game_draw_canonical_entities(bf);
    debug_overlay_draw(bf, g, s_debugFlags);
    viewport_end();

    // --- Player 2 viewport (SIDE_TOP) — render to texture, then flip ---
    // P2 uses rot=+90 (same as P1) for correct seam placement.
    // The RT is flipped vertically when composited to reverse the world-X
    // orientation, giving P2 the opposite (across-the-table) perspective.
    BeginTextureMode(g->p2RT);
    ClearBackground(RAYWHITE);
    // Render with P2's camera but into the RT (no scissor needed — RT is viewport-sized).
    // Override camera offset to center of RT (480,540) instead of screen position (1440,540).
    Camera2D p2CamRT = g->players[1].camera;
    p2CamRT.offset = (Vector2){ g->halfWidth / 2.0f, SCREEN_HEIGHT / 2.0f };
    BeginMode2D(p2CamRT);
    viewport_draw_battlefield_tilemap(bf, SIDE_BOTTOM);
    viewport_draw_battlefield_tilemap(bf, SIDE_TOP);
    sustenance_renderer_draw(&bf->sustenanceField, SIDE_BOTTOM, g->sustenanceTexture, 180.0f);
    sustenance_renderer_draw(&bf->sustenanceField, SIDE_TOP, g->sustenanceTexture, 180.0f);
    spawn_fx_draw(&g->spawnFx, 0.0f);
    game_draw_canonical_entities(bf);
    debug_overlay_draw(bf, g, s_debugFlags);
    EndMode2D();
    EndTextureMode();

    // Composite P2 RT to right half of screen, flipped vertically.
    // Negative height flips Y (OpenGL convention), and we also flip the
    // source rect height to flip the image vertically on screen, which
    // reverses the world-X → screen-Y mapping for across-the-table.
    DrawTexturePro(
        g->p2RT.texture,
        (Rectangle){ 0, 0, (float)g->halfWidth, -(float)SCREEN_HEIGHT },   // src: flip Y (OpenGL)
        (Rectangle){ (float)g->halfWidth, 0, (float)g->halfWidth, (float)SCREEN_HEIGHT },  // dst: right half
        (Vector2){ 0, 0 },
        0.0f,
        WHITE
    );

    // Debug lane overlay
    if (s_showLaneDebug) {
        debug_draw_lane_paths_screen(bf, SIDE_BOTTOM, g->players[0].camera);
        debug_draw_lane_paths_screen(bf, SIDE_TOP, g->players[1].camera);
    }

    // HUD — screen space, drawn after all viewports
    ui_draw_viewport_label("PLAYER 1", g->players[0].screenArea,
                           UI_CORNER_TOP_RIGHT, 90.0f, DARKGREEN);
    ui_draw_viewport_label("PLAYER 2", g->players[1].screenArea,
                           UI_CORNER_BOTTOM_LEFT, 270.0f, MAROON);
    ui_draw_sustenance_counter(&g->players[0], g->players[0].screenArea, 90.0f, DARKGREEN);
    ui_draw_sustenance_counter(&g->players[1], g->players[1].screenArea, 270.0f, MAROON);
    ui_draw_energy_bar(&g->players[0], 0, SCREEN_WIDTH / 2);
    ui_draw_energy_bar(&g->players[1], 960, SCREEN_WIDTH / 2);

    // Match result overlay
    if (g->gameOver) {
        for (int i = 0; i < 2; i++) {
            const bool drawnMatch = (g->winnerID < 0);
            const bool playerWon = (g->winnerID == g->players[i].id);
            const char *text = drawnMatch ? "DRAW" : (playerWon ? "VICTORY" : "DEFEAT");
            Color color = drawnMatch ? LIGHTGRAY : (playerWon ? GOLD : RED);
            float rotation = (i == 0) ? 90.0f : 270.0f;
            ui_draw_match_result(&g->players[i], text, rotation, color);
        }
    }

    EndDrawing();
}

void game_cleanup(GameState *g) {
    nfc_shutdown(&g->nfc);

    // Cleanup players (no resources to free -- Battlefield owns tilemaps and entities)
    player_cleanup(&g->players[0]);
    player_cleanup(&g->players[1]);

    UnloadRenderTexture(g->p2RT);

    // Destroy all live entities before dropping the registry
    for (int i = 0; i < g->battlefield.entityCount; i++) {
        entity_destroy(g->battlefield.entities[i]);
    }
    g->players[0].base = NULL;
    g->players[1].base = NULL;

    // Unload sustenance texture
    UnloadTexture(g->sustenanceTexture);

    spawn_fx_cleanup(&g->spawnFx);
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
