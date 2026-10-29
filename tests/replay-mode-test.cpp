#include "../ReplayMode.h"
#include <QCoreApplication>
#include <cassert>
static bool available = true;
bool replay_player_available() { return available; }
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    assert(obs_startup("en-US", nullptr, nullptr));
    obs_source_info info{}; info.id = "ffmpeg_source"; info.type = OBS_SOURCE_TYPE_INPUT;
    info.output_flags = OBS_SOURCE_AUDIO;
    info.get_name = [](void *) { return "Test source"; };
    info.create = [](obs_data_t *, obs_source_t *source) -> void * {
        proc_handler_add(obs_source_get_proc_handler(source), "void irxt_player_state(out int opens, out float rate)", [](void *, calldata_t *) {}, nullptr);
        return new int(0);
    };
    info.destroy = [](void *p) { delete static_cast<int *>(p); };
    obs_register_source(&info); info.id = "irxt_vlc_player"; obs_register_source(&info);
    // This ownership/UI test has no video renderer or base canvas dimensions.
    auto *privateData = obs_get_private_data(); obs_data_set_bool(privateData, "AbsoluteCoordinates", true); obs_data_release(privateData);
    auto *scene = obs_scene_create("Replay");
    auto *source = obs_source_create("ffmpeg_source", "Standard", nullptr, nullptr);
    auto *item = obs_scene_add(scene, source);
    obs_transform_info transform{}; obs_sceneitem_get_info2(item, &transform);
    transform.pos.x = 123; transform.pos.y = 456; obs_sceneitem_set_info2(item, &transform);
    obs_sceneitem_crop crop{3, 4, 5, 6}; obs_sceneitem_set_crop(item, &crop);
    QString error;
    available = false;
    assert(!configureReplayMode("Replay", "Standard", true, error)); assert(!error.isEmpty()); assert(obs_sceneitem_visible(item));
    available = true; error.clear();
    assert(configureReplayMode("Replay", "Standard", true, error));
    auto *vlcItem = obs_scene_find_source(scene, "Standard (VLC live)"); assert(vlcItem);
    assert(!obs_sceneitem_visible(item) && obs_sceneitem_visible(vlcItem));
    obs_transform_info copied{}; obs_sceneitem_get_info2(vlcItem, &copied);
    assert(copied.pos.x == 123 && copied.pos.y == 456);
    obs_sceneitem_crop copiedCrop{}; obs_sceneitem_get_crop(vlcItem, &copiedCrop); assert(copiedCrop.left == 3 && copiedCrop.right == 5);
    assert(configureReplayMode("Replay", "Standard", false, error));
    assert(obs_sceneitem_visible(item) && !obs_sceneitem_visible(vlcItem));
    assert(configureReplayMode("Replay", "Standard", true, error));
    assert(obs_scene_find_source(scene, "Standard (VLC live)") == vlcItem);
    assert(!configureReplayMode("Missing", "Standard", true, error));
    obs_source_release(source); obs_scene_release(scene); obs_shutdown();
}
