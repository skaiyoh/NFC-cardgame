//
// Created by Nathan Davis on 2/16/26.
//

#include "card_renderer.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *clr_names[CLR_COUNT] = {
    "aqua", "black", "blue", "blue_light", "brown",
    "gray", "green", "magenta", "pink", "purple",
    "red", "white", "yellow"
};

static const char *bg_names[BG_COUNT] = {
    "black", "brown", "paper", "white"
};

static const char *ic_names[IC_COUNT] = {
    "black", "brown", "white", "yellow"
};

static const char *layer_names[CARD_LAYER_COUNT] = {
    "Background", "Description", "Border", "Banner",
    "InnerCorner", "Corner", "Container",
    "Socket", "Gem", "EnergyTop", "EnergyBot"
};

const char *card_color_name(CardColor c) {
    return (c >= 0 && c < CLR_COUNT) ? clr_names[c] : "unknown";
}

const char *bg_style_name(BGStyle s) {
    return (s >= 0 && s < BG_COUNT) ? bg_names[s] : "unknown";
}

const char *container_variant_name(ContainerVariant v) {
    static const char *names[] = {"1", "2", "3"};
    return (v >= 0 && v < CONTAINER_COUNT) ? names[v] : "?";
}

const char *innercorner_style_name(InnerCornerStyle s) {
    return (s >= 0 && s < IC_COUNT) ? ic_names[s] : "unknown";
}

#define R(x, y, w, h)  (Rectangle){ (x), (y), (w), (h) }
#define EMPTY          (Rectangle){ 0, 0, 0, 0 }

