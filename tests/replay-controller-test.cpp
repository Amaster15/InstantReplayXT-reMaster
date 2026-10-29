// Runs real libobs source/data ownership with a fake frontend (no OBS GUI needed).
#include "../InstantReplay.cpp"
#include <cassert>
#include <QTemporaryDir>
#include <fstream>
static obs_source_t *currentScene;
static int saveRequests;
static bool enabled = true;
static std::string backupDir;
static std::string replayPath;
bool get_enabled() { return enabled; }
int get_wait() { return 1000; }
int get_pause() { return 300; }
int get_replay() { return 100; } // Shorter than two transitions: must not underflow.
std::string get_scene() { return "Replay"; }
std::string get_replay_source_name() { return "Media"; }
bool get_D_mute() { return true; }
bool get_A_mute() { return true; }
bool get_backup_enabled() { return !backupDir.empty(); }
std::string get_backup_path() { return backupDir; }
extern "C" {
bool obs_frontend_replay_buffer_active() { return true; }
void obs_frontend_replay_buffer_save() { ++saveRequests; }
obs_source_t *obs_frontend_get_current_scene() { return obs_source_get_ref(currentScene); }
void obs_frontend_set_current_scene(obs_source_t *s) { currentScene = s; }
char *obs_frontend_get_last_replay() { return bstrdup(replayPath.c_str()); }
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
        c.start(); assert(c.phase == ReplayController::Saving && saveRequests == 1);
        c.start(); assert(saveRequests == 1); // repeated trigger ignored
        c.advance(); assert(c.phase == ReplayController::Idle); // save timeout
        c.start(); c.saved(); assert(c.phase == ReplayController::TransitionIn);
        assert(currentScene == obs_scene_get_source(replayScene));
        c.advance(); assert(obs_source_muted(audio));
        assert(c.timer.interval() == 1); // no signed-to-unsigned multi-day wait
        c.advance(); assert(currentScene == obs_scene_get_source(live));
        c.advance(); assert(!obs_source_muted(audio) && c.phase == ReplayController::Idle);
        obs_source_set_muted(audio, true);
        c.start(); c.saved(); c.advance(); c.reset(false);
        assert(obs_source_muted(audio)); // preserve already-muted sources
        currentScene = obs_scene_get_source(live);
        obs_source_set_muted(audio, false);
        controller = &c;
        c.start(); c.saved(); c.advance();
        currentScene = obs_scene_get_source(live);
        frontendEvent(OBS_FRONTEND_EVENT_SCENE_CHANGED, nullptr);
        assert(c.phase == ReplayController::Idle && !obs_source_muted(audio));
        c.start(); c.saved(); c.advance();
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
        c.start(); c.saved();
        c.backups.waitForDone();
        assert(fs::exists(backupPath / fs::u8path(u8"réplay_2.mkv")));
        c.reset(false);
    }
    obs_set_output_source(3, nullptr);
    obs_source_release(audio);
    obs_source_release(mediaSource);
    obs_scene_release(replayScene);
    obs_scene_release(live);
    obs_shutdown();
}
