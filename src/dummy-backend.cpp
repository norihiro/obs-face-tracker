#include <obs-module.h>
#include "plugin-macros.generated.h"
#include "dummy-backend.hpp"

#define debug(...) blog(LOG_INFO, __VA_ARGS__)

dummy_backend::dummy_backend()
{
}

dummy_backend::~dummy_backend()
{
}

void dummy_backend::set_config(struct obs_data *data)
{
}

void dummy_backend::set_pantilt_speed(int pan, int tilt)
{
	if (pan==prev_pan && tilt==prev_tilt)
		return;

	blog(LOG_INFO, "set_pantilt_speed: %d %d", pan, tilt);

	prev_pan = pan;
	prev_tilt = tilt;
}

void dummy_backend::set_zoom_speed(int zoom)
{
	if (zoom==prev_zoom)
		return;

	blog(LOG_INFO, "set_zoom_speed: %d", zoom);

	prev_zoom = zoom;
}

int dummy_backend::get_zoom()
{
	return 0;
}
