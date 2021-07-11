#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <graphics/vec2.h>
#include <graphics/graphics.h>
#include "plugin-macros.generated.h"
#include "texture-object.h"
#include <algorithm>
#include <cmath>
#include <graphics/matrix4.h>
#include "helper.hpp"
#include "face-tracker-ptz.hpp"
#include "face-tracker-preset.h"
#include "face-tracker-manager.hpp"
#include "ptz-device.hpp"

// #define debug_track(fmt, ...) blog(LOG_INFO, fmt, __VA_ARGS__)
#define debug_track(fmt, ...)

#define PTZ_MAX_X 0x18
#define PTZ_MAX_Y 0x14
#define PTZ_MAX_Z 0x07

static gs_effect_t *effect_ft = NULL;

enum ptz_cmd_state_e
{
	ptz_cmd_state_none = 0,
	ptz_cmd_state_reset,
	ptz_cmd_state_pantiltq,
	ptz_cmd_state_zoomq,
};

class ft_manager_for_ftptz : public face_tracker_manager
{
	public:
		struct face_tracker_ptz *ctx;
		class texture_object *cvtex_cache;
		class PTZDevice *ptzdev;
		enum ptz_cmd_state_e ptz_last_cmd;
		int ptz_last_cmd_tick;

	public:
		ft_manager_for_ftptz(struct face_tracker_ptz *ctx_) {
			ctx = ctx_;
			cvtex_cache = NULL;
			ptzdev = NULL;
			ptz_last_cmd = ptz_cmd_state_none;
			ptz_last_cmd_tick = 0;
		}

		bool can_send_ptz_cmd() {
			if (ptzdev) {
				if (ptzdev->got_inquiry())
					return true;
				// If ack takes too much time, send the next command anyway.
				if ((tick_cnt-ptz_last_cmd_tick) > 12)
					ptzdev->timeout();
			}
			return false;
		}

		~ft_manager_for_ftptz()
		{
			release_cvtex();
			delete ptzdev;
		}

		inline void release_cvtex()
		{
			if (cvtex_cache)
				cvtex_cache->release();
			cvtex_cache = NULL;
		}

		class texture_object *get_cvtex() override
		{
			return cvtex_cache;
		};
};

static const char *ftptz_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Face Tracker PTZ");
}

static void copy_string(obs_data_t *data, const char *dst, obs_data_t *settings, const char *src)
{
	obs_data_set_string(data, dst, obs_data_get_string(settings, src));
}

static void copy_int(obs_data_t *data, const char *dst, obs_data_t *settings, const char *src)
{
	obs_data_set_int(data, dst, obs_data_get_int(settings, src));
}

struct ptz_copy_setting_item_s
{
	const char *dst;
	const char *src;
	void (*copy)(obs_data_t *, const char *, obs_data_t *, const char *);
};

static void ptz_copy_settings(obs_data_t *data, obs_data_t *settings, const struct ptz_copy_setting_item_s *list)
{
	for (int i=0; list[i].dst; i++)
		list[i].copy(data, list[i].dst, settings, list[i].src);
}

static obs_data_t *get_ptz_settings(obs_data_t *settings)
{
	obs_data_t *data = obs_data_create();

	const struct ptz_copy_setting_item_s list_generic[] = {
		{"type", "ptz-type", copy_string},
		{"name", "ptz-name", copy_string},
		{NULL, NULL, NULL}
	};

	const struct ptz_copy_setting_item_s list_viscaip[] = {
		{"address", "ptz-viscaip-address", copy_string},
		{"port", "ptz-viscaip-port", copy_int},
		{NULL, NULL, NULL}
	};

#ifdef WITH_PTZ_SERIAL
	const struct ptz_copy_setting_item_s list_viscaserial[] = {
		{"address", "ptz-viscaserial-address", copy_int},
		{"port", "ptz-viscaserial-port", copy_string},
		{NULL, NULL, NULL}
	};
#endif // WITH_PTZ_SERIAL

	ptz_copy_settings(data, settings, list_generic);

	const char *type = obs_data_get_string(data, "type");
	if (!strcmp(type, "visca-over-ip"))
		ptz_copy_settings(data, settings, list_viscaip);
#ifdef WITH_PTZ_SERIAL
	else if (!strcmp(type, "visca"))
		ptz_copy_settings(data, settings, list_viscaserial);
#endif //WITH_PTZ_SERIAL

	return data;
}

