# NFC Tabletop Card Game

## What This Is

A two-player split-screen strategy game where physical NFC cards control the virtual board. Players sit across from each other, each with 3 NFC reader slots wired to an Arduino. Tapping a card spawns troops or casts spells in the corresponding lane. Troops march toward the enemy base — the player who destroys the opponent's base wins.

Built in C with Raylib, PostgreSQL for card data, and Arduino hardware for NFC input.

## Core Value

Two players can sit down, tap physical cards, watch troops fight, and one player wins.

## Requirements

### Validated

- ✓ NFC → Arduino reads — physical cards are read and serial data arrives correctly
- ✓ Card lookup & troop spawning — full pipeline: NFC read → `cards_find()` → `card_action_play()` → troop appears on board
- ✓ Energy system — card cost checking and energy deduction wired into `spawn_troop_from_card`; regen ticks each frame
- ✓ Entity rendering — troops render with animated sprites in the correct split-screen viewport
- ✓ Split-screen viewports — rotated viewports so players sit across from each other
- ✓ Tilemap / biome rendering — map renders with biome tiles (plains, cursed lands, undead, etc.)
- ✓ DB card loading — card stats, sprites, and metadata load from PostgreSQL at runtime
- ✓ Card handler registry — knight, healer, assassin, brute, farmer, spell types registered
- ✓ Dev tools — card preview, biome preview, card enroll utilities are functional

### Active

- [ ] Bases exist on the board — `building_create_base` must create and place base entities for each player
- [ ] Troops move along lane paths — `pathfind_next_step` uses lane waypoints instead of raw straight-line movement
- [ ] Troops fight each other — `combat_find_target`, `combat_in_range`, `combat_resolve` implemented and wired into `entity_update`
- [ ] Game ends when a base is destroyed — `win_check` detects base hp ≤ 0 and calls `win_trigger`
- [ ] Win/lose screen displayed — `win_trigger` sets game-over state and shows a winner screen
- [ ] Game phase state machine — `PHASE_PREGAME → PHASE_PLAYING → PHASE_OVER` wired into `game.c`
- [ ] Death animation plays before removal — `ESTATE_DEAD` waits for `ANIM_DEATH` to complete
- [ ] Spell cards have actual in-game effects — `play_spell` applies damage to targeted entities
- [ ] Card types have unique behaviors — healer heals, assassin has target-priority, brute has taunt/AoE

### Out of Scope

- Real-time networking / online multiplayer — hardware is local-only by design
- Mobile or web client — C/Raylib desktop binary only
- Deck builder UI in-game — card enrollment handled by standalone `card_enroll` tool
- AI opponent — two human players required (NFC hardware input)

## Context

- Language: C (GCC), no engine — Raylib for rendering, libpq for DB, cJSON for card data parsing
- Hardware: two Arduinos, each with 3 NFC readers mapped to in-game lanes
- Database: PostgreSQL via Docker Compose; card stats and sprite metadata stored there
- Entity state machine: `ESTATE_IDLE → ESTATE_WALKING → ESTATE_ATTACKING → ESTATE_DEAD` — walking is wired, attacking and combat resolution are stub declarations only
- `building_create_base` always returns NULL — this blocks both win condition and base-rendering; must be fixed before any combat system can terminate a match
- Card type behaviors (healer, assassin, brute, farmer) all call `spawn_troop_from_card` identically — unique logic is deferred

## Constraints

- **Tech stack**: C + Raylib + libpq + Arduino — no runtime changes planned
- **Hardware dependency**: NFC hardware required for full two-player mode; single-player / keyboard simulation not in scope
- **No game engine**: all ECS, pathfinding, and combat logic must be hand-implemented
- **Docker**: database runs in Docker Compose; local dev requires Docker

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Lane-based pathfinding over A* | Deterministic, predictable troop movement; fits card-lane mapping | — Pending |
| PostgreSQL for card data | Structured card metadata with tooling (pgAdmin, card_enroll) | ✓ Good |
| Split-screen with rotated viewports | Physical face-to-face play at one screen | ✓ Good |
| Stub files with TODO comments | Clear separation of what exists vs what needs implementing | ✓ Good |

---
*Last updated: 2026-03-10 after initialization*
