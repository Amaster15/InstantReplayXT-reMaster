// Runs real libobs source/data ownership with a fake frontend (no OBS GUI needed).
#include "../InstantReplay.cpp"
#include <cassert>
#include <QTemporaryDir>
#include <fstream>
static obs_source_t *currentScene;
static int saveRequests;
static bool enabled = true, active = true, validScene = true;
static std::string status;
static std::string outputDir;
std::string get_replay_path() { return outputDir; }
void replay_status(const std::string &message) { status = message; }
static std::string backupDir;
static std::string replayPath;
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
bool obs_frontend_replay_buffer_active() { return active; }
void obs_frontend_replay_buffer_save() { ++saveRequests; }
obs_source_t *obs_frontend_get_current_scene() { return obs_source_get_ref(currentScene); }
void obs_frontend_set_current_scene(obs_source_t *s) { currentScene = s; }
char *obs_frontend_get_last_replay() { return bstrdup(replayPath.c_str()); }
}
static void finishSave(ReplayController &c) {
    std::ofstream(fs::u8path(replayPath)) << "test replay";
    c.saved();
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
    sourceInfo.output_flags = OBS_SOURCE_AUDIO;
    sourceInfo.get_name = [](void *) { return "Test Media"; };
    sourceInfo.create = [](obs_data_t *, obs_source_t *) -> void * { return new int(0); };
    sourceInfo.destroy = [](void *p) { delete static_cast<int *>(p); };
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
        assert(status.find("Playback skipped") != std::string::npos);
        assert(fs::exists(backupPath / "InstantReplay_2.mkv"));
        validScene = true;
        active = false; c.start(); assert(saveRequests == before + 1);
        assert(status.find("stopped") != std::string::npos);
        active = true;
        assert(fs::exists(testDir / "InstantReplay.mkv"));
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
        try { replay_files::publish(replayPath, blocked.u8string(), ""); }
        catch (const std::exception &) { failed = true; }
        assert(failed && fs::exists(fs::u8path(replayPath)));
        // A backup failure is visible but does not prevent the main save.
        result = replay_files::publish(replayPath, (testDir / "custom folder").u8string(), blocked.u8string());
        assert(!result.warning.empty() && fs::exists(fs::u8path(result.path)));
        assert(fs::exists(fs::u8path(replayPath)));
    }
    obs_set_output_source(3, nullptr);
    obs_source_release(audio);
    obs_source_release(mediaSource);
    obs_scene_release(replayScene);
    obs_scene_release(live);
    // No audio/video threads exist in this shim; explicitly drain deferred destruction.
    obs_queue_task(OBS_TASK_DESTROY, [](void *) {}, nullptr, true);
    obs_shutdown();
}
