#include <obs-module.h>
#include <util/platform.h>
#include <algorithm>
#include "plugin-macros.generated.h"
#include "obsptz-backend.hpp"
#include "helper.hpp"

#define debug(...) blog(LOG_INFO, __VA_ARGS__)

#define SAME_CNT_TH 4
#define PTZ_MAX_X 0x18
#define PTZ_MAX_Y 0x14
#define PTZ_MAX_Z 0x07

obsptz_backend::obsptz_backend() {}

obsptz_backend::~obsptz_backend() {}

void obsptz_backend::set_config(struct obs_data *data)
{
	device_id = (int)obs_data_get_int(data, "device_id");

	ptz_max_x = obs_data_get_int(data, "max_x");
	ptz_max_y = obs_data_get_int(data, "max_y");
	ptz_max_z = obs_data_get_int(data, "max_z");
}

bool obsptz_backend::can_send()
{
	if (!available_ns)
		return true;

	uint64_t ns = os_gettime_ns();
	return ns >= available_ns;
}

void obsptz_backend::tick() {}

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
	pan = std::clamp(pan, -ptz_max_x, ptz_max_x);
	tilt = std::clamp(tilt, -ptz_max_y, ptz_max_y);

	if (pan == prev_pan && tilt == prev_tilt) {
		if (same_pantilt_cnt > SAME_CNT_TH)
			return;
		same_pantilt_cnt++;
	} else {
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
	available_ns = std::max(available_ns, ns) + (60 * 1000 * 1000);
	prev_pan = pan;
	prev_tilt = tilt;
}

void obsptz_backend::set_zoom_speed(int zoom)
{
	zoom = std::clamp(zoom, -ptz_max_z, ptz_max_z);

	if (zoom == prev_zoom) {
		if (same_zoom_cnt > SAME_CNT_TH)
			return;
		same_zoom_cnt++;
	} else {
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
	available_ns = std::max(available_ns, ns) + (60 * 1000 * 1000);
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
	available_ns = std::max(available_ns, ns) + (500 * 1000 * 1000);
}

float obsptz_backend::get_zoom()
{
	// TODO: implement
	return 1.0f;
}

bool obsptz_backend::ptz_type_modified(obs_properties_t *pp, obs_data_t *)
{
	if (obs_properties_get(pp, "ptz.obsptz.device_id"))
		return false;

	obs_properties_add_int(pp, "ptz.obsptz.device_id", obs_module_text("Device ID"), 0, 99, 1);

	obs_properties_add_int_slider(pp, "ptz.obsptz.max_x", "Max control (pan)", 0, PTZ_MAX_X, 1);
	obs_properties_add_int_slider(pp, "ptz.obsptz.max_y", "Max control (tilt)", 0, PTZ_MAX_Y, 1);
	obs_properties_add_int_slider(pp, "ptz.obsptz.max_z", "Max control (zoom)", 0, PTZ_MAX_Z, 1);

	return true;
}
