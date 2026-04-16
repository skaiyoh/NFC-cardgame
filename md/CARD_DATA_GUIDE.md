# Card Data Guide

Reference for authoring card entries in the SQLite database.

---

## Database Schema

Cards are stored in the `cards` table with the following columns:

| Column | Type | Required | Description |
|--------|------|----------|-------------|
| `card_id` | TEXT (PK) | Yes | Unique identifier, used to link NFC tags to cards |
| `name` | TEXT | Yes | Display name shown on the card |
| `cost` | INTEGER | Yes | Cost amount to play |
| `cost_resource` | TEXT | Yes | Resource used to pay the cost: `energy` or `sustenance` |
| `type` | TEXT | Yes | Card type — determines which effect handler runs (see [Card Types](#card-types)) |
| `rules_text` | TEXT | No | Flavor or rules text shown in the card description area |
| `data` | TEXT | No | Visual styling and gameplay stats as JSON (see [Data JSON](#data-json)); runtime code falls back to defaults when it is absent |

---

## Card Types

The following types are registered in `src/logic/card_effects.c`. Using an unregistered type will log an unknown-type message and fail when played.

| Type | Behavior |
|------|----------|
| `knight` | Spawns a troop using the shared troop pipeline |
| `healer` | Spawns a troop using the shared troop pipeline; no healer-specific ability yet |
| `assassin` | Spawns a troop using the shared troop pipeline; no assassin-specific ability yet |
| `brute` | Spawns a troop using the shared troop pipeline; gameplay difference currently comes from JSON stats and targeting |
| `farmer` | Spawns a troop using the shared troop pipeline; no farmer-specific ability yet |
| `bird` | Spawns a ranged troop that releases a bomb projectile on attack; the bomb deals enemy-only splash damage on activation |
| `fishfing` | Spawns a troop using the shared troop pipeline; starts from Knight baseline stats |
| `king` | Plays on the owning base and restarts its attack clip; no spawn, animation-only in this pass |

All troop types (`knight`, `healer`, `assassin`, `brute`, `farmer`, `bird`,
`fishfing`) read their stats from the same troop fields in the JSON `data`
column. `king` does not read troop stats — only the `visual` block is required.

`cost_resource` defaults to `energy` in the schema, so older rows can keep
omitting it until they are re-seeded or edited.

Lookups are case-sensitive: `cards_find()` uses `strcmp()` on `card_id`. Keep
`cards.card_id` and `nfc_tags.card_id` casing consistent across your database.

---

## Data JSON

The `data` column is a JSON text object with two sections: `visual` for card appearance, and flat fields for gameplay stats. Parsed at runtime using cJSON.

```json
{
  "visual": { ... },
  "hp": 100,
  "attack": 10,
  ...
}
```

---

## Visual Fields (`data.visual`)

Controls how every layer of the card sprite sheet is rendered. All fields are optional — omitting a field uses the default value.

### Colors

Used by most layer fields. 13 options:

`aqua` `black` `blue` `blue_light` `brown` `gray` `green` `magenta` `pink` `purple` `red` `white` `yellow`

### Background (`bg_style`, `description_style`)

4 options: `black` `brown` `paper` `white`

### Inner Corner Style (`innercorner_style`)

4 options: `black` `brown` `white` `yellow`

### Container Variant (`container_variant`)

Integer `1`, `2`, or `3`.

---

### Full Visual Field Reference

| Field | Type | Default | Notes |
|-------|------|---------|-------|
| `border_color` | color | `"brown"` | Outer card frame color |
| `show_border` | bool | `true` | |
| `bg_style` | bg | `"paper"` | Card art background texture |
| `show_bg` | bool | `true` | |
| `banner_color` | color | `"brown"` | Name banner across the top |
| `show_banner` | bool | `true` | |
| `corner_color` | color | `"brown"` | Four corner pip decorations |
| `show_corner` | bool | `true` | |
| `container_color` | color | `"brown"` | Center art container frame |
| `container_variant` | int | `1` | Frame shape variant (1–3) |
| `show_container` | bool | `false` | |
| `description_style` | bg | `"paper"` | Rules text area background |
| `show_description` | bool | `true` | |
| `innercorner_style` | ic | `"brown"` | Inner corner accent style |
| `show_innercorner` | bool | `true` | |
| `gem_color` | color | `"green"` | Gem jewel at the top center |
| `show_gem` | bool | `true` | |
| `socket_color` | color | `"brown"` | Socket around the gem |
| `show_socket` | bool | `true` | |
| `energy_top_color` | color | `"red"` | Top energy pip |
| `show_energy_top` | bool | `false` | |
| `energy_bot_color` | color | `"red"` | Bottom energy pip |
| `show_energy_bot` | bool | `false` | |

### Layer Offsets

Each layer can be nudged in pixel space using an `[x, y]` array. All default to `[0, 0]`.

| Key | Layer |
|-----|-------|
| `off_bg` | Background |
| `off_description` | Description box |
| `off_border` | Border frame |
| `off_banner` | Name banner |
| `off_innercorner` | Inner corner accents |
| `off_corner` | Corner pips |
| `off_container` | Art container |
| `off_socket` | Gem socket |
| `off_gem` | Gem jewel |
| `off_energy_top` | Top energy pip |
| `off_energy_bot` | Bottom energy pip |

---

## Gameplay Fields — Troop Cards

Applies to all troop types: `knight`, `healer`, `assassin`, `brute`, `farmer`.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `hp` | int | `100` | Starting health. Also sets `maxHP` if `maxHP` is omitted |
| `maxHP` | int | `hp` | Maximum health (only needed if different from `hp`) |
| `attack` | int | `10` | Damage dealt per hit |
| `healAmount` | int | `0` | HP restored per hit when the target is a friendly troop. Any value > 0 turns the unit into a supporter that prefers injured allies in range over enemies. For `type: "healer"` cards, omitting this field falls back to the `attack` value so older databases stay functional. |
| `attackSpeed` | float | `1.0` | Attacks per second |
| `attackRange` | float | `40.0` | Melee range in pixels |
| `moveSpeed` | float | `60.0` | Movement speed in pixels per second |
| `targeting` | string | `"nearest"` | `"nearest"`, `"building"`, or `"specific"` |
| `targetType` | string | `null` | Parsed and stored when `targeting` is `"specific"`, but current combat code still falls back to nearest-target behavior |

---

## SQL Insert

```sql
INSERT INTO cards (card_id, name, cost, cost_resource, type, rules_text, data)
VALUES (
  'KNIGHT_01',
  'Knight',
  4,
  'energy',
  'knight',
  'A heavily armored warrior that charges toward the nearest enemy.',
  '{
    "visual": {
      "border_color": "gray",
      "show_border": true,
      "bg_style": "paper",
      "show_bg": true,
      "banner_color": "blue",
      "show_banner": true,
      "corner_color": "yellow",
      "show_corner": true,
      "container_color": "gray",
      "container_variant": 1,
      "show_container": false,
      "description_style": "paper",
      "show_description": true,
      "innercorner_style": "yellow",
      "show_innercorner": true,
      "gem_color": "blue",
      "show_gem": true,
      "socket_color": "gray",
      "show_socket": true,
      "energy_top_color": "blue",
      "show_energy_top": false,
      "energy_bot_color": "blue",
      "show_energy_bot": false
    },
    "hp": 220,
    "maxHP": 220,
    "attack": 28,
    "attackSpeed": 0.75,
    "attackRange": 50.0,
    "moveSpeed": 38.0,
    "targeting": "nearest",
    "targetType": null
  }'
);
```

---

## Example Cards

### Knight — Heavy melee troop

```json
{
  "visual": {
    "border_color": "gray",
    "bg_style": "paper",
    "banner_color": "blue",
    "corner_color": "yellow",
    "innercorner_style": "yellow",
    "gem_color": "blue",
    "socket_color": "gray"
  },
  "hp": 220,
  "attack": 28,
  "attackSpeed": 0.75,
  "attackRange": 50.0,
  "moveSpeed": 38.0,
  "targeting": "nearest"
}
```

### Assassin — Fast, fragile, targets troops

```json
{
  "visual": {
    "border_color": "purple",
    "bg_style": "black",
    "banner_color": "purple",
    "corner_color": "black",
    "innercorner_style": "black",
    "gem_color": "magenta",
    "socket_color": "purple"
  },
  "hp": 80,
  "attack": 40,
  "attackSpeed": 1.8,
  "attackRange": 35.0,
  "moveSpeed": 95.0,
  "targeting": "nearest"
}
```

### Brute — Late-game sustenance spender

```json
{
  "cost_resource": "sustenance",
  "hp": 380,
  "attack": 55,
  "attackSpeed": 0.5,
  "attackRange": 48.0,
  "moveSpeed": 28.0,
  "targeting": "building"
}
```

### Healer — Slow support troop

```json
{
  "visual": {
    "border_color": "white",
    "bg_style": "paper",
    "banner_color": "green",
    "corner_color": "white",
    "innercorner_style": "white",
    "gem_color": "green",
    "socket_color": "white"
  },
  "hp": 120,
  "attack": 5,
  "attackSpeed": 0.5,
  "attackRange": 80.0,
  "moveSpeed": 45.0,
  "targeting": "nearest"
}
```

### Brute — Targets buildings only

```json
{
  "visual": {
    "border_color": "red",
    "bg_style": "brown",
    "banner_color": "red",
    "corner_color": "brown",
    "innercorner_style": "brown",
    "gem_color": "red",
    "socket_color": "brown"
  },
  "hp": 300,
  "attack": 50,
  "attackSpeed": 0.5,
  "attackRange": 45.0,
  "moveSpeed": 30.0,
  "targeting": "building"
}
```

---

## Quick Reference

### Visual color options
`aqua` `black` `blue` `blue_light` `brown` `gray` `green` `magenta` `pink` `purple` `red` `white` `yellow`

### Background / description style options
`black` `brown` `paper` `white`

### Inner corner style options
`black` `brown` `white` `yellow`

### Container variant options
`1` `2` `3`

### Targeting options
`nearest` `building` `specific` (+ `targetType` field)

### Registered card types
`assassin` `bird` `brute` `farmer` `fishfing` `healer` `king` `knight`
