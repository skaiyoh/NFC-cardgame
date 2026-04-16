//
// Created by Nathan Davis on 2/16/26.
//

#include "troop.h"
#include "entities.h"
#include "../data/card_catalog.h"
#include "../logic/pathfinding.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    CombatProfileId id;
    AttackEngagementMode engagementMode;
    AttackDeliveryMode deliveryMode;
    ProjectileVisualType projectileVisualType;
    float projectileSpeed;
    float projectileHitRadius;
    float projectileSplashRadius;
    float projectileRenderScale;
    Vector2 projectileLaunchOffset;
} CombatProfile;

static const CombatProfile kDefaultMeleeCombatProfile = {
    .id = COMBAT_PROFILE_DEFAULT_MELEE,
    .engagementMode = ATTACK_ENGAGEMENT_CONTACT,
    .deliveryMode = ATTACK_DELIVERY_INSTANT,
    .projectileVisualType = PROJECTILE_VISUAL_NONE,
    .projectileSpeed = 0.0f,
    .projectileHitRadius = 0.0f,
    .projectileSplashRadius = 0.0f,
    .projectileRenderScale = 1.0f,
    .projectileLaunchOffset = { 0.0f, 0.0f },
};

static const CombatProfile kHealerCombatProfile = {
    .id = COMBAT_PROFILE_HEALER,
    .engagementMode = ATTACK_ENGAGEMENT_DIRECT_RANGE,
    .deliveryMode = ATTACK_DELIVERY_PROJECTILE,
    .projectileVisualType = PROJECTILE_VISUAL_HEALER_BLOB,
    .projectileSpeed = 240.0f,
    .projectileHitRadius = 14.0f,
    .projectileSplashRadius = 0.0f,
    .projectileRenderScale = 1.0f,
    .projectileLaunchOffset = { 0.0f, -12.0f },
};

static const CombatProfile kFishfingCombatProfile = {
    .id = COMBAT_PROFILE_FISHFING,
    .engagementMode = ATTACK_ENGAGEMENT_DIRECT_RANGE,
    .deliveryMode = ATTACK_DELIVERY_PROJECTILE,
    .projectileVisualType = PROJECTILE_VISUAL_FISH,
    .projectileSpeed = 300.0f,
    .projectileHitRadius = 12.0f,
    .projectileSplashRadius = 0.0f,
    .projectileRenderScale = 1.5f,
    .projectileLaunchOffset = { 16.0f, 2.0f },
};

static const CombatProfile kBirdCombatProfile = {
    .id = COMBAT_PROFILE_BIRD,
    .engagementMode = ATTACK_ENGAGEMENT_DIRECT_RANGE,
    .deliveryMode = ATTACK_DELIVERY_PROJECTILE,
    .projectileVisualType = PROJECTILE_VISUAL_BIRD_BOMB,
    .projectileSpeed = 220.0f,
    .projectileHitRadius = 12.0f,
    .projectileSplashRadius = 40.0f,
    .projectileRenderScale = 1.0f,
    .projectileLaunchOffset = { 8.0f, -8.0f },
};

static const CombatProfile *combat_profile_for_card_type(const char *cardType) {
    if (!cardType) return &kDefaultMeleeCombatProfile;
    if (strcmp(cardType, "healer") == 0) return &kHealerCombatProfile;
    if (strcmp(cardType, "fishfing") == 0) return &kFishfingCombatProfile;
    if (strcmp(cardType, "bird") == 0) return &kBirdCombatProfile;
    return &kDefaultMeleeCombatProfile;
}

static EntityRenderLayer troop_default_render_layer(SpriteType type) {
    return (type == SPRITE_TYPE_BIRD)
        ? ENTITY_RENDER_LAYER_FLYING
        : ENTITY_RENDER_LAYER_GROUND;
}

float troop_default_body_radius(SpriteType type) {
    switch (type) {
        case SPRITE_TYPE_ASSASSIN: return 12.0f;
        case SPRITE_TYPE_KNIGHT:   return 14.0f;
        case SPRITE_TYPE_HEALER:   return 14.0f;
        case SPRITE_TYPE_FARMER:   return 14.0f;
        case SPRITE_TYPE_BRUTE:    return 18.0f;
        case SPRITE_TYPE_BIRD:     return 14.0f;
        case SPRITE_TYPE_FISHFING: return 14.0f;
        case SPRITE_TYPE_BASE:     return 16.0f;
        default:                   return 14.0f;
    }
}

