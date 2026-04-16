# NFC Card Game - TODO

Last verified: 2026-04-13

## Current Baseline

- `make test` passes all 15 standalone test executables.
- `make cardgame` is build-valid in the current environment.
- The current character-only card set is:
  - `knight`
  - `healer`
  - `assassin`
  - `farmer`
  - `brute`
  - `fishfing`
  - `bird`
  - `king`
- Demo hands and keyboard smoke paths already expose all 8 cards.
- In-game hand UI exists, but some cards still reuse placeholder rows from the shared `card_sheet.png`.
- Bases now render with King art, and `KING_01` triggers the owning base attack animation only.
- Sustenance placement, claiming, respawn, rendering, and farmer delivery all exist and are test-covered.
- Hurt sprite sheets are loaded for every current character/base set, but runtime damage feedback still does not use a true hurt-state path.

## Highest Priority

- [ ] Finalize walking and attacking art for every current character kit:
  - Knight
  - Healer
  - Assassin
  - Brute
  - Farmer
  - Bird
  - Fishfing
  - King/Base
- [ ] Add real hurt reactions/effects for all damageable entities instead of relying mostly on hit-flash style feedback.
- [ ] Finalize sustenance art and presentation so resources no longer read as placeholder blobs.
- [ ] Complete unique gameplay logic for each character so the roster stops behaving like stat-only variants of the shared troop pipeline.
- [ ] Review clearly unreferenced asset files and decide what should be kept, moved, or deleted.

## Character Backlog

### Knight

- Current state: baseline melee troop using the shared spawn/combat pipeline.
- [ ] Finalize walk and sword-attack sheets and verify attack timing still matches the current hit marker.
- [ ] Decide whether Knight stays the plain control/baseline melee unit or gains one small identity hook.
- [ ] Add/verify Knight-specific hurt reaction timing and screen readability.
- [ ] Smoke check: spawn from the demo hand and verify idle, walk, attack, hurt, death, and hand-card row all read cleanly in both viewports.

### Healer

- Current state: already has heal-first combat targeting, but presentation and gameplay identity are still thin.
- [ ] Finalize walk and staff-attack art and verify the hit/heal moment reads correctly.
- [ ] Decide the final healer gameplay hook beyond raw heal-on-hit stats if needed.
- [ ] Add healer-specific hurt reaction polish so support units are visually distinct under pressure.
- [ ] Smoke check: verify ally-heal priority, fallback-to-enemy behavior, and hurt readability in live play.

### Assassin

- Current state: shared troop pipeline only; no unique assassin logic yet.
- [ ] Finalize walk and attack sheets for fast melee readability.
- [ ] Implement assassin-specific gameplay behavior instead of leaving it as a stat variant.
- [ ] Add a hurt reaction that fits a fast/light unit without losing readability.
- [ ] Smoke check: verify the assassin reads differently from Knight in motion, attack cadence, and damage response.

### Brute

- Current state: gameplay difference is mostly stats/targeting; current attack art is still wired to `block.png`.
- [ ] Replace or validate `block.png` as the final brute attack animation.
- [ ] Finalize brute walk/attack readability so it feels heavy and deliberate.
- [ ] Add brute-specific logic if its final design needs more than building-priority targeting and heavier stats.
- [ ] Add a hurt reaction that sells impact without making the unit unreadable.
- [ ] Smoke check: verify targeting, attack timing, and animation readability against bases and troops.

### Farmer

- Current state: has real sustenance seek/gather/return/deposit logic, but still reuses attack clips as work/deposit animation.
- [ ] Finalize farmer walk and attack/work art so harvesting no longer reads like placeholder combat.
- [ ] Decide whether Farmer needs separate work/deposit animation assets or a dedicated work state.
- [ ] Add proper hurt feedback while preserving gather/return readability.
- [ ] Smoke check: verify full gather loop, deposit loop, death while carrying sustenance, and hurt readability during non-combat states.

### Bird

- Current state: ranged Bird now releases a bomb projectile from its beak, and the bomb deals enemy-only splash damage on activation.
- [ ] Finalize walk and attack art and make sure the movement language fits the intended Bird role.
- [ ] Tune Bird bomb splash radius, speed, and release timing against live gameplay.
- [ ] Add a hurt reaction that still reads clearly at Bird scale/speed.
- [ ] Replace temporary hand-card row reuse once final card-sheet art exists.
- [ ] Smoke check: verify Bird feels visually and mechanically distinct from Knight/Assassin.

