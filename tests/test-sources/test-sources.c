/*
 * A plugin of the sources for the integration tests in tests/integration,
 * which they load next to the face tracker.
 */

#include <obs-module.h>

OBS_DECLARE_MODULE()

void register_test_video_source(void);

bool obs_module_load(void)
{
	register_test_video_source();
	return true;
}
