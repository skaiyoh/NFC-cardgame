//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_CARD_RENDERER_H
#define NFC_CARDGAME_CARD_RENDERER_H

#include "../../lib/raylib.h"
#include <stdbool.h>
#include "../core/config.h"

#define CARD_WIDTH   64
#define CARD_HEIGHT  80
#define CARD_BODY_W  46
#define CARD_BODY_H  62
#define CARD_MARGIN  8

typedef enum {
    CLR_AQUA, CLR_BLACK, CLR_BLUE, CLR_BLUE_LIGHT, CLR_BROWN,
    CLR_GRAY, CLR_GREEN, CLR_MAGENTA, CLR_PINK, CLR_PURPLE,
    CLR_RED, CLR_WHITE, CLR_YELLOW,
    CLR_COUNT
} CardColor;

typedef enum {
    BG_BLACK, BG_BROWN, BG_PAPER, BG_WHITE,
    BG_COUNT
} BGStyle;

typedef enum {
    CONTAINER_1, CONTAINER_2, CONTAINER_3,
    CONTAINER_COUNT
} ContainerVariant;

typedef enum {
    IC_BLACK, IC_BROWN, IC_WHITE, IC_YELLOW,
    IC_COUNT
} InnerCornerStyle;

#define CARD_LAYER_COUNT 11

typedef struct {
    float x[CARD_LAYER_COUNT];
    float y[CARD_LAYER_COUNT];
} CardLayerOffsets;

typedef struct {
    CardColor border_color;
    bool show_border;
    BGStyle bg_style;
    bool show_bg;
    CardColor banner_color;
    bool show_banner;
    CardColor corner_color;
    bool show_corner;
    CardColor container_color;
    ContainerVariant container_variant;
    bool show_container;
    BGStyle description_style;
    bool show_description;
    InnerCornerStyle innercorner_style;
    bool show_innercorner;
    CardColor gem_color;
    bool show_gem;
    CardColor socket_color;
    bool show_socket;
    CardColor energy_top_color;
    bool show_energy_top;
    CardColor energy_bot_color;
    bool show_energy_bot;
    CardLayerOffsets offsets;
} CardVisual;

typedef struct {
    Texture2D sheet;
    Rectangle borders[CLR_COUNT];
    Rectangle corners[CLR_COUNT];
    Rectangle backs[CLR_COUNT];
    Rectangle banners[CLR_COUNT];
    Rectangle bgs[BG_COUNT];
    Rectangle descriptions[BG_COUNT];
    Rectangle containers[CLR_COUNT][CONTAINER_COUNT];
    Rectangle inner_corners[IC_COUNT];
    Rectangle gems[CLR_COUNT];
    Rectangle sockets[CLR_COUNT];
    Rectangle energy_top[CLR_COUNT];
    Rectangle energy_bot[CLR_COUNT];
    Rectangle energy_full[CLR_COUNT]; // not yet populated
} CardAtlas;

void card_atlas_init(CardAtlas *atlas);

void card_atlas_free(CardAtlas *atlas);

void card_draw(const CardAtlas *atlas, const CardVisual *visual, Vector2 pos, float scale);

void card_draw_ex(const CardAtlas *atlas, const CardVisual *visual,
                  const CardLayerOffsets *offsets, Vector2 pos, float scale);

CardLayerOffsets card_layer_offsets_default(void);

void card_draw_back(const CardAtlas *atlas, CardColor color, Vector2 pos, float scale);

CardVisual card_visual_default(void);

CardVisual card_visual_from_json(const char *json_data);

void card_visual_print_json(const CardVisual *visual);

const char *card_color_name(CardColor c);

const char *bg_style_name(BGStyle s);

const char *container_variant_name(ContainerVariant v);

const char *innercorner_style_name(InnerCornerStyle s);

#endif //NFC_CARDGAME_CARD_RENDERER_H