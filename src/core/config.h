//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_CONFIG_H
#define NFC_CARDGAME_CONFIG_H

// Screen
#define SCREEN_WIDTH  1920
#define SCREEN_HEIGHT 1080

// Paths
#define CARD_SHEET_PATH     "src/assets/cards/ModularCardsRPG/modularCardsRPGSheet.png"
#define GRASS_TILESET_PATH  "src/assets/environment/Pixel Art Top Down - Basic v1.2.3/Texture/TX Tileset Grass.png"
#define UNDEAD_TILESET_PATH "src/assets/environment/Undead-Tileset-Top-Down-Pixel-Art/Tiled_files/Ground_rocks.png"
#define UNDEAD_DETAIL_PATH  "src/assets/environment/Undead-Tileset-Top-Down-Pixel-Art/Tiled_files/details.png"
#define FX_SMOKE_PATH       "src/assets/fx/smoke.png"
#define STATUS_BARS_PATH    "src/assets/environment/Objects/health_energy_bars.png"

// Character sprites
#define CHAR_BASE_PATH      "src/assets/characters/Base/"
#define CHAR_KNIGHT_PATH    "src/assets/characters/Knight/"
#define CHAR_HEALER_PATH    "src/assets/characters/Healer/"
#define CHAR_ASSASSIN_PATH  "src/assets/characters/Assassin/"
#define CHAR_BRUTE_PATH     "src/assets/characters/Brute/"
#define CHAR_FARMER_PATH     "src/assets/characters/Farmer/"
#define SPRITE_FRAME_SIZE 79

// Gameplay tuning
#define DEFAULT_TILE_SCALE 2.0f
#define DEFAULT_TILE_SIZE  32.0f
#define DEFAULT_CARD_SCALE 2.5f

// Base geometry
#define BASE_SPAWN_GAP 32.0f
#define BASE_NAV_RADIUS 56.0f   // authored pathfinding footprint, independent of sprite size
#define DEFAULT_MELEE_BODY_RADIUS         14.0f

// Base deposit slot ring (farmers reserve slots instead of pathing to base center).
// 4 primary slots on a 160-deg arc at the canonical ring radius gives
// ~66 px linear spacing between adjacent slots (2 * 74 * sin(26.7 deg))
// with ~36 px clearance between adjacent farmer shells -- enough elbow
// room in the approach corridor without sprawling the ring visually.
// Earlier iterations used 6 primary slots which had only ~11 px clearance
// and deadlocked farmers in the approach phase.
#define BASE_DEPOSIT_PRIMARY_SLOT_COUNT  4
#define BASE_DEPOSIT_QUEUE_SLOT_COUNT    6
#define BASE_DEPOSIT_PRIMARY_ARC_DEGREES 160.0f
#define BASE_DEPOSIT_QUEUE_ARC_DEGREES   170.0f
// Primary ring radius = BASE_NAV_RADIUS + FARMER_DEFAULT_BODY_RADIUS + SLOT_GAP.
#define BASE_DEPOSIT_SLOT_GAP            4.0f
// Queue ring sits this far outside the primary ring.
#define BASE_DEPOSIT_QUEUE_RADIAL_OFFSET 40.0f
#define FARMER_DEFAULT_BODY_RADIUS       14.0f  // used by deposit slot geometry; must match troop.c default
#define FARMER_DEPOSIT_ARRIVAL_RADIUS    10.0f  // per-primary-slot arrival tolerance
#define FARMER_QUEUE_WAIT_PROXIMITY      14.0f  // per-queue-slot arrival tolerance

// Base assault slot ring (combat troops reserve front-arc contact points
// instead of collapsing onto base center).
#define BASE_ASSAULT_PRIMARY_SLOT_COUNT  8
#define BASE_ASSAULT_QUEUE_SLOT_COUNT    8
#define BASE_ASSAULT_PRIMARY_ARC_DEGREES 150.0f
#define BASE_ASSAULT_QUEUE_ARC_DEGREES   170.0f
#define BASE_ASSAULT_SLOT_GAP            2.0f
#define BASE_ASSAULT_QUEUE_RADIAL_OFFSET 22.0f
// Explicit melee contact geometry for static building targets. This is
// intentionally separate from BASE_NAV_RADIUS so pathfinding footprint and
// visual combat contact can be tuned independently.
#define COMBAT_BUILDING_MELEE_INSET      30.0f
#define COMBAT_MELEE_GOAL_SLACK_MAX      8.0f
#define COMBAT_PERIMETER_TANGENT_SCALE   0.65f
// Small deterministic tangent bias used only for the near-base soft fan.
// Strong enough to break perfect center-stacking, but much tighter than the
// old perimeter-slot behavior.
#define COMBAT_STATIC_TARGET_FLOW_TANGENT_SCALE 0.70f
#define COMBAT_STATIC_TARGET_FLOW_ANGLE_MIN_DEG 10.0f
#define COMBAT_STATIC_TARGET_FLOW_ANGLE_MAX_DEG 20.0f

