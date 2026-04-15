//
// Visual debug overlay implementation.
//

#include "debug_overlay.h"
#include "sprite_renderer.h"
#include "../core/debug_events.h"
#include "../core/battlefield.h"
#include "../core/config.h"
#include "../entities/entity_animation.h"
#include "../logic/combat.h"
#include "../logic/deposit_slots.h"
#include <math.h>

// --- Constants ---
#define BAR_WIDTH   30.0f
#define BAR_HEIGHT   4.0f
#define BAR_GAP      6.0f  // pixels above visible sprite top
#define HIT_FLASH_DURATION 0.15f

#define FLASH_RADIUS_MIN  5.0f
#define FLASH_RADIUS_MAX 20.0f

// --- Attack progress bar ---

static void draw_attack_bar(const Entity *e) {
    if (e->state != ESTATE_ATTACKING || !e->sprite) return;

    // Anchor bar above the visible sprite top
    Rectangle vb = sprite_visible_bounds(e->sprite, &e->anim,
                                         e->position, e->spriteScale,
                                         e->spriteRotationDegrees);
    float barX = e->position.x - BAR_WIDTH * 0.5f;
    float barY = (vb.height > 0.0f)
        ? vb.y - BAR_GAP - BAR_HEIGHT
        : e->position.y - 40.0f;

    // Background
    DrawRectangle((int)barX, (int)barY,
                  (int)BAR_WIDTH, (int)BAR_HEIGHT, DARKGRAY);

    // Fill — green→yellow→orange as progress advances
    float t = e->anim.normalizedTime;
    Color fillColor;
    if (t < 0.5f) {
        fillColor = GREEN;
    } else if (t < 0.8f) {
        fillColor = YELLOW;
    } else {
        fillColor = ORANGE;
    }

    // Flash white when hit marker was just crossed (event-driven via hitFlashTimer)
    if (e->hitFlashTimer > 0.0f) {
        fillColor = WHITE;
    }

    float fillWidth = t * BAR_WIDTH;
    if (fillWidth > 0.0f) {
        DrawRectangle((int)barX, (int)barY,
                      (int)fillWidth, (int)BAR_HEIGHT, fillColor);
    }

    // Hit marker tick — white vertical line at hitNormalized
    const EntityAnimSpec *spec = anim_spec_get(e->spriteType, ANIM_ATTACK);
    if (spec->hitNormalized >= 0.0f && spec->hitNormalized <= 1.0f) {
        float tickX = barX + spec->hitNormalized * BAR_WIDTH;
        DrawLineEx((Vector2){tickX, barY - 1.0f},
                   (Vector2){tickX, barY + BAR_HEIGHT + 1.0f},
                   1.5f, WHITE);
    }
}

// --- Target lock line ---

static void draw_target_line(const Entity *e, const Battlefield *bf,
                             const GameState *gs) {
    if (e->state != ESTATE_ATTACKING || e->attackTargetId < 0) return;

    Entity *target = bf_find_entity((Battlefield *)bf, e->attackTargetId);

    if (!target) {
        // Stale ID — red circle at attacker
        DrawCircleLinesV(e->position, 8.0f, RED);
        return;
    }

    Color lineColor;
    if (!target->alive) {
        lineColor = RED;
    } else if (!combat_in_range(e, target, gs)) {
        lineColor = YELLOW;
    } else {
        lineColor = GREEN;
    }

    DrawLineEx(e->position, target->position, 1.5f, lineColor);
}

// --- Attack range circles (moved from game.c) ---

static void draw_range_circles(const Battlefield *bf) {
    for (int i = 0; i < bf->entityCount; i++) {
        const Entity *e = bf->entities[i];
        if (!e || !e->alive) continue;

        Color c;
        switch (e->state) {
            case ESTATE_ATTACKING: c = RED;   break;
            case ESTATE_WALKING:   c = GREEN; break;
            case ESTATE_IDLE:      c = GRAY;  break;
            default:               continue;
        }

        DrawCircleLines((int)e->position.x, (int)e->position.y,
                        e->attackRange, c);
    }
}

// --- Event flashes ---

