.PHONY: clean run init-db

CC = gcc
CFLAGS = -Wall -Wextra -O2
CPPFLAGS = -Ithird_party/cjson
LDFLAGS = -lsqlite3 -lraylib -lm
MACFLAGS = -I/opt/homebrew/include -L/opt/homebrew/lib

# Source files
SRC_CORE = src/core/game.c src/core/battlefield.c src/core/battlefield_math.c src/core/debug_events.c src/core/sustenance.c
SRC_DATA = src/data/db.c src/data/cards.c
SRC_RENDERING = src/rendering/card_renderer.c src/rendering/tilemap_renderer.c src/rendering/viewport.c src/rendering/sprite_renderer.c src/rendering/spawn_fx.c src/rendering/biome.c src/rendering/ui.c src/rendering/debug_overlay.c src/rendering/sustenance_renderer.c
SRC_ENTITIES = src/entities/entities.c src/entities/entity_animation.c src/entities/troop.c src/entities/building.c src/entities/projectile.c
SRC_SYSTEMS = src/systems/player.c src/systems/energy.c src/systems/spawn.c src/systems/match.c
SRC_LOGIC = src/logic/card_effects.c src/logic/combat.c src/logic/farmer.c src/logic/pathfinding.c src/logic/win_condition.c
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

# Single-Arduino test: NFC_PORT=/dev/ttyACM0 ./cardgame
run: clean cardgame
	NFC_PORT="/dev/cu.usbserial-A5069RR4" NFC_PORT_P1="/dev/ttyACM0" NFC_PORT_P2="/dev/ttyACM1" ./cardgame

clean:
	rm -f cardgame card_preview biome_preview card_enroll test_pathfinding test_combat test_battlefield_math test_battlefield test_animation test_debug_events test_spawn_fx test_win_condition test_sustenance test_ore
