cmake_minimum_required(VERSION 3.20)
project(NFC_cardgame C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Compiler warnings
add_compile_options(-Wall -Wextra)

# Include paths
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cjson)

# --- Libraries ---
find_package(SQLite3 REQUIRED)
find_package(PkgConfig QUIET)

set(RAYLIB_TARGET "")
if(PkgConfig_FOUND)
    pkg_check_modules(RAYLIB QUIET IMPORTED_TARGET raylib)
endif()

if(TARGET PkgConfig::RAYLIB)
    set(RAYLIB_TARGET PkgConfig::RAYLIB)
    message(STATUS "Found raylib via pkg-config")
else()
    find_path(RAYLIB_INCLUDE_DIR
        NAMES raylib.h
        HINTS /opt/homebrew/include /usr/local/include
        REQUIRED)
    find_library(RAYLIB_LIBRARY
        NAMES raylib
        HINTS /opt/homebrew/lib /usr/local/lib
        REQUIRED)

    add_library(raylib::raylib UNKNOWN IMPORTED)
    set_target_properties(raylib::raylib PROPERTIES
        IMPORTED_LOCATION "${RAYLIB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${RAYLIB_INCLUDE_DIR}")
    set(RAYLIB_TARGET raylib::raylib)
    message(STATUS "Found raylib via manual search: ${RAYLIB_LIBRARY}")
endif()

if(UNIX AND NOT APPLE)
    find_library(MATH_LIB NAMES m REQUIRED)
else()
    set(MATH_LIB "")
endif()

find_program(SQLITE3_EXECUTABLE NAMES sqlite3)

set(CARDGAME_DB_PATH "${CMAKE_SOURCE_DIR}/cardgame.db" CACHE FILEPATH
    "DB_PATH passed to the run targets")
set(NFC_PORT "" CACHE STRING
    "Single-Arduino serial port used by run targets, for example /dev/ttyACM0")
set(NFC_PORT_P1 "" CACHE STRING
    "Player 1 serial port used by run targets, for example /dev/ttyACM0")
set(NFC_PORT_P2 "" CACHE STRING
    "Player 2 serial port used by run targets, for example /dev/ttyACM1")

function(cardgame_validate_port port_var)
    if(NOT "${${port_var}}" STREQUAL "" AND NOT EXISTS "${${port_var}}")
        message(WARNING "${port_var}='${${port_var}}' does not exist on this machine.")
    endif()
endfunction()

cardgame_validate_port(NFC_PORT)
cardgame_validate_port(NFC_PORT_P1)
cardgame_validate_port(NFC_PORT_P2)

if(NOT "${NFC_PORT}" STREQUAL "" AND
   (NOT "${NFC_PORT_P1}" STREQUAL "" OR NOT "${NFC_PORT_P2}" STREQUAL ""))
    message(WARNING
        "NFC_PORT is set. The runtime prefers single-Arduino mode and will ignore NFC_PORT_P1/NFC_PORT_P2.")
endif()

if(("${NFC_PORT_P1}" STREQUAL "" AND NOT "${NFC_PORT_P2}" STREQUAL "") OR
   (NOT "${NFC_PORT_P1}" STREQUAL "" AND "${NFC_PORT_P2}" STREQUAL ""))
    message(WARNING "Set both NFC_PORT_P1 and NFC_PORT_P2 for dual-Arduino mode.")
endif()

message(STATUS "Configured run DB path: ${CARDGAME_DB_PATH}")
message(STATUS "Configured run ports: NFC_PORT='${NFC_PORT}', NFC_PORT_P1='${NFC_PORT_P1}', NFC_PORT_P2='${NFC_PORT_P2}'")

function(cardgame_link_math target)
    if(MATH_LIB)
        target_link_libraries(${target} PRIVATE ${MATH_LIB})
    endif()
endfunction()

