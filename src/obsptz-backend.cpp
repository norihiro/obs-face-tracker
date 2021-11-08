#include <obs-module.h>
#include <util/platform.h>
#include <algorithm>
#include "plugin-macros.generated.h"
#include "obsptz-backend.hpp"

#define debug(...) blog(LOG_INFO, __VA_ARGS__)

obsptz_backend::obsptz_backend()
{
}

obsptz_backend::~obsptz_backend()
{
}

void obsptz_backend::set_config(struct obs_data *data)
{
	device_id = (int)obs_data_get_int(data, "device_id");
}

bool obsptz_backend::can_send()
{
	if (!available_ns)
		return true;

	uint64_t ns = os_gettime_ns();
	return ns >= available_ns;
}

void obsptz_backend::tick()
{
}

void obsptz_backend::set_pantilt_speed(int pan, int tilt)
{
	proc_handler_t *ph = obs_get_proc_handler();
	if (!ph)
		return;
	calldata_t cd = {0};
	calldata_set_int(&cd, "device_id", device_id);
	calldata_set_float(&cd, "pan", pan / 24.0f);
	calldata_set_float(&cd, "tilt", -tilt / 20.0f);
	proc_handler_call(ph, "ptz_pantilt", &cd);
	calldata_free(&cd);
	uint64_t ns = os_gettime_ns();
	available_ns = std::max(available_ns, ns) + (60*1000*1000);
}

void obsptz_backend::set_zoom_speed(int zoom)
{
	// TODO: implement
}

int obsptz_backend::get_zoom()
{
	// TODO: implement
	return 0;
}
