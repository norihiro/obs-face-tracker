#include <obs-module.h>
#include "plugin-macros.generated.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

void register_face_tracker_filter();

bool obs_module_load(void)
{
	blog(LOG_INFO, "registering face_tracker_filter_info (version %s)", PLUGIN_VERSION);
	register_face_tracker_filter();
	return true;
}

void obs_module_unload()
{
}
