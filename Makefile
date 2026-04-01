.PHONY: clean run preview-run biome-preview-run init-db test test_pathfinding test_battlefield_math

CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lsqlite3 -lraylib -lm
MACFLAGS = -I/opt/homebrew/include -L/opt/homebrew/lib

# Source files
SRC_CORE = src/core/game.c
SRC_DATA = src/data/db.c src/data/cards.c
SRC_RENDERING = src/rendering/card_renderer.c src/rendering/tilemap_renderer.c src/rendering/viewport.c src/rendering/sprite_renderer.c src/rendering/biome.c src/rendering/ui.c
SRC_ENTITIES = src/entities/entities.c src/entities/troop.c src/entities/building.c src/entities/projectile.c
SRC_SYSTEMS = src/systems/player.c src/systems/energy.c src/systems/spawn.c src/systems/match.c
SRC_LOGIC = src/logic/card_effects.c src/logic/combat.c src/logic/pathfinding.c src/logic/win_condition.c
SRC_HARDWARE = src/hardware/nfc_reader.c src/hardware/arduino_protocol.c
SRC_LIB = lib/cJSON.c

SOURCES = $(SRC_CORE) $(SRC_DATA) $(SRC_RENDERING) $(SRC_ENTITIES) $(SRC_SYSTEMS) $(SRC_LOGIC) $(SRC_HARDWARE) $(SRC_LIB)

cardgame: $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o cardgame $(MACFLAGS) $(LDFLAGS)

preview: tools/card_preview.c src/rendering/card_renderer.c lib/cJSON.c
	$(CC) $(CFLAGS) tools/card_preview.c src/rendering/card_renderer.c lib/cJSON.c -o card_preview $(MACFLAGS) -lraylib -lm

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
	$(CC) $(CFLAGS) tools/biome_preview.c src/rendering/tilemap_renderer.c src/rendering/biome.c -o biome_preview $(MACFLAGS) -lraylib -lm

biome-preview-run: biome_preview
	./biome_preview

card_enroll: tools/card_enroll.c src/data/db.c src/data/cards.c src/hardware/nfc_reader.c src/hardware/arduino_protocol.c lib/cJSON.c
	$(CC) $(CFLAGS) tools/card_enroll.c src/data/db.c src/data/cards.c src/hardware/nfc_reader.c src/hardware/arduino_protocol.c lib/cJSON.c -o card_enroll $(MACFLAGS) -lsqlite3 -lm

card-enroll-run: card_enroll
	NFC_PORT="/dev/cu.usbserial-A5069RR4" ./card_enroll

# Test targets
test_pathfinding: tests/test_pathfinding.c src/logic/pathfinding.c
	$(CC) $(CFLAGS) tests/test_pathfinding.c -o test_pathfinding -lm

test_battlefield_math: tests/test_battlefield_math.c src/core/battlefield_math.c
	$(CC) $(CFLAGS) tests/test_battlefield_math.c -o test_battlefield_math -lm

test: test_pathfinding test_battlefield_math
	./test_pathfinding
	./test_battlefield_math

clean:
	rm -f cardgame card_preview biome_preview card_enroll test_pathfinding test_battlefield_math