### Fishfing

- Current state: shared troop pipeline only; no unique Fishfing logic yet.
- [ ] Finalize walk and attack art and make sure its silhouette/motion reads clearly on the battlefield.
- [ ] Implement Fishfing-specific gameplay behavior instead of leaving it as a stat clone.
- [ ] Add a hurt reaction that supports its final identity.
- [ ] Replace temporary hand-card row reuse once final card-sheet art exists.
- [ ] Smoke check: verify Fishfing reads differently from the rest of the melee roster in motion and on hit.

### King / Base

- Current state: bases use King art from match start; `KING_01` only triggers a base attack animation and does not spawn a unit.
- [ ] Finalize King walk/attack/hurt presentation as the base/boss art set.
- [ ] Decide the final gameplay effect for `KING_01` beyond animation-only feedback.
- [ ] Add proper hurt feedback for the base itself, not just death/attack/idle transitions.
- [ ] Replace temporary hand-card row reuse once final King hand art exists.
- [ ] Smoke check: verify base idle, attack trigger, hurt reaction, death, and card activation all read cleanly.

## Shared Combat / Presentation Work

- [ ] Add a real damage-response path that can drive `ANIM_HURT` without breaking attack chaining, farmer work loops, or base attack playback.
- [ ] Decide whether hurt feedback should be:
  - animation-only
  - animation + flash
  - animation + FX burst
- [ ] Make hurt behavior consistent across troops and bases.
- [ ] Add focused tests or smoke coverage for “damage taken but not dead” presentation behavior.
- [ ] Review attack marker timing after any final attack-sheet replacement so gameplay timing still matches the visuals.

## Sustenance Art

- Current state: sustenance gameplay logic is implemented, but rendering still uses one placeholder-style texture from `uvulite_blob.png`.
- [ ] Replace the current sustenance blob with final production art.
- [ ] Verify the final sustenance art reads clearly at current world scale and in both player orientations.
- [ ] Decide whether sustenance should stay as one visual type or expand later through `sustenanceType`.
- [ ] If multiple sustenance looks are desired later, add that only after the single final baseline asset is stable.
- [ ] Smoke check: verify gather readability, resource visibility against each biome, and no clipping/confusion near lanes/base anchors.

## Art / Asset Review

- Review before delete. Do not remove files blindly just because they are currently unreferenced.
- Candidate unreferenced assets to review:
  - `src/assets/cards/assassin_card.png`
  - `src/assets/cards/uvulite_card.png`
  - `src/assets/cards/uvulite_card_sheet.png`
  - `src/assets/characters/Base/color palette.png`
  - `src/assets/environment/Objects/uvulite_lettering.png`
  - `src/assets/environment/Pixel Art Top Down - Basic v1.2.3/Scene Overview.png`
- Keep these in place unless intentionally replaced:
  - `src/assets/cards/ModularCardsRPG/modularCardsRPGSheet.png`
  - `src/assets/cards/card_sheet.png`
  - `src/assets/environment/Objects/health_energy_bars.png`
  - `src/assets/environment/Objects/uvulite_blob.png`
  - current live character sprite sheets
- [ ] After final art is chosen, do one cleanup pass that removes only confirmed-dead assets.

## Secondary Game Gaps

- [ ] Implement final character-specific gameplay for the roster that still behaves as shared-pipeline placeholders.
- [ ] Finish `TARGET_SPECIFIC_TYPE` if any character design depends on species/class targeting.
- [ ] Decide whether slot cooldowns should become real gameplay or be removed as dead complexity.
- [ ] Finalize in-game hand art so Bird/Fishfing/King stop reusing placeholder shared-sheet rows.
- [ ] Polish status-bar readability and final HUD art once the character and sustenance visuals are stable.
- [ ] Replace placeholder biome identity if visual differentiation between territories matters for the final presentation.
- [ ] Implement projectiles only if a finalized character design actually requires ranged entities/effects.
- [ ] Add pregame / match-flow work only after the core character roster, art, and gameplay loop feel final.
