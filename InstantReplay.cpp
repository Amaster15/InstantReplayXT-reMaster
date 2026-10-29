// Modernization of InstantReplayXT; original copyright and license in LICENSE.
#include "InstantReplay.h"
#include "ReplayFiles.h"
#include <QCoreApplication>
#include <QMetaObject>
#include <QObject>
#include <QTimer>
#include <QThreadPool>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <vector>
#include <memory>

namespace {
namespace fs = std::filesystem;
bool supportedReplaySource(obs_source_t *source) {
    if (!source) return false;
    const std::string id = obs_source_get_unversioned_id(source);
    return id == "ffmpeg_source" || id == "irxt_vlc_player";
}
struct MutedSource { obs_source_t *source; bool muted; };
class ReplayController : public QObject {
public:
    QTimer timer, playbackPoll, speedWait;
    int speedPercent = 100, appliedSpeed = 100, speedTicks = 0, playbackTicks = 0;
    int64_t resumePosition = 0;
    bool resumePaused = false, speedUpdated = false, libraryPlayback = false;
    std::string selectedReplay;
    obs_source_t *speedSource = nullptr;
    std::atomic<unsigned> speedEpoch{0};
    QThreadPool backups;
    enum Phase { Idle, Saving, ReleasingMedia, Processing, TransitionIn, Playing, TransitionOut } phase = Idle;
    obs_source_t *original = nullptr;
    obs_source_t *scene = nullptr;
    obs_source_t *media = nullptr;
    std::vector<MutedSource> muted;
    int pause = 0, length = 0;
    bool desktop = false, aux = false;
    std::string backup, replayFolder, lastReplay, pendingReplay, heldMediaFile;
    bool waitingForMedia = false;
    unsigned generation = 0;
    bool canPlay = false, backupRequested = false, autoSwitch = false;

