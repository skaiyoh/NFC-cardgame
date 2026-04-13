//
// Created by Nathan Davis on 2/16/26.
//

#include "troop.h"
#include "entities.h"
#include "../logic/pathfinding.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>

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
    data.name = card->name;
    data.spriteType = sprite_type_from_card(card->type);

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
    } else if (card->type && strcmp(card->type, "healer") == 0) {
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
    }

    cJSON *tgtType = cJSON_GetObjectItem(root, "targetType");
    if (tgtType && cJSON_IsString(tgtType)) {
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
    e->bodyRadius = (data->bodyRadius > 0.0f)
        ? data->bodyRadius
        : troop_default_body_radius(data->spriteType);
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