static void init_rects(CardAtlas *atlas) {
    memset(atlas->borders, 0, sizeof(atlas->borders));
    memset(atlas->corners, 0, sizeof(atlas->corners));
    memset(atlas->backs, 0, sizeof(atlas->backs));
    memset(atlas->banners, 0, sizeof(atlas->banners));
    memset(atlas->bgs, 0, sizeof(atlas->bgs));
    memset(atlas->descriptions, 0, sizeof(atlas->descriptions));
    memset(atlas->containers, 0, sizeof(atlas->containers));
    memset(atlas->inner_corners, 0, sizeof(atlas->inner_corners));
    memset(atlas->gems, 0, sizeof(atlas->gems));
    memset(atlas->sockets, 0, sizeof(atlas->sockets));
    memset(atlas->energy_top, 0, sizeof(atlas->energy_top));
    memset(atlas->energy_bot, 0, sizeof(atlas->energy_bot));
    memset(atlas->energy_full, 0, sizeof(atlas->energy_full));

    atlas->borders[CLR_RED] = R(1, 1, 46, 62);
    atlas->borders[CLR_YELLOW] = R(1, 65, 46, 62);
    atlas->borders[CLR_GREEN] = R(1, 129, 46, 62);
    atlas->borders[CLR_BLUE] = R(1, 193, 46, 62);
    atlas->borders[CLR_PURPLE] = R(1, 257, 46, 62);
    atlas->borders[CLR_MAGENTA] = R(1, 321, 46, 62);
    atlas->borders[CLR_AQUA] = R(49, 1, 46, 62);
    atlas->borders[CLR_BROWN] = R(49, 65, 46, 62);
    atlas->borders[CLR_BLACK] = R(49, 129, 46, 62);
    atlas->borders[CLR_GRAY] = R(49, 193, 46, 62);
    atlas->borders[CLR_WHITE] = R(49, 257, 46, 62);

    atlas->corners[CLR_RED] = R(96, 0, 48, 64);
    atlas->corners[CLR_YELLOW] = R(96, 64, 48, 64);
    atlas->corners[CLR_GREEN] = R(96, 128, 48, 64);
    atlas->corners[CLR_BLUE] = R(96, 192, 48, 64);
    atlas->corners[CLR_PURPLE] = R(96, 256, 48, 64);
    atlas->corners[CLR_MAGENTA] = R(48, 320, 48, 64);
    atlas->corners[CLR_AQUA] = R(144, 0, 48, 64);
    atlas->corners[CLR_BROWN] = R(144, 64, 48, 64);
    atlas->corners[CLR_BLACK] = R(144, 128, 48, 64);
    atlas->corners[CLR_GRAY] = R(144, 192, 48, 64);
    atlas->corners[CLR_WHITE] = R(144, 256, 48, 64);

    atlas->backs[CLR_RED] = R(195, 3, 42, 58);
    atlas->backs[CLR_YELLOW] = R(195, 67, 42, 58);
    atlas->backs[CLR_GREEN] = R(195, 131, 42, 58);
    atlas->backs[CLR_BLUE] = R(195, 195, 42, 58);
    atlas->backs[CLR_PURPLE] = R(195, 259, 42, 58);
    atlas->backs[CLR_MAGENTA] = R(195, 323, 42, 58);
    atlas->backs[CLR_AQUA] = R(243, 3, 42, 58);
    atlas->backs[CLR_BROWN] = R(243, 67, 42, 58);
    atlas->backs[CLR_BLACK] = R(243, 131, 42, 58);
    atlas->backs[CLR_GRAY] = R(243, 195, 42, 58);
    atlas->backs[CLR_WHITE] = R(243, 259, 42, 58);

    atlas->banners[CLR_MAGENTA] = R(288, 2, 48, 11);
    atlas->banners[CLR_RED] = R(288, 18, 48, 11);
    atlas->banners[CLR_YELLOW] = R(288, 34, 48, 11);
    atlas->banners[CLR_GREEN] = R(288, 50, 48, 11);
    atlas->banners[CLR_AQUA] = R(288, 66, 48, 11);
    atlas->banners[CLR_BLUE] = R(288, 82, 48, 11);
    atlas->banners[CLR_PURPLE] = R(288, 98, 48, 11);
    atlas->banners[CLR_BROWN] = R(288, 114, 48, 11);
    atlas->banners[CLR_BLACK] = R(288, 130, 48, 11);
    atlas->banners[CLR_GRAY] = R(288, 146, 48, 11);
    atlas->banners[CLR_WHITE] = R(288, 162, 48, 11);

    atlas->bgs[BG_BLACK] = R(341, 277, 38, 25);
    atlas->bgs[BG_BROWN] = R(341, 181, 38, 25);
    atlas->bgs[BG_PAPER] = R(341, 213, 38, 25);
    atlas->bgs[BG_WHITE] = R(341, 245, 38, 25);

    atlas->descriptions[BG_BROWN] = R(293, 184, 38, 19);
    atlas->descriptions[BG_PAPER] = R(293, 216, 38, 19);
    atlas->descriptions[BG_WHITE] = R(293, 248, 38, 19);
    atlas->descriptions[BG_BLACK] = R(293, 280, 38, 19);

    atlas->containers[CLR_MAGENTA][CONTAINER_1] = R(401, 1, 14, 14);
    atlas->containers[CLR_RED][CONTAINER_1] = R(401, 17, 14, 14);
    atlas->containers[CLR_YELLOW][CONTAINER_1] = R(401, 33, 14, 14);
    atlas->containers[CLR_GREEN][CONTAINER_1] = R(401, 49, 14, 14);
    atlas->containers[CLR_AQUA][CONTAINER_1] = R(401, 65, 14, 14);
    atlas->containers[CLR_BLUE][CONTAINER_1] = R(401, 81, 14, 14);
    atlas->containers[CLR_PURPLE][CONTAINER_1] = R(401, 97, 14, 14);
    atlas->containers[CLR_BROWN][CONTAINER_1] = R(401, 113, 14, 14);
    atlas->containers[CLR_BLACK][CONTAINER_1] = R(401, 129, 14, 14);
    atlas->containers[CLR_GRAY][CONTAINER_1] = R(401, 145, 14, 14);
    atlas->containers[CLR_WHITE][CONTAINER_1] = R(401, 161, 14, 14);

    atlas->containers[CLR_MAGENTA][CONTAINER_2] = R(417, 1, 14, 14);
    atlas->containers[CLR_RED][CONTAINER_2] = R(417, 17, 14, 14);
    atlas->containers[CLR_YELLOW][CONTAINER_2] = R(417, 33, 14, 14);
    atlas->containers[CLR_GREEN][CONTAINER_2] = R(417, 49, 14, 14);
    atlas->containers[CLR_AQUA][CONTAINER_2] = R(417, 65, 14, 14);
    atlas->containers[CLR_BLUE][CONTAINER_2] = R(417, 81, 14, 14);
    atlas->containers[CLR_PURPLE][CONTAINER_2] = R(417, 97, 14, 14);
    atlas->containers[CLR_BROWN][CONTAINER_2] = R(417, 113, 14, 14);
    atlas->containers[CLR_BLACK][CONTAINER_2] = R(417, 129, 14, 14);
    atlas->containers[CLR_GRAY][CONTAINER_2] = R(417, 145, 14, 14);
    atlas->containers[CLR_WHITE][CONTAINER_2] = R(417, 161, 14, 14);

    atlas->containers[CLR_MAGENTA][CONTAINER_3] = R(432, 0, 16, 16);
    atlas->containers[CLR_RED][CONTAINER_3] = R(432, 16, 16, 16);
    atlas->containers[CLR_YELLOW][CONTAINER_3] = R(432, 32, 16, 16);
    atlas->containers[CLR_GREEN][CONTAINER_3] = R(432, 48, 16, 16);
    atlas->containers[CLR_AQUA][CONTAINER_3] = R(432, 64, 16, 16);
    atlas->containers[CLR_BLUE][CONTAINER_3] = R(432, 80, 16, 16);
    atlas->containers[CLR_PURPLE][CONTAINER_3] = R(432, 96, 16, 16);
    atlas->containers[CLR_BROWN][CONTAINER_3] = R(432, 112, 16, 16);
    atlas->containers[CLR_BLACK][CONTAINER_3] = R(432, 128, 16, 16);
    atlas->containers[CLR_GRAY][CONTAINER_3] = R(432, 144, 16, 16);
    atlas->containers[CLR_WHITE][CONTAINER_3] = R(432, 160, 16, 16);

    atlas->inner_corners[IC_BROWN] = R(389, 181, 38, 6);
    atlas->inner_corners[IC_BLACK] = R(389, 197, 38, 6);
    atlas->inner_corners[IC_YELLOW] = R(389, 213, 38, 6);
    atlas->inner_corners[IC_WHITE] = R(389, 229, 38, 6);

    atlas->gems[CLR_RED] = R(450, 58, 4, 4);
    atlas->gems[CLR_GREEN] = R(450, 66, 4, 4);
    atlas->gems[CLR_BLUE] = R(450, 74, 4, 4);
    atlas->gems[CLR_BROWN] = R(450, 82, 4, 4);
    atlas->gems[CLR_BLACK] = R(450, 90, 4, 4);
    atlas->gems[CLR_MAGENTA] = R(450, 98, 4, 4);
    atlas->gems[CLR_PINK] = R(458, 50, 4, 4);
    atlas->gems[CLR_YELLOW] = R(458, 58, 4, 4);
    atlas->gems[CLR_AQUA] = R(458, 66, 4, 4);
    atlas->gems[CLR_PURPLE] = R(458, 74, 4, 4);
    atlas->gems[CLR_WHITE] = R(458, 82, 4, 4);
    atlas->gems[CLR_GRAY] = R(458, 90, 4, 4);
    atlas->gems[CLR_BLUE_LIGHT] = R(458, 98, 4, 4);

    atlas->sockets[CLR_RED] = R(448, 0, 8, 8);
    atlas->sockets[CLR_GREEN] = R(448, 8, 8, 8);
    atlas->sockets[CLR_BLUE] = R(448, 16, 8, 8);
    atlas->sockets[CLR_BROWN] = R(448, 24, 8, 8);
    atlas->sockets[CLR_BLACK] = R(448, 32, 8, 8);
    atlas->sockets[CLR_MAGENTA] = R(448, 40, 8, 8);
    atlas->sockets[CLR_PINK] = R(448, 48, 8, 8);
    atlas->sockets[CLR_YELLOW] = R(456, 0, 8, 8);
    atlas->sockets[CLR_AQUA] = R(456, 8, 8, 8);
    atlas->sockets[CLR_PURPLE] = R(456, 16, 8, 8);
    atlas->sockets[CLR_WHITE] = R(456, 24, 8, 8);
    atlas->sockets[CLR_GRAY] = R(456, 32, 8, 8);
    atlas->sockets[CLR_BLUE_LIGHT] = R(456, 40, 8, 8);

    atlas->energy_top[CLR_MAGENTA] = R(338, 2, 12, 12);
    atlas->energy_top[CLR_RED] = R(338, 18, 12, 12);
    atlas->energy_top[CLR_YELLOW] = R(338, 34, 12, 12);
    atlas->energy_top[CLR_GREEN] = R(338, 50, 12, 12);
    atlas->energy_top[CLR_AQUA] = R(338, 66, 12, 12);
    atlas->energy_top[CLR_BLUE] = R(338, 82, 12, 12);
    atlas->energy_top[CLR_PURPLE] = R(338, 98, 12, 12);
    atlas->energy_top[CLR_BROWN] = R(338, 114, 12, 12);
    atlas->energy_top[CLR_BLACK] = R(338, 130, 12, 12);
    atlas->energy_top[CLR_GRAY] = R(338, 146, 12, 12);
    atlas->energy_top[CLR_WHITE] = R(338, 162, 12, 12);
    atlas->energy_top[CLR_BLUE_LIGHT] = R(370, 34, 12, 12);
    atlas->energy_top[CLR_PINK] = R(370, 50, 12, 12);

    atlas->energy_bot[CLR_MAGENTA] = R(354, 2, 12, 12);
    atlas->energy_bot[CLR_RED] = R(354, 18, 12, 12);
    atlas->energy_bot[CLR_YELLOW] = R(354, 34, 12, 12);
    atlas->energy_bot[CLR_GREEN] = R(354, 50, 12, 12);
    atlas->energy_bot[CLR_AQUA] = R(354, 66, 12, 12);
    atlas->energy_bot[CLR_BLUE] = R(354, 82, 12, 12);
    atlas->energy_bot[CLR_PURPLE] = R(354, 98, 12, 12);
    atlas->energy_bot[CLR_BROWN] = R(354, 114, 12, 12);
    atlas->energy_bot[CLR_BLACK] = R(354, 130, 12, 12);
    atlas->energy_bot[CLR_GRAY] = R(354, 146, 12, 12);
    atlas->energy_bot[CLR_WHITE] = R(354, 162, 12, 12);
    atlas->energy_bot[CLR_BLUE_LIGHT] = R(386, 34, 12, 12);
    atlas->energy_bot[CLR_PINK] = R(386, 50, 12, 12);
}

