# NFC Card Game — Project To-Do List

## Implementation Status

### Done

| Feature | Files |
|---------|-------|
| Database layer (SQLite connection, card loading) | `src/data/db.c`, `cards.c` |
| Card effect handler registry | `src/logic/card_effects.c` |
| Card effects → troop spawning (all 5 types) | `src/logic/card_effects.c`, `src/entities/troop.c` |
| Entity lifecycle (create/update/draw/destroy, state machine) | `src/entities/entities.c` |
| Entity movement (waypoint pathfinding, lane-based walking) | `src/logic/pathfinding.c`, `src/entities/entities.c` |
| Sprite animation system (5 character types, all anim types) | `src/rendering/sprite_renderer.c` |
| Card visual rendering (13 colors, 4 bg styles, layer offsets) | `src/rendering/card_renderer.c` |
| Procedural tilemap generation (biome-aware) | `src/rendering/tilemap_renderer.c`, `biome.c` |
| Split-screen viewports (canonical single-world-space, RenderTexture flip) | `src/rendering/viewport.c` |
| Player setup (camera, 3 card slots, energy regen) | `src/systems/player.c` |
| Core game loop (init → update → render → cleanup) | `src/core/game.c` |
| Canonical Battlefield model (single authoritative world space) | `src/core/battlefield.c`, `battlefield_math.c` |
| Combat system — targeting, range check, damage resolution | `src/logic/combat.c` |
| Energy enforcement — cost check and consumption on card play | `src/systems/energy.c` |
| Attack animation state — `ESTATE_ATTACKING` in entity state machine | `src/entities/entities.c`, `src/core/types.h` |
| NFC hardware — Arduino serial protocol, card UID reading | `src/hardware/nfc_reader.c`, `arduino_protocol.c` |
| UI — energy bar, viewport labels | `src/rendering/ui.c` |
| Makefile/CMake sync — all source files in both build systems | `Makefile`, `CMakeLists.txt` |

---

## Production Flow

End-to-end system from physical hardware to rendered frame.

```
HARDWARE LAYER
  NFC Reader (×3 per player) → Arduino Nano
    → Serial USB → nfc_reader.c → arduino_protocol.c
    → Card UID string → db lookup → Card* pointer

PREGAME PHASE  [match.c — stub]
  pregame_init()                    → allocate per-match state
  pregame_handle_card_scan() ×n    → each player scans their deck cards
  pregame_are_players_ready()      → both ready → start match
  pregame_update/render()          → lobby countdown screen

IN-GAME LOOP  [game.c]
  game_update(deltaTime)
  ├── NFC scan event → card_action_play(card, gs)           [card_effects.c]
  │     └── Handler dispatch by card->type
  │           ├── Troop card → spawn_troop_from_card()
  │           │     ├── energy_can_afford()  → reject if < cost
  │           │     ├── energy_consume()     → deduct cost
  │           │     ├── troop_create_data_from_card() → parse JSON stats
  │           │     ├── bf_spawn_pos()       → canonical spawn position
  │           │     ├── troop_spawn()        → entity_create() + set stats
  │           │     └── bf_add_entity()      → register in Battlefield
  │           └── Spell card → apply effect (damage, buff, debuff) [stub]
  │
  ├── player_update() ×2                                     [player.c]
  │     ├── energy_update()         → regen energy (capped at maxEnergy)
  │     └── slot cooldown decrement
  │
  ├── entity_update() for all Battlefield entities            [entities.c]
  │     └── per entity:
  │           ├── WALKING → pathfind_step_entity(); combat_find_target()
  │           │     └── target in attackRange → transition ATTACKING
  │           ├── ATTACKING → combat_find_target() + combat_resolve()
  │           │     ├── target.hp -= attacker.attack (cooldown-gated)
  │           │     ├── target.hp ≤ 0 → entity_set_state(DEAD)
  │           │     └── no target in range → back to WALKING
  │           └── DEAD → markedForRemoval = true
  │
  └── sweep dead entities (backward iteration, swap-remove)

  game_render()
  ├── Viewport P1 (scissor left 960px, Camera2D +90°, direct to screen)
  │     ├── viewport_draw_battlefield_tilemap(SIDE_BOTTOM)
  │     ├── viewport_draw_battlefield_tilemap(SIDE_TOP)
  │     ├── game_draw_canonical_entities()
  │     └── Player 1 label
  ├── Viewport P2 (Camera2D +90°, rendered to RenderTexture)
  │     ├── Same tilemap + entity drawing as P1
  │     └── Composited to right half with vertical flip (across-table perspective)
  └── HUD (screen space)
        ├── ui_draw_viewport_label("PLAYER 2")
        └── ui_draw_energy_bar() ×2

END-GAME STATE  [win_condition.c — stub]
  win_trigger()  → freeze game, show winner overlay, prompt rematch
  win_check()    → called each frame, monitors both bases' HP
```

---

## Remaining Work

### Critical (no game without these)

| # | Feature | Files | Notes |
|---|---------|-------|-------|
| 1 | **Building system** — base entity with HP, takes damage from crossed troops | `src/entities/building.c` | `building_create_base()` returns NULL; no bases exist |
| 2 | **Win conditions** — base HP tracking, victory/defeat detection | `src/logic/win_condition.c` | `win_check()` and `win_trigger()` are stubs |

### Important (needed for real gameplay)

| # | Feature | Files | Notes |
|---|---------|-------|-------|
| 3 | **UI — base HP bars** — display each player's base health | `src/rendering/ui.c` | Energy bar done; health bars and card hand display still TODO |
| 4 | **Spell card effects** — actual damage/buff logic (not just printf) | `src/logic/card_effects.c` | `play_spell()` only prints debug output |
| 5 | **Projectile system** — spawn projectile entity, fly toward target | `src/entities/projectile.c` | Completely empty |
| 6 | **Death animation** — let ANIM_DEATH play before entity removal | `src/entities/entities.c` | ESTATE_DEAD immediately marks for removal |

### Nice to Have (polish & full experience)

| # | Feature | Files | Notes |
|---|---------|-------|-------|
| 7 | **Pregame/match lobby** — player readiness, card selection phase | `src/systems/match.c` | 5 bare declarations |
| 8 | **Biome completion** — Snow and Swamp biomes (currently reuse Grass) | `src/rendering/biome.c` | Placeholder implementations |
| 9 | **IDLE combat scanning** — idle entities should detect nearby enemies | `src/entities/entities.c` | ESTATE_IDLE has no combat logic |

---

## Implementation Order (remaining work)

1. **Building system** — give each player a base entity with HP; register bases in Battlefield
2. **Win conditions** — `win_check()` scans bases each frame; `win_trigger()` freezes the game
3. **UI — base HP bars** — display both bases' health in screen space HUD
4. **Death animation** — wait for ANIM_DEATH to complete before marking for removal
5. **Spell effects** — implement actual damage/buff logic in `play_spell()`
6. **Projectile system** — ranged attack entities
7. **Pregame lobby** — scan-in phase before match begins
