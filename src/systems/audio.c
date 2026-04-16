//
// Phase-based background music streaming.
//

#include "audio.h"
#include "../core/config.h"
#include <stdio.h>
#include <string.h>

static float audio_clamp_unit(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static bool audio_phase_is_valid(MusicPhase phase) {
    return phase >= MUSIC_PHASE_1 && phase < MUSIC_PHASE_COUNT;
}

static const char *audio_phase_name(MusicPhase phase) {
    switch (phase) {
        case MUSIC_PHASE_1: return "phase 1";
        case MUSIC_PHASE_2: return "phase 2";
        case MUSIC_PHASE_3: return "phase 3";
        case MUSIC_PHASE_4: return "phase 4";
        case MUSIC_PHASE_COUNT:
        default:            return "unknown";
    }
}

static const char *audio_phase_path(MusicPhase phase) {
    switch (phase) {
        case MUSIC_PHASE_1: return MUSIC_PHASE1_PATH;
        case MUSIC_PHASE_2: return MUSIC_PHASE2_PATH;
        case MUSIC_PHASE_3: return MUSIC_PHASE3_PATH;
        case MUSIC_PHASE_4: return MUSIC_PHASE4_PATH;
        case MUSIC_PHASE_COUNT:
        default:            return NULL;
    }
}

static bool audio_music_loaded(const Music *music) {
    return music && music->ctxData != NULL && music->frameCount > 0;
}

static Music *audio_track(AudioSystem *audio, MusicPhase phase) {
    if (!audio || !audio_phase_is_valid(phase)) return NULL;
    if (!audio->trackLoaded[phase]) return NULL;
    return &audio->tracks[phase];
}

static void audio_stop_and_rewind_track(Music *track) {
    if (!track) return;
    if (IsMusicStreamPlaying(*track)) {
        StopMusicStream(*track);
    }
    SeekMusicStream(*track, 0.0f);
}

static void audio_unload_tracks(AudioSystem *audio) {
    if (!audio) return;

    for (int i = 0; i < MUSIC_PHASE_COUNT; i++) {
        if (!audio->trackLoaded[i]) continue;
        audio_stop_and_rewind_track(&audio->tracks[i]);
        UnloadMusicStream(audio->tracks[i]);
        audio->tracks[i] = (Music){0};
        audio->trackLoaded[i] = false;
    }
}

static void audio_disable(AudioSystem *audio) {
    if (!audio) return;

    audio_unload_tracks(audio);
    if (audio->deviceReady && IsAudioDeviceReady()) {
        CloseAudioDevice();
    }

    audio->currentPhase = MUSIC_PHASE_1;
    audio->incomingPhase = MUSIC_PHASE_1;
    audio->fadeDurationSeconds = 0.0f;
    audio->fadeElapsedSeconds = 0.0f;
    audio->baseVolume = 0.0f;
    audio->hasCurrentPhase = false;
    audio->fadeActive = false;
    audio->deviceReady = false;
    audio->enabled = false;
}

static bool audio_load_track(AudioSystem *audio, MusicPhase phase) {
    const char *path = audio_phase_path(phase);
    if (!audio || !audio_phase_is_valid(phase) || !path) return false;

    Music music = LoadMusicStream(path);
    if (!audio_music_loaded(&music)) {
        printf("[AUDIO] Failed to load %s from %s\n", audio_phase_name(phase), path);
        return false;
    }

    music.looping = true;
    SetMusicVolume(music, 0.0f);
    audio->tracks[phase] = music;
    audio->trackLoaded[phase] = true;
    return true;
}

static void audio_update_track_stream(Music *track) {
    if (!track) return;

    if (!IsMusicStreamPlaying(*track)) {
        SeekMusicStream(*track, 0.0f);
        PlayMusicStream(*track);
    }

    UpdateMusicStream(*track);
}

static void audio_start_phase(AudioSystem *audio, MusicPhase phase) {
    Music *track = audio_track(audio, phase);
    if (!audio || !track) return;

    audio_stop_and_rewind_track(track);
    SetMusicVolume(*track, audio->baseVolume);
    PlayMusicStream(*track);

    audio->currentPhase = phase;
    audio->incomingPhase = phase;
    audio->fadeElapsedSeconds = 0.0f;
    audio->hasCurrentPhase = true;
    audio->fadeActive = false;

    printf("[AUDIO] Playing %s\n", audio_phase_name(phase));
}

static void audio_cancel_fade(AudioSystem *audio) {
    if (!audio || !audio->fadeActive) return;

    Music *current = audio_track(audio, audio->currentPhase);
    Music *incoming = audio_track(audio, audio->incomingPhase);
    if (incoming) {
        audio_stop_and_rewind_track(incoming);
        SetMusicVolume(*incoming, 0.0f);
    }
    if (current) {
        SetMusicVolume(*current, audio->baseVolume);
    }

    audio->incomingPhase = audio->currentPhase;
    audio->fadeElapsedSeconds = 0.0f;
    audio->fadeActive = false;
}

static void audio_begin_phase_change(AudioSystem *audio, MusicPhase phase) {
    if (!audio || !audio_phase_is_valid(phase)) return;

    if (!audio->hasCurrentPhase) {
        audio_start_phase(audio, phase);
        return;
    }

    if (phase == audio->currentPhase && !audio->fadeActive) {
        return;
    }

    if (audio->fadeActive) {
        if (phase == audio->incomingPhase) {
            return;
        }
        audio_cancel_fade(audio);
        if (phase == audio->currentPhase) {
            return;
        }
    }

    Music *current = audio_track(audio, audio->currentPhase);
    Music *incoming = audio_track(audio, phase);
    if (!current || !incoming) return;

    if (audio->fadeDurationSeconds <= 0.0f) {
        audio_stop_and_rewind_track(current);
        SetMusicVolume(*current, 0.0f);
        audio_start_phase(audio, phase);
        return;
    }

    audio_stop_and_rewind_track(incoming);
    SetMusicVolume(*current, audio->baseVolume);
    SetMusicVolume(*incoming, 0.0f);
    PlayMusicStream(*incoming);

    audio->incomingPhase = phase;
    audio->fadeElapsedSeconds = 0.0f;
    audio->fadeActive = true;

    printf("[AUDIO] Crossfading %s -> %s\n",
           audio_phase_name(audio->currentPhase),
           audio_phase_name(phase));
}

void audio_init(AudioSystem *audio) {
    if (!audio) return;

    memset(audio, 0, sizeof(*audio));
    audio->currentPhase = MUSIC_PHASE_1;
    audio->incomingPhase = MUSIC_PHASE_1;
    audio->baseVolume = audio_clamp_unit(MUSIC_DEFAULT_VOLUME);
    audio->fadeDurationSeconds = (MUSIC_FADE_SECONDS > 0.0f) ? MUSIC_FADE_SECONDS : 0.0f;

    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        printf("[AUDIO] Audio device unavailable; music disabled\n");
        audio_disable(audio);
        return;
    }
    audio->deviceReady = true;

    for (int i = 0; i < MUSIC_PHASE_COUNT; i++) {
        if (!audio_load_track(audio, (MusicPhase)i)) {
            printf("[AUDIO] Music disabled after load failure\n");
            audio_disable(audio);
            return;
        }
    }

    audio->enabled = true;
}

