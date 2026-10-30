// Runs real libobs source/data ownership with a fake frontend (no OBS GUI needed).
#include "../InstantReplay.cpp"
#include <cassert>
#include <QTemporaryDir>
#include <fstream>
static obs_source_t *currentScene;
static int saveRequests;
static int64_t mediaPosition = 0;
static obs_media_state mediaState = OBS_MEDIA_STATE_STOPPED;
static bool enabled = true, active = true, validScene = true, autoSwitch = true;
bool get_auto_switch() { return autoSwitch; }
static std::string status;
static std::string outputDir;
std::string get_replay_path() { return outputDir; }
void replay_backup_saved() {}
void replay_status(const std::string &message) { status = message; }
static std::string backupDir;
static std::string replayPath;
bool replay_ensure_player() { return true; }
bool get_enabled() { return enabled; }
int get_wait() { return 1000; }
int get_pause() { return 300; }
int get_replay() { return 100; } // Shorter than two transitions: must not underflow.
std::string get_scene() { return validScene ? "Replay" : "Missing"; }
std::string get_replay_source_name() { return "Media"; }
bool get_D_mute() { return true; }
bool get_A_mute() { return true; }
bool get_backup_enabled() { return !backupDir.empty(); }
std::string get_backup_path() { return backupDir; }
extern "C" {
// No video thread in this test frontend: observe media commands at the public API.
void obs_source_media_set_time(obs_source_t *, int64_t value) { mediaPosition = value; }
void obs_source_media_play_pause(obs_source_t *, bool pause) { mediaState = pause ? OBS_MEDIA_STATE_PAUSED : OBS_MEDIA_STATE_PLAYING; }
bool obs_frontend_replay_buffer_active() { return active; }
void obs_frontend_replay_buffer_save() { ++saveRequests; }
obs_source_t *obs_frontend_get_current_scene() { return obs_source_get_ref(currentScene); }
void obs_frontend_set_current_scene(obs_source_t *s) { currentScene = s; }
char *obs_frontend_get_last_replay() { return bstrdup(replayPath.c_str()); }
}
static void finishSave(ReplayController &c) {
    std::ofstream(fs::u8path(replayPath)) << "test replay";
    c.saved();
    QCoreApplication::processEvents(); // Acknowledge asynchronous Media Source release.
    c.backups.waitForDone();
    QCoreApplication::processEvents();
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    assert(temp.isValid());
    auto testDir = fs::u8path(temp.path().toUtf8().constData());
    replayPath = (testDir / fs::u8path(u8"réplay.mkv")).u8string();
    assert(obs_startup("en-US", nullptr, nullptr));
    obs_source_info sourceInfo = {};
    sourceInfo.id = "ffmpeg_source";
    sourceInfo.type = OBS_SOURCE_TYPE_INPUT;
    sourceInfo.output_flags = OBS_SOURCE_AUDIO | OBS_SOURCE_CONTROLLABLE_MEDIA;
    sourceInfo.get_name = [](void *) { return "Test Media"; };
    sourceInfo.create = [](obs_data_t *, obs_source_t *) -> void * { return new int(0); };
    sourceInfo.update = [](void *, obs_data_t *) {};
    sourceInfo.destroy = [](void *p) { delete static_cast<int *>(p); };
    sourceInfo.media_get_time = [](void *) -> int64_t { return mediaPosition; };
    sourceInfo.media_set_time = [](void *, int64_t value) { mediaPosition = value; };
    sourceInfo.media_get_duration = [](void *) -> int64_t { return 20000; };
    sourceInfo.media_get_state = [](void *) { return mediaState; };
    sourceInfo.media_play_pause = [](void *, bool pause) { mediaState = pause ? OBS_MEDIA_STATE_PAUSED : OBS_MEDIA_STATE_PLAYING; };
    obs_register_source(&sourceInfo);
    auto *live = obs_scene_create("Live");
    auto *replayScene = obs_scene_create("Replay");
    auto *mediaSource = obs_source_create("ffmpeg_source", "Media", nullptr, nullptr);
    auto *audio = obs_source_create("ffmpeg_source", "Mic", nullptr, nullptr);
    obs_set_output_source(3, audio);
    obs_source_set_muted(audio, false);
    currentScene = obs_scene_get_source(live);
    {
        ReplayController c;
        controller = &c;
        instant_replay_hotkey(nullptr, OBS_INVALID_HOTKEY_ID, nullptr, false);
        QCoreApplication::processEvents(); assert(saveRequests == 0);
        instant_replay_hotkey(nullptr, OBS_INVALID_HOTKEY_ID, nullptr, true);
        QCoreApplication::processEvents();
        controller = nullptr;
        assert(c.phase == ReplayController::Saving && saveRequests == 1);
        c.start(); assert(saveRequests == 1); // repeated trigger ignored
        c.advance(); assert(c.phase == ReplayController::Idle); // save timeout
        c.start(); finishSave(c); assert(c.phase == ReplayController::TransitionIn);
        assert(currentScene == obs_scene_get_source(replayScene));
        c.advance(); assert(obs_source_muted(audio));
        assert(c.timer.interval() == 1); // no signed-to-unsigned multi-day wait
        c.advance(); assert(currentScene == obs_scene_get_source(live));
        c.advance(); assert(!obs_source_muted(audio) && c.phase == ReplayController::Idle);
        obs_source_set_muted(audio, true);
        c.start(); finishSave(c); c.advance(); c.reset(false);
        assert(obs_source_muted(audio)); // preserve already-muted sources
        currentScene = obs_scene_get_source(live);
        obs_source_set_muted(audio, false);
        controller = &c;
        c.start(); finishSave(c); c.advance();
        currentScene = obs_scene_get_source(live);
        frontendEvent(OBS_FRONTEND_EVENT_SCENE_CHANGED, nullptr);
        assert(c.phase == ReplayController::Idle && !obs_source_muted(audio));
        c.start(); finishSave(c); c.advance();
        frontendEvent(OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP, nullptr);
        assert(c.phase == ReplayController::Idle && !obs_source_muted(audio));
        controller = nullptr;
        currentScene = obs_scene_get_source(live);
        enabled = false; c.start(); assert(c.phase == ReplayController::Idle); enabled = true;
        auto backupPath = testDir / fs::u8path(u8"réplays");
        fs::create_directories(backupPath);
        std::ofstream(fs::u8path(replayPath)) << "test replay";
        std::ofstream(backupPath / fs::u8path(u8"réplay.mkv")) << "existing";
        backupDir = backupPath.u8string();
        c.start(); finishSave(c);
        c.backups.waitForDone();
        assert(fs::exists(backupPath / fs::u8path("InstantReplay_1.mkv")));
        c.reset(false);
        currentScene = obs_scene_get_source(live);
        validScene = false;
        const int before = saveRequests;
        c.start(); assert(saveRequests == before + 1); finishSave(c);
        assert(c.phase == ReplayController::Idle);
        assert(status.find("Main replay saved") != std::string::npos);
        assert(fs::exists(backupPath / "InstantReplay_2.mkv"));
        validScene = true;
        active = false; c.start(); assert(saveRequests == before + 1);
        assert(status.find("stopped") != std::string::npos);
        active = true;
        assert(fs::exists(testDir / "InstantReplay.mkv"));
        // Save-only mode arms the media source and never switches away from live.
        autoSwitch = false;
        currentScene = obs_scene_get_source(live);
        c.start(); finishSave(c);
        assert(c.phase == ReplayController::Idle);
        assert(currentScene == obs_scene_get_source(live));
        auto *armed = obs_source_get_settings(mediaSource);
        assert(std::string(obs_data_get_string(armed, "local_file")) == c.lastReplay);
        obs_data_release(armed);
        const int savedCount = saveRequests;
        active = false; // Playing an existing clip does not need the buffer running.
        controller = &c;
        instant_replay_play_hotkey(nullptr, OBS_INVALID_HOTKEY_ID, nullptr, false);
        QCoreApplication::processEvents(); assert(c.phase == ReplayController::Idle);
        instant_replay_play_hotkey(nullptr, OBS_INVALID_HOTKEY_ID, nullptr, true);
        QCoreApplication::processEvents();
        assert(c.phase == ReplayController::TransitionIn && saveRequests == savedCount);
        assert(currentScene == obs_scene_get_source(replayScene));
        c.advance(); c.advance(); c.advance();
        assert(currentScene == obs_scene_get_source(live));
        // Manually entering the scene restarts playback but never forces a scene return.
        currentScene = obs_scene_get_source(replayScene);
        frontendEvent(OBS_FRONTEND_EVENT_SCENE_CHANGED, nullptr);
        assert(c.phase == ReplayController::TransitionIn && !c.original);
        c.advance(); c.advance();
        assert(c.phase == ReplayController::Idle && currentScene == obs_scene_get_source(replayScene));
        assert(saveRequests == savedCount);
        // After restart, use the clip saved in Media Source settings.
        c.lastReplay.clear();
        currentScene = obs_scene_get_source(live);
        c.playLatest(); assert(c.phase == ReplayController::TransitionIn);
        currentScene = obs_scene_get_source(live);
        frontendEvent(OBS_FRONTEND_EVENT_SCENE_CHANGED, nullptr);
        assert(c.phase == ReplayController::Idle);
        controller = nullptr;
        active = true;
        // Library selection can arm a clip without changing scenes or muting live audio.
        auto chosen = (backupPath / "InstantReplay_2.mkv").u8string();
        currentScene = obs_scene_get_source(live);
        c.playLatest(true, chosen, true);
        assert(c.phase == ReplayController::Idle && c.selectedReplay == chosen);
        assert(currentScene == obs_scene_get_source(live) && !obs_source_muted(audio));
        currentScene = obs_scene_get_source(replayScene);
        c.sceneChanged();
        assert(c.libraryPlayback && !c.original);
        c.advance(); assert(c.playbackPoll.isActive() && !c.timer.isActive());
        c.reset(false);
        currentScene = obs_scene_get_source(live);
        c.playLatest(false, chosen, true);
        assert(currentScene == obs_scene_get_source(replayScene));
        c.advance();
        // A new row click replaces a running clip and preserves the live return scene.
        c.playLatest(false, chosen, true);
        assert(c.phase == ReplayController::TransitionIn && c.original == obs_scene_get_source(live));
        c.advance();
        const auto preserved = c.media;
        c.playLatest(false, (testDir / "missing.mkv").u8string(), true);
        assert(c.phase == ReplayController::Playing && c.media == preserved);
        mediaState = OBS_MEDIA_STATE_PLAYING; mediaPosition = 7500;
        c.setSpeed(50); assert(c.speedSource);
        QCoreApplication::processEvents();
        assert(c.speedUpdated);
        mediaPosition = 0; // The native source reloads on a speed change.
        QMetaObject::invokeMethod(&c.speedWait, "timeout", Qt::DirectConnection);
        assert(!c.speedSource && mediaPosition == 7500);
        auto *speedSettings = obs_source_get_settings(mediaSource);
        assert(obs_data_get_int(speedSettings, "speed_percent") == 50);
        obs_data_release(speedSettings);
        mediaState = OBS_MEDIA_STATE_PAUSED; mediaPosition = 9000;
        c.setSpeed(75); QCoreApplication::processEvents();
        mediaState = OBS_MEDIA_STATE_PLAYING; mediaPosition = 0;
        QMetaObject::invokeMethod(&c.speedWait, "timeout", Qt::DirectConnection);
        assert(mediaPosition == 9000 && mediaState == OBS_MEDIA_STATE_PAUSED);
        // Standard Media Source seeks without unpausing or reopening it.
        auto standardPosition = c.position();
        assert(standardPosition.duration == 20000);
        assert(c.seek(2000, standardPosition.clip));
        assert(mediaPosition == 2000 && mediaState == OBS_MEDIA_STATE_PAUSED);
        // Replacement also cancels an unfinished Standard Media Source speed seek.
        c.setSpeed(80); assert(c.speedSource);
        c.playLatest(false, chosen, true);
        assert(!c.speedSource && c.phase == ReplayController::TransitionIn);
        assert(c.original == obs_scene_get_source(live));
        c.advance();
        mediaState = OBS_MEDIA_STATE_ENDED;
        QMetaObject::invokeMethod(&c.playbackPoll, "timeout", Qt::DirectConnection);
        assert(currentScene == obs_scene_get_source(live) && c.phase == ReplayController::TransitionOut);
        c.advance(); assert(!obs_source_muted(audio));
        c.setSpeed(100);
        c.playLatest(false, (testDir / "missing.mkv").u8string(), true);
        assert(c.phase == ReplayController::Idle && currentScene == obs_scene_get_source(live));
        // Play without switching replaces immediately and stays on the replay scene.
        currentScene = obs_scene_get_source(replayScene);
        c.playLatest(true, chosen, true); c.advance();
        c.playLatest(true, chosen, true);
        assert(c.phase == ReplayController::TransitionIn && !c.original);
        controller = &c;
        assert(replay_prepare_player_change());
        assert(c.phase == ReplayController::Idle && !c.timer.isActive() && !c.playbackPoll.isActive());
        controller = nullptr;
        // Never replace a file until the Media Source acknowledges closing it.
        currentScene = obs_scene_get_source(live);
        c.start();
        std::ofstream(fs::u8path(replayPath)) << "release timeout retained";
        c.saved(); assert(c.phase == ReplayController::ReleasingMedia);
        c.playLatest(false, chosen, true); assert(c.phase == ReplayController::ReleasingMedia);
        controller = &c; assert(!replay_prepare_player_change()); controller = nullptr;
        c.advance(); // Simulate an unresponsive video update: retain the original and restore source.
        assert(c.phase == ReplayController::Idle && fs::exists(fs::u8path(replayPath)));
        QCoreApplication::processEvents();
        assert(c.phase == ReplayController::Idle);
        // Continue after the highest existing number, even with gaps.
        std::ofstream(backupPath / "InstantReplay_9.mkv") << "older backup";
        std::ofstream(fs::u8path(replayPath)) << "new replay";
        auto result = replay_files::publish(replayPath, "", backupDir);
        assert(fs::exists(backupPath / "InstantReplay_10.mkv"));
        std::ifstream fixed(testDir / "InstantReplay.mkv");
        std::string content; std::getline(fixed, content); assert(content == "new replay");
        assert(!fs::exists(fs::u8path(replayPath)));
        // A destination failure must preserve the original OBS save.
        std::ofstream(fs::u8path(replayPath)) << "retained";
        auto blocked = testDir / "not-a-directory";
        std::ofstream(blocked) << "occupied";
        bool failed = false;
        try { failed = !replay_files::publish(replayPath, blocked.u8string(), "").error.empty(); }
        catch (const std::exception &) { failed = true; }
        assert(failed && fs::exists(fs::u8path(replayPath)));
        auto partial = replay_files::publish(replayPath, blocked.u8string(), backupDir);
        assert(!partial.error.empty() && !partial.backupFile.empty());
        assert(fs::exists(fs::u8path(partial.backupFile)) && fs::exists(fs::u8path(replayPath)));
        // A backup failure is visible but does not prevent the main save.
        result = replay_files::publish(replayPath, (testDir / "custom folder").u8string(), blocked.u8string());
        assert(!result.warning.empty() && fs::exists(fs::u8path(result.path)));
        assert(fs::exists(fs::u8path(replayPath)));
    }
    // VLC's public rate procedure must not invoke a source update or seek.
    static int liveRate = 100, liveUpdates = 0;
    static bool liveReverse = false;
    sourceInfo.id = "irxt_vlc_player";
    sourceInfo.create = [](obs_data_t *, obs_source_t *source) -> void * {
        proc_handler_add(obs_source_get_proc_handler(source), "void irxt_set_rate(in int percent, out bool success)",
            [](void *, calldata_t *call) { liveRate = int(calldata_int(call, "percent")); calldata_set_bool(call, "success", true); }, nullptr);
        proc_handler_add(obs_source_get_proc_handler(source), "void irxt_get_direction(out bool reverse)",
            [](void *, calldata_t *call) { calldata_set_bool(call, "reverse", liveReverse); }, nullptr);
        proc_handler_add(obs_source_get_proc_handler(source), "void irxt_set_direction(in bool reverse, out bool success)",
            [](void *, calldata_t *call) { liveReverse = calldata_bool(call, "reverse"); calldata_set_bool(call, "success", true); }, nullptr);
        return new int(0);
    };
    sourceInfo.update = [](void *, obs_data_t *) { ++liveUpdates; };
    obs_register_source(&sourceInfo);
    auto *vlcSource = obs_source_create("irxt_vlc_player", "Live replay", nullptr, nullptr);
    {
        ReplayController c; c.media = obs_source_get_ref(vlcSource); c.phase = ReplayController::Playing;
        c.libraryPlayback = true; mediaPosition = 2345;
        const auto before = liveUpdates;
        c.setSpeed(50); assert(liveRate == 50 && liveUpdates == before && mediaPosition == 2345 && !c.speedSource);
        c.setSpeed(1); assert(liveRate == 1 && liveUpdates == before && mediaPosition == 2345 && !c.speedSource);
        c.setSpeed(175); assert(liveRate == 175 && liveUpdates == before && mediaPosition == 2345);
        mediaState = OBS_MEDIA_STATE_PLAYING;
        c.togglePause(); assert(mediaState == OBS_MEDIA_STATE_PAUSED && c.playbackState() == 2);
        QMetaObject::invokeMethod(&c.playbackPoll, "timeout", Qt::DirectConnection);
        assert(c.phase == ReplayController::Playing && mediaPosition == 2345);
        c.setSpeed(1); assert(c.playbackState() == 2);
        c.togglePause(); assert(mediaState == OBS_MEDIA_STATE_PLAYING && mediaPosition == 2345);
        const auto timeline = c.position();
        assert(timeline.position == 2345 && timeline.duration == 20000);
        assert(c.seek(12000, timeline.clip) && mediaPosition == 12000);
        c.togglePause();
        assert(c.seek(3000, timeline.clip) && mediaPosition == 3000 && c.playbackState() == 2);
        assert(c.seek(-100, timeline.clip) && mediaPosition == 0);
        assert(c.seek(30000, timeline.clip) && mediaPosition == 19999);
        assert(!c.seek(1000, "previous clip") && mediaPosition == 19999);
        c.togglePause(); mediaPosition = 2345;
        // Freeze must also suspend timed replay's automatic scene return.
        c.setSpeed(100); c.libraryPlayback = false; c.timer.start(5000);
        // Qt's coarse timer rounds its deadline; compare with its actual
        // remaining time instead of assuming precisely 5000 milliseconds.
        const int beforeFreeze = c.timer.remainingTime();
        c.togglePause();
        const int frozen = c.frozenRemaining;
        assert(!c.timer.isActive() && frozen > 0 && frozen <= beforeFreeze && frozen >= beforeFreeze - 100);
        c.setSpeed(50); assert(c.frozenRemaining == frozen * 2 && !c.timer.isActive());
        const auto frozenBeforeSeek = c.frozenRemaining;
        assert(c.seek(3345, c.position().clip));
        assert(c.frozenRemaining == frozenBeforeSeek - 2000 && !c.timer.isActive());
        mediaPosition = 2345; // Simulate an asynchronous player's stale time report.
        assert(c.seek(4345, c.position().clip));
        assert(c.frozenRemaining == frozenBeforeSeek - 4000 && !c.timer.isActive());
        c.togglePause(); assert(c.timer.isActive() && c.frozenRemaining == -1);
        c.setSpeed(100); c.timer.start(5000); c.timedEndPosition = -1;
        mediaPosition = 5000;
        c.toggleDirection(); assert(c.direction() == 1 && !c.timer.isActive() && c.frozenRemaining > 0);
        const auto endPosition = c.timedEndPosition;
        mediaPosition = 3000;
        c.togglePause(); c.togglePause(); assert(!c.timer.isActive());
        c.toggleDirection(); assert(c.direction() == 0 && c.timer.isActive());
        assert(c.timer.remainingTime() > 6500 && c.timedEndPosition == endPosition);
        c.phase = ReplayController::Saving;
        assert(c.direction() == -1);
        assert(c.playbackState() == 0); c.togglePause(); assert(mediaState == OBS_MEDIA_STATE_PLAYING);
        assert(!c.seek(1000, timeline.clip));

        c.reset(false);
    }
    obs_source_release(vlcSource);
    obs_set_output_source(3, nullptr);
    obs_source_release(audio);
    obs_source_release(mediaSource);
    obs_scene_release(replayScene);
    obs_scene_release(live);
    // No audio/video threads exist in this shim; explicitly drain deferred destruction.
    obs_queue_task(OBS_TASK_DESTROY, [](void *) {}, nullptr, true);
    obs_shutdown();
}
