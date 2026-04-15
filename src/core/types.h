//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_TYPES_H

#include <raylib.h>
#include "config.h"
#include "../data/db.h"
#include "../data/cards.h"
#include "../rendering/card_renderer.h"
#include "../rendering/tilemap_renderer.h"
#include "../rendering/sprite_renderer.h"
#include "../rendering/spawn_fx.h"
#include "../rendering/biome.h"
#include "../hardware/nfc_reader.h"
#include "../logic/nav_frame.h"
#include "battlefield.h"

// Forward declarations
typedef struct Entity Entity;
typedef struct Player Player;
typedef struct GameState GameState;

// Entity enums
typedef enum { ENTITY_TROOP, ENTITY_BUILDING, ENTITY_PROJECTILE } EntityType;

typedef enum { FACTION_PLAYER1, FACTION_PLAYER2 } Faction;

typedef enum { ESTATE_IDLE, ESTATE_WALKING, ESTATE_ATTACKING, ESTATE_DEAD } EntityState;

// Targeting preference for combat (used by Entity and TroopData)
typedef enum {
    TARGET_NEAREST,
    TARGET_BUILDING,
    TARGET_SPECIFIC_TYPE
} TargetingMode;

// Unit role -- distinguishes combat troops from economic units
typedef enum { UNIT_ROLE_COMBAT, UNIT_ROLE_FARMER } UnitRole;

// Farmer behavior phases
typedef enum {
    FARMER_SEEKING,
    FARMER_WALKING_TO_SUSTENANCE,
    FARMER_GATHERING,
    FARMER_RETURNING,
    FARMER_DEPOSITING
} FarmerState;

// Movement profile. In the Phase 3 flow-field mover, LANE vs ASSAULT vs
// FREE_GOAL chooses which flow field the entity consults: lane-march
// (LANE), target-pursuit (ASSAULT, set on aggro acquisition), or
// free-goal (FREE_GOAL, farmers and helpers). STATIC entities are never
// stepped. The ASSAULT profile is also still read by the old candidate
// fan in pathfind_try_step_toward (NULL-nav fallback) for its soft
// overlap shaping -- retiring it entirely is outstanding work. LANE = 0
// so memset defaults produce a safe lane-marcher.
typedef enum {
    NAV_PROFILE_LANE = 0,
    NAV_PROFILE_ASSAULT,
    NAV_PROFILE_FREE_GOAL,
    NAV_PROFILE_STATIC
} UnitNavProfile;

// Kind of deposit slot a farmer is currently holding a reservation on.
// NONE = 0 so memset defaults indicate "no reservation" without code help.
typedef enum {
    DEPOSIT_SLOT_NONE = 0,
    DEPOSIT_SLOT_PRIMARY,
    DEPOSIT_SLOT_QUEUE
} DepositSlotKind;

// A single reservable contact point around a base, computed once at base
// creation. Farmers walk to this world position instead of base->position
// so they do not all collapse onto a single unreachable cell.
typedef struct {
    Vector2 worldPos;
    int     claimedByEntityId; // -1 when unclaimed
} DepositSlot;

// Ring of deposit slots owned by a single base entity. `initialized` gates
// safe API reads in case a non-base entity's zeroed memory is queried.
typedef struct {
    DepositSlot primary[BASE_DEPOSIT_PRIMARY_SLOT_COUNT];
    DepositSlot queue  [BASE_DEPOSIT_QUEUE_SLOT_COUNT];
    bool initialized;
} DepositSlotRing;

// Entity definition
struct Entity {
    int id;
    EntityType type;
    Faction faction;
    EntityState state;

    // Transform
    Vector2 position;
    float moveSpeed;

    // Stats
    int hp, maxHP;
    int attack;
    float attackSpeed;
    float attackRange;
    float attackCooldown;       // time remaining before next attack
    int attackTargetId;         // locked target for current swing, -1 if none
    TargetingMode targeting;    // targeting preference
    const char *targetType;     // for TARGET_SPECIFIC_TYPE (owned, freed in entity_destroy)

    // Animation
    AnimState anim;
    const CharacterSprite *sprite;
    SpriteType spriteType;
    float spriteScale;
    float spriteRotationDegrees;
    BattleSide presentationSide;

    // Ownership
    int ownerID; // Player index (0 or 1)
    int lane; // Which lane (0-2)
    int waypointIndex; // Derived: first authored waypoint still ahead along the lane path
    float laneProgress; // Monotonic distance traveled along the authored lane polyline

    // Debug
    float hitFlashTimer;        // countdown for hit-marker visual flash (debug overlay)

    // Unit role (farmer vs combat)
    UnitRole unitRole;
    FarmerState farmerState;
    int claimedSustenanceNodeId;       // sustenance node ID this farmer is targeting, -1 if none
    int carriedSustenanceValue;        // sustenance value being carried back to base
    float workTimer;            // elapsed time in current work cycle (gathering/depositing)