static void draw_event_flashes(void) {
    const DebugFlash *buf = debug_events_buffer();

    for (int i = 0; i < DEBUG_EVENT_CAPACITY; i++) {
        const DebugFlash *f = &buf[i];
        if (f->remaining <= 0.0f || f->duration <= 0.0f) continue;

        float progress = 1.0f - (f->remaining / f->duration);  // 0→1
        float radius = FLASH_RADIUS_MIN +
                       (FLASH_RADIUS_MAX - FLASH_RADIUS_MIN) * progress;
        float alpha = 1.0f - progress;  // fade out
        Vector2 pos = { f->posX, f->posY };

        Color baseColor;
        switch (f->type) {
            case DEBUG_EVT_STATE_CHANGE: baseColor = SKYBLUE; break;
            case DEBUG_EVT_HIT:          baseColor = RED;     break;
            case DEBUG_EVT_DEATH_FINISH: baseColor = PURPLE;  break;
            default:                     baseColor = WHITE;    break;
        }

        Color c = Fade(baseColor, alpha * 0.8f);
        DrawCircleLinesV(pos, radius, c);

        // Hit events also get a small filled circle
        if (f->type == DEBUG_EVT_HIT && alpha > 0.3f) {
            DrawCircleV(pos, 3.0f, Fade(RED, alpha * 0.6f));
        }
    }
}

// --- Sustenance placement diagnostics overlay (F7) ---

static Color sustenance_reason_color(SustenanceCellReason reason) {
    switch (reason) {
        case SUSTENANCE_CELL_VALID:          return Fade(GREEN,    0.15f);
        case SUSTENANCE_CELL_EDGE_BLOCKED:   return Fade(DARKGRAY, 0.15f);
        case SUSTENANCE_CELL_OUT_OF_PLAY:    return Fade(PURPLE,   0.18f);
        case SUSTENANCE_CELL_LANE_BLOCKED:   return Fade(RED,      0.20f);
        case SUSTENANCE_CELL_BASE_BLOCKED:   return Fade(ORANGE,   0.20f);
        case SUSTENANCE_CELL_SPAWN_BLOCKED:  return Fade(YELLOW,   0.20f);
        case SUSTENANCE_CELL_NODE_BLOCKED:   return Fade(SKYBLUE,  0.20f);
        default:                      return Fade(WHITE,    0.10f);
    }
}

static void draw_sustenance_placement(const Battlefield *bf) {
    float halfCell = SUSTENANCE_GRID_CELL_SIZE_PX * 0.5f;

    // --- Cell grid for both sides ---
    for (int s = 0; s < 2; s++) {
        BattleSide side = (BattleSide)s;
        for (int r = 0; r < SUSTENANCE_GRID_ROWS; r++) {
            for (int c = 0; c < SUSTENANCE_GRID_COLS; c++) {
                SustenanceCellDebugInfo info = sustenance_debug_classify_cell(bf, side, r, c);
                Rectangle cell = {
                    info.centerX - halfCell,
                    info.centerY - halfCell,
                    SUSTENANCE_GRID_CELL_SIZE_PX,
                    SUSTENANCE_GRID_CELL_SIZE_PX
                };

                // Fill by classification reason
                DrawRectangleRec(cell, sustenance_reason_color(info.reason));
                // Faint cell outline
                DrawRectangleLinesEx(cell, 1.0f, Fade(LIGHTGRAY, 0.3f));
            }
        }
    }

    // --- Shared clearance shapes ---

    // Lane corridor lines (all 6 lanes), drawn once so both territories have
    // consistent visual intensity.
    for (int ls = 0; ls < 2; ls++) {
        for (int lane = 0; lane < 3; lane++) {
            for (int wp = 0; wp < LANE_WAYPOINT_COUNT - 1; wp++) {
                CanonicalPos wpA = bf_waypoint(bf, (BattleSide)ls, lane, wp);
                CanonicalPos wpB = bf_waypoint(bf, (BattleSide)ls, lane, wp + 1);
                float thickness = SUSTENANCE_LANE_CLEARANCE_CELLS * SUSTENANCE_GRID_CELL_SIZE_PX * 2.0f;
                DrawLineEx(wpA.v, wpB.v, thickness, Fade(RED, 0.10f));
            }
        }
    }

    // Side-specific base and spawn clearance guides.
    for (int s = 0; s < 2; s++) {
        BattleSide side = (BattleSide)s;
        // Base anchor clearance circle
        CanonicalPos baseAnchor = bf_base_anchor(bf, side);
        float baseRadius = SUSTENANCE_BASE_CLEARANCE_CELLS * SUSTENANCE_GRID_CELL_SIZE_PX;
        DrawCircleLinesV(baseAnchor.v, baseRadius, Fade(ORANGE, 0.4f));

        // Spawn anchor clearance circles
        for (int slot = 0; slot < NUM_CARD_SLOTS; slot++) {
            CanonicalPos spawnAnchor = bf_spawn_pos(bf, side, slot);
            float spawnRadius = SUSTENANCE_SPAWN_CLEARANCE_CELLS * SUSTENANCE_GRID_CELL_SIZE_PX;
            DrawCircleLinesV(spawnAnchor.v, spawnRadius, Fade(YELLOW, 0.4f));
        }
    }
}