// TODO: card_atlas_init does not check whether LoadTexture succeeded. If CARD_SHEET_PATH is
// TODO: missing, Raylib returns a 1x1 white fallback texture and the printf below reports "0x0"
// TODO: (or "1x1"). All card draws will then produce white rectangles with no error logged.
// TODO: Check atlas->sheet.id > 1 (or IsTextureValid) and abort with an error message if it fails.
void card_atlas_init_layout(CardAtlas *atlas) {
    memset(atlas, 0, sizeof(CardAtlas));
    init_rects(atlas);
}

void card_atlas_init(CardAtlas *atlas) {
    card_atlas_init_layout(atlas);
    atlas->sheet = LoadTexture(CARD_SHEET_PATH);
    SetTextureFilter(atlas->sheet, TEXTURE_FILTER_POINT);
    printf("Card atlas loaded (sheet %dx%d)\n", atlas->sheet.width, atlas->sheet.height);
}

void card_atlas_free(CardAtlas *atlas) {
    if (atlas->sheet.id) UnloadTexture(atlas->sheet);
    memset(atlas, 0, sizeof(CardAtlas));
}

static void draw_layer(Texture2D sheet, Rectangle src,
                       Vector2 card_pos, float ox, float oy, float scale) {
    if (src.width == 0 || src.height == 0) return;

    DrawTexturePro(sheet, src,
                   (Rectangle)
    {
        card_pos.x + ox * scale,
                card_pos.y + oy * scale,
                src.width * scale,
                src.height * scale
    }
    ,
    (Vector2)
    {
        0, 0
    }
    ,
    0.0f, WHITE
    )
    ;
}

