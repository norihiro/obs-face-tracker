#include <obs-module.h>
#include "plugin-macros.generated.h"
#ifdef WITH_DOCK
#include "../ui/face-tracker-dock.hpp"
#endif // WITH_DOCK

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

void register_face_tracker_filter();
void register_face_tracker_ptz();
void register_face_tracker_monitor();

bool obs_module_load(void)
{
	blog(LOG_INFO, "registering face_tracker_filter_info (version %s)", PLUGIN_VERSION);
	register_face_tracker_filter();
	register_face_tracker_ptz();
	register_face_tracker_monitor();
#ifdef WITH_DOCK
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
