#include "ReplayPlayer.h"
#include <obs-module.h>
#include <util/platform.h>
#include <QLibrary>
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <string>
#include <cstring>
#include <cstdarg>
#include <cstdio>

namespace {
// Stable libVLC 3 C ABI. Opaque handles avoid a build/link dependency on VLC;
// the installed 64-bit VLC supplies codecs and plugins at runtime.
struct libvlc_instance_t;
struct libvlc_media_t;
struct libvlc_media_player_t;
// libVLC 3 media track layouts (public libvlc_media.h ABI).
struct VlcVideoTrack {
    unsigned height, width, sarNumerator, sarDenominator, fpsNumerator, fpsDenominator;
    int orientation, projection;
    float yaw, pitch, roll, fieldOfView;
};
struct VlcMediaTrack {
    uint32_t codec, originalFourcc;
    int id, type, profile, level;
    union { void *audio; VlcVideoTrack *video; void *subtitle; };
    unsigned bitrate;
    char *language, *description;
};
struct Vlc {
    QLibrary library, core;
    libvlc_instance_t *instance = nullptr;
#define API(ret, name, ...) ret (*name)(__VA_ARGS__) = nullptr
    API(libvlc_instance_t *, libvlc_new, int, const char *const *);
    API(void, libvlc_release, libvlc_instance_t *);
    API(const char *, libvlc_get_version);
    API(int64_t, libvlc_clock);
    API(const char *, libvlc_errmsg);
    API(void, libvlc_log_set, libvlc_instance_t *, void (*)(void *, int, const void *, const char *, va_list), void *);
    API(libvlc_media_t *, libvlc_media_new_path, libvlc_instance_t *, const char *);
    API(void, libvlc_media_release, libvlc_media_t *);
    API(libvlc_media_t *, libvlc_media_player_get_media, libvlc_media_player_t *);
    API(unsigned, libvlc_media_tracks_get, libvlc_media_t *, VlcMediaTrack ***);
    API(void, libvlc_media_tracks_release, VlcMediaTrack **, unsigned);
    API(libvlc_media_player_t *, libvlc_media_player_new, libvlc_instance_t *);
    API(void, libvlc_media_player_release, libvlc_media_player_t *);
    API(void, libvlc_media_player_set_media, libvlc_media_player_t *, libvlc_media_t *);
    API(int, libvlc_media_player_play, libvlc_media_player_t *);
    API(void, libvlc_media_player_stop, libvlc_media_player_t *);
    API(void, libvlc_media_player_set_pause, libvlc_media_player_t *, int);
    API(int, libvlc_media_player_set_rate, libvlc_media_player_t *, float);
    API(float, libvlc_media_player_get_rate, libvlc_media_player_t *);
    API(int64_t, libvlc_media_player_get_time, libvlc_media_player_t *);
    API(void, libvlc_media_player_set_time, libvlc_media_player_t *, int64_t);
    API(int64_t, libvlc_media_player_get_length, libvlc_media_player_t *);
    API(int, libvlc_media_player_get_state, libvlc_media_player_t *);
    API(void, libvlc_video_set_callbacks, libvlc_media_player_t *, void *(*)(void *, void **), void (*)(void *, void *, void *const *), void (*)(void *, void *), void *);
    API(void, libvlc_video_set_format_callbacks, libvlc_media_player_t *, unsigned (*)(void **, char *, unsigned *, unsigned *, unsigned *, unsigned *), void (*)(void *));
    API(void, libvlc_audio_set_callbacks, libvlc_media_player_t *, void (*)(void *, const void *, unsigned, int64_t), void (*)(void *, int64_t), void (*)(void *, int64_t), void (*)(void *, int64_t), void (*)(void *), void *);
    API(void, libvlc_audio_set_format, libvlc_media_player_t *, const char *, unsigned, unsigned);
#undef API
    std::mutex loadMutex;
    bool load() {
        std::lock_guard<std::mutex> guard(loadMutex);
        if (instance) return true;
#ifdef _WIN32
        QString folder;
        for (const auto &key : {"HKEY_LOCAL_MACHINE\\SOFTWARE\\VideoLAN\\VLC", "HKEY_CURRENT_USER\\SOFTWARE\\VideoLAN\\VLC"}) {
            QSettings registry(QString::fromLatin1(key), QSettings::Registry64Format);
            const auto candidate = registry.value("InstallDir").toString();
            if (!candidate.isEmpty() && QFileInfo::exists(candidate + "/libvlc.dll")) { folder = candidate; break; }
        }
        if (folder.isEmpty()) {
            const auto programFiles = qEnvironmentVariable("ProgramW6432", qEnvironmentVariable("ProgramFiles"));
            const auto candidate = programFiles + "/VideoLAN/VLC";
            if (!programFiles.isEmpty() && QFileInfo::exists(candidate + "/libvlc.dll")) folder = candidate;
        }
        if (folder.isEmpty()) { blog(LOG_WARNING, "[InstantReplayXT] VLC was not found in the registry or Program Files."); return false; }
        // Load the sibling core explicitly, without changing the process DLL
        // search path used by OBS and other plugins.
        core.setFileName(folder + "/libvlccore.dll");
        if (!core.load()) { blog(LOG_WARNING, "[InstantReplayXT] VLC core: %s", qPrintable(core.errorString())); return false; }
        library.setFileName(folder + "/libvlc.dll");
#else
        library.setFileName("libvlc.so.5");
#endif
        if (!library.load()) { blog(LOG_WARNING, "[InstantReplayXT] VLC library: %s", qPrintable(library.errorString())); return false; }
#define LOAD(name) name = reinterpret_cast<decltype(name)>(library.resolve(#name)); if (!name) { blog(LOG_WARNING, "[InstantReplayXT] Missing VLC API: %s", #name); return false; }
        LOAD(libvlc_errmsg); LOAD(libvlc_log_set);
        LOAD(libvlc_new); LOAD(libvlc_release); LOAD(libvlc_get_version); LOAD(libvlc_clock);
        LOAD(libvlc_media_player_get_media); LOAD(libvlc_media_tracks_get); LOAD(libvlc_media_tracks_release);
        LOAD(libvlc_media_new_path); LOAD(libvlc_media_release); LOAD(libvlc_media_player_new);
        LOAD(libvlc_media_player_release); LOAD(libvlc_media_player_set_media); LOAD(libvlc_media_player_play);
        LOAD(libvlc_media_player_stop); LOAD(libvlc_media_player_set_pause); LOAD(libvlc_media_player_set_rate);
        LOAD(libvlc_media_player_get_rate); LOAD(libvlc_media_player_get_time); LOAD(libvlc_media_player_set_time);
        LOAD(libvlc_media_player_get_length); LOAD(libvlc_media_player_get_state);
        LOAD(libvlc_video_set_callbacks); LOAD(libvlc_video_set_format_callbacks);
        LOAD(libvlc_audio_set_callbacks); LOAD(libvlc_audio_set_format);
#undef LOAD
        // VLC 4 changes callback and control ABIs; reject it instead of guessing.
        if (std::strncmp(libvlc_get_version(), "3.", 2) != 0) { blog(LOG_WARNING, "[InstantReplayXT] VLC 3.x required; found %s", libvlc_get_version()); return false; }
        const char *options[] = {"--no-video-title-show", "--no-snapshot-preview", "--no-osd", "--no-stats", "--no-spu"};
        instance = libvlc_new(5, options);
        if (instance) libvlc_log_set(instance, [](void *, int level, const void *, const char *format, va_list args) {
            if (level < 3) return;
            char message[1024]{}; std::vsnprintf(message, sizeof(message), format, args);
            blog(LOG_WARNING, "[InstantReplayXT VLC] %s", message);
        }, nullptr);
        if (!instance) blog(LOG_WARNING, "[InstantReplayXT] VLC could not create a playback instance.");
        return instance != nullptr;
    }
} vlc;
struct Player {
    obs_source_t *source;
    libvlc_media_player_t *player = nullptr;
    std::mutex mutex;
    std::string path;
    int speed = 100;
    std::atomic<int> previousState{0};
    std::atomic<bool> failed{false};
    uint64_t opens = 0;
    obs_source_frame frame{};
    int64_t clockOffset;
    explicit Player(obs_source_t *s) : source(s) {
        clockOffset = int64_t(os_gettime_ns()) - vlc.libvlc_clock() * 1000;
        player = vlc.libvlc_media_player_new(vlc.instance);
        if (!player) return;
        vlc.libvlc_video_set_callbacks(player, lock, nullptr, display, this);
        vlc.libvlc_video_set_format_callbacks(player, format, nullptr);
        vlc.libvlc_audio_set_callbacks(player, audio, nullptr, nullptr, nullptr, nullptr, this);
        vlc.libvlc_audio_set_format(player, "FL32", 48000, 2);
    }
    ~Player() {
        if (player) { vlc.libvlc_media_player_stop(player); vlc.libvlc_media_player_release(player); }
        obs_source_frame_free(&frame);
    }
    static unsigned format(void **opaque, char *chroma, unsigned *width, unsigned *height, unsigned *pitches, unsigned *lines) {
        auto *s = static_cast<Player *>(*opaque);
        // VLC 3 passes padded buffer dimensions. Use the public track metadata
        // for the display size, including sample aspect ratio and rotation.
        auto *media = vlc.libvlc_media_player_get_media(s->player);
        if (media) {
            VlcMediaTrack **tracks = nullptr;
            const auto count = vlc.libvlc_media_tracks_get(media, &tracks);
            for (unsigned i = 0; i < count; ++i) {
                if (tracks[i]->type != 1 || !tracks[i]->video) continue;
                const auto &video = *tracks[i]->video;
                uint64_t w = video.width, h = video.height;
                if (video.sarNumerator && video.sarDenominator) w = w * video.sarNumerator / video.sarDenominator;
                if (video.orientation >= 4 && video.orientation <= 7) std::swap(w, h);
                if (w && h && w <= 8192 && h <= 8192) { *width = unsigned(w); *height = unsigned(h); }
                break;
            }
            if (tracks) vlc.libvlc_media_tracks_release(tracks, count);
            vlc.libvlc_media_release(media);
        }
        if (!*width || !*height || *width > 8192 || *height > 8192) return 0;
        std::memcpy(chroma, "RV32", 4);
        obs_source_frame_free(&s->frame);
        obs_source_frame_init(&s->frame, VIDEO_FORMAT_BGRX, *width, *height);
        pitches[0] = s->frame.linesize[0]; lines[0] = *height;
        s->frame.full_range = true;
        return 1;
    }
    static void *lock(void *opaque, void **planes) {
        *planes = static_cast<Player *>(opaque)->frame.data[0]; return nullptr;
    }
    static void display(void *opaque, void *) {
        auto *s = static_cast<Player *>(opaque);
        s->frame.timestamp = os_gettime_ns();
        obs_source_output_video(s->source, &s->frame);
    }
    static void audio(void *opaque, const void *samples, unsigned count, int64_t pts) {
        auto *s = static_cast<Player *>(opaque);
        obs_source_audio audio{};
        audio.data[0] = (const uint8_t *)samples; audio.frames = count;
        audio.speakers = SPEAKERS_STEREO; audio.format = AUDIO_FORMAT_FLOAT; audio.samples_per_sec = 48000;
        audio.timestamp = uint64_t(std::max<int64_t>(0, pts * 1000 + s->clockOffset));
        obs_source_output_audio(s->source, &audio);
    }
    bool rate(int percent) {
        const int next = std::clamp(percent, 25, 200);
        if (vlc.libvlc_media_player_set_rate(player, float(next) / 100.f) != 0) return false;
        speed = next; return true;
    }
    void start() {
        if (path.empty() || failed) return;
        vlc.libvlc_media_player_set_rate(player, float(speed) / 100.f);
        if (vlc.libvlc_media_player_play(player) != 0) {
            failed = true;
            const auto *error = vlc.libvlc_errmsg();
            blog(LOG_WARNING, "[InstantReplayXT] VLC playback could not start: %s", error ? error : "unknown error");
        }
    }
    void update(obs_data_t *settings) {
        std::lock_guard<std::mutex> guard(mutex);
        const std::string next = obs_data_get_string(settings, "local_file");
        const int percent = int(obs_data_get_int(settings, "speed_percent"));
        if (path == next && !failed) { rate(percent); return; }
        // Only a file change replaces the input. Live rate changes never stop,
        // seek, rebuild the decoder, or alter scene visibility.
        vlc.libvlc_media_player_stop(player);
        vlc.libvlc_media_player_set_media(player, nullptr);
        path = next; previousState = 0; failed = false;
        obs_source_output_video(source, nullptr);
        if (!path.empty()) {
            // libVLC 3 on Windows requires native backslashes even though Qt
            // and OBS store forward-slash paths. Preserve UTF-8 for VLC.
            const auto nativePath = QDir::toNativeSeparators(QString::fromUtf8(path.c_str())).toUtf8();
            auto *media = vlc.libvlc_media_new_path(vlc.instance, nativePath.constData());
            if (media) {
                vlc.libvlc_media_player_set_media(player, media); vlc.libvlc_media_release(media); ++opens;
            } else {
                failed = true;
                const auto *error = vlc.libvlc_errmsg();
                blog(LOG_WARNING, "[InstantReplayXT] VLC could not open the clip: %s", error ? error : "invalid media path");
            }
        }
        rate(percent);
        if (source && obs_source_active(source)) start();
    }
};
static void liveRate(void *data, calldata_t *call) {
    auto *s = static_cast<Player *>(data); std::lock_guard<std::mutex> guard(s->mutex);
    const bool success = s->rate(int(calldata_int(call, "percent")));
    calldata_set_bool(call, "success", success);
}
static void diagnostics(void *data, calldata_t *call) {
    auto *s = static_cast<Player *>(data); std::lock_guard<std::mutex> guard(s->mutex);
    calldata_set_int(call, "opens", int64_t(s->opens));
    calldata_set_float(call, "rate", vlc.libvlc_media_player_get_rate(s->player));
}
static void *create(obs_data_t *settings, obs_source_t *source) {
    if (!vlc.load()) return nullptr;
    auto *s = new Player(source);
    if (!s->player) { delete s; return nullptr; }
    proc_handler_add(obs_source_get_proc_handler(source), "void irxt_set_rate(in int percent, out bool success)", liveRate, s);
    proc_handler_add(obs_source_get_proc_handler(source), "void irxt_player_state(out int opens, out float rate)", diagnostics, s);
    s->update(settings); return s;
}
static enum obs_media_state state(void *data) {
    auto *s = static_cast<Player *>(data); std::lock_guard<std::mutex> guard(s->mutex);
    if (s->failed) return OBS_MEDIA_STATE_ERROR;
    // libvlc_state_t values in the stable VLC 3 ABI.
    switch (vlc.libvlc_media_player_get_state(s->player)) {
    case 1: return OBS_MEDIA_STATE_OPENING;
    case 2: return OBS_MEDIA_STATE_BUFFERING;
    case 3: return OBS_MEDIA_STATE_PLAYING;
    case 4: return OBS_MEDIA_STATE_PAUSED;
    case 5: return OBS_MEDIA_STATE_STOPPED;
    case 6: return OBS_MEDIA_STATE_ENDED;
    case 7: return OBS_MEDIA_STATE_ERROR;
    default: return OBS_MEDIA_STATE_NONE;
    }
}
}
bool replay_player_available() { return vlc.load(); }
void replay_player_shutdown() {
    if (vlc.instance) { vlc.libvlc_release(vlc.instance); vlc.instance = nullptr; }
}
void replay_player_register() {
    obs_source_info info{};
    info.id = "irxt_vlc_player";
    info.type = OBS_SOURCE_TYPE_INPUT;
    info.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_CONTROLLABLE_MEDIA;
    info.get_name = [](void *) { return "Instant Replay VLC Player"; };
    info.create = create;
    info.destroy = [](void *data) { delete static_cast<Player *>(data); };
    info.update = [](void *data, obs_data_t *settings) { static_cast<Player *>(data)->update(settings); };
    info.get_defaults = [](obs_data_t *settings) { obs_data_set_default_int(settings, "speed_percent", 100); };
    info.get_properties = [](void *) {
        auto *properties = obs_properties_create();
        obs_properties_add_path(properties, "local_file", "Clip", OBS_PATH_FILE, "Video (*.mkv *.mp4 *.mov *.flv *.ts *.webm);;All files (*.*)", nullptr);
        obs_properties_add_int_slider(properties, "speed_percent", "Playback speed (%)", 25, 200, 1);
        return properties;
    };
    info.activate = [](void *data) { auto *s = static_cast<Player *>(data); std::lock_guard<std::mutex> guard(s->mutex); s->start(); };
    info.deactivate = [](void *data) {
        auto *s = static_cast<Player *>(data); std::lock_guard<std::mutex> guard(s->mutex);
        vlc.libvlc_media_player_stop(s->player); obs_source_output_video(s->source, nullptr);
    };
    info.media_restart = [](void *data) {
        auto *s = static_cast<Player *>(data); std::lock_guard<std::mutex> guard(s->mutex);
        vlc.libvlc_media_player_stop(s->player); s->start();
    };
    info.media_play_pause = [](void *data, bool pause) {
        auto *s = static_cast<Player *>(data); std::lock_guard<std::mutex> guard(s->mutex);
        vlc.libvlc_media_player_set_pause(s->player, pause);
    };
    info.media_stop = [](void *data) { auto *s = static_cast<Player *>(data); std::lock_guard<std::mutex> guard(s->mutex); vlc.libvlc_media_player_stop(s->player); };
    info.media_get_state = state;
    info.media_get_time = [](void *data) { auto *s = static_cast<Player *>(data); std::lock_guard<std::mutex> guard(s->mutex); return vlc.libvlc_media_player_get_time(s->player); };
    info.media_get_duration = [](void *data) { auto *s = static_cast<Player *>(data); std::lock_guard<std::mutex> guard(s->mutex); return vlc.libvlc_media_player_get_length(s->player); };
    info.media_set_time = [](void *data, int64_t time) { auto *s = static_cast<Player *>(data); std::lock_guard<std::mutex> guard(s->mutex); vlc.libvlc_media_player_set_time(s->player, time); };
    info.video_tick = [](void *data, float) {
        auto *s = static_cast<Player *>(data);
        const auto current = state(data);
        if (int(current) == s->previousState.exchange(int(current))) return;
        if (current == OBS_MEDIA_STATE_PLAYING) obs_source_media_started(s->source);
        if (current == OBS_MEDIA_STATE_ENDED) obs_source_media_ended(s->source);
    };
    obs_register_source(&info);
}