static void get_base_positions(float *ox, float *oy) {
    float bx = CARD_MARGIN;
    float by = CARD_MARGIN;

    ox[0] = bx + (CARD_BODY_W - 38) / 2.0f;
    oy[0] = by + 4;
    ox[1] = bx + (CARD_BODY_W - 38) / 2.0f;
    oy[1] = by + 32 + 7;
    ox[2] = bx;
    oy[2] = by;
    ox[3] = bx - 1;
    oy[3] = by + 27 + 2;
    ox[4] = bx + (CARD_BODY_W - 38) / 2.0f;
    oy[4] = by + 24 - 20;
    ox[5] = bx - 1;
    oy[5] = by - 1;
    ox[6] = 3;
    oy[6] = 3;
    ox[7] = 4;
    oy[7] = 4;
    ox[8] = 4 + (8 - 4) / 2.0f;
    oy[8] = 4 + (8 - 4) / 2.0f;
    ox[9] = ox[6] + (14 - 12) / 2.0f;
    oy[9] = oy[6] + (14 - 12) / 2.0f;
    ox[10] = ox[6] + (14 - 12) / 2.0f;
    oy[10] = oy[6] + (14 - 12) / 2.0f;
}

static bool get_layer_source_and_visibility(const CardAtlas *atlas, const CardVisual *visual,
                                            int layerIndex, Rectangle *src, bool *visible) {
    if (!atlas || !visual || !src || !visible) return false;

    switch (layerIndex) {
        case 0:
            *src = atlas->bgs[visual->bg_style];
            *visible = visual->show_bg;
            return true;
        case 1:
            *src = atlas->descriptions[visual->description_style];
            *visible = visual->show_description;
            return true;
        case 2:
            *src = atlas->borders[visual->border_color];
            *visible = visual->show_border;
            return true;
        case 3:
            *src = atlas->banners[visual->banner_color];
            *visible = visual->show_banner;
            return true;
        case 4:
            *src = atlas->inner_corners[visual->innercorner_style];
            *visible = visual->show_innercorner;
            return true;
        case 5:
            *src = atlas->corners[visual->corner_color];
            *visible = visual->show_corner;
            return true;
        case 6:
            *src = atlas->containers[visual->container_color][visual->container_variant];
            *visible = visual->show_container;
            return true;
        case 7:
            *src = atlas->sockets[visual->socket_color];
            *visible = visual->show_socket;
            return true;
        case 8:
            *src = atlas->gems[visual->gem_color];
            *visible = visual->show_gem;
            return true;
        case 9:
            *src = atlas->energy_top[visual->energy_top_color];
            *visible = visual->show_energy_top;
            return true;
        case 10:
            *src = atlas->energy_bot[visual->energy_bot_color];
            *visible = visual->show_energy_bot;
            return true;
        default:
            *src = EMPTY;
            *visible = false;
            return false;
    }
}