    // Flags
    bool alive;
    bool markedForRemoval;

    // Support stats
    int healAmount;             // > 0 marks this unit as a supporter; HP restored per hit on a friendly troop

    // Local steering
    float bodyRadius;           // collision/footprint radius in canonical world units
    float navRadius;            // pathfinding footprint; 0 => fall back to bodyRadius
    UnitNavProfile navProfile;  // steering algorithm selector (LANE / ASSAULT / FREE_GOAL / STATIC)
    int movementTargetId;       // local aggro pursuit target, -1 when none
    int ticksSinceProgress;     // ticks since the last forward step toward the current goal
    int lastSteerSideSign;      // continuity bias for scored sidestep selection (-1/0/+1)

    // Deposit slot reservation (farmers only)
    int             reservedDepositSlotIndex;  // -1 when no reservation held
    DepositSlotKind reservedDepositSlotKind;   // DEPOSIT_SLOT_NONE when unclaimed

    // Base-only payloads. Non-base entities leave these zero-initialized.
    DepositSlotRing depositSlots;
    int  baseLevel;                    // 1..PROGRESSION_MAX_LEVEL; 0 for non-bases
    bool basePendingKingBurst;         // true while a queued King swing awaits hit-marker
    int  basePendingKingBurstDamage;   // damage to apply at the swing's hit frame
};

// Card slot - represents a physical NFC reader position
typedef struct {
    Vector2 worldPos; // Spawn position in world coordinates
    Card *activeCard; // Currently placed card (NULL if empty)
    float cooldownTimer; // Cooldown before slot can be used again
} CardSlot;

// Player state -- seat/view/input/resource owner (per D-12)
// Battlefield owns all geometry, entities, and lane waypoints.
// Player retains only identity, camera/viewport, card slots, and energy.
struct Player {
    int id;                    // 0 or 1
    BattleSide side;           // SIDE_BOTTOM or SIDE_TOP (per D-12)
    Rectangle screenArea;      // Full half-screen owned by this player (kept for reference)
    Rectangle battlefieldArea; // Inner sub-rect that hosts world-space rendering
    Rectangle handArea;        // Outer-edge hand-bar strip (HAND_UI_DEPTH_PX deep)
    Camera2D camera;           // Camera for this player's view
    float cameraRotation;      // 90 or -90 for split screen orientation

    // Card slots (3 NFC readers per player)
    CardSlot slots[NUM_CARD_SLOTS];

    // Visible hand contents, independent from the three NFC/lane slots.
    Card *handCards[HAND_MAX_CARDS];
    bool handCardAnimating[HAND_MAX_CARDS];
    float handCardAnimElapsed[HAND_MAX_CARDS];

    // Energy system
    float energy;
    float maxEnergy;
    float energyRegenRate;

    // Non-owning: Battlefield entity registry owns the base entity.
    // NULL if destroyed or not yet spawned.
    Entity *base;

    // Sustenance scoring (incremented on deposit or carrying-farmer death)
    int sustenanceCollected;
};

// Game state
struct GameState {
    // Database & cards
    DB db;
    Deck deck;
    CardAtlas cardAtlas;

    // Players
    Player players[2];

    // Biome definitions (shared textures, per-biome tile mappings)
    BiomeDef biomeDefs[BIOME_COUNT];

    // Canonical battlefield -- authoritative world model (per D-11)
    Battlefield battlefield;

    // Per-frame flow-field navigation cache. Reset each tick by
    // nav_begin_frame() before the entity update loop.
    NavFrame nav;

    // Character sprites (shared by all entities)
    SpriteAtlas spriteAtlas;
    SpawnFxSystem spawnFx;

    // Sustenance node texture (shared by sustenance_renderer)
    Texture2D sustenanceTexture;
    Texture2D statusBarsTexture;
    Texture2D troopHealthBarTexture;

    // Shared hand UI textures (shared by hand_ui)
    Texture2D handBarBackgroundTexture;
    Texture2D handCardSheetTexture;

    // Bitmap font sheet for HUD and match-result overlays
    Texture2D uvuliteLetteringTexture;

    // Screen layout
    int halfWidth; // Half screen width for split screen

    // P2 viewport render target -- P2 uses rot=+90 (same as P1) for correct
    // seam placement, then the texture is flipped vertically when composited
    // to reverse the X orientation for across-the-table perspective.
    RenderTexture2D p2RT;

    // NFC hardware (two Arduinos, one per player)
    NFCReader nfc;

    // Match result (latched on first lethal base hit, or defensive draw fallback)
    bool gameOver;
    int winnerID;   // -1 = unset or draw if gameOver=true, 0 = P1, 1 = P2
};

#endif //NFC_CARDGAME_TYPES_H
