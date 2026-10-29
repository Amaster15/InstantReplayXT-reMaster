/*******************************************
A Docile Sloth 2017 (adocilesloth@gmail.com)

spreadfire1 2018 (https://twitter.com/@spreadfire1)

*******************************************/
#pragma once
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <string>

void replayStartup();
void instant_replay_hotkey(void*, obs_hotkey_id, obs_hotkey_t*, bool);
void instant_replay_save(obs_data_t *save_data, bool saving, void *);

bool get_enabled();
bool get_auto_switch();
void instant_replay_play_hotkey(void*, obs_hotkey_id, obs_hotkey_t*, bool);
int get_wait();
int get_pause();
int get_replay();
std::string get_scene();
bool get_D_mute();
bool get_A_mute();

// Instant Replay XT extended settings
bool get_backup_enabled();
std::string get_backup_path();
std::string get_replay_path();
void replay_backup_saved();
void replay_status(const std::string &message);
std::string get_replay_source_name();


void replay_callout_init();

void replayShutdown();
void replay_callout_shutdown();

void replay_play_clip(const std::string &path, bool switchScene);
void replay_set_speed(int percent);
std::string replay_current_file();