CardLayerOffsets card_layer_offsets_default(void) {
    CardLayerOffsets o = {0};
    return o;
}

// TODO: card_draw and card_draw_ex are never called from game.c or any game-loop code.
// TODO: Card visuals are only used in the card_preview standalone tool. Cards are not rendered
// TODO: in-game at all — players have no visible hand, played cards produce no on-screen feedback.
// TODO: Integrate card_draw into the main render pass (e.g. draw the active hand above each viewport).
void card_draw_ex(const CardAtlas *atlas, const CardVisual *visual,
                  const CardLayerOffsets *offsets, Vector2 pos, float scale) {
    Texture2D s = atlas->sheet;
    float ox[CARD_LAYER_COUNT], oy[CARD_LAYER_COUNT];
    get_base_positions(ox, oy);

    if (offsets) {
        for (int i = 0; i < CARD_LAYER_COUNT; i++) {
            ox[i] += offsets->x[i];
            oy[i] += offsets->y[i];
        }
    }

    for (int i = 0; i < CARD_LAYER_COUNT; i++) {
        Rectangle src = EMPTY;
        bool visible = false;
        if (get_layer_source_and_visibility(atlas, visual, i, &src, &visible) && visible) {
            draw_layer(s, src, pos, ox[i], oy[i], scale);
        }
    }
}