// --- Sustenance node overlay (F6) ---

static void draw_sustenance_nodes(const Battlefield *bf) {
    const SustenanceField *field = &bf->sustenanceField;
    float halfCell = SUSTENANCE_GRID_CELL_SIZE_PX * 0.5f;

    for (int s = 0; s < 2; s++) {
        for (int i = 0; i < SUSTENANCE_MATCH_COUNT_PER_SIDE; i++) {
            const SustenanceNode *n = &field->nodes[s][i];
            if (!n->active) continue;

            Rectangle cell = {
                n->worldPos.v.x - halfCell,
                n->worldPos.v.y - halfCell,
                SUSTENANCE_GRID_CELL_SIZE_PX,
                SUSTENANCE_GRID_CELL_SIZE_PX
            };

            // Determine state color
            Color outlineColor;
            Color markerColor;
            bool hasClaim = (n->claimedByEntityId >= 0);
            bool claimantAlive = false;
            Entity *claimant = NULL;

            if (hasClaim) {
                claimant = bf_find_entity((Battlefield *)bf, n->claimedByEntityId);
                claimantAlive = (claimant != NULL && claimant->alive);
                if (claimantAlive) {
                    outlineColor = ORANGE;
                    markerColor = YELLOW;
                } else {
                    outlineColor = RED;
                    markerColor = RED;
                }
            } else {
                outlineColor = GREEN;
                markerColor = (Color){0, 220, 220, 200};
            }

            // Cell outline
            DrawRectangleLinesEx(cell, 1.5f, Fade(outlineColor, 0.7f));

            // Center marker
            DrawCircleV(n->worldPos.v, 4.0f, Fade(markerColor, 0.8f));

            // Claimed state ring
            if (hasClaim) {
                DrawCircleLinesV(n->worldPos.v, 7.0f, Fade(outlineColor, 0.8f));
            }

            // Claim line to owning entity
            if (claimantAlive) {
                DrawLineEx(n->worldPos.v, claimant->position, 1.0f,
                           Fade(YELLOW, 0.4f));
            }
        }
    }
}

// --- Deposit slot overlay (F8) ---

static void draw_deposit_slot_ring(const Entity *base) {
    if (!base || !base->depositSlots.initialized) return;

    // Draw the base nav radius shell so farmers' stop distance is visible.
    float navR = (base->navRadius > 0.0f) ? base->navRadius : base->bodyRadius;
    DrawCircleLinesV(base->position, navR, Fade(ORANGE, 0.5f));

    int primaryCount = deposit_slots_primary_count(base);
    for (int i = 0; i < primaryCount; i++) {
        const DepositSlot *s = deposit_slots_primary_at(base, i);
        if (!s) continue;
        bool claimed = (s->claimedByEntityId >= 0);
        Color outline = claimed ? RED : GREEN;
        DrawCircleLinesV(s->worldPos, 10.0f, Fade(outline, 0.85f));
        if (claimed) {
            DrawCircleV(s->worldPos, 3.0f, Fade(RED, 0.6f));
        }
    }

    int queueCount = deposit_slots_queue_count(base);
    for (int i = 0; i < queueCount; i++) {
        const DepositSlot *s = deposit_slots_queue_at(base, i);
        if (!s) continue;
        bool claimed = (s->claimedByEntityId >= 0);
        Color outline = claimed ? ORANGE : YELLOW;
        DrawCircleLinesV(s->worldPos, 8.0f, Fade(outline, 0.7f));
        if (claimed) {
            DrawCircleV(s->worldPos, 2.0f, Fade(ORANGE, 0.6f));
        }
    }
}