function(cardgame_add_run_target target_name executable)
    set(env_args "DB_PATH=${CARDGAME_DB_PATH}")

    if(NOT "${NFC_PORT}" STREQUAL "")
        list(APPEND env_args "NFC_PORT=${NFC_PORT}")
    endif()
    if(NOT "${NFC_PORT_P1}" STREQUAL "")
        list(APPEND env_args "NFC_PORT_P1=${NFC_PORT_P1}")
    endif()
    if(NOT "${NFC_PORT_P2}" STREQUAL "")
        list(APPEND env_args "NFC_PORT_P2=${NFC_PORT_P2}")
    endif()

    add_custom_target(${target_name}
        COMMAND ${CMAKE_COMMAND} -E env ${env_args} $<TARGET_FILE:${executable}>
        DEPENDS ${executable}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        USES_TERMINAL)
endfunction()

# --- Source file groups ---
set(SRC_CORE       src/core/game.c src/core/battlefield.c src/core/battlefield_math.c src/core/debug_events.c src/core/sustenance.c)
set(SRC_DATA       src/data/db.c src/data/cards.c)
set(SRC_RENDERING  src/rendering/card_renderer.c
                   src/rendering/tilemap_renderer.c
                   src/rendering/viewport.c
                   src/rendering/sprite_renderer.c
                   src/rendering/spawn_fx.c
                   src/rendering/status_bars.c
                   src/rendering/biome.c
                   src/rendering/ui.c
                   src/rendering/debug_overlay.c
                   src/rendering/debug_overlay_input.c
                   src/rendering/sustenance_renderer.c
                   src/rendering/hand_ui.c
                   src/rendering/uvulite_font.c)
set(SRC_ENTITIES   src/entities/entities.c
                   src/entities/entity_animation.c
                   src/entities/troop.c
                   src/entities/building.c
                   src/entities/projectile.c)
set(SRC_SYSTEMS    src/systems/player.c
                   src/systems/energy.c
                   src/systems/spawn.c
                   src/systems/spawn_placement.c
                   src/systems/match.c
                   src/systems/progression.c)
set(SRC_LOGIC      src/logic/card_effects.c
                   src/logic/combat.c
                   src/logic/deposit_slots.c
                   src/logic/farmer.c
                   src/logic/nav_frame.c
                   src/logic/pathfinding.c
                   src/logic/win_condition.c)
set(SRC_HARDWARE   src/hardware/nfc_reader.c
                   src/hardware/arduino_protocol.c)
set(SRC_LIB        third_party/cjson/cJSON.c)

set(ALL_SOURCES
    ${SRC_CORE} ${SRC_DATA} ${SRC_RENDERING} ${SRC_ENTITIES}
    ${SRC_SYSTEMS} ${SRC_LOGIC} ${SRC_HARDWARE} ${SRC_LIB})

# --- Main game executable ---
add_executable(cardgame ${ALL_SOURCES})
target_link_libraries(cardgame PRIVATE SQLite::SQLite3 ${RAYLIB_TARGET})
cardgame_link_math(cardgame)

# --- Tools ---
add_executable(card_preview
    tools/card_preview.c
    tools/card_psd_export.c
    src/rendering/card_renderer.c
    third_party/cjson/cJSON.c)
target_link_libraries(card_preview PRIVATE ${RAYLIB_TARGET})
cardgame_link_math(card_preview)

add_executable(biome_preview
    tools/biome_preview.c
    src/rendering/tilemap_renderer.c
    src/rendering/biome.c)
target_link_libraries(biome_preview PRIVATE ${RAYLIB_TARGET})
cardgame_link_math(biome_preview)

add_executable(card_enroll
    tools/card_enroll.c
    src/data/db.c
    src/data/cards.c
    src/hardware/nfc_reader.c
    src/hardware/arduino_protocol.c
    third_party/cjson/cJSON.c)
target_link_libraries(card_enroll PRIVATE SQLite::SQLite3)
cardgame_link_math(card_enroll)

cardgame_add_run_target(run-cardgame cardgame)
cardgame_add_run_target(run-card-enroll card_enroll)

# --- Tests ---
include(CTest)

