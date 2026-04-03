# Backend

## Update Order

`game_update()` currently does exactly this:

1. poll NFC hardware
2. process keyboard debug input
3. update both players' energy and slot cooldown timers
4. update every entity in `Battlefield.entities[]`
5. sweep `markedForRemoval` entities backward and free them
6. tick the debug-event ring buffer

There is no match-phase gate around any of that yet. The game updates directly in play mode from launch.

## Input Paths

### NFC Path

```text
Arduino serial packet
-> arduino_read_packet()
-> nfc_poll()
-> cards_find_by_uid()
-> card_action_play()
```

### Keyboard Debug Path

```text
1 / 2 / 3 -> Player 0 slot 0 / 1 / 2
Q / W / E -> Player 1 slot 0 / 1 / 2
```

Important caveat:

- the debug spawn path hardcodes `KNIGHT_01`
- that matches the checked-in `cardgame.db`
- it does not match a fresh database created from the current lowercase `seed.sql`

## Card Dispatch

- `card_action_init()` registers six lower-case card types:
  - `spell`
  - `knight`
  - `healer`
  - `assassin`
  - `brute`
  - `farmer`
- `card_action_play()` dispatches by `Card.type`.
- `spawn_troop_from_card()` performs:
  - slot availability check
  - energy consume
  - canonical spawn position lookup
  - card JSON stat parse
  - troop entity creation
  - lane assignment
  - `waypointIndex = 1`
  - `bf_add_entity()`

## Entity State Machine

`Entity.state` has four live states:

- `ESTATE_IDLE`
  - currently does not scan for targets or re-enter motion on its own
- `ESTATE_WALKING`
  - advances through canonical waypoints with `pathfind_step_entity()`
  - checks for nearby enemies and transitions into `ESTATE_ATTACKING`
- `ESTATE_ATTACKING`
  - locks a target by `attackTargetId`
  - advances the attack animation
  - applies damage when the clip crosses its configured hit marker
  - chains the next swing or falls back to walking
- `ESTATE_DEAD`
  - plays the death clip once
  - sets `markedForRemoval` only after the clip finishes

## Combat Model

- Range checks use direct canonical distance with `bf_distance()`.
- `combat_find_target()` iterates the Battlefield registry, skips friendlies, and can prioritize buildings when `targeting == TARGET_BUILDING`.
- `combat_apply_hit()` is the active troop damage path used by `entity_update()`.
- `combat_resolve()` still exists as a legacy cooldown-based helper, but the main troop loop does not use it.

## Player/Energy Systems

- `player_init()` sets camera state from the canonical territory bounds and copies the three spawn anchors into `slots[]`.
- `energy_init()` sets each player to:
  - `maxEnergy = 10.0`
  - `energy = 10.0`
  - `energyRegenRate = 1.0`
- `player_update()` only handles energy regen and slot cooldown countdown.

## Backend Gaps

- `play_spell()` only logs its parsed spell data.
- `building_create_base()` is still a stub, so there are no bases on the battlefield.
- `win_condition.c` is unimplemented.
- `match.c` is unimplemented.
- `projectile.c` is unimplemented.
- Slot cooldowns never activate because nothing sets `cooldownTimer > 0`.