void card_draw(const CardAtlas *atlas, const CardVisual *visual,
               Vector2 pos, float scale) {
    card_draw_ex(atlas, visual, &visual->offsets, pos, scale);
}

void card_draw_back(const CardAtlas *atlas, CardColor color,
                    Vector2 pos, float scale) {
    float ox = CARD_MARGIN + (CARD_BODY_W - 42) / 2.0f;
    float oy = CARD_MARGIN + (CARD_BODY_H - 58) / 2.0f;
    draw_layer(atlas->sheet, atlas->backs[color], pos, ox, oy, scale);
}

CardVisual card_visual_default(void) {
    return (CardVisual)
    {
        .
        border_color = CLR_BROWN,
        .
        show_border = true,
        .
        bg_style = BG_PAPER,
        .
        show_bg = true,
        .
        banner_color = CLR_BROWN,
        .
        show_banner = true,
        .
        corner_color = CLR_BROWN,
        .
        show_corner = true,
        .
        container_color = CLR_BROWN,
        .
        container_variant = CONTAINER_1,
        .
        show_container = false,
        .
        description_style = BG_PAPER,
        .
        show_description = true,
        .
        innercorner_style = IC_BROWN,
        .
        show_innercorner = true,
        .
        gem_color = CLR_GREEN,
        .
        show_gem = true,
        .
        socket_color = CLR_BROWN,
        .
        show_socket = true,
        .
        energy_top_color = CLR_RED,
        .
        show_energy_top = false,
        .
        energy_bot_color = CLR_RED,
        .
        show_energy_bot = false,
    };
}

static CardColor parse_color(const char *name) {
    if (!name) return CLR_BROWN;
    for (int i = 0; i < CLR_COUNT; i++) {
        if (strcmp(name, clr_names[i]) == 0) return (CardColor) i;
    }
    return CLR_BROWN;
}

static BGStyle parse_bg(const char *name) {
    if (!name) return BG_PAPER;
    for (int i = 0; i < BG_COUNT; i++) {
        if (strcmp(name, bg_names[i]) == 0) return (BGStyle) i;
    }
    return BG_PAPER;
}

static InnerCornerStyle parse_ic(const char *name) {
    if (!name) return IC_BROWN;
    for (int i = 0; i < IC_COUNT; i++) {
        if (strcmp(name, ic_names[i]) == 0) return (InnerCornerStyle) i;
    }
    return IC_BROWN;
}

