//
// Ore node renderer implementation.
//

#include "ore_renderer.h"
#include "../core/config.h"

#define ORE_TEXTURE_PATH "src/assets/environment/Uvulite-objects/Uvulite_Ore_tmp.png"

// Draw scale: ore source is ~32px, grid cell is 64px.
#define ORE_RENDER_SCALE 2.0f

Texture2D ore_renderer_load(void) {
    return LoadTexture(ORE_TEXTURE_PATH);
}

void ore_renderer_draw(const OreField *field, BattleSide side, Texture2D texture) {
    if (texture.id == 0) return;

    for (int i = 0; i < ORE_MATCH_COUNT_PER_SIDE; i++) {
        const OreNode *n = &field->nodes[side][i];
        if (!n->active) continue;

        float drawW = (float)texture.width  * ORE_RENDER_SCALE;
        float drawH = (float)texture.height * ORE_RENDER_SCALE;

        Rectangle src = { 0, 0, (float)texture.width, (float)texture.height };
        Rectangle dst = {
            n->worldPos.v.x - drawW * 0.5f,
            n->worldPos.v.y - drawH * 0.5f,
            drawW,
            drawH
        };
        DrawTexturePro(texture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
    }
}
