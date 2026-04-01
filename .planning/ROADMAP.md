# Roadmap: NFC Tabletop Card Game

## Overview

This roadmap takes the existing C/Raylib NFC card game from "troops spawn and walk" to "two players can play a complete match with a winner." The work progresses through three arcs: movement and board setup (pathfinding, bases), combat and win conditions (targeting, damage, death, base destruction), and match polish (state machine, win screen, spell effects, unique troop behaviors). Each phase delivers a verifiable capability that builds toward the core value: two players sit down, tap physical cards, watch troops fight, and one player wins.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Lane Pathfinding** - Troops follow per-lane waypoints instead of raw straight-line movement
- [ ] **Phase 2: Base Creation** - Each player gets a base entity with HP placed on the board at match start
- [ ] **Phase 3: Combat Detection** - Troops detect the nearest enemy entity or base when entering attack range
- [ ] **Phase 4: Combat Resolution** - Troops deal damage per attack and targets die at 0 HP
- [ ] **Phase 5: Death Animation** - Death animation plays to completion before entity removal
- [ ] **Phase 6: Base Destruction & Win Condition** - Troops attack bases, game detects base death, winner is determined
- [ ] **Phase 7: Match State Machine** - Pregame confirmation, playing, and game-over phase transitions
- [ ] **Phase 8: Win Screen** - Winner display shown when a base is destroyed
- [ ] **Phase 9: Spell Effects** - Spell cards apply real in-game damage to targeted entities
- [ ] **Phase 10: Troop Type Behaviors** - Healer, assassin, brute, and farmer have unique gameplay mechanics

## Phase Details

### Phase 1: Lane Pathfinding
**Goal**: Troops navigate along predefined lane waypoints toward the enemy base
**Depends on**: Nothing (first phase)
**Requirements**: CORE-01
**Success Criteria** (what must be TRUE):
  1. A spawned troop walks along the waypoints defined for its lane, visibly following the path curves rather than moving in a straight line
  2. Troops in different lanes follow different paths without crossing into each other's lanes
  3. A troop that reaches the end of its lane waypoints stops or idles at the enemy side of the board
**Plans:** 3 plans
Plans:
- [x] 01-00-PLAN.md -- Test infrastructure: test_pathfinding.c and Makefile test target (Wave 0)
- [x] 01-01-PLAN.md -- Config defines, struct fields, waypoint generation, player init integration
- [x] 01-02-PLAN.md -- Waypoint-following movement, sprite direction, debug overlay, visual verification

### Phase 2: Base Creation
**Goal**: Each player has a visible base entity with HP on the board when a match begins
**Depends on**: Phase 1
**Requirements**: CORE-06
**Success Criteria** (what must be TRUE):
  1. `building_create_base` returns a valid entity (not NULL) for each player
  2. Both bases render visibly on the board at their respective sides
  3. Each base has an HP value that can be inspected (e.g., via debug overlay or log)
**Plans**: TBD

### Phase 3: Combat Detection
**Goal**: Troops identify the nearest enemy to engage when within attack range
**Depends on**: Phase 2
**Requirements**: CORE-02
**Success Criteria** (what must be TRUE):
  1. A troop walking toward an enemy stops and transitions to `ESTATE_ATTACKING` when an enemy entity enters its attack range
  2. A troop with no enemies in range continues walking (does not stop or attack air)
  3. A troop targets the nearest enemy among multiple candidates, including bases
**Plans**: TBD

### Phase 4: Combat Resolution
**Goal**: Troops deal and receive damage, and entities die when HP reaches zero
**Depends on**: Phase 3
**Requirements**: CORE-03, CORE-04
**Success Criteria** (what must be TRUE):
  1. An attacking troop deals damage equal to its `attack` stat at the cadence defined by `attackSpeed`
  2. A troop receiving damage has its HP visibly reduced (log or health bar)
  3. A troop whose HP reaches 0 transitions to `ESTATE_DEAD`
  4. Two troops fighting each other both deal and receive damage simultaneously
**Plans**: TBD

### Phase 5: Death Animation
**Goal**: Dead entities play their death animation before being removed from the board
**Depends on**: Phase 4
**Requirements**: CORE-05
**Success Criteria** (what must be TRUE):
  1. A troop entering `ESTATE_DEAD` plays `ANIM_DEATH` visibly on screen
  2. The entity is not removed from the board until the death animation completes
  3. After the death animation finishes, the entity is fully removed and no longer rendered or updated
**Plans**: TBD

### Phase 6: Base Destruction & Win Condition
**Goal**: Troops can destroy a base and the game determines a winner
**Depends on**: Phase 4, Phase 2
**Requirements**: CORE-07, CORE-08, CORE-09
**Success Criteria** (what must be TRUE):
  1. A troop that reaches an enemy base attacks it and visibly reduces the base HP
  2. When a base HP reaches 0, `win_trigger` fires and sets `gs->gameOver = true`
  3. `gs->winnerID` is set to the player whose base was NOT destroyed
  4. No further troop spawning or combat occurs after `gameOver` is true
