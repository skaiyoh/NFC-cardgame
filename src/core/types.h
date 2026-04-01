//
// Created by Nathan Davis on 2/16/26.
//

#ifndef NFC_CARDGAME_TYPES_H
#define NFC_CARDGAME_TYPES_H

#include "../../lib/raylib.h"
#include "config.h"
#include "../data/db.h"
#include "../data/cards.h"
#include "../rendering/card_renderer.h"
#include "../rendering/tilemap_renderer.h"
#include "../rendering/sprite_renderer.h"
#include "../rendering/biome.h"
#include "../hardware/nfc_reader.h"

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
    TargetingMode targeting;    // targeting preference
    const char *targetType;     // for TARGET_SPECIFIC_TYPE (owned, freed in entity_destroy)

    // Animation
    AnimState anim;
    const CharacterSprite *sprite;
    float spriteScale;

    // Ownership
    int ownerID; // Player index (0 or 1)
    int lane; // Which lane (0-2)
    int waypointIndex; // Current target waypoint index along lane path

    // Flags
    bool alive;
    bool markedForRemoval;
};

// Constants
#define NUM_CARD_SLOTS 3
#define MAX_ENTITIES 64

// Card slot - represents a physical NFC reader position
typedef struct {
    Vector2 worldPos; // Spawn position in world coordinates
    Card *activeCard; // Currently placed card (NULL if empty)
    float cooldownTimer; // Cooldown before slot can be used again
} CardSlot;

// Player state
struct Player {
    int id; // 0 or 1
    Rectangle playArea; // World space play area
    Rectangle screenArea; // Screen space viewport
    Camera2D camera; // Camera for this player's view
    float cameraRotation; // 90 or -90 for split screen orientation

    // Tilemap (per-player biome tile definitions)
    TileMap tilemap;
    BiomeType biome;
    const BiomeDef *biomeDef; // pointer into GameState::biomeDefs[]
    TileDef tileDefs[TILE_COUNT];
    int tileDefCount;
    TileDef detailDefs[MAX_DETAIL_DEFS];
    int detailDefCount;

    // Card slots (3 NFC readers per player)
    CardSlot slots[NUM_CARD_SLOTS];

    // Entities (troops, buildings) - will be expanded in Phase 7
    Entity *entities[MAX_ENTITIES];
    int entityCount;

    // Energy system - will be expanded in Phase 5
    float energy;
    float maxEnergy;
    float energyRegenRate;

    // Base building reference (for win condition)
    Entity *base;

    // Pre-computed lane waypoints (generated once at init)
    Vector2 laneWaypoints[3][LANE_WAYPOINT_COUNT];
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

    // Character sprites (shared by all entities)
    SpriteAtlas spriteAtlas;

    // Screen layout
    int halfWidth; // Half screen width for split screen

    // Render target for crossed entities near the center seam.
    // Drawn without scissor so sprites extend past x=halfWidth seamlessly.
    RenderTexture2D seamRT;

    // NFC hardware (two Arduinos, one per player)
    NFCReader nfc;
};

#endif //NFC_CARDGAME_TYPES_H