#include <obs-module.h>
#include <util/config-file.h>
#include <obs-frontend-api.h>
#include "plugin-macros.generated.h"
#ifdef WITH_DOCK
#include "../ui/face-tracker-dock.hpp"
#endif // WITH_DOCK

#define CONFIG_SECTION_NAME "face-tracker"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

void register_face_tracker_filter(bool hide_filter, bool hide_source);
void register_face_tracker_ptz(bool hide_ptz);
void register_face_tracker_monitor(bool hide_monitor);

bool obs_module_load(void)
{
	blog(LOG_INFO, "registering face_tracker_filter_info (version %s)", PLUGIN_VERSION);

	config_t *cfg = obs_frontend_get_global_config();

	config_set_default_bool(cfg, CONFIG_SECTION_NAME, "ShowFilter", true);
	config_set_default_bool(cfg, CONFIG_SECTION_NAME, "ShowSource", true);
	config_set_default_bool(cfg, CONFIG_SECTION_NAME, "ShowPTZ", true);
#ifdef ENABLE_MONITOR_USER
	config_set_default_bool(cfg, CONFIG_SECTION_NAME, "ShowMonitor", true);
#else
	config_set_default_bool(cfg, CONFIG_SECTION_NAME, "ShowMonitor", false);
#endif

	bool show_filter = config_get_bool(cfg, CONFIG_SECTION_NAME, "ShowFilter");
	bool show_source = config_get_bool(cfg, CONFIG_SECTION_NAME, "ShowSource");
	bool show_ptz = config_get_bool(cfg, CONFIG_SECTION_NAME, "ShowPTZ");
	bool show_monitor = config_get_bool(cfg, CONFIG_SECTION_NAME, "ShowMonitor");

	register_face_tracker_filter(!show_filter, !show_source);
	register_face_tracker_ptz(!show_ptz);
	register_face_tracker_monitor(!show_monitor);

#ifdef WITH_DOCK
	config_set_default_bool(cfg, CONFIG_SECTION_NAME, "LoadDock", true);
	bool load_dock = config_get_bool(cfg, CONFIG_SECTION_NAME, "LoadDock");
	if (load_dock)
		ft_docks_init();
#endif // WITH_DOCK
	return true;
}

void obs_module_unload()
{
#ifdef WITH_DOCK
	ft_docks_release();
#endif // WITH_DOCK
}
