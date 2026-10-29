// Exercise real VLC decoding and rate changes without needing OBS's renderer.
#include <obs-module.h>
void testVideo(obs_source_t *, const obs_source_frame *);
void testAudio(obs_source_t *, const obs_source_audio *);
#define obs_source_output_video testVideo
#define obs_source_output_audio testAudio
#include "../ReplayPlayer.cpp"
#undef obs_source_output_video
#undef obs_source_output_audio
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QThread>
#include <cassert>
#include <atomic>
static std::atomic<unsigned> videoFrames{0}, audioFrames{0}, uniqueFrames{0};
static uint64_t lastHash = 0;
static std::atomic<uint64_t> lastVideo{0}, lastAudio{0};
void testVideo(obs_source_t *, const obs_source_frame *frame) {
    if (!frame) return;
    if (videoFrames == 0) std::printf("Decoded frame: %ux%u, stride %u\n", frame->width, frame->height, frame->linesize[0]);
    assert(frame->data[0] && frame->width == 160 && frame->height == 90);
    uint64_t hash = 1469598103934665603ULL;
    for (unsigned y = 0; y < frame->height; ++y)
        for (unsigned x = 0; x < frame->width * 4; ++x)
            hash = (hash ^ frame->data[0][y * frame->linesize[0] + x]) * 1099511628211ULL;
    if (hash != lastHash) { lastHash = hash; ++uniqueFrames; }
    assert(frame->timestamp >= lastVideo.exchange(frame->timestamp)); ++videoFrames;
}
void testAudio(obs_source_t *, const obs_source_audio *audio) {
    assert(audio->samples_per_sec == 48000 && audio->speakers == SPEAKERS_STEREO);
    assert(audio->timestamp > 0); lastAudio = audio->timestamp; audioFrames += audio->frames;
}
static bool setRate(Player &s, int percent) { std::lock_guard<std::mutex> guard(s.mutex); return s.rate(percent); }
static void waitPlaying(Player &s) {
    for (int i = 0; i < 500 && vlc.libvlc_media_player_get_state(s.player) != 3; ++i) QThread::msleep(20);
    std::fprintf(stderr, "VLC %s playback state %d, input opens %llu\n", vlc.libvlc_get_version(), vlc.libvlc_media_player_get_state(s.player), (unsigned long long)s.opens);
    assert(vlc.libvlc_media_player_get_state(s.player) == 3);
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv); assert(argc == 2);
    assert(obs_startup("en-US", nullptr, nullptr));
    assert(replay_player_available());
    QTemporaryDir temp; assert(temp.isValid());
    const auto path = temp.path() + "/replay test.mp4";
    assert(QFile::copy(QString::fromLocal8Bit(argv[1]), path));
    {
        Player s(nullptr); assert(s.player);
        auto *settings = obs_data_create();
        obs_data_set_string(settings, "local_file", path.toUtf8().constData());
        obs_data_set_int(settings, "speed_percent", 100);
        s.update(settings); { std::lock_guard<std::mutex> guard(s.mutex); s.start(); } waitPlaying(s); QThread::msleep(1200);
        assert(s.opens == 1); assert(videoFrames > 10 && audioFrames > 10000);
        const auto before = vlc.libvlc_media_player_get_time(s.player);
        assert(before > 500);
        assert(setRate(s, 50)); QThread::msleep(1600);
        const auto slow = vlc.libvlc_media_player_get_time(s.player);
        assert(slow > before && slow - before < 1300);
        assert(s.opens == 1 && vlc.libvlc_media_player_get_state(s.player) == 3);
        assert(setRate(s, 150)); QThread::msleep(1600);
        const auto fast = vlc.libvlc_media_player_get_time(s.player);
        assert(fast - slow > 1500 && s.opens == 1);
        // VLC reports the requested rate even when its input clamps it.
        // Count distinct decoded pictures, excluding VLC's paused redraws.
        for (int from : {100, 50}) {
            assert(setRate(s, from)); QThread::msleep(800);
            const auto position = vlc.libvlc_media_player_get_time(s.player);
            assert(setRate(s, 1)); QThread::msleep(500);
            const auto first = uniqueFrames.load();
            const auto audioBefore = audioFrames.load();
            QThread::msleep(8000);
            const auto advanced = uniqueFrames.load() - first;
            std::fprintf(stderr, "%d%% to 1%%: %u distinct frames in 8 seconds\n", from, advanced);
            assert(advanced >= 1 && advanced <= 4);
            assert(audioFrames.load() == audioBefore);
            assert(state(&s) == OBS_MEDIA_STATE_PLAYING && s.opens == 1);
            assert(vlc.libvlc_media_player_get_time(s.player) >= position - 100);
        }
        // Updating only speed in source properties also keeps the input open.
        obs_data_set_int(settings, "speed_percent", 75); s.update(settings);
        assert(s.opens == 1 && vlc.libvlc_media_player_get_time(s.player) >= fast);
        { std::lock_guard<std::mutex> guard(s.mutex); s.pause(true); } QThread::msleep(100);
        assert(setRate(s, 25)); assert(vlc.libvlc_media_player_get_state(s.player) == 4);
        assert(setRate(s, 1)); QThread::msleep(500);
        const auto pausedFrames = uniqueFrames.load();
        QThread::msleep(1500);
        assert(uniqueFrames.load() == pausedFrames && state(&s) == OBS_MEDIA_STATE_PAUSED);
        assert(setRate(s, 100)); QThread::msleep(100);
        assert(state(&s) == OBS_MEDIA_STATE_PAUSED);
        { std::lock_guard<std::mutex> guard(s.mutex); s.pause(false); }
        QThread::msleep(600);
        assert(uniqueFrames.load() > pausedFrames);
        // Restart at 1% and verify the first frames are paced too.
        { std::lock_guard<std::mutex> guard(s.mutex);
          s.stopSteps(); vlc.libvlc_media_player_stop(s.player);
          assert(s.rate(1)); s.start(); }
        QThread::msleep(1000);
        const auto initialFrames = uniqueFrames.load();
        QThread::msleep(8000);
        const auto initialAdvance = uniqueFrames.load() - initialFrames;
        assert(initialAdvance >= 1 && initialAdvance <= 4);
        assert(state(&s) == OBS_MEDIA_STATE_PLAYING);
        // Release the input before overwriting the fixed replay file (Windows).
        obs_data_set_string(settings, "local_file", ""); s.update(settings);
        assert(QFile::rename(path, path + ".old"));
        assert(QFile::remove(path + ".old"));
        obs_data_release(settings);
        assert(lastAudio > 0);
    }
    replay_player_shutdown(); obs_shutdown();
    std::puts("Real VLC rate changes, video/audio decoding, pause and file release passed.");
}
