.PHONY: clean run preview-run biome-preview-run init-db test test_pathfinding test_combat test_entities test_troop test_projectiles test_battlefield_math test_battlefield test_animation test_debug_events test_spawn_fx test_spawn_placement test_deposit_slots test_nav_frame test_farmer test_status_bars test_win_condition test_sustenance test_hand_ui test_card_effects test_uvulite_font test_progression test_debug_overlay test_game_debug_input sprite-frame-atlas

CC = gcc
CFLAGS = -Wall -Wextra -O2
CPPFLAGS = -Ithird_party/cjson
LDFLAGS = -lsqlite3 -lraylib -lm
MACFLAGS = -I/opt/homebrew/include -L/opt/homebrew/lib

# Source files
SRC_CORE = src/core/game.c src/core/battlefield.c src/core/battlefield_math.c src/core/debug_events.c src/core/sustenance.c
SRC_DATA = src/data/db.c src/data/cards.c
SRC_RENDERING = src/rendering/card_renderer.c src/rendering/tilemap_renderer.c src/rendering/viewport.c src/rendering/sprite_renderer.c src/rendering/spawn_fx.c src/rendering/status_bars.c src/rendering/biome.c src/rendering/ui.c src/rendering/debug_overlay.c src/rendering/debug_overlay_input.c src/rendering/sustenance_renderer.c src/rendering/hand_ui.c src/rendering/uvulite_font.c
SRC_ENTITIES = src/entities/entities.c src/entities/entity_animation.c src/entities/troop.c src/entities/building.c src/entities/projectile.c
SRC_SYSTEMS = src/systems/player.c src/systems/energy.c src/systems/spawn.c src/systems/spawn_placement.c src/systems/match.c src/systems/progression.c
SRC_LOGIC = src/logic/card_effects.c src/logic/combat.c src/logic/deposit_slots.c src/logic/farmer.c src/logic/nav_frame.c src/logic/pathfinding.c src/logic/win_condition.c
SRC_HARDWARE = src/hardware/nfc_reader.c src/hardware/arduino_protocol.c
SRC_LIB = third_party/cjson/cJSON.c

SOURCES = $(SRC_CORE) $(SRC_DATA) $(SRC_RENDERING) $(SRC_ENTITIES) $(SRC_SYSTEMS) $(SRC_LOGIC) $(SRC_HARDWARE) $(SRC_LIB)

cardgame: $(SOURCES)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SOURCES) -o cardgame $(MACFLAGS) $(LDFLAGS)

preview: tools/card_preview.c tools/card_psd_export.c src/rendering/card_renderer.c third_party/cjson/cJSON.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/card_preview.c tools/card_psd_export.c src/rendering/card_renderer.c third_party/cjson/cJSON.c -o card_preview $(MACFLAGS) -lraylib -lm

# Initialize a fresh SQLite database from schema + seed data
init-db:
	sqlite3 cardgame.db < sqlite/schema.sql
	sqlite3 cardgame.db < sqlite/seed.sql
	@echo "cardgame.db initialized"

# Single-Arduino test: NFC_PORT=/dev/ttyACM0 ./cardgame
run: clean cardgame
	NFC_PORT="/dev/cu.usbserial-A5069RR4" NFC_PORT_P1="/dev/ttyACM0" NFC_PORT_P2="/dev/ttyACM1" ./cardgame

preview-run: preview
	./card_preview

biome_preview: tools/biome_preview.c src/rendering/tilemap_renderer.c src/rendering/biome.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/biome_preview.c src/rendering/tilemap_renderer.c src/rendering/biome.c -o biome_preview $(MACFLAGS) -lraylib -lm

biome-preview-run: biome_preview
	./biome_preview

card_enroll: tools/card_enroll.c src/data/db.c src/data/cards.c src/hardware/nfc_reader.c src/hardware/arduino_protocol.c third_party/cjson/cJSON.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tools/card_enroll.c src/data/db.c src/data/cards.c src/hardware/nfc_reader.c src/hardware/arduino_protocol.c third_party/cjson/cJSON.c -o card_enroll $(MACFLAGS) -lsqlite3 -lm

card-enroll-run: card_enroll
	NFC_PORT="/dev/cu.usbserial-A5069RR4" ./card_enroll

# Test targets
test_pathfinding: tests/test_pathfinding.c src/logic/pathfinding.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_pathfinding.c -o test_pathfinding -lm

