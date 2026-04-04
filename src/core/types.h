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
#include "../rendering/biome.h"
#include "../hardware/nfc_reader.h"
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

    // Ownership
    int ownerID; // Player index (0 or 1)
    int lane; // Which lane (0-2)
    int waypointIndex; // Current target waypoint index along lane path

    // Debug
    float hitFlashTimer;        // countdown for hit-marker visual flash (debug overlay)

    // Flags
    bool alive;
    bool markedForRemoval;
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
    Rectangle screenArea;      // Screen space viewport
    Camera2D camera;           // Camera for this player's view
    float cameraRotation;      // 90 or -90 for split screen orientation

    // Card slots (3 NFC readers per player)
    CardSlot slots[NUM_CARD_SLOTS];

    // Energy system
    float energy;
    float maxEnergy;
    float energyRegenRate;

    // Non-owning: Battlefield entity registry owns the base entity.
    // NULL if destroyed or not yet spawned.
    Entity *base;
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

    // Character sprites (shared by all entities)
    SpriteAtlas spriteAtlas;

    // Screen layout
    int halfWidth; // Half screen width for split screen

    // P2 viewport render target -- P2 uses rot=+90 (same as P1) for correct
    // seam placement, then the texture is flipped vertically when composited
    // to reverse the X orientation for across-the-table perspective.
    RenderTexture2D p2RT;

    // NFC hardware (two Arduinos, one per player)
    NFCReader nfc;
};

#endif //NFC_CARDGAME_TYPES_H