if(BUILD_TESTING)
    add_executable(test_pathfinding
        tests/test_pathfinding.c)
    cardgame_link_math(test_pathfinding)
    add_test(NAME pathfinding COMMAND test_pathfinding)

    add_executable(test_battlefield_math
        tests/test_battlefield_math.c)
    cardgame_link_math(test_battlefield_math)
    add_test(NAME battlefield_math COMMAND test_battlefield_math)

    add_executable(test_battlefield
        tests/test_battlefield.c)
    cardgame_link_math(test_battlefield)
    add_test(NAME battlefield COMMAND test_battlefield)

    add_executable(test_combat
        tests/test_combat.c)
    cardgame_link_math(test_combat)
    add_test(NAME combat COMMAND test_combat)

    add_executable(test_entities
        tests/test_entities.c)
    cardgame_link_math(test_entities)
    add_test(NAME entities COMMAND test_entities)

    add_executable(test_troop
        tests/test_troop.c)
    cardgame_link_math(test_troop)
    add_test(NAME troop COMMAND test_troop)

    add_executable(test_projectiles
        tests/test_projectiles.c)
    cardgame_link_math(test_projectiles)
    add_test(NAME projectiles COMMAND test_projectiles)

    add_executable(test_animation
        tests/test_animation.c)
    cardgame_link_math(test_animation)
    add_test(NAME animation COMMAND test_animation)

    add_executable(test_debug_events
        tests/test_debug_events.c)
    cardgame_link_math(test_debug_events)
    add_test(NAME debug_events COMMAND test_debug_events)

    add_executable(test_spawn_fx
        tests/test_spawn_fx.c)
    cardgame_link_math(test_spawn_fx)
    add_test(NAME spawn_fx COMMAND test_spawn_fx)

    add_executable(test_spawn_placement
        tests/test_spawn_placement.c)
    cardgame_link_math(test_spawn_placement)
    add_test(NAME spawn_placement COMMAND test_spawn_placement)

    add_executable(test_deposit_slots
        tests/test_deposit_slots.c)
    cardgame_link_math(test_deposit_slots)
    add_test(NAME deposit_slots COMMAND test_deposit_slots)

    add_executable(test_nav_frame
        tests/test_nav_frame.c)
    cardgame_link_math(test_nav_frame)
    add_test(NAME nav_frame COMMAND test_nav_frame)

    add_executable(test_status_bars
        tests/test_status_bars.c)
    cardgame_link_math(test_status_bars)
    add_test(NAME status_bars COMMAND test_status_bars)

    add_executable(test_win_condition
        tests/test_win_condition.c)
    cardgame_link_math(test_win_condition)
    add_test(NAME win_condition COMMAND test_win_condition)

    add_executable(test_sustenance
        tests/test_sustenance.c)
    cardgame_link_math(test_sustenance)
    add_test(NAME sustenance COMMAND test_sustenance)

    add_executable(test_hand_ui
        tests/test_hand_ui.c)
    cardgame_link_math(test_hand_ui)
    add_test(NAME hand_ui COMMAND test_hand_ui)

    add_executable(test_uvulite_font
        tests/test_uvulite_font.c)
    cardgame_link_math(test_uvulite_font)
    add_test(NAME uvulite_font COMMAND test_uvulite_font)

    add_executable(test_card_effects
        tests/test_card_effects.c)
    cardgame_link_math(test_card_effects)
    add_test(NAME card_effects COMMAND test_card_effects)

    add_executable(test_progression
        tests/test_progression.c)
    cardgame_link_math(test_progression)
    add_test(NAME progression COMMAND test_progression)

    add_executable(test_player
        tests/test_player.c)
    cardgame_link_math(test_player)
    add_test(NAME player COMMAND test_player)

    add_executable(test_debug_overlay
        tests/test_debug_overlay.c)
    cardgame_link_math(test_debug_overlay)
    add_test(NAME debug_overlay COMMAND test_debug_overlay)

    add_executable(test_game_debug_input
        tests/test_game_debug_input.c)
    cardgame_link_math(test_game_debug_input)
    add_test(NAME game_debug_input COMMAND test_game_debug_input)
endif()

# --- Database init (convenience target) ---
if(SQLITE3_EXECUTABLE)
    add_custom_target(init-db
        COMMAND ${SQLITE3_EXECUTABLE} ${CARDGAME_DB_PATH}
                ".read ${CMAKE_SOURCE_DIR}/sqlite/schema.sql"
                ".read ${CMAKE_SOURCE_DIR}/sqlite/seed.sql"
        COMMENT "Initializing ${CARDGAME_DB_PATH} from schema + seed"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
else()
    message(WARNING "sqlite3 CLI not found; init-db target will not be generated.")
endif()
