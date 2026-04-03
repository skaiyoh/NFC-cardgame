# Requirements: NFC Tabletop Card Game

**Defined:** 2026-03-10
**Core Value:** Two players can sit down, tap physical cards, watch troops fight, and one player wins.

## v1 Requirements

### Core Gameplay Loop

- [x] **CORE-01**: Troops move along per-lane waypoints toward the enemy base (not raw straight-line Y movement)
- [ ] **CORE-02**: Troops detect the nearest enemy entity or base when entering attack range
- [ ] **CORE-03**: Troops attack their target at the rate defined by `attackSpeed`, dealing `attack` damage per hit
- [ ] **CORE-04**: Troops take damage and transition to `ESTATE_DEAD` when HP reaches 0
- [ ] **CORE-05**: Death animation plays to completion before the entity is removed from the board
- [ ] **CORE-06**: `building_create_base` creates a base entity with HP for each player at match start
- [ ] **CORE-07**: Base entities take damage when a troop reaches and attacks them
- [ ] **CORE-08**: Game detects when a base HP drops to 0 and triggers the win condition
- [ ] **CORE-09**: `win_trigger` sets `gs->gameOver = true` and `gs->winnerID` to the winning player

### Match Flow

- [ ] **FLOW-01**: Game starts in `PHASE_PREGAME`; both players must confirm via NFC scan before play begins
- [ ] **FLOW-02**: Match transitions from `PHASE_PREGAME -> PHASE_PLAYING` once both players are ready
- [ ] **FLOW-03**: Match transitions from `PHASE_PLAYING -> PHASE_OVER` when `win_trigger` fires
- [ ] **FLOW-04**: Win screen is displayed showing which player destroyed the enemy base

### Card Effects

- [ ] **CARD-01**: `play_spell` applies actual HP damage to targeted entities in-game (not just console output)
- [ ] **CARD-02**: Healer troop applies HP regen to friendly entities in range (aura or on-attack heal)
- [ ] **CARD-03**: Assassin troop prioritizes targeting buildings/bases over troops
- [ ] **CARD-04**: Brute troop has a taunt mechanic — nearby enemies target it preferentially
- [ ] **CARD-05**: Farmer troop passively increases the owner's energy regen rate while alive

## v2 Requirements

### Polish & Balance

- **POL-01**: Energy regen rate balance pass — card costs and regen tuned for pacing
- **POL-02**: Projectile system for ranged troops — `projectile.c` implemented and integrated
- **POL-03**: Biome selection in pregame — players choose or randomize the map biome
- **POL-04**: Card slot cooldown visual indicator in UI
- **POL-05**: Sound effects for attacks, deaths, and base destruction

### Reliability

- **REL-01**: Entity ID overflow protection — reset between matches or use `uint32_t` with wrap
- **REL-02**: Card handler registry duplicate-type guard

## Out of Scope

| Feature | Reason |
|---------|--------|
| Online multiplayer | Hardware is local-only; networking out of scope for v1 |
| AI opponent | NFC hardware requires human input per player |
| In-game deck builder UI | Handled by standalone `card_enroll` tool |
| Mobile / web client | C + Raylib desktop binary only |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| CORE-01 | Phase 1: Lane Pathfinding | Complete |
| CORE-02 | Phase 3: Combat Detection | Pending |
| CORE-03 | Phase 4: Combat Resolution | Pending |
| CORE-04 | Phase 4: Combat Resolution | Pending |
| CORE-05 | Phase 5: Death Animation | Pending |
| CORE-06 | Phase 2: Base Creation | Pending |
| CORE-07 | Phase 6: Base Destruction & Win Condition | Pending |
| CORE-08 | Phase 6: Base Destruction & Win Condition | Pending |
| CORE-09 | Phase 6: Base Destruction & Win Condition | Pending |
| FLOW-01 | Phase 7: Match State Machine | Pending |
| FLOW-02 | Phase 7: Match State Machine | Pending |
| FLOW-03 | Phase 7: Match State Machine | Pending |
| FLOW-04 | Phase 8: Win Screen | Pending |
| CARD-01 | Phase 9: Spell Effects | Pending |
| CARD-02 | Phase 10: Troop Type Behaviors | Pending |
| CARD-03 | Phase 10: Troop Type Behaviors | Pending |
| CARD-04 | Phase 10: Troop Type Behaviors | Pending |
| CARD-05 | Phase 10: Troop Type Behaviors | Pending |

**Coverage:**
- v1 requirements: 18 total
- Mapped to phases: 18
- Unmapped: 0

---
*Requirements defined: 2026-03-10*
*Last updated: 2026-03-10 after roadmap creation*