    ReplayController() {
        playbackPoll.setInterval(100);
        connect(&playbackPoll, &QTimer::timeout, this, [this] {
            if (!libraryPlayback || !media || phase != Playing || speedSource) return;
            const auto state = obs_source_media_get_state(media);
            if (state == OBS_MEDIA_STATE_ENDED || state == OBS_MEDIA_STATE_ERROR) {
                if (state == OBS_MEDIA_STATE_ERROR) replay_status("OBS could not play this clip. Check the OBS log.");
                advance();
            } else if (++playbackTicks >= 100 && state != OBS_MEDIA_STATE_PLAYING && state != OBS_MEDIA_STATE_PAUSED) {
                replay_status("Clip did not start. Make sure the replay Media Source is visible in the replay scene.");
                advance();
            }
        });
        speedWait.setInterval(50);
        connect(&speedWait, &QTimer::timeout, this, [this] {
            if (!speedSource) return;
            if (speedUpdated && obs_source_media_get_duration(speedSource) > 0) {
                obs_source_media_set_time(speedSource, resumePosition);
                if (resumePaused) obs_source_media_play_pause(speedSource, true);
                finishSpeed();
                if (appliedSpeed != speedPercent) setSpeed(speedPercent);
            } else if (++speedTicks >= 100) {
                finishSpeed();
                replay_status("Speed changed, but OBS did not confirm a seekable clip; playback position could not be restored.");
            }
        });
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
        playbackPoll.stop();
        finishSpeed();
        libraryPlayback = false;
        disconnectMediaUpdate();
        if (!heldMediaFile.empty() && media) configureMedia(heldMediaFile);
        heldMediaFile.clear();
        ++generation;
        if (returnToScene && original && phase != Saving && phase != Idle)
            obs_frontend_set_current_scene(original);
        restoreAudio();
        obs_source_release(original); original = nullptr;
        obs_source_release(scene); scene = nullptr;
        obs_source_release(media); media = nullptr;
        phase = Idle;
    }
    static void speedUpdate(void *data, calldata_t *) {
        auto *self = static_cast<ReplayController *>(data);
        const auto epoch = self->speedEpoch.load();
        QMetaObject::invokeMethod(self, [self, epoch] { if (epoch == self->speedEpoch.load() && self->speedSource) self->speedUpdated = true; }, Qt::QueuedConnection);
    }
    void finishSpeed() {
        ++speedEpoch;
        speedWait.stop();
        if (speedSource) {
            signal_handler_disconnect(obs_source_get_signal_handler(speedSource), "update", speedUpdate, this);
            obs_source_release(speedSource); speedSource = nullptr;
        }
    }
    void setSpeed(int percent) {
        speedPercent = std::clamp(percent, 1, 200);
        if (speedSource || phase == Saving || phase == ReleasingMedia || phase == Processing) return;
        auto *source = media ? obs_source_get_ref(media) : obs_get_source_by_name(get_replay_source_name().c_str());
        if (!source) return;
        if (std::string(obs_source_get_unversioned_id(source)) == "irxt_vlc_player") {
            calldata_t call; calldata_init(&call); calldata_set_int(&call, "percent", speedPercent);
            const bool success = proc_handler_call(obs_source_get_proc_handler(source), "irxt_set_rate", &call) && calldata_bool(&call, "success");
            calldata_free(&call);
            if (success) {
                if (!libraryPlayback && phase == Playing && timer.isActive())
                    timer.start(std::max(1, timer.remainingTime() * appliedSpeed / speedPercent));
                appliedSpeed = speedPercent;
                // Persist the setting without triggering a source update/reopen.
                auto *settings = obs_source_get_settings(source);
                obs_data_set_int(settings, "speed_percent", speedPercent); obs_data_release(settings);
            } else replay_status("VLC could not apply this playback speed.");
            obs_source_release(source); return;
        }
        if (std::string(obs_source_get_unversioned_id(source)) != "ffmpeg_source") { obs_source_release(source); return; }
        auto *settings = obs_source_get_settings(source);
        int oldSpeed = (int)obs_data_get_int(settings, "speed_percent");
        obs_data_release(settings);
        if (oldSpeed == speedPercent) { obs_source_release(source); return; }
        const auto state = obs_source_media_get_state(source);
        const bool running = state == OBS_MEDIA_STATE_PLAYING || state == OBS_MEDIA_STATE_PAUSED;
        if (running) {
            speedSource = source;
            resumePosition = std::max<int64_t>(0, obs_source_media_get_time(source));
            resumePaused = state == OBS_MEDIA_STATE_PAUSED;
            speedUpdated = false; speedTicks = 0;
            signal_handler_connect(obs_source_get_signal_handler(source), "update", speedUpdate, this);
            speedWait.start();
        }
        appliedSpeed = speedPercent;
        // Keep the existing replay's wall-clock budget proportional to playback speed.
        if (!libraryPlayback && phase == Playing && timer.isActive())
            timer.start(std::max(1, timer.remainingTime() * (oldSpeed > 0 ? oldSpeed : 100) / speedPercent));
        auto *update = obs_data_create();
        obs_data_set_int(update, "speed_percent", speedPercent);
        obs_source_update(source, update); obs_data_release(update);
        if (!running) obs_source_release(source);
    }
    void start() {
        if (phase != Idle) { replay_status("A replay is already in progress."); return; }
        if (!get_enabled()) { replay_status("Enable Instant Replay in Tools > Instant Replay XT settings."); return; }
        if (!obs_frontend_replay_buffer_active()) {
            replay_status("Replay Buffer is stopped. Click Start Replay Buffer in OBS, let it collect video, then try again.");
            return;
        }
        finishSpeed(); // Cancel a pending seek before saving can replace the source file.
        scene = obs_get_source_by_name(get_scene().c_str());
        media = obs_get_source_by_name(get_replay_source_name().c_str());
        original = obs_frontend_get_current_scene();
        canPlay = scene && obs_scene_from_source(scene) && media && original &&
            supportedReplaySource(media) && scene != original;
        pause = std::max(0, get_pause());
        length = std::max(1, get_replay());
        desktop = get_D_mute(); aux = get_A_mute();
        autoSwitch = get_auto_switch();
        backupRequested = get_backup_enabled();
        backup = backupRequested ? get_backup_path() : "";
        replayFolder = get_replay_path();
        phase = Saving;
        replay_status("Saving replay buffer…");
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
        pendingReplay = replay;
        if (media && supportedReplaySource(media)) {
            auto *settings = obs_source_get_settings(media);
            heldMediaFile = obs_data_get_string(settings, "local_file");
            obs_data_release(settings);
            if (!heldMediaFile.empty()) {
                phase = ReleasingMedia;
                waitingForMedia = true;
                signal_handler_connect(obs_source_get_signal_handler(media), "update", mediaUpdated, this);
                replay_status("Releasing replay Media Source before replacing the fixed file…");
                timer.start(5000);
                configureMedia("");
                return;
            }
        }
        processSaved();
    }
    static void mediaUpdated(void *data, calldata_t *) {
        auto *self = static_cast<ReplayController *>(data);
        QMetaObject::invokeMethod(self, [self] {
            if (self->phase != ReleasingMedia) return;
            self->disconnectMediaUpdate();
            self->timer.stop();
            self->processSaved();
        }, Qt::QueuedConnection);
    }
    void disconnectMediaUpdate() {
        if (waitingForMedia && media)
            signal_handler_disconnect(obs_source_get_signal_handler(media), "update", mediaUpdated, this);
        waitingForMedia = false;
    }
    void processSaved() {
        const auto replay = pendingReplay;
        phase = Processing;
        const auto request = generation;
        replay_status("Writing InstantReplay and numbered backup…");
        backups.setMaxThreadCount(1);
        backups.start([this, replay, backupPath = backup, folder = replayFolder, backupEnabled = backupRequested, request] {
            replay_files::Result result;
            std::string error;
            try {
                result = replay_files::publish(replay, folder, backupPath);
                if (backupEnabled && backupPath.empty()) result.warning = "Backup enabled but no active match / backup folder selected; no backup written.";
            }
            catch (const std::exception &e) { error = e.what(); }
            QMetaObject::invokeMethod(this, [this, result, error, request, replay]() mutable {
                if (request != generation || phase != Processing) return;
                if (!result.backupFile.empty()) replay_backup_saved();
                if (!error.empty() || !result.error.empty()) {
                    if (error.empty()) error = result.error;
                    replay_status("Cannot write fixed replay: " + error + ". OBS original retained at " + replay + (result.backupFile.empty() ? "" : ". Backup saved at " + result.backupFile));
                    reset(false); return;
                }
                replay_status("Main replay saved at " + result.path + (result.backupFile.empty() ? "" : ". Backup saved at " + result.backupFile) + (result.warning.empty() ? "" : ". " + result.warning));
                heldMediaFile.clear();
                lastReplay = result.path;
                selectedReplay.clear();
                // Arm the Media Source for normal scene activation, without changing scenes.
                if (media && supportedReplaySource(media))
                    configureMedia(result.path);
                if (autoSwitch && canPlay) {
                    play(result.path);
                } else {
                    reset(false);
                    // If the operator entered the replay scene while saving, play there now.
                    sceneChanged();
                }
            }, Qt::QueuedConnection);
        });
    }
    void configureMedia(const std::string &replay) {
        obs_data_t *settings = obs_data_create();
        obs_data_set_string(settings, "local_file", replay.c_str());
        obs_data_set_int(settings, "speed_percent", speedPercent);
        obs_data_set_bool(settings, "is_local_file", true);
        obs_data_set_bool(settings, "close_when_inactive", true);
        obs_data_set_bool(settings, "restart_on_activate", true);
        obs_data_set_bool(settings, "looping", false);
        obs_source_update(media, settings);
        appliedSpeed = speedPercent;
        obs_data_release(settings);
    }
    void play(const std::string &replay, bool manual = false) {
        configureMedia(replay);
        phase = TransitionIn;
        if (!manual) obs_frontend_set_current_scene(scene);
        obs_source_media_restart(media);
        timer.start(pause);
    }
    void playLatest(bool manual = false, const std::string &clip = "", bool library = false) {
        if (!get_enabled()) { replay_status("Enable Instant Replay in Replay settings first."); return; }
        if (phase == Saving || phase == ReleasingMedia || phase == Processing) {
            replay_status("A replay file is being saved. Try playing the clip after the save finishes."); return;
        }
        std::error_code selectionError;
        if (!clip.empty() && !fs::is_regular_file(fs::u8path(clip), selectionError)) {
            replay_status("This clip has moved or been deleted. Refresh the list."); return;
        }
        // Preserve the original return scene when replacing an automatic replay.
        std::unique_ptr<obs_source_t, decltype(&obs_source_release)> returnScene(
            !manual && original ? obs_source_get_ref(original) : nullptr, obs_source_release);
        if (media) obs_source_media_stop(media);
        reset(false); // Cancel old end timers, speed seeks and audio muting.
        if (!replay_ensure_player()) return;
        scene = obs_get_source_by_name(get_scene().c_str());
        media = obs_get_source_by_name(get_replay_source_name().c_str());
        original = obs_frontend_get_current_scene();
        if (!scene || !obs_scene_from_source(scene) || !media || !original ||
            !supportedReplaySource(media)) {
            replay_status("Cannot play: select a valid replay scene and Media Source.");
            reset(false); return;
        }
        std::string replay = clip.empty() ? lastReplay : clip;
        if (replay.empty()) {
            auto *settings = obs_source_get_settings(media);
            replay = obs_data_get_string(settings, "local_file");
            obs_data_release(settings);
        }
        std::error_code error;
        if (replay.empty() || !fs::is_regular_file(fs::u8path(replay), error)) {
            replay_status("No saved replay available. Save a replay first.");
            reset(false); return;
        }
        if (library) {
            selectedReplay = replay;
            // Arm a hidden source for manual scene activation without muting live audio.
            if (manual && original != scene) {
                configureMedia(replay);
                reset(false);
                replay_status("Selected " + replay + ". Enter the replay scene to show it.");
                return;
            }
        }
        libraryPlayback = library;
        playbackTicks = 0;
        // Manual scene selection stays on that scene; hotkey playback returns to live.
        if (manual || original == scene) {
            obs_source_release(original); original = manual ? nullptr : returnScene.release();
        }
        pause = std::max(0, get_pause());
        length = std::max(1, get_replay());
        desktop = get_D_mute(); aux = get_A_mute();
        replay_status("Playing " + replay);
        play(replay, manual);
    }
    void sceneChanged() {
        if (phase == Saving || phase == ReleasingMedia || phase == Processing) { canPlay = false; return; }
        auto *current = obs_frontend_get_current_scene();
        if (phase == TransitionIn || phase == Playing) {
            const bool left = current != scene;
            obs_source_release(current);
            if (left) reset(false);
            return;
        }
        bool entered = phase == Idle && current && get_scene() == obs_source_get_name(current);
        obs_source_release(current);
        if (entered) playLatest(true, selectedReplay, !selectedReplay.empty());
    }
    void advance() {
        timer.stop();
        switch (phase) {
        case ReleasingMedia:
            replay_status("Could not release replay source in time. OBS original retained at " + pendingReplay);
            reset(false); break;
        case Saving:
            replay_status("OBS did not report a saved replay before the timeout. Check the OBS log, recording folder permissions, and Replay Buffer settings.");
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
            if (libraryPlayback) playbackPoll.start();
            else timer.start(std::max(1, (length - 2 * pause) * 100 / speedPercent));
            break;
        case Playing:
            playbackPoll.stop();
            if (!original) { reset(false); break; }
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
obs_hotkey_id playHotkeyId = OBS_INVALID_HOTKEY_ID;
void frontendEvent(obs_frontend_event event, void *) {
    if (!controller) return;
    // OBS frontend events run on the UI thread. Manual scene changes cancel replay
    // without pulling the operator back to a scene they have deliberately left.
    if (event == OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED) controller->saved();
    else if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) controller->sceneChanged();
    else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP ||
               event == OBS_FRONTEND_EVENT_EXIT || event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN) {
        controller->reset(false);
        controller->lastReplay.clear();
        controller->selectedReplay.clear();
        controller->speedPercent = 100;
    }
    else if (event == OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED && controller->phase == ReplayController::Saving)
        controller->reset(false);
}
}

void replayStartup() {
    controller = new ReplayController;
    hotkeyId = obs_hotkey_register_frontend("Instant Replay", "Instant Replay XT: Save replay", instant_replay_hotkey, nullptr);
    playHotkeyId = obs_hotkey_register_frontend("Instant Replay Play", "Instant Replay XT: Play latest replay", instant_replay_play_hotkey, nullptr);
    obs_frontend_add_event_callback(frontendEvent, nullptr);
}
void replayShutdown() {
    obs_frontend_remove_event_callback(frontendEvent, nullptr);
    if (hotkeyId != OBS_INVALID_HOTKEY_ID) obs_hotkey_unregister(hotkeyId);
    if (playHotkeyId != OBS_INVALID_HOTKEY_ID) obs_hotkey_unregister(playHotkeyId);
    playHotkeyId = OBS_INVALID_HOTKEY_ID;
    hotkeyId = OBS_INVALID_HOTKEY_ID;
    delete controller; controller = nullptr;
}
void instant_replay_hotkey(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed) {
    if (pressed && controller)
        QMetaObject::invokeMethod(controller, [] { if (controller) controller->start(); }, Qt::QueuedConnection);
}
void instant_replay_play_hotkey(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed) {
    if (pressed && controller)
        QMetaObject::invokeMethod(controller, [] { if (controller) controller->playLatest(); }, Qt::QueuedConnection);
}
void instant_replay_save(obs_data_t *save_data, bool saving, void *) {
    if (hotkeyId == OBS_INVALID_HOTKEY_ID) return;
    if (saving) {
        auto *obj = obs_data_create();
        if (controller) obs_data_set_string(obj, "lastReplayFile", controller->lastReplay.c_str());
        auto *keys = obs_hotkey_save(hotkeyId);
        obs_data_set_array(obj, "hotkey", keys);
        auto *playKeys = obs_hotkey_save(playHotkeyId);
        obs_data_set_array(obj, "play_hotkey", playKeys);
        obs_data_array_release(playKeys);
        obs_data_set_obj(save_data, "InstantReplay", obj);
        obs_data_array_release(keys);
        obs_data_release(obj);
    } else {
        auto *obj = obs_data_get_obj(save_data, "InstantReplay");
        if (controller) controller->lastReplay = obs_data_get_string(obj, "lastReplayFile");
        auto *keys = obs_data_get_array(obj, "hotkey");
        obs_hotkey_load(hotkeyId, keys);
        auto *playKeys = obs_data_get_array(obj, "play_hotkey");
        obs_hotkey_load(playHotkeyId, playKeys);
        obs_data_array_release(playKeys);
        obs_data_array_release(keys);
        obs_data_release(obj);
    }
}

void replay_play_clip(const std::string &path, bool switchScene) {
    if (controller) controller->playLatest(!switchScene, path, true);
}
void replay_set_speed(int percent) {
    if (controller) controller->setSpeed(percent);
}
std::string replay_current_file() { return controller ? controller->lastReplay : ""; }

bool replay_is_busy() { return controller && (controller->phase != ReplayController::Idle || controller->speedSource); }

bool replay_prepare_player_change() {
    if (!controller) return true;
    if (controller->phase == ReplayController::Saving || controller->phase == ReplayController::ReleasingMedia || controller->phase == ReplayController::Processing) return false;
    if (controller->media) obs_source_media_stop(controller->media);
    controller->reset(false);
    return true;
}
