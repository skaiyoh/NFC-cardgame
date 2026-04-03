# Architecture

Verified from source on 2026-04-03.

## Runtime Model

- Single-threaded Raylib loop:
  - `game_init()`
  - `while (!WindowShouldClose()) { game_update(); game_render(); }`
  - `game_cleanup()`
- One canonical battlefield owned by `Battlefield`
  - width `1080`
  - height `1920`
  - seam `y = 960`
- Two territories inside that one world:
  - `SIDE_TOP` -> `{0, 0, 1080, 960}`
  - `SIDE_BOTTOM` -> `{0, 960, 1080, 960}`
- Players are no longer world owners.
  - `Player` stores identity, viewport/camera state, card slots, and energy.
  - `Battlefield` stores canonical waypoints, spawn anchors, territory tilemaps, and the entity registry.

## Ownership

| Owner | Owns Now | Does Not Own |
|------|-----------|--------------|
| `GameState` | root composition of DB, deck, players, battlefield, atlases, NFC state, `p2RT` | per-entity gameplay logic |
| `Battlefield` | world geometry, territories, tilemaps, canonical lane waypoints, spawn anchors, `entities[]` | player energy or slot state |
| `Player` | `id`, `side`, `screenArea`, `camera`, `cameraRotation`, `slots[]`, `energy`, `maxEnergy`, `energyRegenRate` | entities, tilemaps, lane geometry |
| `Entity` | troop/building/projectile state | direct ownership of shared textures |
| `BiomeDef` / `SpriteAtlas` / `CardAtlas` | shared loaded rendering assets | match-specific gameplay state |

## Layer Boundaries

```text
game.c
  |- data/        SQLite open/load and deck lookup
  |- rendering/   viewports, tilemaps, sprites, UI, debug overlay
  |- entities/    per-entity state machine and draw
  |- systems/     player state and energy
  |- logic/       card dispatch, pathfinding, combat
  `- hardware/    serial NFC ingest
```

## Primary Flows

### Card Play

```text
NFC packet or keyboard key
-> cards_find_by_uid() or cards_find()
-> card_action_play()
-> spawn_troop_from_card()
-> troop_create_data_from_card()
-> troop_spawn()
-> bf_add_entity()
```

### Frame Update

```text
game_update()
-> nfc_poll()
-> keyboard debug input
-> player_update() for both players
-> entity_update() for every Battlefield entity
-> backward sweep of marked-for-removal entities
-> debug_events_tick()
```

### Frame Render

```text
game_render()
-> draw P1 viewport directly
-> draw P2 viewport into RenderTexture2D
-> composite P2 texture to right half of screen
-> draw HUD in screen space
```

## Architectural Facts That Matter

- Canonical world-space refactor is already in place.
  - `Entity.position` is now treated as canonical world space.
  - pathfinding reads `Battlefield` waypoints directly.
  - combat uses direct canonical distance through `bf_distance()`.
- Player 2 still uses a render texture, but the old seam-remap architecture is gone.
  - `p2RT` is just the P2 viewport composite surface.
  - there is no `seamRT` path anymore.
- The runtime currently initializes both territories with `BIOME_GRASS`.
  - four biome definitions exist
  - the game startup currently passes `BIOME_GRASS, BIOME_GRASS` to `bf_init()`

## Stub Boundaries

These modules are still architectural placeholders rather than active systems:

- `src/entities/building.c`
- `src/entities/projectile.c`
- `src/logic/win_condition.c`
- `src/systems/match.c`