TroopData troop_create_data_from_card(const Card *card) {
    TroopData data = {0};
    const char *cardType = card_catalog_resolved_type(card);
    data.name = card->name;
    data.spriteType = sprite_type_from_card(cardType);

    // Sensible defaults
    data.hp = 100;
    data.maxHP = 100;
    data.attack = 10;
    data.attackSpeed = 1.0f;
    data.attackRange = 40.0f;
    data.moveSpeed = 60.0f;
    data.targeting = TARGET_NEAREST;
    data.targetType = NULL;
    data.bodyRadius = troop_default_body_radius(data.spriteType);
    data.renderLayer = troop_default_render_layer(data.spriteType);
    const CombatProfile *profile = combat_profile_for_card_type(cardType);
    data.combatProfileId = profile->id;
    data.engagementMode = profile->engagementMode;
    data.deliveryMode = profile->deliveryMode;
    data.projectileVisualType = profile->projectileVisualType;
    data.projectileSpeed = profile->projectileSpeed;
    data.projectileHitRadius = profile->projectileHitRadius;
    data.projectileSplashRadius = profile->projectileSplashRadius;
    data.projectileRenderScale = profile->projectileRenderScale;
    data.projectileLaunchOffset = profile->projectileLaunchOffset;

    // Override from card JSON data if available
    if (!card->data) return data;

    cJSON *root = cJSON_Parse(card->data);
    if (!root) return data;

    cJSON *hp = cJSON_GetObjectItem(root, "hp");
    if (hp && cJSON_IsNumber(hp)) {
        data.hp = hp->valueint;
        data.maxHP = hp->valueint;
    }

    cJSON *maxHP = cJSON_GetObjectItem(root, "maxHP");
    if (maxHP && cJSON_IsNumber(maxHP)) {
        data.maxHP = maxHP->valueint;
    }

    cJSON *atk = cJSON_GetObjectItem(root, "attack");
    if (atk && cJSON_IsNumber(atk)) {
        data.attack = atk->valueint;
    }

    cJSON *heal = cJSON_GetObjectItem(root, "healAmount");
    if (heal && cJSON_IsNumber(heal)) {
        data.healAmount = heal->valueint;
    } else if (cardType && strcmp(cardType, "healer") == 0) {
        // Older DBs without healAmount: keep healers functional by reusing attack.
        data.healAmount = data.attack;
    }

    cJSON *atkSpd = cJSON_GetObjectItem(root, "attackSpeed");
    if (atkSpd && cJSON_IsNumber(atkSpd)) {
        data.attackSpeed = (float) atkSpd->valuedouble;
    }

    cJSON *atkRange = cJSON_GetObjectItem(root, "attackRange");
    if (atkRange && cJSON_IsNumber(atkRange)) {
        data.attackRange = (float) atkRange->valuedouble;
    }

    cJSON *spd = cJSON_GetObjectItem(root, "moveSpeed");
    if (spd && cJSON_IsNumber(spd)) {
        data.moveSpeed = (float) spd->valuedouble;
    }

    cJSON *tgt = cJSON_GetObjectItem(root, "targeting");
    if (tgt && cJSON_IsString(tgt)) {
        if (strcmp(tgt->valuestring, "building") == 0)
            data.targeting = TARGET_BUILDING;
        else if (strcmp(tgt->valuestring, "specific") == 0)
            data.targeting = TARGET_SPECIFIC_TYPE;
        else if (strcmp(tgt->valuestring, "farmer_first_lowest_hp") == 0 ||
                 strcmp(tgt->valuestring, "anti_air_first") == 0) {
            data.targeting = TARGET_SPECIFIC_TYPE;
            data.targetType = strdup(tgt->valuestring);
        }
    }

    cJSON *tgtType = cJSON_GetObjectItem(root, "targetType");
    if (tgtType && cJSON_IsString(tgtType)) {
        free((char *)data.targetType);
        data.targetType = strdup(tgtType->valuestring);
    }

    cJSON_Delete(root);
    return data;
}

