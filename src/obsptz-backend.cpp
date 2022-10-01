#include <obs-module.h>
#include <util/platform.h>
#include <algorithm>
#include "plugin-macros.generated.h"
#include "obsptz-backend.hpp"
#include "helper.hpp"

#define debug(...) blog(LOG_INFO, __VA_ARGS__)

#define SAME_CNT_TH 4

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

	CALLDATA_FIXED_DECL(cd, 128);
	proc_handler_call(ph, "ptz_get_proc_handler", &cd);
	calldata_get_ptr(&cd, "return", &ptz_ph);

	return ptz_ph;
}

void obsptz_backend::set_pantilt_speed(int pan, int tilt)
{
	if (pan==prev_pan && tilt==prev_tilt) {
		if (same_pantilt_cnt > SAME_CNT_TH)
			return;
		same_pantilt_cnt ++;
	}
	else {
		same_pantilt_cnt = 0;
	}

	CALLDATA_FIXED_DECL(cd, 128);
	calldata_set_int(&cd, "device_id", device_id);
	calldata_set_float(&cd, "pan", pan / 24.0f);
	calldata_set_float(&cd, "tilt", -tilt / 20.0f);
	proc_handler_t *ph = get_ptz_ph();
	if (ph)
		proc_handler_call(ph, "ptz_move_continuous", &cd);
	else {
		// compatibility
		ph = obs_get_proc_handler();
		proc_handler_call(ph, "ptz_pantilt", &cd);
	}
	uint64_t ns = os_gettime_ns();
	available_ns = std::max(available_ns, ns) + (60*1000*1000);
	prev_pan = pan;
	prev_tilt = tilt;
}

void obsptz_backend::set_zoom_speed(int zoom)
{
	if (zoom==prev_zoom) {
		if (same_zoom_cnt > SAME_CNT_TH)
			return;
		same_zoom_cnt ++;
	}
	else {
		same_zoom_cnt = 0;
	}

	proc_handler_t *ph = get_ptz_ph();
	if (!ph)
		return;

	CALLDATA_FIXED_DECL(cd, 128);
	calldata_set_int(&cd, "device_id", device_id);
	calldata_set_float(&cd, "zoom", -zoom / 7.0f);
	proc_handler_call(ph, "ptz_move_continuous", &cd);

	uint64_t ns = os_gettime_ns();
	available_ns = std::max(available_ns, ns) + (60*1000*1000);
	prev_zoom = zoom;
}

void obsptz_backend::recall_preset(int preset)
{
	proc_handler_t *ph = get_ptz_ph();
	if (!ph)
		return;

	CALLDATA_FIXED_DECL(cd, 128);
	calldata_set_int(&cd, "device_id", device_id);
	calldata_set_int(&cd, "preset_id", preset);
	proc_handler_call(ph, "ptz_preset_recall", &cd);

	uint64_t ns = os_gettime_ns();
	available_ns = std::max(available_ns, ns) + (500*1000*1000);
}

int obsptz_backend::get_zoom()
{
	// TODO: implement
	return 0;
}
