//
// Sustenance node renderer implementation.
//
// TODO: Single sustenance texture for all types. Add type-based texture selection
// when sustenance variety is implemented (keyed on SustenanceNode.sustenanceType).

#include "sustenance_renderer.h"
#include "../core/config.h"
#include <string.h>

#define SUSTENANCE_TEXTURE_PATH "src/assets/environment/Objects/rotten_roast_sheet.png"
#define SUSTENANCE_FRAME_COUNT 9
#define SUSTENANCE_IDLE_FRAME 0
#define SUSTENANCE_ANIM_START_FRAME 1
#define SUSTENANCE_ANIM_END_FRAME 8
#define SUSTENANCE_ANIM_FRAME_SECONDS 0.1
#define SUSTENANCE_IDLE_MIN_SECONDS 6.0
#define SUSTENANCE_IDLE_MAX_SECONDS 12.0
#define SUSTENANCE_BURST_FRAME_COUNT (SUSTENANCE_ANIM_END_FRAME - SUSTENANCE_ANIM_START_FRAME + 1)
#define SUSTENANCE_BURST_DURATION_SECONDS (SUSTENANCE_BURST_FRAME_COUNT * SUSTENANCE_ANIM_FRAME_SECONDS)

// Draw scale: sustenance source is ~32px, grid cell is 64px.
#define SUSTENANCE_RENDER_SCALE 2.0f

typedef struct {
    bool initialized;
    int lastGridRow;
    int lastGridCol;
    double activationTime;
} SustenanceAnimState;

static SustenanceAnimState s_sustenanceAnimStates[SUSTENANCE_MATCH_COUNT_PER_SIDE * 2];

Texture2D sustenance_renderer_load(void) {
    memset(s_sustenanceAnimStates, 0, sizeof(s_sustenanceAnimStates));
    return LoadTexture(SUSTENANCE_TEXTURE_PATH);
}

static unsigned int sustenance_anim_mix(unsigned int value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

static double sustenance_anim_idle_delay_seconds(const SustenanceNode *node, unsigned int cycleIndex) {
    unsigned int seed = (unsigned int) (node->id + 1) * 0x9e3779b9u;
    seed ^= (unsigned int) (node->gridRow + 1) * 0x85ebca6bu;
    seed ^= (unsigned int) (node->gridCol + 1) * 0xc2b2ae35u;
    seed ^= (cycleIndex + 1u) * 0x27d4eb2du;

    double unit = (double) sustenance_anim_mix(seed) / (double) UINT32_MAX;
    return SUSTENANCE_IDLE_MIN_SECONDS +
           unit * (SUSTENANCE_IDLE_MAX_SECONDS - SUSTENANCE_IDLE_MIN_SECONDS);
}

static double sustenance_anim_initial_phase_seconds(const SustenanceNode *node) {
    double firstCycleDuration = sustenance_anim_idle_delay_seconds(node, 0) +
                                SUSTENANCE_BURST_DURATION_SECONDS;
    unsigned int seed = (unsigned int) (node->id + 1) * 0x165667b1u;
    seed ^= (unsigned int) (node->gridRow + 1) * 0xd3a2646cu;
    seed ^= (unsigned int) (node->gridCol + 1) * 0xfd7046c5u;

    return ((double) sustenance_anim_mix(seed) / (double) UINT32_MAX) * firstCycleDuration;
}

static SustenanceAnimState *sustenance_anim_state_for_node(const SustenanceNode *node) {
    if (node->id < 0 || node->id >= SUSTENANCE_MATCH_COUNT_PER_SIDE * 2) return NULL;
    return &s_sustenanceAnimStates[node->id];
}

static void sustenance_anim_sync_state(const SustenanceNode *node, double now) {
    SustenanceAnimState *state = sustenance_anim_state_for_node(node);
    if (!state) return;

    if (!state->initialized ||
        state->lastGridRow != node->gridRow ||
        state->lastGridCol != node->gridCol) {
        bool isInitialActivation = !state->initialized;

        state->initialized = true;
        state->lastGridRow = node->gridRow;
        state->lastGridCol = node->gridCol;
        state->activationTime = isInitialActivation
                                ? now - sustenance_anim_initial_phase_seconds(node)
                                : now;
    }
}

static int sustenance_anim_frame_for_node(const SustenanceNode *node, double now) {
    SustenanceAnimState *state = sustenance_anim_state_for_node(node);
    if (!state || !state->initialized) return SUSTENANCE_IDLE_FRAME;

    double elapsed = now - state->activationTime;
    if (elapsed <= 0.0) return SUSTENANCE_IDLE_FRAME;

    double cycleStart = 0.0;
    unsigned int cycleIndex = 0;

    while (true) {
        double idleDelay = sustenance_anim_idle_delay_seconds(node, cycleIndex);
        if (elapsed < cycleStart + idleDelay) return SUSTENANCE_IDLE_FRAME;

        double burstElapsed = elapsed - (cycleStart + idleDelay);
        if (burstElapsed < SUSTENANCE_BURST_DURATION_SECONDS) {
            int burstFrame = (int) (burstElapsed / SUSTENANCE_ANIM_FRAME_SECONDS);
            if (burstFrame < 0) burstFrame = 0;
            if (burstFrame >= SUSTENANCE_BURST_FRAME_COUNT) burstFrame = SUSTENANCE_BURST_FRAME_COUNT - 1;
            return SUSTENANCE_ANIM_START_FRAME + burstFrame;
        }

        cycleStart += idleDelay + SUSTENANCE_BURST_DURATION_SECONDS;
        cycleIndex++;
    }
}

void sustenance_renderer_draw(const SustenanceField *field, BattleSide side, Texture2D texture,
                       float rotationDegrees) {
    if (texture.id == 0) return;

    float frameWidth = (float) texture.width / (float) SUSTENANCE_FRAME_COUNT;
    float frameHeight = (float) texture.height;

    for (int i = 0; i < SUSTENANCE_MATCH_COUNT_PER_SIDE; i++) {
        const SustenanceNode *n = &field->nodes[side][i];
        if (!n->active) continue;

        double now = GetTime();
        sustenance_anim_sync_state(n, now);
        int frame = sustenance_anim_frame_for_node(n, now);
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