static void ftptz_update(void *data, obs_data_t *settings)
{
	auto *s = (struct face_tracker_ptz*)data;

	s->ftm->update(settings);
	s->ftm->scale = roundf(s->ftm->scale);
	s->track_z = obs_data_get_double(settings, "track_z");
	s->track_x = obs_data_get_double(settings, "track_x");
	s->track_y = obs_data_get_double(settings, "track_y");

	float ki = (float)obs_data_get_double(settings, "Ki");
	double td = obs_data_get_double(settings, "Td");
	float inv_x = obs_data_get_bool(settings, "invert_x") ? -1.0f : 1.0f;
	float inv_y = obs_data_get_bool(settings, "invert_y") ? -1.0f : 1.0f;
	float inv_z = obs_data_get_bool(settings, "invert_z") ? -1.0f : 1.0f;
	s->kp_x = powf(10.0f, (float)obs_data_get_double(settings, "Kp_x_db")/20.0f) * inv_x;
	s->kp_y = powf(10.0f, (float)obs_data_get_double(settings, "Kp_y_db")/20.0f) * inv_y;
	s->kp_z = powf(10.0f, (float)obs_data_get_double(settings, "Kp_z_db")/20.0f) * inv_z;
	s->ki = ki;
	s->klpf = (float)td;
	s->tlpf = (float)obs_data_get_double(settings, "Tdlpf");
	s->e_deadband.v[0] = (float)obs_data_get_double(settings, "e_deadband_x") * 1e-2;
	s->e_deadband.v[1] = (float)obs_data_get_double(settings, "e_deadband_y") * 1e-2;
	s->e_deadband.v[2] = (float)obs_data_get_double(settings, "e_deadband_z") * 1e-2;
	s->e_nonlinear.v[0] = (float)obs_data_get_double(settings, "e_nonlinear_x") * 1e-2;
	s->e_nonlinear.v[1] = (float)obs_data_get_double(settings, "e_nonlinear_y") * 1e-2;
	s->e_nonlinear.v[2] = (float)obs_data_get_double(settings, "e_nonlinear_z") * 1e-2;

	s->debug_faces = obs_data_get_bool(settings, "debug_faces");
	s->debug_notrack = obs_data_get_bool(settings, "debug_notrack");
	s->debug_always_show = obs_data_get_bool(settings, "debug_always_show");

	s->ptz_max_x = obs_data_get_int(settings, "ptz_max_x");
	s->ptz_max_y = obs_data_get_int(settings, "ptz_max_y");
	s->ptz_max_z = obs_data_get_int(settings, "ptz_max_z");

	const char *ptz_type = obs_data_get_string(settings, "ptz-type");
	if (!s->ftm->ptzdev || !s->ptz_type || strcmp(ptz_type, s->ptz_type)) {
		if (s->ftm->ptzdev)
			delete s->ftm->ptzdev;
		obs_data_t *data = get_ptz_settings(settings);
		blog(LOG_INFO, "creating new PTZDevice type=%s", ptz_type);
		s->ftm->ptzdev = PTZDevice::make_device(data);
		obs_data_release(data);
		bfree(s->ptz_type);
		s->ptz_type = bstrdup(ptz_type);
	}
	else {
		obs_data_t *data = get_ptz_settings(settings);
		s->ftm->ptzdev->set_config(data);
		obs_data_release(data);
	}
}

static void *ftptz_create(obs_data_t *settings, obs_source_t *context)
{
	auto *s = (struct face_tracker_ptz*)bzalloc(sizeof(struct face_tracker_ptz));
	s->ftm = new ft_manager_for_ftptz(s);
	s->ftm->crop_cur.x1 = s->ftm->crop_cur.y1 = -2;
	s->context = context;
	s->ftm->scale = 2.0f;

	obs_enter_graphics();
	if (!effect_ft) {
		char *f = obs_module_file("face-tracker.effect");
		effect_ft = gs_effect_create_from_file(f, NULL);
		if (!effect_ft)
			blog(LOG_ERROR, "Cannot load '%s'", f);
		bfree(f);
	}
	obs_leave_graphics();

	obs_source_update(context, settings);
	return s;
}