Entity *troop_spawn(Player *owner, const TroopData *data, Vector2 position,
                    const SpriteAtlas *atlas) {
    Faction faction = (owner->id == 0) ? FACTION_PLAYER1 : FACTION_PLAYER2;
    Entity *e = entity_create(ENTITY_TROOP, faction, position);
    if (!e) return NULL;

    // Stats
    e->hp = data->hp;
    e->maxHP = data->maxHP;
    e->attack = data->attack;
    e->healAmount = data->healAmount;
    e->attackSpeed = data->attackSpeed;
    e->attackRange = data->attackRange;
    e->moveSpeed = data->moveSpeed;
    e->combatProfileId = data->combatProfileId;
    e->engagementMode = data->engagementMode;
    e->deliveryMode = data->deliveryMode;
    e->projectileVisualType = data->projectileVisualType;
    e->projectileSpeed = data->projectileSpeed;
    e->projectileHitRadius = data->projectileHitRadius;
    e->projectileSplashRadius = data->projectileSplashRadius;
    e->projectileRenderScale = data->projectileRenderScale;
    e->projectileLaunchOffset = data->projectileLaunchOffset;
    e->bodyRadius = (data->bodyRadius > 0.0f)
        ? data->bodyRadius
        : troop_default_body_radius(data->spriteType);
    e->renderLayer = data->renderLayer;
    e->navProfile = NAV_PROFILE_LANE; // farmer block below overrides to FREE_GOAL

    // Targeting
    e->targeting = data->targeting;
    e->targetType = data->targetType; // ownership transfers to Entity

    // Ownership
    e->ownerID = owner->id;
    e->presentationSide = owner->side;

    // Sprite
    e->spriteType = data->spriteType;
    e->sprite = sprite_atlas_get(atlas, data->spriteType);

    // Start walking immediately
    entity_set_state(e, ESTATE_WALKING);
    pathfind_apply_direction_for_side(&e->anim, (Vector2){ 1.0f, 0.0f }, e->presentationSide);
    e->spriteRotationDegrees = pathfind_sprite_rotation_for_side(e->anim.dir, owner->side);

    // Farmer role: override combat stats from code, not card JSON
    if (data->spriteType == SPRITE_TYPE_FARMER) {
        e->unitRole = UNIT_ROLE_FARMER;
        e->navProfile = NAV_PROFILE_FREE_GOAL;
        e->attack = 0;
        e->attackSpeed = 0.0f;
        e->attackRange = 0.0f;
        e->combatProfileId = COMBAT_PROFILE_DEFAULT_MELEE;
        e->engagementMode = ATTACK_ENGAGEMENT_CONTACT;
        e->deliveryMode = ATTACK_DELIVERY_INSTANT;
        e->projectileVisualType = PROJECTILE_VISUAL_NONE;
        e->projectileSpeed = 0.0f;
        e->projectileHitRadius = 0.0f;
        e->projectileSplashRadius = 0.0f;
        e->projectileRenderScale = 1.0f;
        e->projectileLaunchOffset = (Vector2){ 0.0f, 0.0f };
        e->renderLayer = ENTITY_RENDER_LAYER_GROUND;
        e->farmerState = FARMER_SEEKING;
        e->claimedSustenanceNodeId = -1;
        e->carriedSustenanceValue = 0;
        e->workTimer = 0.0f;
        e->lane = -1;
        e->waypointIndex = -1;
        entity_set_state(e, ESTATE_IDLE); // seek on first update, not walk-in-place
        pathfind_apply_direction_for_side(&e->anim, (Vector2){ 1.0f, 0.0f }, e->presentationSide);
        e->spriteRotationDegrees = (owner->side == SIDE_TOP) ? 180.0f : 0.0f;
    }

    printf("[TROOP] Spawned '%s' (id=%d) for player %d at (%.0f, %.0f)\n",
           data->name, e->id, owner->id, position.x, position.y);

    return e;
}