CardVisual card_visual_from_json(const char *json_data) {
    CardVisual v = card_visual_default();
    if (!json_data) return v;

    cJSON *root = cJSON_Parse(json_data);

    // TODO: The wrapped-string fallback below is a tech-debt workaround for malformed JSON stored
    // TODO: in the database — if cJSON_Parse fails or the root is not an object, the string is
    // TODO: wrapped in "{...}" and re-parsed. This masks data integrity issues. The correct fix is
    // TODO: to ensure all card data rows in the DB contain valid JSON objects, then remove this fallback.
    if (!root || !cJSON_IsObject(root)) {
        if (root) cJSON_Delete(root);
        size_t len = strlen(json_data);
        char *wrapped = malloc(len + 3);
        if (!wrapped) return v;
        wrapped[0] = '{';
        memcpy(wrapped + 1, json_data, len);
        wrapped[len + 1] = '}';
        wrapped[len + 2] = '\0';
        root = cJSON_Parse(wrapped);
        free(wrapped);
        if (!root) return v;
    }

    cJSON *vis = cJSON_GetObjectItemCaseSensitive(root, "visual");
    if (!vis || !cJSON_IsObject(vis)) {
        cJSON_Delete(root);
        return v;
    }

    cJSON *item;
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "border_color")))
        v.border_color = parse_color(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_border")) && cJSON_IsBool(item))
        v.show_border = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "bg_style")))
        v.bg_style = parse_bg(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_bg")) && cJSON_IsBool(item))
        v.show_bg = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "banner_color")))
        v.banner_color = parse_color(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_banner")) && cJSON_IsBool(item))
        v.show_banner = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "corner_color")))
        v.corner_color = parse_color(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_corner")) && cJSON_IsBool(item))
        v.show_corner = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "container_color")))
        v.container_color = parse_color(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "container_variant")) && cJSON_IsNumber(item)) {
        int cv = item->valueint;
        if (cv >= 1 && cv <= 3) v.container_variant = (ContainerVariant)(cv - 1);
    }
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_container")) && cJSON_IsBool(item))
        v.show_container = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "description_style")))
        v.description_style = parse_bg(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_description")) && cJSON_IsBool(item))
        v.show_description = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "innercorner_style")))
        v.innercorner_style = parse_ic(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_innercorner")) && cJSON_IsBool(item))
        v.show_innercorner = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "gem_color")))
        v.gem_color = parse_color(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_gem")) && cJSON_IsBool(item))
        v.show_gem = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "socket_color")))
        v.socket_color = parse_color(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_socket")) && cJSON_IsBool(item))
        v.show_socket = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "energy_top_color")))
        v.energy_top_color = parse_color(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_energy_top")) && cJSON_IsBool(item))
        v.show_energy_top = cJSON_IsTrue(item);
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "energy_bot_color")))
        v.energy_bot_color = parse_color(cJSON_GetStringValue(item));
    if ((item = cJSON_GetObjectItemCaseSensitive(vis, "show_energy_bot")) && cJSON_IsBool(item))
        v.show_energy_bot = cJSON_IsTrue(item);

    static const char *offset_keys[CARD_LAYER_COUNT] = {
        "off_bg", "off_description", "off_border", "off_banner",
        "off_innercorner", "off_corner", "off_container",
        "off_socket", "off_gem", "off_energy_top", "off_energy_bot"
    };
    for (int i = 0; i < CARD_LAYER_COUNT; i++) {
        cJSON *off = cJSON_GetObjectItemCaseSensitive(vis, offset_keys[i]);
        if (off && cJSON_IsArray(off) && cJSON_GetArraySize(off) == 2) {
            cJSON *ox = cJSON_GetArrayItem(off, 0);
            cJSON *oy = cJSON_GetArrayItem(off, 1);
            if (cJSON_IsNumber(ox)) v.offsets.x[i] = (float) ox->valuedouble;
            if (cJSON_IsNumber(oy)) v.offsets.y[i] = (float) oy->valuedouble;
        }
    }

    cJSON_Delete(root);
    return v;
}