static void ftptz_destroy(void *data)
{
	auto *s = (struct face_tracker_ptz*)data;
	delete s->ftm;
	bfree(s->ptz_type);

	bfree(s);
}

static bool ftptz_reset_tracking(obs_properties_t *, obs_property_t *, void *data)
{
	auto *s = (struct face_tracker_ptz*)data;

	s->detect_err = f3(0, 0, 0);
	s->filter_int = f3(0, 0, 0);
	s->filter_lpf = f3(0, 0, 0);
	s->ftm->reset_requested = true;
	s->ptz_request_reset = true;

	return true;
}

static obs_properties_t *ftptz_properties(void *data)
{
	auto *s = (struct face_tracker_ptz*)data;
	obs_properties_t *props;
	props = obs_properties_create();

	obs_properties_add_button(props, "ftptz_reset_tracking", obs_module_text("Reset tracking"), ftptz_reset_tracking);

	{
		obs_properties_t *pp = obs_properties_create();
		obs_property_t *p = obs_properties_add_list(pp, "preset_name", obs_module_text("Preset"), OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
		obs_data_t *settings = obs_source_get_settings(s->context);
		if (settings) {
			ftf_preset_item_to_list(p, settings);
			obs_data_release(settings);
		}
		obs_properties_add_button(pp, "preset_load", obs_module_text("Load preset"), ftf_preset_load);
		obs_properties_add_button(pp, "preset_save", obs_module_text("Save preset"), ftf_preset_save);
		obs_properties_add_button(pp, "preset_delete", obs_module_text("Delete preset"), ftf_preset_delete);
		obs_properties_add_bool(pp, "preset_mask_track", obs_module_text("Save and load tracking parameters"));
		obs_properties_add_bool(pp, "preset_mask_control", obs_module_text("Save and load control parameters"));
		obs_properties_add_group(props, "preset_grp", obs_module_text("Preset"), OBS_GROUP_NORMAL, pp);
	}

	{
		obs_properties_t *pp = obs_properties_create();
		face_tracker_manager::get_properties(pp);
		obs_properties_add_group(props, "ftm", obs_module_text("Face detection options"), OBS_GROUP_NORMAL, pp);
	}

	{
		obs_properties_t *pp = obs_properties_create();
		obs_properties_add_float(pp, "track_z", obs_module_text("Zoom"), 0.1, 2.0, 0.1);
		obs_properties_add_float(pp, "track_x", obs_module_text("X"), -1.0, +1.0, 0.05);
		obs_properties_add_float(pp, "track_y", obs_module_text("Y"), -1.0, +1.0, 0.05);
		obs_properties_add_group(props, "track", obs_module_text("Tracking target location"), OBS_GROUP_NORMAL, pp);
	}

	{
		obs_properties_t *pp = obs_properties_create();
		obs_property_t *p;
		p = obs_properties_add_float(pp, "Kp_x_db", "Track Kp (Pan)",  -40.0, +80.0, 1.0);
		obs_property_float_set_suffix(p, " dB");
		p = obs_properties_add_float(pp, "Kp_y_db", "Track Kp (Tilt)", -40.0, +80.0, 1.0);
		obs_property_float_set_suffix(p, " dB");
		p = obs_properties_add_float(pp, "Kp_z_db", "Track Kp (Zoom)", -40.0, +40.0, 1.0);
		obs_property_float_set_suffix(p, " dB");
		obs_properties_add_float(pp, "Ki", "Track Ki", 0.0, 5.0, 0.01);
		obs_properties_add_float(pp, "Td", "Track Td", 0.0, 5.0, 0.01);
		obs_properties_add_float(pp, "Tdlpf", "Track LPF for Td", 0.0, 10.0, 0.1);
		obs_properties_add_float(pp, "e_deadband_x", "Dead band (X)", 0.0, 50, 0.1);
		obs_properties_add_float(pp, "e_deadband_y", "Dead band (Y)", 0.0, 50, 0.1);
		obs_properties_add_float(pp, "e_deadband_z", "Dead band (Z)", 0.0, 50, 0.1);
		obs_properties_add_float(pp, "e_nonlinear_x", "Nonlinear band (X)", 0.0, 50, 0.1);
		obs_properties_add_float(pp, "e_nonlinear_y", "Nonlinear band (Y)", 0.0, 50, 0.1);
		obs_properties_add_float(pp, "e_nonlinear_z", "Nonlinear band (Z)", 0.0, 50, 0.1);
		obs_properties_add_group(props, "ctrl", obs_module_text("Tracking response"), OBS_GROUP_NORMAL, pp);
	}

	{
		obs_properties_t *pp = obs_properties_create();
		obs_properties_add_bool(pp, "debug_faces", "Show face detection results");
		obs_properties_add_bool(pp, "debug_always_show", "Always show information (useful for demo)");
		obs_property_t *p = obs_properties_add_list(pp, "ptz-type", obs_module_text("PTZ Type"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
		obs_property_list_add_string(p, obs_module_text("None"), "sim");
		obs_property_list_add_string(p, obs_module_text("VISCA over IP"), "visca-over-ip");
#ifdef WITH_PTZ_SERIAL
		obs_property_list_add_string(p, obs_module_text("VISCA over serial port"), "visca");
#endif // WITH_PTZ_SERIAL
		obs_properties_add_text(pp, "ptz-viscaip-address", obs_module_text("IP address"), OBS_TEXT_DEFAULT);
		obs_properties_add_int(pp, "ptz-viscaip-port", obs_module_text("UDP port"), 1, 65535, 1);
#ifdef WITH_PTZ_SERIAL
		obs_properties_add_text(pp, "ptz-viscaserial-port", obs_module_text("Serial port"), OBS_TEXT_DEFAULT);
		obs_properties_add_int(pp, "ptz-viscaserial-address", obs_module_text("Address"), 0, 7, 1);
#endif // WITH_PTZ_SERIAL
		obs_properties_add_int_slider(pp, "ptz_max_x", "Max control (pan)",  0, PTZ_MAX_X, 1);
		obs_properties_add_int_slider(pp, "ptz_max_y", "Max control (tilt)", 0, PTZ_MAX_Y, 1);
		obs_properties_add_int_slider(pp, "ptz_max_z", "Max control (zoom)", 0, PTZ_MAX_Z, 1);
		obs_properties_add_bool(pp, "invert_x", obs_module_text("Invert control (Pan)"));
		obs_properties_add_bool(pp, "invert_y", obs_module_text("Invert control (Tilt)"));
		obs_properties_add_bool(pp, "invert_z", obs_module_text("Invert control (Zoom)"));
		obs_properties_add_group(props, "output", obs_module_text("Output"), OBS_GROUP_NORMAL, pp);
	}

	return props;
}

static void ftptz_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "preset_mask_track", true);
	obs_data_set_default_bool(settings, "preset_mask_control", true);
	face_tracker_manager::get_defaults(settings);
	obs_data_set_default_double(settings, "track_z",  0.25); // Smaller is preferable for PTZ not to lose the face.
	obs_data_set_default_double(settings, "track_y", +0.00); // +0.00 +0.10 +0.30

	obs_data_set_default_double(settings, "Kp_x_db", 50.0);
	obs_data_set_default_double(settings, "Kp_y_db", 50.0);
	obs_data_set_default_double(settings, "Kp_z_db", 40.0);
	obs_data_set_default_double(settings, "Ki", 0.3);
	obs_data_set_default_double(settings, "Td", 0.42);
	obs_data_set_default_double(settings, "Tdlpf", 2.0);

	obs_data_t *presets = obs_data_create();
	obs_data_set_default_obj(settings, "presets", presets);
	obs_data_release(presets);

	obs_data_set_default_string(settings, "ptz-type", "visca-over-ip");
	obs_data_set_default_int(settings, "ptz-viscaip-port", 1259);
#ifdef WITH_PTZ_SERIAL
	obs_data_set_default_int(settings, "ptz-viscaserial-address", 1);
#endif // WITH_PTZ_SERIAL
	obs_data_set_default_int(settings, "ptz_max_x", PTZ_MAX_X);
	obs_data_set_default_int(settings, "ptz_max_y", PTZ_MAX_Y);
	obs_data_set_default_int(settings, "ptz_max_z", PTZ_MAX_Z);
}

static inline float raw2zoomfactor(int zoom)
{
	// TODO: configurable
	return expf((float)zoom * (logf(20.0f) / 16384.f));
}

static inline int pan_flt2raw(float x)
{
	// TODO: configurable
	// TODO: send zero with plus or minus sign, which makes small move.
	if (x<0.0f) return -pan_flt2raw(-x);
	if (x<0.50f) return  0;
	if (x<1.25f) return  1;
	if (x<1.85f) return  2;
	if (x<2.30f) return  3;
	if (x<2.50f) return  4;
	if (x<2.70f) return  5;
	if (x<2.90f) return  6;
	if (x<3.10f) return  7;
	if (x<3.30f) return  8;
	if (x<3.60f) return  9;
	if (x<4.15f) return 10;
	if (x<5.25f) return 11;
	if (x<7.50f) return 12;
	if (x<12.0f) return 13;
	if (x<17.0f) return 14;
	if (x<22.0f) return 15;
	if (x<28.5f) return 16;
	if (x<35.0f) return 17;
	if (x<41.5f) return 18;
	if (x<51.5f) return 19;
	if (x<66.5f) return 20;
	if (x<81.5f) return 21;
	if (x<96.5f) return 22;
	if (x<112.5f) return 23;
	else return 24;
}

static inline int tilt_flt2raw(float x)
{
	// TODO: configurable
	// TODO: send zero with plus or minus sign, which makes small move.
	if (x<0.0f) return -pan_flt2raw(-x);
	if (x<0.50f) return  0;
	if (x<1.25f) return  1;
	if (x<1.85f) return  2;
	if (x<2.30f) return  3;
	if (x<4.15f) return  4;
	if (x<5.35f) return  5;
	if (x<7.00f) return  6;
	if (x<9.00f) return  7;
	if (x<11.0f) return  8;
	if (x<13.5f) return  9;
	if (x<16.5f) return 10;
	if (x<20.5f) return 11;
	if (x<26.5f) return 12;
	if (x<34.5f) return 13;
	if (x<43.5f) return 14;
	if (x<53.5f) return 15;
	if (x<64.0f) return 16;
	if (x<74.5f) return 17;
	else return 18;
}

static void tick_filter(struct face_tracker_ptz *s, float second)
{
	const float srwh = sqrtf((float)s->known_width * s->known_height);

	f3 e = s->detect_err;
	f3 e_int = e;
	for (int i=0; i<3; i++) {
		float x = e.v[i];
		float d = srwh * s->e_deadband.v[i];
		float n = srwh * s->e_nonlinear.v[i];
		if (std::abs(x) <= d)
			x = 0.0f;
		else if (std::abs(x) < (d + n)) {
			if (x > 0)
				x = +sqf(x - d) / (2.0f * n);
			else
				x = -sqf(x - d) / (2.0f * n);
		}
		else if (x > 0)
			x -= d + n * 0.5f;
		else
			x += d + n * 0.5f;
		if (second * s->ki > 1.0e-10) {
			if (s->filter_int.v[i] < 0.0f && e.v[i] > 0.0f)
				e_int.v[i] = std::min(e.v[i], -s->filter_int.v[i] / (second * s->ki));
			else if (s->filter_int.v[i] > 0.0f && e.v[i] < 0.0f)
				e_int.v[i] = std::max(e.v[i], -s->filter_int.v[i] / (second * s->ki));
			else
				e_int.v[i] = x;
		}
		e.v[i] = x;
	}

	f3 filter_lpf_prev = s->filter_lpf;
	s->filter_int += e_int * (second * s->ki);
	s->filter_lpf = (s->filter_lpf * s->tlpf + e * second) * (1.f/(s->tlpf + second));
	f3 uf = (e + s->filter_int) * second + (s->filter_lpf - filter_lpf_prev) * s->klpf;
	const int u_max[3] = {s->ptz_max_x, s->ptz_max_y, s->ptz_max_z};
	const float kp[3] = {
		s->kp_x / srwh / raw2zoomfactor(s->ptz_query[2]),
		s->kp_y / srwh / raw2zoomfactor(s->ptz_query[2]),
		s->kp_z / srwh
	};
	for (int i=0; i<3; i++) {
		float x = uf.v[i] * kp[i];
		int n;
		switch(i) {
			case 0:  n = pan_flt2raw(x); break;
			case 1:  n = tilt_flt2raw(x); break;
			default: n = (int)roundf(x); break;
		}
		if      (n < -u_max[i]) n = -u_max[i];
		else if (n > +u_max[i]) n = +u_max[i];
		s->u[i] = n;
	}
	debug_track("tick_filter: u: %d %d %d uf: %f %f %f", s->u[0], s->u[1], s->u[2], uf.v[0], uf.v[1], uf.v[2]);
}

static void ftf_activate(void *data)
{
	auto *s = (struct face_tracker_ptz*)data;
	s->is_active = true;
}

static void ftf_deactivate(void *data)
{
	auto *s = (struct face_tracker_ptz*)data;
	s->is_active = false;
}

template <typename T> static inline bool diff3(T a, T b, T c)
{
	return a!=b || b!=c || c!=a;
}

static inline void send_ptz_cmd_immediate(struct face_tracker_ptz *s)
{
	if (diff3(s->u[0], s->u_prev[0], s->u_prev1[0]) || diff3(s->u[1], s->u_prev[1], s->u_prev1[1]))
		s->ftm->ptzdev->pantilt(s->u[0], s->u[1]);

	if (diff3(s->u[2], s->u_prev[2], s->u_prev1[2])) {
		if (s->u[2]>0)
			s->ftm->ptzdev->zoom_wide(s->u[2]);
		else if (s->u[2]<0)
			s->ftm->ptzdev->zoom_tele(-s->u[2]);
		else
			s->ftm->ptzdev->zoom_stop();
	}

	s->u_prev1[0] = s->u_prev[0];
	s->u_prev1[1] = s->u_prev[1];
	s->u_prev1[2] = s->u_prev[2];
	s->u_prev[0] = s->u[0];
	s->u_prev[1] = s->u[1];
	s->u_prev[2] = s->u[2];

}

static inline void recvsend_ptz_cmd(struct face_tracker_ptz *s)
{
	ptz_cmd_state_e cmd_next = ptz_cmd_state_none;
	switch(s->ftm->ptz_last_cmd) {
		case ptz_cmd_state_none:
			cmd_next = ptz_cmd_state_pantiltq;
			break;
		case ptz_cmd_state_pantiltq:
			if (s->ftm->ptzdev && s->ftm->ptzdev->got_inquiry()) {
				s->ptz_query[0] = s->ftm->ptzdev->get_pan();
				s->ptz_query[1] = s->ftm->ptzdev->get_tilt();
			};
			cmd_next = ptz_cmd_state_zoomq;
			break;
		case ptz_cmd_state_zoomq:
			if (s->ftm->ptzdev && s->ftm->ptzdev->got_inquiry()) {
				s->ptz_query[2] = s->ftm->ptzdev->get_zoom();
			};
			cmd_next = ptz_cmd_state_pantiltq;
			break;
		default:
			cmd_next = ptz_cmd_state_none;
			break;
	}

	// skip unnecessary command
	if (cmd_next==ptz_cmd_state_pantiltq) {
		if (
				s->u_prev[0]==0 && s->u_prev1[0]==0 &&
				s->u_prev[1]==0 || s->u_prev1[1]==0 )
			cmd_next = ptz_cmd_state_zoomq;
	}
	if (cmd_next==ptz_cmd_state_zoomq) {
		if (s->u_prev[2]==0 || s->u_prev1[2]==0)
			cmd_next = ptz_cmd_state_none;
	}

	if (s->ptz_request_reset)
		cmd_next = ptz_cmd_state_reset;

	switch (cmd_next) {
		case ptz_cmd_state_reset:
			s->ftm->ptzdev->pantilt_home();
			s->ptz_request_reset = false;
			break;
		case ptz_cmd_state_pantiltq:
			s->ftm->ptzdev->pantilt_inquiry();
			s->ftm->ptz_last_cmd = ptz_cmd_state_pantiltq;
			s->ftm->ptz_last_cmd_tick = s->ftm->tick_cnt;
			break;
		case ptz_cmd_state_zoomq:
			s->ftm->ptzdev->zoom_inquiry();
			s->ftm->ptz_last_cmd = ptz_cmd_state_zoomq;
			s->ftm->ptz_last_cmd_tick = s->ftm->tick_cnt;
			break;
		default:
			s->ftm->ptz_last_cmd = ptz_cmd_state_none;
			break;
	}
}

static inline void calculate_error(struct face_tracker_ptz *s);

static void ftptz_tick(void *data, float second)
{
	auto *s = (struct face_tracker_ptz*)data;
	const bool was_rendered = s->rendered;
	s->ftm->tick(second);

	obs_source_t *target = obs_filter_get_target(s->context);
	if (!target)
		return;

	s->rendered = false;

	s->known_width = obs_source_get_base_width(target);
	s->known_height = obs_source_get_base_height(target);

	if (s->known_width<=0 || s->known_height<=0)
		return;

	if (was_rendered) {
		calculate_error(s);
		tick_filter(s, second);
		send_ptz_cmd_immediate(s);
	}

	if (s->ftm && s->ftm->ptzdev && s->ftm->can_send_ptz_cmd()) {
		recvsend_ptz_cmd(s);
	}
}

static inline void render_target(struct face_tracker_ptz *s, obs_source_t *target, obs_source_t *parent)
{
	s->ftm->release_cvtex();
}

static inline void calculate_error(struct face_tracker_ptz *s)
{
	f3 e_tot(0.0f, 0.0f, 0.0f);
	float sc_tot = 0.0f;
	bool found = false;
	auto &tracker_rects = s->ftm->tracker_rects;
	for (int i=0; i<tracker_rects.size(); i++) {
		f3 r (tracker_rects[i].rect);
		r.v[0] -= get_width(tracker_rects[i].crop_rect) * s->track_x;
		r.v[1] += get_height(tracker_rects[i].crop_rect) * s->track_y;
		r.v[2] /= s->track_z;
		f3 w (tracker_rects[i].crop_rect);
		float score = tracker_rects[i].rect.score;
		f3 e = (r-w) * score;
		debug_track("calculate_error: %d %f %f %f %f", i, e.v[0], e.v[1], e.v[2], score);
		if (score>0.0f && !isnan(e)) {
			e_tot += e;
			sc_tot += score;
			found = true;
		}
	}

	if (found)
		s->detect_err = e_tot * (1.0f / sc_tot);
	else
		s->detect_err = f3(0, 0, 0);
}

static struct obs_source_frame *ftptz_filter_video(void *data, struct obs_source_frame *frame)
{
	auto *s = (struct face_tracker_ptz*)data;

	s->known_width = frame->width;
	s->known_height = frame->height;
	s->ftm->release_cvtex();
	s->ftm->cvtex_cache = new texture_object();
	s->ftm->cvtex_cache->scale = s->ftm->scale;
	s->ftm->cvtex_cache->tick = s->ftm->tick_cnt;
	s->ftm->cvtex_cache->set_texture_obsframe_scale(frame, s->ftm->scale);
	s->ftm->crop_cur.x0 = 0;
	s->ftm->crop_cur.y0 = 0;
	s->ftm->crop_cur.x1 = frame->width;
	s->ftm->crop_cur.y1 = frame->height;
	s->rendered = true;

	s->ftm->post_render();
	return frame;
}

static inline void draw_frame_info(struct face_tracker_ptz *s)
{
	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_SOLID);
	while (gs_effect_loop(effect, "Solid")) {
		gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"), 0xFF0000FF);
		for (int i=0; i<s->ftm->detect_rects.size(); i++)
			draw_rect_upsize(s->ftm->detect_rects[i], s->ftm->upsize_l, s->ftm->upsize_r, s->ftm->upsize_t, s->ftm->upsize_b);

		gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"), 0xFF00FF00);
		for (int i=0; i<s->ftm->tracker_rects.size(); i++) {
			draw_rect_upsize(s->ftm->tracker_rects[i].rect);
		}
	}
}

static void ftptz_video_render(void *data, gs_effect_t *)
{
	auto *s = (struct face_tracker_ptz*)data;
	obs_source_skip_video_filter(s->context);

	if (s->debug_faces && (!s->is_active || s->debug_always_show))
		draw_frame_info(s);
}

extern "C"
void register_face_tracker_ptz()
{
	struct obs_source_info info = {};
	info.id = "face_tracker_ptz";
	info.type = OBS_SOURCE_TYPE_FILTER;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_ASYNC;
	info.get_name = ftptz_get_name;
	info.create = ftptz_create;
	info.destroy = ftptz_destroy;
	info.update = ftptz_update;
	info.get_properties = ftptz_properties;
	info.get_defaults = ftptz_get_defaults;
	info.activate = ftf_activate,
	info.deactivate = ftf_deactivate,
	info.video_tick = ftptz_tick;
	info.filter_video = ftptz_filter_video;
	info.video_render = ftptz_video_render;
	obs_register_source(&info);
}
