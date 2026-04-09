//
// Sustenance node renderer implementation.
//
// TODO: Single sustenance texture for all types. Add type-based texture selection
// when sustenance variety is implemented (keyed on SustenanceNode.sustenanceType).

#include "sustenance_renderer.h"
#include "../core/config.h"

#define SUSTENANCE_TEXTURE_PATH "src/assets/environment/Objects/uvulite_blob.png"
#define SUSTENANCE_FRAME_COUNT 2
#define SUSTENANCE_ANIM_FPS 2.0

// Draw scale: sustenance source is ~32px, grid cell is 64px.
#define SUSTENANCE_RENDER_SCALE 2.0f

Texture2D sustenance_renderer_load(void) {
    return LoadTexture(SUSTENANCE_TEXTURE_PATH);
}

static float sustenance_anim_phase_seconds(const SustenanceNode *node) {
    unsigned int hash = (unsigned int) (node->id + 1) * 2654435761u;
    float cycleDuration = (float) SUSTENANCE_FRAME_COUNT / (float) SUSTENANCE_ANIM_FPS;
    return ((float) (hash & 0xFFFFu) / 65535.0f) * cycleDuration;
}

void sustenance_renderer_draw(const SustenanceField *field, BattleSide side, Texture2D texture,
                       float rotationDegrees) {
    if (texture.id == 0) return;

    float frameWidth = (float) texture.width / (float) SUSTENANCE_FRAME_COUNT;
    float frameHeight = (float) texture.height;

    for (int i = 0; i < SUSTENANCE_MATCH_COUNT_PER_SIDE; i++) {
        const SustenanceNode *n = &field->nodes[side][i];
        if (!n->active) continue;

        float animTime = (float) GetTime() + sustenance_anim_phase_seconds(n);
        int frame = ((int) (animTime * (float) SUSTENANCE_ANIM_FPS)) % SUSTENANCE_FRAME_COUNT;
        float drawW = frameWidth * SUSTENANCE_RENDER_SCALE;
        float drawH = frameHeight * SUSTENANCE_RENDER_SCALE;

        Rectangle src = {
            frameWidth * (float) frame,
            0.0f,
            frameWidth,
            frameHeight
        };
        Rectangle dst = {
            n->worldPos.v.x,
            n->worldPos.v.y,
            drawW,
            drawH
        };
        DrawTexturePro(texture, src, dst, (Vector2){drawW * 0.5f, drawH * 0.5f},
                       rotationDegrees, WHITE);
    }
}
