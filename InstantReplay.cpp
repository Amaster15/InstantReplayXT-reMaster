// Modernization of InstantReplayXT; original copyright and license in LICENSE.
#include "InstantReplay.h"
#include <QCoreApplication>
#include <QMetaObject>
#include <QObject>
#include <QTimer>
#include <QThreadPool>
#include <algorithm>
#include <filesystem>
#include <vector>

namespace {
namespace fs = std::filesystem;
struct MutedSource { obs_source_t *source; bool muted; };
class ReplayController : public QObject {
public:
    QTimer timer;
    QThreadPool backups;
    enum Phase { Idle, Saving, TransitionIn, Playing, TransitionOut } phase = Idle;
    obs_source_t *original = nullptr;
    obs_source_t *scene = nullptr;
    obs_source_t *media = nullptr;
    std::vector<MutedSource> muted;
    int pause = 0, length = 0;
    bool desktop = false, aux = false;
    std::string backup;

    ReplayController() {
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, this, [this] { advance(); });
    }
    ~ReplayController() override { reset(false); backups.waitForDone(); }
    void restoreAudio() {
        for (const auto &entry : muted) {
            obs_source_set_muted(entry.source, entry.muted);
            obs_source_release(entry.source);
        }
        muted.clear();
    }
    void reset(bool returnToScene) {
        timer.stop();
        if (returnToScene && original && phase != Saving && phase != Idle)
            obs_frontend_set_current_scene(original);
        restoreAudio();
        obs_source_release(original); original = nullptr;
        obs_source_release(scene); scene = nullptr;
        obs_source_release(media); media = nullptr;
        phase = Idle;
    }
    void start() {
        if (phase != Idle || !get_enabled() || !obs_frontend_replay_buffer_active()) return;
        scene = obs_get_source_by_name(get_scene().c_str());
        media = obs_get_source_by_name(get_replay_source_name().c_str());
        original = obs_frontend_get_current_scene();
        if (!scene || !obs_scene_from_source(scene) || !media || !original ||
            std::string(obs_source_get_unversioned_id(media)) != "ffmpeg_source" || scene == original) {
            blog(LOG_WARNING, "[InstantReplayXT] Select a valid replay scene and Media Source, and trigger from a different scene.");
            reset(false); return;
        }
        pause = std::max(0, get_pause());
        length = std::max(1, get_replay());
        desktop = get_D_mute(); aux = get_A_mute();
        backup = get_backup_enabled() ? get_backup_path() : "";
        phase = Saving;
        timer.start(std::max(1000, get_wait()));
        obs_frontend_replay_buffer_save();
    }
    void saved() {
        if (phase != Saving) return;
        char *path = obs_frontend_get_last_replay();
        std::string replay = path ? path : "";
        bfree(path);
        if (replay.empty()) return;
        timer.stop();
        if (!backup.empty()) {
            // Copying large clips must not freeze OBS rendering/UI dispatch.
            backups.setMaxThreadCount(1);
            backups.start([replay, backupPath = backup] {
            try {
                auto directory = fs::u8path(backupPath);
                auto source = fs::u8path(replay);
                fs::create_directories(directory);
                auto destination = directory / source.filename();
                unsigned suffix = 2;
                while (fs::exists(destination))
                    destination = directory / fs::u8path(source.stem().u8string() + "_" +
                        std::to_string(suffix++) + source.extension().u8string());
                fs::copy_file(source, destination);
            } catch (const fs::filesystem_error &e) {
                blog(LOG_WARNING, "[InstantReplayXT] Replay backup failed: %s", e.what());
            }
            });
        }
        obs_data_t *settings = obs_data_create();
        obs_data_set_string(settings, "local_file", replay.c_str());
        obs_data_set_bool(settings, "is_local_file", true);
        obs_data_set_bool(settings, "close_when_inactive", true);
        obs_data_set_bool(settings, "restart_on_activate", true);
        obs_data_set_bool(settings, "looping", false);
        obs_source_update(media, settings);
        obs_data_release(settings);
        phase = TransitionIn;
        obs_frontend_set_current_scene(scene);
        obs_source_media_restart(media);
        timer.start(pause);
    }
    void advance() {
        switch (phase) {
        case Saving:
            blog(LOG_WARNING, "[InstantReplayXT] Timed out waiting for replay save.");
            reset(false); break;
        case TransitionIn:
            for (unsigned channel = 1; channel <= 5; ++channel) {
                if ((channel <= 2 && !desktop) || (channel >= 3 && !aux)) continue;
                auto *source = obs_get_output_source(channel);
                if (!source) continue;
                // A source may occupy multiple channels; preserve its first state.
                if (std::any_of(muted.begin(), muted.end(), [source](const auto &m) { return m.source == source; })) {
                    obs_source_release(source); continue;
                }
                muted.push_back({source, obs_source_muted(source)});
                obs_source_set_muted(source, true);
            }
            phase = Playing;
            timer.start(std::max(1, length - 2 * pause));
            break;
        case Playing:
            phase = TransitionOut;
            obs_frontend_set_current_scene(original);
            timer.start(pause);
            break;
        case TransitionOut: reset(false); break;
        default: break;
        }
    }
};
ReplayController *controller = nullptr;
obs_hotkey_id hotkeyId = OBS_INVALID_HOTKEY_ID;
void frontendEvent(obs_frontend_event event, void *) {
    if (!controller) return;
    // OBS frontend events run on the UI thread. Manual scene changes cancel replay
    // without pulling the operator back to a scene they have deliberately left.
    if (event == OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED) controller->saved();
    else if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED &&
             (controller->phase == ReplayController::TransitionIn || controller->phase == ReplayController::Playing)) {
        auto *current = obs_frontend_get_current_scene();
        bool left = current != controller->scene;
        obs_source_release(current);
        if (left) controller->reset(false);
    } else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP ||
               event == OBS_FRONTEND_EVENT_EXIT || event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN)
        controller->reset(false);
    else if (event == OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED && controller->phase == ReplayController::Saving)
        controller->reset(false);
}
}

void replayStartup() {
    controller = new ReplayController;
    hotkeyId = obs_hotkey_register_frontend("Instant Replay", "Instant Replay XT: Save and play", instant_replay_hotkey, nullptr);
    obs_frontend_add_event_callback(frontendEvent, nullptr);
}
void replayShutdown() {
    obs_frontend_remove_event_callback(frontendEvent, nullptr);
    if (hotkeyId != OBS_INVALID_HOTKEY_ID) obs_hotkey_unregister(hotkeyId);
    hotkeyId = OBS_INVALID_HOTKEY_ID;
    delete controller; controller = nullptr;
}
void instant_replay_hotkey(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed) {
    if (pressed && controller)
        QMetaObject::invokeMethod(controller, [] { if (controller) controller->start(); }, Qt::QueuedConnection);
}
void instant_replay_save(obs_data_t *save_data, bool saving, void *) {
    if (hotkeyId == OBS_INVALID_HOTKEY_ID) return;
    if (saving) {
        auto *obj = obs_data_create();
        auto *keys = obs_hotkey_save(hotkeyId);
        obs_data_set_array(obj, "hotkey", keys);
        obs_data_set_obj(save_data, "InstantReplay", obj);
        obs_data_array_release(keys);
        obs_data_release(obj);
    } else {
        auto *obj = obs_data_get_obj(save_data, "InstantReplay");
        auto *keys = obs_data_get_array(obj, "hotkey");
        obs_hotkey_load(hotkeyId, keys);
        obs_data_array_release(keys);
        obs_data_release(obj);
    }
}
