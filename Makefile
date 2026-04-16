.PHONY: clean run init-db

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

# Initialize a fresh SQLite database from schema + seed data
init-db:
	sqlite3 cardgame.db < sqlite/schema.sql
	sqlite3 cardgame.db < sqlite/seed.sql
	@echo "cardgame.db initialized"

run: clean cardgame
	@if [ -z "$(NFC_PORT_P1)" ] || [ -z "$(NFC_PORT_P2)" ]; then \
		echo "Set NFC_PORT_P1 and NFC_PORT_P2 before running 'make run'."; \
		exit 1; \
	fi
	NFC_PORT_P1="$(NFC_PORT_P1)" NFC_PORT_P2="$(NFC_PORT_P2)" ./cardgame

clean:
	rm -f cardgame card_preview biome_preview card_enroll test_pathfinding test_combat test_entities test_troop test_projectiles test_battlefield_math test_battlefield test_animation test_debug_events test_spawn_fx test_spawn_placement test_deposit_slots test_nav_frame test_farmer test_status_bars test_win_condition test_sustenance test_hand_ui test_card_effects test_uvulite_font test_progression test_player test_debug_overlay test_game_debug_input test_ore
