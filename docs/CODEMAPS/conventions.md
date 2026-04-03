# Conventions

## Naming

- files: lowercase `snake_case.c` / `snake_case.h`
- directories: lowercase by subsystem (`core`, `data`, `logic`, ...)
- functions: module-style verbs such as `entity_update()`, `player_init()`, `cards_load()`
- structs/types: `PascalCase`
- enum values and macros: `UPPER_SNAKE_CASE`
- local variables and struct fields: `camelCase`

## Header Style

- include guards use the `NFC_CARDGAME_*` pattern
- forward declarations are used aggressively to avoid circular includes
- test files commonly predefine include guards to isolate production `.c` files from heavy dependencies

## Ownership And Lifetime

- `GameState` is the root runtime owner.
- Entities are heap allocated and destroyed explicitly with `entity_destroy()`.
- `Battlefield` owns the authoritative entity registry, but not the entity memory itself.
- `Entity.targetType` is heap owned and freed in `entity_destroy()`.
- Shared render assets live in atlases/biome defs and are not copied into entities.

## Coordinate Convention

- `Entity.position` is canonical world space.
- `Battlefield` is the single source of truth for geometry.
- `Player.side` is the view/seat identity:
  - `SIDE_BOTTOM`
  - `SIDE_TOP`
- P2 slot-to-lane mapping is mirrored through `bf_slot_to_lane()`.

## Animation Convention

- `entity_set_state()` maps entity states onto animation clips.
- attack timing is clip-driven, not just cooldown-driven
- death is treated as one-shot even if sprite metadata would fall back differently

## Logging Convention

- logs are plain text
- tagged prefixes are used instead of a logging framework
- errors usually degrade gracefully instead of aborting the process

## Test Convention

- tests are executable C files, not a framework runner
- production `.c` files are included directly
- stub layouts include explicit sync warnings where they must track production structs

## Build Convention

- `Makefile` is a practical local entrypoint
- `CMakeLists.txt` mirrors the main binaries and tests
- `third_party/cjson` is compiled directly into consumers rather than linked as a separate package