**Plans**: TBD

### Phase 7: Match State Machine
**Goal**: Matches follow a structured lifecycle from pregame through playing to game over
**Depends on**: Phase 6
**Requirements**: FLOW-01, FLOW-02, FLOW-03
**Success Criteria** (what must be TRUE):
  1. Game starts in `PHASE_PREGAME` and NFC card taps do not spawn troops during this phase
  2. Both players can confirm readiness via NFC scan, and the match transitions to `PHASE_PLAYING` only after both confirm
  3. When `win_trigger` fires, the game transitions to `PHASE_OVER` and the playing phase ends
  4. Phase transitions are logged or visibly indicated (debug output or on-screen text)
**Plans**: TBD

### Phase 8: Win Screen
**Goal**: Players see a clear winner announcement when the match ends
**Depends on**: Phase 7
**Requirements**: FLOW-04
**Success Criteria** (what must be TRUE):
  1. When the game enters `PHASE_OVER`, a win screen overlay renders on top of the board
  2. The win screen identifies which player won (e.g., "Player 1 Wins!")
  3. The win screen is visible in both split-screen viewports (both players can read it)
**Plans**: TBD

### Phase 9: Spell Effects
**Goal**: Spell cards deal real damage to entities on the board
**Depends on**: Phase 4
**Requirements**: CARD-01
**Success Criteria** (what must be TRUE):
  1. Playing a spell card via NFC reduces HP on targeted entities (not just console output)
  2. Entities killed by spell damage transition to `ESTATE_DEAD` and follow the normal death flow
  3. Spell damage respects entity ownership (does not damage friendly entities)
**Plans**: TBD

### Phase 10: Troop Type Behaviors
**Goal**: Each troop type (healer, assassin, brute, farmer) has a unique gameplay mechanic
**Depends on**: Phase 4, Phase 9
**Requirements**: CARD-02, CARD-03, CARD-04, CARD-05
**Success Criteria** (what must be TRUE):
  1. Healer troop restores HP to nearby friendly entities while alive (aura or on-attack heal)
  2. Assassin troop prioritizes attacking buildings/bases over other troops when choosing targets
  3. Brute troop causes nearby enemies to target it preferentially over other friendly entities (taunt)
  4. Farmer troop increases its owner's energy regen rate while alive, and the bonus disappears when it dies
  5. Each troop type's behavior is observably different from a default troop in gameplay
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> 8 -> 9 -> 10 -> 11

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Lane Pathfinding | 0/3 | Planning complete | - |
| 2. Base Creation | 0/TBD | Not started | - |
| 3. Combat Detection | 0/TBD | Not started | - |
| 4. Combat Resolution | 0/TBD | Not started | - |
| 5. Death Animation | 0/TBD | Not started | - |
| 6. Base Destruction & Win Condition | 0/TBD | Not started | - |
| 7. Match State Machine | 0/TBD | Not started | - |
| 8. Win Screen | 0/TBD | Not started | - |
| 9. Spell Effects | 0/TBD | Not started | - |
| 10. Troop Type Behaviors | 0/TBD | Not started | - |
| 11. Canonical World-Space Refactor | 2/5 | In Progress|  |

### Phase 11: Canonical single-world-space refactor: extract battlefield math, fix seam rendering, introduce Battlefield model, migrate simulation and rendering to canonical coordinates

**Goal:** Unify the dual-coordinate-space system into a single canonical 1080x1920 world space with a first-class Battlefield model, eliminate duplicated remap formulas, and resolve the seam disappearance by removing cross-space handoff logic entirely
**Requirements**: REFACTOR-11 (structural refactor, no new gameplay requirements)
**Depends on:** Phase 10
**Plans:** 2/5 plans executed

Plans:
- [x] 11-01-PLAN.md -- Extract battlefield_math module with typed coordinate wrappers and test suite (Wave 1)
- [x] 11-02-PLAN.md -- Introduce Battlefield model, canonical board geometry, and the migration adapter boundary (Wave 2)
- [ ] 11-03-PLAN.md -- Migrate simulation and entity ownership to canonical Battlefield coordinates (Wave 3)
- [ ] 11-04-PLAN.md -- Rewrite rendering to use canonical world space and delete seam remap / RenderTexture special cases (Wave 4)
- [ ] 11-05-PLAN.md -- Remove remaining adapters, add invariants, and complete final validation for the canonical rewrite (Wave 5)

**Important note:** With two opposite rotated cameras (`+90` / `-90`), exact pixel-identical continuity at the screen center seam is not a valid architectural goal. Phase 11's success criterion is correct independent rendering from one canonical world with no entity disappearance or remap-driven popping.
