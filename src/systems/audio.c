//
// Phase-based background music streaming.
//

#include "audio.h"
#include "../core/config.h"
#include <dirent.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *s_phase_dirs[MUSIC_PHASE_COUNT] = {
    MUSIC_PHASE1_DIR,
    MUSIC_PHASE2_DIR,
    MUSIC_PHASE3_DIR,
    MUSIC_PHASE4_DIR,
};

static const char *s_phase_fallback_paths[MUSIC_PHASE_COUNT] = {
    MUSIC_PHASE1_PATH,
    MUSIC_PHASE2_PATH,
    MUSIC_PHASE3_PATH,
    MUSIC_PHASE4_PATH,
};

static float audio_clamp_unit(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static bool audio_phase_is_valid(MusicPhase phase) {
    return phase >= MUSIC_PHASE_1 && phase < MUSIC_PHASE_COUNT;
}

static AudioPlaylist *audio_playlist(AudioSystem *audio, MusicPhase phase) {
    if (!audio || !audio_phase_is_valid(phase)) return NULL;
    return &audio->playlists[phase];
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

static const char *audio_track_label(const AudioPlaylist *playlist, int trackIndex) {
    if (!playlist || trackIndex < 0 || trackIndex >= playlist->trackCount) return "unknown";

    const char *path = playlist->trackPaths[trackIndex];
    const char *base = strrchr(path, '/');
    return base ? (base + 1) : path;
}

static int audio_path_compare(const void *lhs, const void *rhs) {
    const char *a = (const char *)lhs;
    const char *b = (const char *)rhs;
    return strcmp(a, b);
}

static void audio_copy_path(char destination[MUSIC_MAX_PATH_LENGTH], const char *source) {
    if (!destination) return;
    if (!source) {
        destination[0] = '\0';
        return;
    }

    size_t length = strlen(source);
    if (length >= MUSIC_MAX_PATH_LENGTH) {
        length = MUSIC_MAX_PATH_LENGTH - 1;
    }

    memcpy(destination, source, length);
    destination[length] = '\0';
}

static bool audio_path_has_mp3_extension(const char *path) {
    if (!path) return false;

    size_t length = strlen(path);
    if (length < 4) return false;

    const char *ext = path + length - 4;
    return tolower((unsigned char)ext[0]) == '.' &&
           tolower((unsigned char)ext[1]) == 'm' &&
           tolower((unsigned char)ext[2]) == 'p' &&
           tolower((unsigned char)ext[3]) == '3';
}

static bool audio_collect_phase_paths(char paths[MUSIC_MAX_TRACKS_PER_PHASE][MUSIC_MAX_PATH_LENGTH],
                                      int *pathCount,
                                      const char *dirPath,
                                      const char *fallbackPath) {
    if (!paths || !pathCount) return false;

    *pathCount = 0;

    DIR *dir = opendir(dirPath);
    if (!dir) {
        printf("[AUDIO] Could not open %s; falling back if configured\n", dirPath);
    } else {
        struct dirent *entry = NULL;
        while ((entry = readdir(dir)) != NULL) {
            if (!audio_path_has_mp3_extension(entry->d_name)) continue;
            if (*pathCount >= MUSIC_MAX_TRACKS_PER_PHASE) {
                printf("[AUDIO] Track cap reached for %s; ignoring extra files\n", dirPath);
                break;
            }

            snprintf(paths[*pathCount], MUSIC_MAX_PATH_LENGTH, "%s/%s", dirPath, entry->d_name);
            (*pathCount)++;
        }
        closedir(dir);
    }

    if (*pathCount == 0 && fallbackPath && fallbackPath[0] != '\0') {
        snprintf(paths[0], MUSIC_MAX_PATH_LENGTH, "%s", fallbackPath);
        *pathCount = 1;
    }

    if (*pathCount > 1) {
        qsort(paths, (size_t)*pathCount, sizeof(paths[0]), audio_path_compare);
    }

    return *pathCount > 0;
}

static void audio_shuffle_phase(AudioPlaylist *playlist, int avoidTrackIndex) {
    if (!playlist || playlist->trackCount <= 0) return;

    for (int i = 0; i < playlist->trackCount; i++) {
        playlist->shuffleOrder[i] = i;
    }

    for (int i = playlist->trackCount - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = playlist->shuffleOrder[i];
        playlist->shuffleOrder[i] = playlist->shuffleOrder[j];
        playlist->shuffleOrder[j] = temp;
    }

    if (playlist->trackCount > 1 && playlist->shuffleOrder[0] == avoidTrackIndex) {
        int swapIndex = 1 + (rand() % (playlist->trackCount - 1));
        int temp = playlist->shuffleOrder[0];
        playlist->shuffleOrder[0] = playlist->shuffleOrder[swapIndex];
        playlist->shuffleOrder[swapIndex] = temp;
    }

    playlist->nextShuffleIndex = 0;
}

static bool audio_music_loaded(const Music *music) {
    return music && music->ctxData != NULL && music->frameCount > 0;
}

static int audio_pick_next_track_index(AudioSystem *audio, MusicPhase phase, int avoidTrackIndex) {
    AudioPlaylist *playlist = audio_playlist(audio, phase);
    if (!playlist || playlist->trackCount <= 0) return -1;
    if (playlist->trackCount == 1) return 0;

    if (playlist->nextShuffleIndex >= playlist->trackCount) {
        audio_shuffle_phase(playlist, avoidTrackIndex);
    }

    return playlist->shuffleOrder[playlist->nextShuffleIndex++];
}

static Music *audio_track(AudioSystem *audio, MusicPhase phase, int trackIndex) {
    AudioPlaylist *playlist = audio_playlist(audio, phase);
    if (!playlist || trackIndex < 0 || trackIndex >= playlist->trackCount) return NULL;
    return &playlist->tracks[trackIndex];
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

    for (int phase = 0; phase < MUSIC_PHASE_COUNT; phase++) {
        AudioPlaylist *playlist = &audio->playlists[phase];
        for (int trackIndex = 0; trackIndex < playlist->trackCount; trackIndex++) {
            audio_stop_and_rewind_track(&playlist->tracks[trackIndex]);
            UnloadMusicStream(playlist->tracks[trackIndex]);
            playlist->tracks[trackIndex] = (Music){0};
            playlist->trackPaths[trackIndex][0] = '\0';
        }

        playlist->trackCount = 0;
        playlist->nextShuffleIndex = 0;
        playlist->lastPlayedTrackIndex = -1;
    }
}

static void audio_disable(AudioSystem *audio) {
    if (!audio) return;

    audio_unload_tracks(audio);
    if (audio->deviceReady && IsAudioDeviceReady()) {
        CloseAudioDevice();
    }

    audio->currentPhase = MUSIC_PHASE_1;
    audio->currentTrackIndex = -1;
    audio->incomingPhase = MUSIC_PHASE_1;
    audio->incomingTrackIndex = -1;
    audio->fadeDurationSeconds = 0.0f;
    audio->fadeElapsedSeconds = 0.0f;
    audio->baseVolume = 0.0f;
    audio->hasCurrentTrack = false;
    audio->fadeActive = false;
    audio->deviceReady = false;
    audio->enabled = false;
}

static bool audio_load_phase_playlist(AudioSystem *audio, MusicPhase phase) {
    AudioPlaylist *playlist = audio_playlist(audio, phase);
    if (!playlist) return false;

    char discoveredPaths[MUSIC_MAX_TRACKS_PER_PHASE][MUSIC_MAX_PATH_LENGTH] = {{0}};
    int discoveredCount = 0;
    if (!audio_collect_phase_paths(discoveredPaths, &discoveredCount,
                                   s_phase_dirs[phase], s_phase_fallback_paths[phase])) {
        printf("[AUDIO] No tracks found for %s\n", audio_phase_name(phase));
        return false;
    }

    playlist->trackCount = 0;
    playlist->nextShuffleIndex = 0;
    playlist->lastPlayedTrackIndex = -1;

    for (int i = 0; i < discoveredCount; i++) {
        Music music = LoadMusicStream(discoveredPaths[i]);
        if (!audio_music_loaded(&music)) {
            printf("[AUDIO] Failed to load %s track %s\n",
                   audio_phase_name(phase), discoveredPaths[i]);
            continue;
        }

        music.looping = false;
        SetMusicVolume(music, 0.0f);

        int slot = playlist->trackCount++;
        playlist->tracks[slot] = music;
        audio_copy_path(playlist->trackPaths[slot], discoveredPaths[i]);
    }

    if (playlist->trackCount <= 0) {
        printf("[AUDIO] No playable tracks loaded for %s\n", audio_phase_name(phase));
        return false;
    }

    playlist->nextShuffleIndex = playlist->trackCount;
    return true;
}

static void audio_update_track_stream(Music *track) {
    if (!track) return;

    UpdateMusicStream(*track);
}

static void audio_start_track(AudioSystem *audio, MusicPhase phase, int trackIndex) {
    AudioPlaylist *playlist = audio_playlist(audio, phase);
    Music *track = audio_track(audio, phase, trackIndex);
    if (!audio || !playlist || !track) return;

    audio_stop_and_rewind_track(track);
    SetMusicVolume(*track, audio->baseVolume);
    PlayMusicStream(*track);

    audio->currentPhase = phase;
    audio->currentTrackIndex = trackIndex;
    audio->incomingPhase = phase;
    audio->incomingTrackIndex = trackIndex;
    audio->fadeElapsedSeconds = 0.0f;
    audio->hasCurrentTrack = true;
    audio->fadeActive = false;
    playlist->lastPlayedTrackIndex = trackIndex;

    printf("[AUDIO] Playing %s: %s\n",
           audio_phase_name(phase),
           audio_track_label(playlist, trackIndex));
}

static void audio_cancel_fade(AudioSystem *audio) {
    if (!audio || !audio->fadeActive) return;

    Music *current = audio_track(audio, audio->currentPhase, audio->currentTrackIndex);
    Music *incoming = audio_track(audio, audio->incomingPhase, audio->incomingTrackIndex);
    if (incoming) {
        audio_stop_and_rewind_track(incoming);
        SetMusicVolume(*incoming, 0.0f);
    }
    if (current) {
        SetMusicVolume(*current, audio->baseVolume);
    }

    audio->incomingPhase = audio->currentPhase;
    audio->incomingTrackIndex = audio->currentTrackIndex;
    audio->fadeElapsedSeconds = 0.0f;
    audio->fadeActive = false;
}

static void audio_begin_track_change(AudioSystem *audio, MusicPhase phase, int trackIndex) {
    if (!audio || !audio_phase_is_valid(phase) || trackIndex < 0) return;

    if (!audio->hasCurrentTrack) {
        audio_start_track(audio, phase, trackIndex);
        return;
    }

    if (phase == audio->currentPhase &&
        trackIndex == audio->currentTrackIndex &&
        !audio->fadeActive) {
        return;
    }

    if (audio->fadeActive) {
        if (phase == audio->incomingPhase &&
            trackIndex == audio->incomingTrackIndex) {
            return;
        }
        audio_cancel_fade(audio);
        if (phase == audio->currentPhase &&
            trackIndex == audio->currentTrackIndex) {
            return;
        }
    }

    AudioPlaylist *incomingPlaylist = audio_playlist(audio, phase);
    Music *current = audio_track(audio, audio->currentPhase, audio->currentTrackIndex);
    Music *incoming = audio_track(audio, phase, trackIndex);
    if (!current || !incoming) return;

    if (audio->fadeDurationSeconds <= 0.0f) {
        audio_stop_and_rewind_track(current);
        SetMusicVolume(*current, 0.0f);
        audio_start_track(audio, phase, trackIndex);
        return;
    }

    audio_stop_and_rewind_track(incoming);
    SetMusicVolume(*current, audio->baseVolume);
    SetMusicVolume(*incoming, 0.0f);
    PlayMusicStream(*incoming);

    audio->incomingPhase = phase;
    audio->incomingTrackIndex = trackIndex;
    audio->fadeElapsedSeconds = 0.0f;
    audio->fadeActive = true;
    incomingPlaylist->lastPlayedTrackIndex = trackIndex;

    printf("[AUDIO] Crossfading %s -> %s: %s\n",
           audio_phase_name(audio->currentPhase),
           audio_phase_name(phase),
           audio_track_label(incomingPlaylist, trackIndex));
}

static void audio_begin_phase_change(AudioSystem *audio, MusicPhase phase) {
    AudioPlaylist *playlist = audio_playlist(audio, phase);
    if (!audio || !playlist) return;

    int avoidTrackIndex = playlist->lastPlayedTrackIndex;
    if (audio->hasCurrentTrack && phase == audio->currentPhase) {
        avoidTrackIndex = audio->currentTrackIndex;
    }

    int trackIndex = audio_pick_next_track_index(audio, phase, avoidTrackIndex);
    if (trackIndex < 0) return;

    audio_begin_track_change(audio, phase, trackIndex);
}

static bool audio_should_advance_within_phase(AudioSystem *audio, MusicPhase desiredPhase) {
    if (!audio || !audio->hasCurrentTrack || audio->fadeActive) return false;
    if (desiredPhase != audio->currentPhase) return false;

    AudioPlaylist *playlist = audio_playlist(audio, audio->currentPhase);
    Music *current = audio_track(audio, audio->currentPhase, audio->currentTrackIndex);
    if (!playlist || !current || playlist->trackCount <= 1) return false;

    float length = GetMusicTimeLength(*current);
    if (length <= 0.0f) return false;

    float remaining = length - GetMusicTimePlayed(*current);
    float threshold = (audio->fadeDurationSeconds > 0.0f) ? audio->fadeDurationSeconds : 0.05f;
    if (threshold > length) threshold = length;
    return remaining <= threshold;
}

static void audio_restart_single_track(AudioSystem *audio) {
    if (!audio || !audio->hasCurrentTrack) return;

    Music *current = audio_track(audio, audio->currentPhase, audio->currentTrackIndex);
    if (!current || IsMusicStreamPlaying(*current)) return;

    audio_stop_and_rewind_track(current);
    SetMusicVolume(*current, audio->baseVolume);
    PlayMusicStream(*current);
}

void audio_init(AudioSystem *audio) {
    if (!audio) return;

    memset(audio, 0, sizeof(*audio));
    audio->currentPhase = MUSIC_PHASE_1;
    audio->currentTrackIndex = -1;
    audio->incomingPhase = MUSIC_PHASE_1;
    audio->incomingTrackIndex = -1;
    audio->baseVolume = audio_clamp_unit(MUSIC_DEFAULT_VOLUME);
    audio->fadeDurationSeconds = (MUSIC_FADE_SECONDS > 0.0f) ? MUSIC_FADE_SECONDS : 0.0f;

    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        printf("[AUDIO] Audio device unavailable; music disabled\n");
        audio_disable(audio);
        return;
    }
    audio->deviceReady = true;

    bool anyTracksLoaded = false;
    for (int phase = 0; phase < MUSIC_PHASE_COUNT; phase++) {
        if (audio_load_phase_playlist(audio, (MusicPhase)phase)) {
            anyTracksLoaded = true;
        }
    }

    if (!anyTracksLoaded) {
        printf("[AUDIO] Music disabled after playlist discovery found no playable tracks\n");
        audio_disable(audio);
        return;
    }

    audio->enabled = true;
}

void audio_tick(AudioSystem *audio, MusicPhase desiredPhase, float dt) {
    if (!audio || !audio->enabled) return;
    if (!audio_phase_is_valid(desiredPhase)) desiredPhase = MUSIC_PHASE_1;
    if (dt < 0.0f) dt = 0.0f;

    if (!audio->hasCurrentTrack) {
        audio_begin_phase_change(audio, desiredPhase);
        if (!audio->hasCurrentTrack) return;
    } else if (!audio->fadeActive && desiredPhase != audio->currentPhase) {
        audio_begin_phase_change(audio, desiredPhase);
    } else if (audio->fadeActive) {
        if (desiredPhase == audio->currentPhase) {
            audio_cancel_fade(audio);
        } else if (desiredPhase != audio->incomingPhase) {
            audio_begin_phase_change(audio, desiredPhase);
        }
    }

    Music *current = audio_track(audio, audio->currentPhase, audio->currentTrackIndex);
    if (!current) return;

    if (audio->fadeActive) {
        Music *incoming = audio_track(audio, audio->incomingPhase, audio->incomingTrackIndex);
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
            audio->currentTrackIndex = audio->incomingTrackIndex;
            audio->incomingPhase = audio->currentPhase;
            audio->incomingTrackIndex = audio->currentTrackIndex;
            audio->fadeElapsedSeconds = 0.0f;
            audio->fadeActive = false;
        }
        return;
    }

    audio_update_track_stream(current);
    SetMusicVolume(*current, audio->baseVolume);

    if (audio_should_advance_within_phase(audio, desiredPhase)) {
        audio_begin_phase_change(audio, audio->currentPhase);
        return;
    }

    AudioPlaylist *playlist = audio_playlist(audio, audio->currentPhase);
    if (!playlist) return;

    if (playlist->trackCount > 1 && !IsMusicStreamPlaying(*current)) {
        audio_begin_phase_change(audio, audio->currentPhase);
        return;
    }

    if (playlist->trackCount == 1) {
        audio_restart_single_track(audio);
    }
}

void audio_cleanup(AudioSystem *audio) {
    if (!audio) return;

    audio_unload_tracks(audio);
    if (audio->deviceReady && IsAudioDeviceReady()) {
        CloseAudioDevice();
    }

    memset(audio, 0, sizeof(*audio));
}