void card_visual_print_json(const CardVisual *visual) {
    printf("{\n\"visual\": {\n");
    printf("  \"border_color\": \"%s\",\n", card_color_name(visual->border_color));
    printf("  \"show_border\": %s,\n", visual->show_border ? "true" : "false");
    printf("  \"bg_style\": \"%s\",\n", bg_style_name(visual->bg_style));
    printf("  \"show_bg\": %s,\n", visual->show_bg ? "true" : "false");
    printf("  \"banner_color\": \"%s\",\n", card_color_name(visual->banner_color));
    printf("  \"show_banner\": %s,\n", visual->show_banner ? "true" : "false");
    printf("  \"corner_color\": \"%s\",\n", card_color_name(visual->corner_color));
    printf("  \"show_corner\": %s,\n", visual->show_corner ? "true" : "false");
    printf("  \"container_color\": \"%s\",\n", card_color_name(visual->container_color));
    printf("  \"container_variant\": %d,\n", visual->container_variant + 1);
    printf("  \"show_container\": %s,\n", visual->show_container ? "true" : "false");
    printf("  \"description_style\": \"%s\",\n", bg_style_name(visual->description_style));
    printf("  \"show_description\": %s,\n", visual->show_description ? "true" : "false");
    printf("  \"innercorner_style\": \"%s\",\n", innercorner_style_name(visual->innercorner_style));
    printf("  \"show_innercorner\": %s,\n", visual->show_innercorner ? "true" : "false");
    printf("  \"gem_color\": \"%s\",\n", card_color_name(visual->gem_color));
    printf("  \"show_gem\": %s,\n", visual->show_gem ? "true" : "false");
    printf("  \"socket_color\": \"%s\",\n", card_color_name(visual->socket_color));
    printf("  \"show_socket\": %s,\n", visual->show_socket ? "true" : "false");
    printf("  \"energy_top_color\": \"%s\",\n", card_color_name(visual->energy_top_color));
    printf("  \"show_energy_top\": %s,\n", visual->show_energy_top ? "true" : "false");
    printf("  \"energy_bot_color\": \"%s\",\n", card_color_name(visual->energy_bot_color));
    static const char *offset_keys[CARD_LAYER_COUNT] = {
        "off_bg", "off_description", "off_border", "off_banner",
        "off_innercorner", "off_corner", "off_container",
        "off_socket", "off_gem", "off_energy_top", "off_energy_bot"
    };
    int off_count = 0;
    for (int i = 0; i < CARD_LAYER_COUNT; i++) {
        if (visual->offsets.x[i] != 0.0f || visual->offsets.y[i] != 0.0f)
            off_count++;
    }
    if (off_count == 0) {
        printf("  \"show_energy_bot\": %s\n", visual->show_energy_bot ? "true" : "false");
    } else {
        printf("  \"show_energy_bot\": %s,\n", visual->show_energy_bot ? "true" : "false");
        int printed = 0;
        for (int i = 0; i < CARD_LAYER_COUNT; i++) {
            if (visual->offsets.x[i] != 0.0f || visual->offsets.y[i] != 0.0f) {
                printed++;
                printf("  \"%s\": [%.0f, %.0f]%s\n", offset_keys[i],
                       visual->offsets.x[i], visual->offsets.y[i],
                       (printed < off_count) ? "," : "");
            }
        }
    }
    printf("}\n}\n");
}

bool card_layer_export_info(const CardAtlas *atlas, const CardVisual *visual,
                            int layerIndex, CardLayerExportInfo *out) {
    if (!atlas || !visual || !out || layerIndex < 0 || layerIndex >= CARD_LAYER_COUNT) return false;

    Rectangle src = EMPTY;
    bool visible = false;
    if (!get_layer_source_and_visibility(atlas, visual, layerIndex, &src, &visible)) return false;

    float ox[CARD_LAYER_COUNT], oy[CARD_LAYER_COUNT];
    get_base_positions(ox, oy);
    ox[layerIndex] += visual->offsets.x[layerIndex];
    oy[layerIndex] += visual->offsets.y[layerIndex];

    *out = (CardLayerExportInfo){
        .name = layer_names[layerIndex],
        .source = src,
        .bounds = (Rectangle){ ox[layerIndex], oy[layerIndex], src.width, src.height },
        .visible = visible,
    };
    return true;
}
