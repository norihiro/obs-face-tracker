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

proc_handler_t *obsptz_backend::get_ptz_ph()
{
	if (ptz_ph)
		return ptz_ph;

	proc_handler_t *ph = obs_get_proc_handler();
	if (!ph)
		return NULL;

	calldata_t cd = {0};
	proc_handler_call(ph, "ptz_get_proc_handler", &cd);
	calldata_get_ptr(&cd, "return", &ptz_ph);
	calldata_free(&cd);

	return ptz_ph;
}

void obsptz_backend::set_pantiltzoom_speed(int pan, int tilt, int zoom)
{
	if (pan==prev_pan && tilt==prev_tilt && zoom==prev_zoom)
		return;

	calldata_t cd = {0};
	calldata_set_int(&cd, "device_id", device_id);
	calldata_set_float(&cd, "pan", pan / 24.0f);
	calldata_set_float(&cd, "tilt", -tilt / 20.0f);
	calldata_set_float(&cd, "zoom", -zoom / 7.0f);
	proc_handler_t *ph = get_ptz_ph();
	if (ph)
		proc_handler_call(ph, "ptz_move_continuous", &cd);
	else {
		// compatibility
		ph = obs_get_proc_handler();
		proc_handler_call(ph, "ptz_pantilt", &cd);
	}
	calldata_free(&cd);
	uint64_t ns = os_gettime_ns();
	available_ns = std::max(available_ns, ns) + (60*1000*1000);
	prev_pan = pan;
	prev_tilt = tilt;
	prev_zoom = zoom;
}

int obsptz_backend::get_zoom()
{
	// TODO: implement
	return 0;
}
