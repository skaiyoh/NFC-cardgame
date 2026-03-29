//
// Created by Nathan Davis on 2/16/26.
//

#include "match.h"
#include "../core/types.h"
#include "../hardware/nfc_reader.h"

// TODO: The pregame / match phase system is completely unimplemented — only forward declarations
// TODO: exist here. The game currently skips directly into gameplay with no lobby or ready-check.
// TODO: Implement:
// TODO:   pregame_init()             — reset player ready states, display "waiting for players"
// TODO:   pregame_update()           — poll NFC for deck confirmation scans, advance when ready
// TODO:   pregame_render()           — draw pregame UI (player names, biome selection, ready status)
// TODO:   pregame_handle_card_scan() — mark the scanning player as ready and record their deck
// TODO:   pregame_are_players_ready()— return true only when both players have confirmed
// TODO: Wire these into a GamePhase state machine in game.c (PHASE_PREGAME → PHASE_PLAYING → PHASE_OVER).
void pregame_init(GameState * gs);
void pregame_update(GameState * gs);
void pregame_render(GameState * gs);
void pregame_handle_card_scan(GameState * gs, NFCEvent * event);
bool pregame_are_players_ready(GameState * gs);