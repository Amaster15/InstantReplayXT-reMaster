#pragma once
#include "ReplayPlayer.h"
#include <obs-module.h>
#include <QString>
#include <string>

// Only change the selected replay scene. Keep the original source and its
// settings for switching back; never replace or rename a user's source.
inline bool configureReplayMode(const QString &sceneName, const QString &standardName, bool useVlc, QString &error) {
    auto *sceneSource = obs_get_source_by_name(sceneName.toUtf8().constData());
    auto *scene = obs_scene_from_source(sceneSource);
    const auto vlcName = standardName + " (VLC live)";
    auto *standardItem = scene ? obs_scene_find_source(scene, standardName.toUtf8().constData()) : nullptr;
    auto *vlcItem = scene ? obs_scene_find_source(scene, vlcName.toUtf8().constData()) : nullptr;
    if (!scene || !standardItem || std::string(obs_source_get_unversioned_id(obs_sceneitem_get_source(standardItem))) != "ffmpeg_source") {
        error = "Choose a replay scene containing the configured standard Media Source directly (outside a group), then select the player mode.";
        obs_source_release(sceneSource); return false;
    }
    if (useVlc && !replay_player_available()) {
        error = "VLC 3.x was not found. Install 64-bit VLC on Windows, or VLC/libVLC on Linux, restart OBS, then select VLC live slow motion.";
        obs_source_release(sceneSource); return false;
    }
    auto *vlcSource = obs_get_source_by_name(vlcName.toUtf8().constData());
    if (vlcSource && std::string(obs_source_get_unversioned_id(vlcSource)) != "irxt_vlc_player") {
        error = "A different source already uses the VLC replay name: " + vlcName;
        obs_source_release(vlcSource); obs_source_release(sceneSource); return false;
    }
    if (useVlc && !vlcItem) {
        if (!vlcSource) {
            auto *settings = obs_data_create();
            obs_data_set_int(settings, "speed_percent", 100);
            vlcSource = obs_source_create("irxt_vlc_player", vlcName.toUtf8().constData(), settings, nullptr);
            obs_data_release(settings);
        }
        if (vlcSource) {
            calldata_t call; calldata_init(&call);
            const bool ready = proc_handler_call(obs_source_get_proc_handler(vlcSource), "irxt_player_state", &call);
            calldata_free(&call);
            if (!ready) { obs_source_release(vlcSource); obs_source_release(sceneSource); error = "VLC could not create the replay player. Check the OBS log."; return false; }
            vlcItem = obs_scene_add(scene, vlcSource);
        }
        if (!vlcItem) {
            obs_source_release(vlcSource); obs_source_release(sceneSource); error = "Could not add the VLC player to the replay scene."; return false;
        }
        obs_transform_info info{}; obs_sceneitem_get_info2(standardItem, &info); obs_sceneitem_set_info2(vlcItem, &info);
        obs_sceneitem_crop crop{}; obs_sceneitem_get_crop(standardItem, &crop); obs_sceneitem_set_crop(vlcItem, &crop);
        obs_sceneitem_set_order_position(vlcItem, obs_sceneitem_get_order_position(standardItem) + 1);
        auto *standard = obs_sceneitem_get_source(standardItem);
        obs_source_copy_filters(vlcSource, standard);
        obs_source_set_volume(vlcSource, obs_source_get_volume(standard));
        obs_source_set_muted(vlcSource, obs_source_muted(standard));
        obs_source_set_audio_mixers(vlcSource, obs_source_get_audio_mixers(standard));
        obs_source_set_monitoring_type(vlcSource, obs_source_get_monitoring_type(standard));
        obs_source_set_sync_offset(vlcSource, obs_source_get_sync_offset(standard));
    }
    // Clear the outgoing input to release the fixed replay file on Windows.
    auto *outgoing = useVlc ? obs_sceneitem_get_source(standardItem) : vlcSource;
    auto *incoming = useVlc ? vlcSource : obs_sceneitem_get_source(standardItem);
    auto *old = outgoing ? obs_source_get_settings(outgoing) : nullptr;
    const QString clip = QString::fromUtf8(obs_data_get_string(old, "local_file"));
    obs_data_release(old);
    if (outgoing) {
        obs_source_media_stop(outgoing);
        auto *clear = obs_data_create(); obs_data_set_string(clear, "local_file", ""); obs_source_update(outgoing, clear); obs_data_release(clear);
    }
    obs_sceneitem_set_visible(standardItem, !useVlc);
    if (vlcItem) obs_sceneitem_set_visible(vlcItem, useVlc);
    if (incoming && !clip.isEmpty()) {
        auto *settings = obs_data_create(); obs_data_set_string(settings, "local_file", clip.toUtf8().constData());
        obs_source_update(incoming, settings); obs_data_release(settings);
    }
    obs_source_release(vlcSource); obs_source_release(sceneSource); return true;
}
