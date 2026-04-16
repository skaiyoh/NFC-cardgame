//
// Phase-based background music streaming.
//

#ifndef NFC_CARDGAME_AUDIO_H
#define NFC_CARDGAME_AUDIO_H

#include "../core/types.h"

void audio_init(AudioSystem *audio);
void audio_tick(AudioSystem *audio, MusicPhase desiredPhase, float dt);
void audio_cleanup(AudioSystem *audio);

#endif // NFC_CARDGAME_AUDIO_H