test_combat: tests/test_combat.c src/logic/combat.c src/entities/building.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_combat.c -o test_combat -lm

test_entities: tests/test_entities.c src/entities/entities.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_entities.c -o test_entities -lm

test_troop: tests/test_troop.c src/entities/troop.c src/entities/building.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_troop.c -o test_troop -lm

test_projectiles: tests/test_projectiles.c src/entities/projectile.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_projectiles.c -o test_projectiles -lm

test_battlefield_math: tests/test_battlefield_math.c src/core/battlefield_math.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_battlefield_math.c -o test_battlefield_math -lm

test_battlefield: tests/test_battlefield.c src/core/battlefield.c src/core/battlefield_math.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_battlefield.c -o test_battlefield -lm

test_animation: tests/test_animation.c src/entities/entity_animation.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_animation.c -o test_animation -lm

test_debug_events: tests/test_debug_events.c src/core/debug_events.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_debug_events.c -o test_debug_events -lm

test_spawn_fx: tests/test_spawn_fx.c src/rendering/spawn_fx.c src/systems/spawn.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_spawn_fx.c -o test_spawn_fx -lm

test_spawn_placement: tests/test_spawn_placement.c src/systems/spawn_placement.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_spawn_placement.c -o test_spawn_placement -lm

test_deposit_slots: tests/test_deposit_slots.c src/logic/deposit_slots.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_deposit_slots.c -o test_deposit_slots -lm

test_nav_frame: tests/test_nav_frame.c src/logic/nav_frame.c src/logic/nav_frame.h
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_nav_frame.c -o test_nav_frame -lm

test_farmer: tests/test_farmer.c src/logic/farmer.c src/logic/deposit_slots.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_farmer.c -o test_farmer -lm

test_status_bars: tests/test_status_bars.c src/rendering/status_bars.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_status_bars.c -o test_status_bars -lm

test_win_condition: tests/test_win_condition.c src/logic/win_condition.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_win_condition.c -o test_win_condition -lm

test_sustenance: tests/test_sustenance.c src/core/sustenance.c src/core/battlefield.c src/core/battlefield_math.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_sustenance.c -o test_sustenance -lm

test_hand_ui: tests/test_hand_ui.c src/rendering/hand_ui.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_hand_ui.c -o test_hand_ui -lm

test_card_effects: tests/test_card_effects.c src/logic/card_effects.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_card_effects.c -o test_card_effects -lm

test_uvulite_font: tests/test_uvulite_font.c src/rendering/uvulite_font.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_uvulite_font.c -o test_uvulite_font -lm

test_progression: tests/test_progression.c src/systems/progression.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_progression.c -o test_progression -lm

test_debug_overlay: tests/test_debug_overlay.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_debug_overlay.c -o test_debug_overlay -lm

test_game_debug_input: tests/test_game_debug_input.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/test_game_debug_input.c -o test_game_debug_input -lm

test: test_pathfinding test_combat test_entities test_troop test_projectiles test_battlefield_math test_battlefield test_animation test_debug_events test_spawn_fx test_spawn_placement test_deposit_slots test_nav_frame test_farmer test_status_bars test_win_condition test_sustenance test_hand_ui test_card_effects test_uvulite_font test_progression test_debug_overlay test_game_debug_input
	./test_pathfinding
	./test_combat
	./test_entities
	./test_troop
	./test_projectiles
	./test_battlefield_math
	./test_battlefield
	./test_animation
	./test_debug_events
	./test_spawn_fx
	./test_spawn_placement
	./test_deposit_slots
	./test_nav_frame
	./test_farmer
	./test_status_bars
	./test_win_condition
	./test_sustenance
	./test_hand_ui
	./test_card_effects
	./test_uvulite_font
	./test_progression
	./test_debug_overlay
	./test_game_debug_input

sprite-frame-atlas:
	python3 tools/generate_sprite_frame_atlas.py

clean:
	rm -f cardgame card_preview biome_preview card_enroll test_pathfinding test_combat test_entities test_troop test_projectiles test_battlefield_math test_battlefield test_animation test_debug_events test_spawn_fx test_spawn_placement test_deposit_slots test_nav_frame test_farmer test_status_bars test_win_condition test_sustenance test_hand_ui test_card_effects test_uvulite_font test_progression test_debug_overlay test_game_debug_input test_ore
