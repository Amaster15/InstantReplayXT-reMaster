/*******************************************
A Docile Sloth 2017 (adocilesloth@gmail.com)
*******************************************/
#include "InstantReplay.h"
#include "ReplayPlayer.h"

#if LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(31, 0, 0)
#error "InstantReplayXT requires OBS Studio 31.0.0 or newer headers"
#endif
#if defined(IRXT_REQUIRE_BASELINE) && LIBOBS_API_VER != MAKE_SEMANTIC_VERSION(31, 0, 0)
#error "Baseline packages must be compiled with the genuine OBS 31.0.0 SDK"
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("InstantReplay", "en-US")

bool obs_module_load(void)
{
	if (obs_get_version() < MAKE_SEMANTIC_VERSION(31, 0, 0)) {
        blog(LOG_ERROR, "[InstantReplayXT] OBS Studio 31 or newer is required.");
        return false;
    }
    replay_player_register();
	obs_frontend_add_save_callback(instant_replay_save, nullptr);
	replay_callout_init();
	replayStartup();
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_save_callback(instant_replay_save, nullptr);
	replayShutdown();
	replay_callout_shutdown();
    replay_player_shutdown();
}

const char *obs_module_author(void)
{
	return "A Docile Sloth & spreadfire1";
}

const char *obs_module_name(void)
{
	return "Instant Replay XT";
}

const char *obs_module_description(void)
{
	return "Extended Edition. Scene switching support for instant replays";
}
