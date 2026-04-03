# Frontend

## Viewports And Cameras

- The window is split into two `960 x 1080` halves.
- Both players use `Camera2D` with `rotation = 90.0f`.
- Player 1 renders directly to the left half.
- Player 2 renders into `RenderTexture2D p2RT`, then that texture is composited into the right half.
- The P2 composite uses a vertically flipped source rectangle to produce the across-the-table perspective.

## World Rendering Order

Inside each world-space view:

1. draw bottom territory tilemap
2. draw top territory tilemap
3. draw all live Battlefield entities
4. draw enabled debug overlays

After world-space drawing:

5. draw viewport labels / HUD in screen space
6. draw energy bars for both players

## Tilemaps And Biomes

- `biome_init_all()` defines four biomes:
  - grass
  - undead
  - snow
  - swamp
- `tilemap_create_biome()` builds per-territory grids from a `BiomeDef`.
- Both territories are currently rendered from the Battlefield model.
- The top territory is drawn with a `180` degree tile rotation so that terrain art reads correctly from the opposite seat.
- Even though four biomes exist, `game_init()` currently initializes both territories as `BIOME_GRASS`.

## Sprite And Animation Rendering

- `sprite_atlas_init()` loads a base sprite set plus type-specific sets for:
  - knight
  - healer
  - assassin
  - brute
  - farmer
- `AnimState` is lightweight per-entity playback state.
- `entity_animation.c` defines clip policy:
  - default cycle times
  - one-shot vs loop
  - attack hit markers
- `sprite_draw()` chooses the frame from `normalizedTime` and the current facing direction.

## Card Rendering

- `card_renderer.c` parses the card sheet at `CARD_SHEET_PATH`.
- Card visuals are composed from 11 layers.
- `card_visual_from_json()` reads a nested `visual` object from card JSON.
- `card_draw_back()` supports the preview tool's back-side mode.

## UI And Debug

### Player-Facing UI

- `ui_draw_energy_bar()` draws one energy bar per player in screen space.
- `ui_draw_viewport_label()` draws the P2 label in rotated screen space.
- P1's `PLAYER 1` label is currently drawn in world space during the left-viewport pass.

### Debug Overlay

- `F1` toggles colored lane path lines
- `F2` toggles attack progress bars
- `F3` toggles target lock lines
- `F4` toggles debug event flashes
- `F5` toggles attack range circles

## Frontend Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| `card_preview` | interactive card visual editor | can import JSON from a file and print JSON back to stdout |
| `biome_preview` | biome authoring and preview tool | supports built-in biome import, save/load, and export |
