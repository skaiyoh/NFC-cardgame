cmake_minimum_required(VERSION 3.20)
project(NFC_cardgame C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler warnings
add_compile_options(-Wall -Wextra)

# Include paths
include_directories(${CMAKE_SOURCE_DIR})

# --- Libraries ---
find_library(RAYLIB_LIB raylib)
find_library(SQLITE3_LIB sqlite3)
find_library(MATH_LIB m)

# macOS Homebrew paths
if(APPLE)
    include_directories(/opt/homebrew/include)
    link_directories(/opt/homebrew/lib)
endif()

# --- Source file groups ---
set(SRC_CORE       src/core/game.c src/core/battlefield.c src/core/battlefield_math.c src/core/debug_events.c)
set(SRC_DATA       src/data/db.c src/data/cards.c)
set(SRC_RENDERING  src/rendering/card_renderer.c
                   src/rendering/tilemap_renderer.c
                   src/rendering/viewport.c
                   src/rendering/sprite_renderer.c
                   src/rendering/biome.c
                   src/rendering/ui.c
                   src/rendering/debug_overlay.c)
set(SRC_ENTITIES   src/entities/entities.c
                   src/entities/entity_animation.c
                   src/entities/troop.c
                   src/entities/building.c
                   src/entities/projectile.c)
set(SRC_SYSTEMS    src/systems/player.c
                   src/systems/energy.c
                   src/systems/spawn.c
                   src/systems/match.c)
set(SRC_LOGIC      src/logic/card_effects.c
                   src/logic/combat.c
                   src/logic/pathfinding.c
                   src/logic/win_condition.c)
set(SRC_HARDWARE   src/hardware/nfc_reader.c
                   src/hardware/arduino_protocol.c)
set(SRC_LIB        lib/cJSON.c)

set(ALL_SOURCES
    ${SRC_CORE} ${SRC_DATA} ${SRC_RENDERING} ${SRC_ENTITIES}
    ${SRC_SYSTEMS} ${SRC_LOGIC} ${SRC_HARDWARE} ${SRC_LIB})

# --- Main game executable ---
add_executable(cardgame ${ALL_SOURCES})
target_link_libraries(cardgame ${SQLITE3_LIB} ${RAYLIB_LIB} ${MATH_LIB})

# --- Tools ---
add_executable(card_preview
    tools/card_preview.c
    src/rendering/card_renderer.c
    lib/cJSON.c)
target_link_libraries(card_preview ${RAYLIB_LIB} ${MATH_LIB})

add_executable(biome_preview
    tools/biome_preview.c
    src/rendering/tilemap_renderer.c
    src/rendering/biome.c)
target_link_libraries(biome_preview ${RAYLIB_LIB} ${MATH_LIB})

add_executable(card_enroll
    tools/card_enroll.c
    src/data/db.c
    src/data/cards.c
    src/hardware/nfc_reader.c
    src/hardware/arduino_protocol.c
    lib/cJSON.c)
target_link_libraries(card_enroll ${SQLITE3_LIB} ${MATH_LIB})

# --- Tests ---
enable_testing()

add_executable(test_pathfinding
    tests/test_pathfinding.c)
target_link_libraries(test_pathfinding ${MATH_LIB})
add_test(NAME pathfinding COMMAND test_pathfinding)

add_executable(test_battlefield_math
    tests/test_battlefield_math.c)
target_link_libraries(test_battlefield_math ${MATH_LIB})
add_test(NAME battlefield_math COMMAND test_battlefield_math)

add_executable(test_battlefield
    tests/test_battlefield.c)
target_link_libraries(test_battlefield ${MATH_LIB})
add_test(NAME battlefield COMMAND test_battlefield)

add_executable(test_combat
    tests/test_combat.c)
target_link_libraries(test_combat ${MATH_LIB})
add_test(NAME combat COMMAND test_combat)

add_executable(test_animation
    tests/test_animation.c)
target_link_libraries(test_animation ${MATH_LIB})
add_test(NAME animation COMMAND test_animation)

add_executable(test_debug_events
    tests/test_debug_events.c)
target_link_libraries(test_debug_events ${MATH_LIB})
add_test(NAME debug_events COMMAND test_debug_events)

# --- Database init (convenience target) ---
add_custom_target(init-db
    COMMAND sqlite3 ${CMAKE_SOURCE_DIR}/cardgame.db < ${CMAKE_SOURCE_DIR}/sqlite/schema.sql
    COMMAND sqlite3 ${CMAKE_SOURCE_DIR}/cardgame.db < ${CMAKE_SOURCE_DIR}/sqlite/seed.sql
    COMMENT "Initializing cardgame.db from schema + seed"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
