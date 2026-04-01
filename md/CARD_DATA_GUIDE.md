# Card Data Guide

Reference for authoring card entries in the SQLite database.

---

## Database Schema

Cards are stored in the `cards` table with the following columns:

| Column | Type | Required | Description |
|--------|------|----------|-------------|
| `card_id` | TEXT (PK) | Yes | Unique identifier, used to link NFC tags to cards |
| `name` | TEXT | Yes | Display name shown on the card |
| `cost` | INTEGER | Yes | Energy cost to play |
| `type` | TEXT | Yes | Card type — determines which effect handler runs (see [Card Types](#card-types)) |
| `rules_text` | TEXT | No | Flavor or rules text shown in the card description area |
| `data` | TEXT | Yes | All visual styling and gameplay stats as JSON (see [Data JSON](#data-json)) |

---

## Card Types

The following types are registered in `src/logic/card_effects.c`. Using an unregistered type will log an unknown-type message and fail when played.

| Type | Behavior |
|------|----------|
| `knight` | Spawns a melee troop |
| `healer` | Spawns a healer troop |
| `assassin` | Spawns an assassin troop |
| `brute` | Spawns a brute troop |
| `farmer` | Spawns a farmer troop |
| `spell` | Applies a spell effect (damage, element, targets) |

All troop types (`knight`, `healer`, `assassin`, `brute`, `farmer`) read their stats from the same troop fields in the JSON `data` column.

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
| `attackSpeed` | float | `1.0` | Attacks per second |
| `attackRange` | float | `40.0` | Melee range in pixels |
| `moveSpeed` | float | `60.0` | Movement speed in pixels per second |
| `targeting` | string | `"nearest"` | `"nearest"`, `"building"`, or `"specific"` |
| `targetType` | string | `null` | Entity type string — only used when `targeting` is `"specific"` |

---

## Gameplay Fields — Spell Cards

Applies to `type: "spell"`. Note: spell logic is currently print-only (stubbed).

| Field | Type | Description |
|-------|------|-------------|
| `damage` | int | Damage amount |
| `element` | string | Element type (e.g. `"fire"`, `"ice"`) |
| `targets` | array of strings | Target categories (e.g. `["troops", "buildings"]`) |

---

## SQL Insert

```sql
INSERT INTO cards (card_id, name, cost, type, rules_text, data)
VALUES (
  'knight_01',
  'Knight',
  4,
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

### Fireball — Spell card

```json
{
  "visual": {
    "border_color": "red",
    "bg_style": "black",
    "banner_color": "red",
    "gem_color": "red",
    "show_energy_top": true,
    "energy_top_color": "red"
  },
  "damage": 120,
  "element": "fire",
  "targets": ["troops", "buildings"]
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
`knight` `healer` `assassin` `brute` `farmer` `spell`