static void draw_deposit_slots(const Battlefield *bf) {
    for (int i = 0; i < bf->entityCount; i++) {
        const Entity *e = bf->entities[i];
        if (!e || e->markedForRemoval) continue;
        if (e->type != ENTITY_BUILDING) continue;
        if (!e->depositSlots.initialized) continue;
        draw_deposit_slot_ring(e);
    }
}

static float debug_nav_radius(const Entity *e) {
    if (!e) return 0.0f;
    return (e->navRadius > 0.0f) ? e->navRadius : e->bodyRadius;
}

static float debug_soft_shell_radius(const Entity *e) {
    float hardRadius = debug_nav_radius(e);
    float ratio = PATHFIND_ALLY_SOFT_OVERLAP_RATIO;
    float maxAllowance = PATHFIND_ALLY_SOFT_OVERLAP_MAX;

    if (e && e->navProfile == NAV_PROFILE_ASSAULT) {
        ratio = PATHFIND_ASSAULT_ALLY_SOFT_OVERLAP_RATIO;
        maxAllowance = PATHFIND_ASSAULT_ALLY_SOFT_OVERLAP_MAX;
        if (e->movementTargetId != -1) {
            maxAllowance = PATHFIND_ASSAULT_SAME_TARGET_SOFT_OVERLAP_MAX;
        }
    }

    float allowance = (hardRadius + hardRadius) * ratio;
    if (allowance > maxAllowance) {
        allowance = maxAllowance;
    }
    if (e && e->navProfile == NAV_PROFILE_ASSAULT && e->movementTargetId != -1) {
        allowance += PATHFIND_ASSAULT_SAME_TARGET_SOFT_OVERLAP_BONUS;
        if (allowance > PATHFIND_ASSAULT_SAME_TARGET_SOFT_OVERLAP_MAX) {
            allowance = PATHFIND_ASSAULT_SAME_TARGET_SOFT_OVERLAP_MAX;
        }
    }
    float softRadius = hardRadius - allowance * 0.5f;
    return (softRadius > 0.0f) ? softRadius : 0.0f;
}

static void draw_crowd_shells(const Battlefield *bf) {
    for (int i = 0; i < bf->entityCount; i++) {
        const Entity *e = bf->entities[i];
        if (!e || e->markedForRemoval || !e->alive) continue;
        if (e->type != ENTITY_TROOP) continue;

        float hardRadius = debug_nav_radius(e);
        float softRadius = debug_soft_shell_radius(e);
        DrawCircleLinesV(e->position, hardRadius, Fade(RED, 0.55f));
        if (softRadius > 0.0f && softRadius < hardRadius - 0.01f) {
            DrawCircleLinesV(e->position, softRadius, Fade(SKYBLUE, 0.7f));
        }
    }
}

// --- Public API ---

void debug_overlay_draw(const Battlefield *bf, const GameState *gs,
                        DebugOverlayFlags flags) {
    for (int i = 0; i < bf->entityCount; i++) {
        const Entity *e = bf->entities[i];
        if (!e || e->markedForRemoval) continue;

        if (flags.attackBars)   draw_attack_bar(e);
        if (flags.targetLines)  draw_target_line(e, bf, gs);
    }

    if (flags.sustenancePlacement)  draw_sustenance_placement(bf);
    if (flags.sustenanceNodes)      draw_sustenance_nodes(bf);
    if (flags.depositSlots)         draw_deposit_slots(bf);
    if (flags.crowdShells)          draw_crowd_shells(bf);
    if (flags.eventFlashes)  draw_event_flashes();
    if (flags.rangeCirlces)  draw_range_circles(bf);
}
