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
static std::atomic<unsigned> videoFrames{0}, audioFrames{0};
static std::atomic<uint64_t> lastVideo{0}, lastAudio{0};
void testVideo(obs_source_t *, const obs_source_frame *frame) {
    if (!frame) return;
    if (videoFrames == 0) std::printf("Decoded frame: %ux%u, stride %u\n", frame->width, frame->height, frame->linesize[0]);
    assert(frame->data[0] && frame->width == 160 && frame->height == 90);
    assert(frame->timestamp >= lastVideo.exchange(frame->timestamp)); ++videoFrames;
}
void testAudio(obs_source_t *, const obs_source_audio *audio) {
    assert(audio->samples_per_sec == 48000 && audio->speakers == SPEAKERS_STEREO);
    assert(audio->timestamp > 0); lastAudio = audio->timestamp; audioFrames += audio->frames;
}
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
        s.update(settings); s.start(); waitPlaying(s); QThread::msleep(1200);
        assert(s.opens == 1); assert(videoFrames > 10 && audioFrames > 10000);
        const auto before = vlc.libvlc_media_player_get_time(s.player);
        assert(before > 500);
        assert(s.rate(50)); QThread::msleep(1600);
        const auto slow = vlc.libvlc_media_player_get_time(s.player);
        assert(slow > before && slow - before < 1300);
        assert(s.opens == 1 && vlc.libvlc_media_player_get_state(s.player) == 3);
        assert(s.rate(150)); QThread::msleep(1600);
        const auto fast = vlc.libvlc_media_player_get_time(s.player);
        assert(fast - slow > 1500 && s.opens == 1);
        // Extreme slow motion must keep the same input open and accept 1%.
        assert(s.rate(1)); QThread::msleep(500);
        assert(std::abs(vlc.libvlc_media_player_get_rate(s.player) - 0.01f) < 0.001f);
        assert(vlc.libvlc_media_player_get_state(s.player) == 3 && s.opens == 1);
        // Updating only speed in source properties also keeps the input open.
        obs_data_set_int(settings, "speed_percent", 75); s.update(settings);
        assert(s.opens == 1 && vlc.libvlc_media_player_get_time(s.player) >= fast);
        vlc.libvlc_media_player_set_pause(s.player, 1); QThread::msleep(100);
        assert(s.rate(25)); assert(vlc.libvlc_media_player_get_state(s.player) == 4);
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