void audio_tick(AudioSystem *audio, MusicPhase desiredPhase, float dt) {
    if (!audio || !audio->enabled) return;
    if (!audio_phase_is_valid(desiredPhase)) desiredPhase = MUSIC_PHASE_1;
    if (dt < 0.0f) dt = 0.0f;

    audio_begin_phase_change(audio, desiredPhase);
    if (!audio->hasCurrentPhase) return;

    Music *current = audio_track(audio, audio->currentPhase);
    if (!current) return;

    if (audio->fadeActive) {
        Music *incoming = audio_track(audio, audio->incomingPhase);
        if (!incoming) {
            audio_cancel_fade(audio);
            audio_update_track_stream(current);
            SetMusicVolume(*current, audio->baseVolume);
            return;
        }

        audio_update_track_stream(current);
        audio_update_track_stream(incoming);

        audio->fadeElapsedSeconds += dt;
        float t = 1.0f;
        if (audio->fadeDurationSeconds > 0.0f) {
            t = audio->fadeElapsedSeconds / audio->fadeDurationSeconds;
        }
        t = audio_clamp_unit(t);

        SetMusicVolume(*current, audio->baseVolume * (1.0f - t));
        SetMusicVolume(*incoming, audio->baseVolume * t);

        if (t >= 1.0f) {
            audio_stop_and_rewind_track(current);
            SetMusicVolume(*current, 0.0f);
            SetMusicVolume(*incoming, audio->baseVolume);

            audio->currentPhase = audio->incomingPhase;
            audio->incomingPhase = audio->currentPhase;
            audio->fadeElapsedSeconds = 0.0f;
            audio->fadeActive = false;
        }
        return;
    }

    audio_update_track_stream(current);
    SetMusicVolume(*current, audio->baseVolume);
}

void audio_cleanup(AudioSystem *audio) {
    if (!audio) return;

    audio_unload_tracks(audio);
    if (audio->deviceReady && IsAudioDeviceReady()) {
        CloseAudioDevice();
    }

    memset(audio, 0, sizeof(*audio));
}
