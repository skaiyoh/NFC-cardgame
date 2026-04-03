#include "../src/rendering/card_renderer.h"
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>

/* Read entire file into a malloc'd string. Returns NULL on failure. */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

/* ── layer editor ───────────────────────────────────────────────── */

typedef enum {
    LAYER_BG, LAYER_DESCRIPTION, LAYER_BORDER, LAYER_BANNER,
    LAYER_INNERCORNER, LAYER_CORNER, LAYER_CONTAINER,
    LAYER_SOCKET, LAYER_GEM, LAYER_ENERGY_TOP, LAYER_ENERGY_BOT,
    LAYER_COUNT
} LayerID;

static const char *layer_names[LAYER_COUNT] = {
    "Background", "Description", "Border", "Banner",
    "InnerCorner", "Corner", "Container",
    "Socket", "Gem", "EnergyTop", "EnergyBot"
};

/* Wrap an integer value around a range [0, max) */
static int wrap(int val, int max) {
    return ((val % max) + max) % max;
}

int main(int argc, char *argv[]) {
    const int winW = 620;
    const int winH = 750;

    InitWindow(winW, winH, "Card Preview Tool");
    SetTargetFPS(60);

    CardAtlas atlas;
    card_atlas_init(&atlas);

    CardVisual vis = card_visual_default();

    /* Load template from CLI argument: ./card_preview template.json */
    if (argc > 1) {
        char *json = read_file(argv[1]);
        if (json) {
            vis = card_visual_from_json(json);
            printf("Loaded template from %s\n", argv[1]);
            free(json);
        } else {
            fprintf(stderr, "Could not read file: %s\n", argv[1]);
        }
    }
    float scale = 4.0f;
    int active_layer = LAYER_BORDER;
    bool show_back = false;
    CardColor back_color = CLR_BROWN;

    while (!WindowShouldClose()) {
        /* ── input ─────────────────────────────────────────────── */

        /* Select active layer with number keys 1-9, 0 */
        for (int k = 0; k < LAYER_COUNT && k < 10; k++) {
            if (IsKeyPressed(KEY_ONE + k)) active_layer = k;
        }

        /* Tab cycles through layers */
        if (IsKeyPressed(KEY_TAB)) {
            active_layer = wrap(active_layer + (IsKeyDown(KEY_LEFT_SHIFT) ? -1 : 1), LAYER_COUNT);
        }

        /* LEFT / RIGHT: change color/style; with Shift: shift X position */
        int dir = 0;
        if (IsKeyPressed(KEY_RIGHT)) dir = 1;
        if (IsKeyPressed(KEY_LEFT))  dir = -1;

        if (dir != 0 && show_back) {
            /* In back view, left/right changes back color */
            back_color = wrap(back_color + dir, CLR_COUNT);
        } else if (dir != 0 && IsKeyDown(KEY_LEFT_SHIFT)) {
            /* Shift+Left/Right: nudge layer X position */
            vis.offsets.x[active_layer] += dir;
        } else if (dir != 0) {
            switch (active_layer) {
            case LAYER_BORDER:      vis.border_color = wrap(vis.border_color + dir, CLR_COUNT); break;
            case LAYER_BG:          vis.bg_style = wrap(vis.bg_style + dir, BG_COUNT); break;
            case LAYER_BANNER:      vis.banner_color = wrap(vis.banner_color + dir, CLR_COUNT); break;
            case LAYER_CORNER:      vis.corner_color = wrap(vis.corner_color + dir, CLR_COUNT); break;
            case LAYER_CONTAINER:   vis.container_color = wrap(vis.container_color + dir, CLR_COUNT); break;
            case LAYER_DESCRIPTION: vis.description_style = wrap(vis.description_style + dir, BG_COUNT); break;
            case LAYER_INNERCORNER: vis.innercorner_style = wrap(vis.innercorner_style + dir, IC_COUNT); break;
            case LAYER_GEM:         vis.gem_color = wrap(vis.gem_color + dir, CLR_COUNT); break;
            case LAYER_SOCKET:      vis.socket_color = wrap(vis.socket_color + dir, CLR_COUNT); break;
            case LAYER_ENERGY_TOP:  vis.energy_top_color = wrap(vis.energy_top_color + dir, CLR_COUNT); break;
            case LAYER_ENERGY_BOT:  vis.energy_bot_color = wrap(vis.energy_bot_color + dir, CLR_COUNT); break;
            }
        }

        /* UP / DOWN: nudge layer Y position; with Shift on Container: change variant */
        int vdir = 0;
        if (IsKeyPressed(KEY_UP))   vdir = -1;
        if (IsKeyPressed(KEY_DOWN)) vdir = 1;

        if (vdir != 0) {
            if (active_layer == LAYER_CONTAINER && IsKeyDown(KEY_LEFT_SHIFT)) {
                vis.container_variant = wrap(vis.container_variant + vdir, CONTAINER_COUNT);
            } else {
                vis.offsets.y[active_layer] += vdir;
            }
        }

        /* SPACE toggles any layer */
        if (IsKeyPressed(KEY_SPACE)) {
            switch (active_layer) {
            case LAYER_BG:          vis.show_bg          = !vis.show_bg;          break;
            case LAYER_DESCRIPTION: vis.show_description = !vis.show_description; break;
            case LAYER_BORDER:      vis.show_border      = !vis.show_border;      break;
            case LAYER_BANNER:      vis.show_banner      = !vis.show_banner;      break;
            case LAYER_INNERCORNER: vis.show_innercorner = !vis.show_innercorner; break;
            case LAYER_CORNER:      vis.show_corner      = !vis.show_corner;      break;
            case LAYER_CONTAINER:   vis.show_container   = !vis.show_container;   break;
            case LAYER_SOCKET:      vis.show_socket      = !vis.show_socket;      break;
            case LAYER_GEM:         vis.show_gem         = !vis.show_gem;         break;
            case LAYER_ENERGY_TOP:  vis.show_energy_top  = !vis.show_energy_top;  break;
            case LAYER_ENERGY_BOT:  vis.show_energy_bot  = !vis.show_energy_bot;  break;
            }
        }

        /* Zoom */
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))      scale += 0.5f;
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) scale -= 0.5f;
        if (scale < 1.0f) scale = 1.0f;
        if (scale > 10.0f) scale = 10.0f;

        /* Export JSON */
        if (IsKeyPressed(KEY_E)) {
            printf("\n── Card Visual JSON ──\n");
            card_visual_print_json(&vis);
            printf("──────────────────────\n\n");
        }

        /* Import from file (I key) — reads import.json from current directory */
        if (IsKeyPressed(KEY_I)) {
            const char *import_path = "import.json";
            char *json = read_file(import_path);
            if (json) {
                printf("Read file contents:\n%s\n", json);
                vis = card_visual_from_json(json);
                printf("After parse: border=%s bg=%s\n",
                       card_color_name(vis.border_color),
                       bg_style_name(vis.bg_style));
                free(json);
            } else {
                printf("No %s found in current directory\n", import_path);
            }
        }

        /* Reset to defaults */
        if (IsKeyPressed(KEY_R)) {
            vis = card_visual_default();
            back_color = CLR_BROWN;
        }

        /* Toggle front/back view */
        if (IsKeyPressed(KEY_B)) {
            show_back = !show_back;
        }

        /* ── draw ──────────────────────────────────────────────── */

        BeginDrawing();
        ClearBackground((Color){ 40, 40, 40, 255 });

        /* Card preview centered */
        float cardW = CARD_WIDTH * scale;
        float cardH = CARD_HEIGHT * scale;
        Vector2 cardPos = {
            (winW - cardW) / 2.0f,
            40.0f
        };
        if (show_back) {
            card_draw_back(&atlas, back_color, cardPos, scale);
        } else {
            card_draw_ex(&atlas, &vis, &vis.offsets, cardPos, scale);
        }

        /* HUD */
        int hudY = (int)(cardPos.y + cardH + 20);
        DrawText("CARD PREVIEW TOOL", 20, hudY, 20, WHITE);
        hudY += 28;
        DrawText("Tab/1-0: Select layer | Left/Right: Change color | Up/Down: Shift Y", 20, hudY, 10, GRAY);
        hudY += 14;
        DrawText("Shift+Left/Right: Shift X | Shift+Up/Down on Container: Variant", 20, hudY, 10, GRAY);
        hudY += 14;
        DrawText("Space: Toggle | +/-: Zoom | B: Back | E: Export | I: Import | R: Reset", 20, hudY, 10, GRAY);
        hudY += 22;

        if (show_back) {
            char backLine[64];
            snprintf(backLine, sizeof(backLine), "BACK VIEW  |  Color: %s  |  Left/Right to change",
                     card_color_name(back_color));
            DrawText(backLine, 20, hudY, 16, ORANGE);
            hudY += 25;
        }

        for (int i = 0; i < LAYER_COUNT; i++) {
            Color col = (i == active_layer) ? YELLOW : LIGHTGRAY;
            const char *val = "";
            const char *extra = "";

            switch (i) {
            case LAYER_BG:
                val = bg_style_name(vis.bg_style);
                extra = vis.show_bg ? "ON" : "OFF";
                break;
            case LAYER_DESCRIPTION:
                val = bg_style_name(vis.description_style);
                extra = vis.show_description ? "ON" : "OFF";
                break;
            case LAYER_BORDER:
                val = card_color_name(vis.border_color);
                extra = vis.show_border ? "ON" : "OFF";
                break;
            case LAYER_BANNER:
                val = card_color_name(vis.banner_color);
                extra = vis.show_banner ? "ON" : "OFF";
                break;
            case LAYER_INNERCORNER:
                val = innercorner_style_name(vis.innercorner_style);
                extra = vis.show_innercorner ? "ON" : "OFF";
                break;
            case LAYER_CORNER:
                val = card_color_name(vis.corner_color);
                extra = vis.show_corner ? "ON" : "OFF";
                break;
            case LAYER_CONTAINER:
                val = card_color_name(vis.container_color);
                break;
            case LAYER_SOCKET:
                val = card_color_name(vis.socket_color);
                extra = vis.show_socket ? "ON" : "OFF";
                break;
            case LAYER_GEM:
                val = card_color_name(vis.gem_color);
                extra = vis.show_gem ? "ON" : "OFF";
                break;
            case LAYER_ENERGY_TOP:
                val = card_color_name(vis.energy_top_color);
                extra = vis.show_energy_top ? "ON" : "OFF";
                break;
            case LAYER_ENERGY_BOT:
                val = card_color_name(vis.energy_bot_color);
                extra = vis.show_energy_bot ? "ON" : "OFF";
                break;
            }

            char line[180];
            char offset_str[32] = "";
            if (vis.offsets.x[i] != 0 || vis.offsets.y[i] != 0) {
                snprintf(offset_str, sizeof(offset_str), "  (x%+.0f y%+.0f)", vis.offsets.x[i], vis.offsets.y[i]);
            }

            char marker = (i == active_layer) ? '>' : ' ';
            if (i == LAYER_CONTAINER)
                snprintf(line, sizeof(line), "%c %2d. %-13s: %-12s  var:%s [%s]%s",
                         marker, i + 1, layer_names[i], val,
                         container_variant_name(vis.container_variant),
                         vis.show_container ? "ON" : "OFF",
                         offset_str);
            else if (extra[0])
                snprintf(line, sizeof(line), "%c %2d. %-13s: %-12s  [%s]%s",
                         marker, i + 1, layer_names[i], val, extra, offset_str);
            else
                snprintf(line, sizeof(line), "%c %2d. %-13s: %s%s",
                         marker, i + 1, layer_names[i], val, offset_str);

            DrawText(line, 20, hudY + i * 20, 14, col);
        }

        /* Scale indicator */
        char scaleTxt[32];
        snprintf(scaleTxt, sizeof(scaleTxt), "Scale: %.1fx", scale);
        DrawText(scaleTxt, winW - 120, hudY, 14, LIGHTGRAY);

        EndDrawing();
    }

    card_atlas_free(&atlas);
    CloseWindow();
    return 0;
}