// Lane pathfinding
#define LANE_WAYPOINT_COUNT  8
#define LANE_BOW_INTENSITY   0.3f
#define LANE_OUTER_INSET_RATIO 0.25f
#define LANE_BASE_APPROACH_START 0.72f
#define LANE_BASE_APPROACH_GAP 16.0f
#define LANE_JITTER_RADIUS   10.0f
#define PI_F 3.14159265f

// Local steering / anti-stacking
#define PATHFIND_AGGRO_RADIUS             192.0f
#define PATHFIND_AGGRO_HYSTERESIS         32.0f
#define PATHFIND_CANDIDATE_ANGLE_SOFT_DEG 30.0f
#define PATHFIND_CANDIDATE_ANGLE_HARD_DEG 60.0f
#define PATHFIND_CANDIDATE_ANGLE_SIDE_DEG 90.0f
#define PATHFIND_CANDIDATE_ANGLE_ESCAPE_DEG 120.0f
#define PATHFIND_CONTACT_GAP              2.0f
#define PATHFIND_WAYPOINT_REACH_GAP       4.0f
#define PATHFIND_JAM_RELIEF_TICKS         6
#define PATHFIND_ESCAPE_BACKTRACK_STEP_RATIO 0.5f
#define PATHFIND_LANE_DRIFT_MAX_RATIO     0.65f
#define PATHFIND_LANE_LOOKAHEAD_DISTANCE  48.0f
#define PATHFIND_PURSUIT_REAR_TOLERANCE   32.0f
#define PATHFIND_ASSAULT_JAM_RELIEF_TICKS 3
#define PATHFIND_ASSAULT_LATERAL_TOLERANCE_RATIO 1.0f
#define PATHFIND_ALLY_SOFT_OVERLAP_RATIO  0.12f
#define PATHFIND_ALLY_SOFT_OVERLAP_MAX    6.0f
#define PATHFIND_ASSAULT_ALLY_SOFT_OVERLAP_RATIO 0.20f
#define PATHFIND_ASSAULT_ALLY_SOFT_OVERLAP_MAX   8.0f
#define PATHFIND_ASSAULT_SAME_TARGET_SOFT_OVERLAP_BONUS 2.0f
#define PATHFIND_ASSAULT_SAME_TARGET_SOFT_OVERLAP_MAX   10.0f
// Inside the base assault cloud, overlap remains legal but is not fully free.
// A small residual penalty encourages subtle local flow around the blob instead
// of perfectly center-stacking from a single approach lane.
#define PATHFIND_ASSAULT_CLOUD_SOFT_OVERLAP_SCALE 1.0f
// Additional score for candidates that match a unit's deterministic near-base
// flow direction. This only applies inside the static-target assault cloud.
#define PATHFIND_ASSAULT_CLOUD_FLOW_WEIGHT 18.0f
/// Free-mover lateral slack: fraction of one move-step by which the goal
// distance may grow when picking a tangential candidate. At 1.5 a pure
// perpendicular sidestep is always legal near arrival (goal-distance change
// is bounded by step for small step/d ratios), letting farmers wiggle past
// each other instead of stalling in packed approach corridors. Bumped from
// 0.35 after the first playtest showed residual clustering near the ring.
#define PATHFIND_FREE_MOVER_LATERAL_TOLERANCE_RATIO 1.5f

// Entity / slot limits (shared by types.h and battlefield.h)
#define NUM_CARD_SLOTS 3
#define MAX_ENTITIES   64

// Canonical board dimensions (per D-01)
#define BOARD_WIDTH   1080
#define BOARD_HEIGHT  1920
#define SEAM_Y        960

// Sustenance resource nodes
#define SUSTENANCE_GRID_CELL_SIZE_PX        64.0f
#define SUSTENANCE_GRID_COLS                16
#define SUSTENANCE_GRID_ROWS                15
#define SUSTENANCE_EDGE_MARGIN_CELLS        1
#define SUSTENANCE_LANE_CLEARANCE_CELLS     1.0f
#define SUSTENANCE_BASE_CLEARANCE_CELLS     2.0f
#define SUSTENANCE_SPAWN_CLEARANCE_CELLS    1.5f
#define SUSTENANCE_NODE_CLEARANCE_CELLS     1.5f
#define SUSTENANCE_MATCH_COUNT_PER_SIDE     8

// Farmer tuning
#define FARMER_SUSTENANCE_INTERACT_RADIUS   40.0f
#define FARMER_DEFAULT_SUSTENANCE_VALUE     1
#define FARMER_DEFAULT_SUSTENANCE_DURABILITY 1

// Hand UI (outer-edge card strip per player)
#define HAND_UI_DEPTH_PX               180
#define HAND_MAX_CARDS                 8
#define HAND_CARD_WIDTH                128
#define HAND_CARD_HEIGHT               160
#define HAND_CARD_GAP                  4
#define HAND_CARD_SHEET_PATH           "src/assets/cards/card_sheet.png"
#define HAND_CARD_SHEET_ROWS           8
#define HAND_CARD_FRAME_COUNT          6
#define HAND_CARD_FRAME_TIME           0.05f
#define HAND_CARD_PLAY_LIFT_PEAK_SCALE 1.06f

#endif //NFC_CARDGAME_CONFIG_H
