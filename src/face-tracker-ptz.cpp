#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <graphics/vec2.h>
#include <graphics/graphics.h>
#include "plugin-macros.generated.h"
#include "texture-object.h"
#include <algorithm>
#include <graphics/matrix4.h>
#include "helper.hpp"
#include "face-tracker-ptz.hpp"
#include "face-tracker-preset.h"
#include "face-tracker-manager.hpp"
#include "ptz-backend.hpp"
#include "obsptz-backend.hpp"
#ifdef WITH_PTZ_TCP
#include "libvisca-thread.hpp"
#endif
#include "dummy-backend.hpp"

#define PTZ_MAX_X 0x18
#define PTZ_MAX_Y 0x14
#define PTZ_MAX_Z 0x07

enum ptz_cmd_state_e
{
	ptz_cmd_state_none = 0,
	ptz_cmd_state_pantilt,
	ptz_cmd_state_zoom,
};

class ft_manager_for_ftptz : public face_tracker_manager
{
	public:
		struct face_tracker_ptz *ctx;
		class texture_object *cvtex_cache;
		enum ptz_cmd_state_e ptz_last_cmd;
		class ptz_backend *dev;

	public:
		ft_manager_for_ftptz(struct face_tracker_ptz *ctx_) {
			ctx = ctx_;
			cvtex_cache = NULL;
			dev = NULL;
		}

		bool can_send_ptz_cmd() {
			if (dev) {
				return dev->can_send();
			}
			return false;
		}

		void release_dev() {
			if (dev) {
				dev->set_pantilt_speed(0, 0);
				dev->set_zoom_speed(0);
				dev->release();
				dev = NULL;
			}
		}

		~ft_manager_for_ftptz()
		{
			release_cvtex();
			release_dev();
		}

		inline void release_cvtex()
		{
			if (cvtex_cache)
				cvtex_cache->release();
			cvtex_cache = NULL;
		}

		class texture_object *get_cvtex() override
		{
			if (cvtex_cache)
				cvtex_cache->addref();
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
		{NULL, NULL, NULL}
	};

	const struct ptz_copy_setting_item_s list_obsptz[] = {
		{"device_id", "ptz-obsptz-device_id", copy_int},
		{NULL, NULL, NULL}
	};

	const struct ptz_copy_setting_item_s list_viscaip[] = {
		{"address", "ptz-viscaip-address", copy_string},
		{"port", "ptz-viscaip-port", copy_int},
		{NULL, NULL, NULL}
	};

	ptz_copy_settings(data, settings, list_generic);

	const char *type = obs_data_get_string(data, "type");
	if (!strcmp(type, "visca-over-tcp"))
		ptz_copy_settings(data, settings, list_viscaip);
	else if (!strcmp(type, "obsptz"))
		ptz_copy_settings(data, settings, list_obsptz);

	return data;
}

static void make_deice_obsptz(struct face_tracker_ptz *s, obs_data_t *data)
{
	s->ftm->release_dev();

	s->ftm->dev = new obsptz_backend();
	s->ftm->dev->set_config(data);
}

#ifdef WITH_PTZ_TCP
static void make_device_libvisca_tcp(struct face_tracker_ptz *s, obs_data_t *data)
{
	s->ftm->release_dev();

	if (!obs_data_get_string(data, "address"))
		return;
	if (obs_data_get_int(data, "port") <= 0)
		return;
	s->ftm->dev = new libvisca_thread();
	s->ftm->dev->set_config(data);
}
#endif // WITH_PTZ_TCP

static void make_device_dummy(struct face_tracker_ptz *s, obs_data_t *data)
{
	s->ftm->release_dev();

	s->ftm->dev = new dummy_backend();
	UNUSED_PARAMETER(data);
}

static void ftptz_update(void *data, obs_data_t *settings)
{
	auto *s = (struct face_tracker_ptz*)data;

	s->ftm->update(settings);
	s->ftm->scale = roundf(s->ftm->scale);
	s->track_z = obs_data_get_double(settings, "track_z");
	s->track_x = obs_data_get_double(settings, "track_x");
	s->track_y = obs_data_get_double(settings, "track_y");

	s->ki.v[0] = (float)obs_data_get_double(settings, "Ki_x");
	s->ki.v[1] = (float)obs_data_get_double(settings, "Ki_y");
	s->ki.v[2] = (float)obs_data_get_double(settings, "Ki_z");
	float inv_x = obs_data_get_bool(settings, "invert_x") ? -1.0f : 1.0f;
	float inv_y = obs_data_get_bool(settings, "invert_y") ? -1.0f : 1.0f;
	float inv_z = obs_data_get_bool(settings, "invert_z") ? -1.0f : 1.0f;
	s->kp_x = (float)from_dB(obs_data_get_double(settings, "Kp_x_db")) * inv_x;
	s->kp_y = (float)from_dB(obs_data_get_double(settings, "Kp_y_db")) * inv_y;
	s->kp_z = (float)from_dB(obs_data_get_double(settings, "Kp_z_db")) * inv_z;
	s->klpf.v[0] = obs_data_get_double(settings, "Td_x");
	s->klpf.v[1] = obs_data_get_double(settings, "Td_y");
	s->klpf.v[2] = obs_data_get_double(settings, "Td_z");
	s->tlpf.v[0] = s->tlpf.v[1] = (float)obs_data_get_double(settings, "Tdlpf");
	s->tlpf.v[2] = (float)obs_data_get_double(settings, "Tdlpf_z");
	s->e_deadband.v[0] = (float)obs_data_get_double(settings, "e_deadband_x") * 1e-2;
	s->e_deadband.v[1] = (float)obs_data_get_double(settings, "e_deadband_y") * 1e-2;
	s->e_deadband.v[2] = (float)obs_data_get_double(settings, "e_deadband_z") * 1e-2;
	s->e_nonlinear.v[0] = (float)obs_data_get_double(settings, "e_nonlinear_x") * 1e-2;
	s->e_nonlinear.v[1] = (float)obs_data_get_double(settings, "e_nonlinear_y") * 1e-2;
	s->e_nonlinear.v[2] = (float)obs_data_get_double(settings, "e_nonlinear_z") * 1e-2;
	float Tatt_int = (float)obs_data_get_double(settings, "Tatt_int");
	s->f_att_int = Tatt_int > 0.0f ? 1.0f / Tatt_int : 1e3;

	s->debug_faces = obs_data_get_bool(settings, "debug_faces");
	s->debug_notrack = obs_data_get_bool(settings, "debug_notrack");
	s->debug_always_show = obs_data_get_bool(settings, "debug_always_show");

	debug_data_open(&s->debug_data_tracker, &s->debug_data_tracker_last, settings, "debug_data_tracker");
	debug_data_open(&s->debug_data_error, &s->debug_data_error_last, settings, "debug_data_error");
	debug_data_open(&s->debug_data_control, &s->debug_data_control_last, settings, "debug_data_control");

	s->ptz_max_x = obs_data_get_int(settings, "ptz_max_x");
	s->ptz_max_y = obs_data_get_int(settings, "ptz_max_y");
	s->ptz_max_z = obs_data_get_int(settings, "ptz_max_z");

	const char *ptz_type = obs_data_get_string(settings, "ptz-type");
	if (!s->ptz_type || strcmp(ptz_type, s->ptz_type)) {
		obs_data_t *data = get_ptz_settings(settings);

		if (!strcmp(ptz_type, "obsptz")) {
			make_deice_obsptz(s, data);
		}
#ifdef WITH_PTZ_TCP
		else if (!strcmp(ptz_type, "visca-over-tcp")) {
			make_device_libvisca_tcp(s, data);
		}
#endif // WITH_PTZ_TCP
		else if (!strcmp(ptz_type, "dummy")) {
			make_device_dummy(s, data);
		}
		else if (s->ftm->dev) {
			s->ftm->dev->release();
			s->ftm->dev = NULL;
		}
		bfree(s->ptz_type);
		s->ptz_type = bstrdup(ptz_type);
		obs_data_release(data);
	}
	else {
		obs_data_t *data = get_ptz_settings(settings);
		if (s->ftm->dev)
			s->ftm->dev->set_config(data);
		obs_data_release(data);
	}
}

static void cb_render_info(void *data, calldata_t *cd);
static void cb_get_state(void *data, calldata_t *cd);
static void cb_set_state(void *data, calldata_t *cd);
static const char *ftptz_signals[] = {
	"void state_changed()",
	NULL
};
static void emit_state_changed(struct face_tracker_ptz *);

static void *ftptz_create(obs_data_t *settings, obs_source_t *context)
{
	auto *s = (struct face_tracker_ptz*)bzalloc(sizeof(struct face_tracker_ptz));
	s->ftm = new ft_manager_for_ftptz(s);
	s->ftm->crop_cur.x1 = s->ftm->crop_cur.y1 = -2;
	s->context = context;
	s->ftm->scale = 2.0f;
	s->hotkey_pause = OBS_INVALID_HOTKEY_PAIR_ID;
	s->hotkey_reset = OBS_INVALID_HOTKEY_ID;

	obs_source_update(context, settings);

	proc_handler_t *ph = obs_source_get_proc_handler(context);
	proc_handler_add(ph, "void render_info()", cb_render_info, s);
	proc_handler_add(ph, "void get_state()", cb_get_state, s);
	proc_handler_add(ph, "void set_state()", cb_set_state, s);

	signal_handler_t *sh = obs_source_get_signal_handler(context);
	signal_handler_add_array(sh, ftptz_signals);

	return s;
}

static void ftptz_destroy(void *data)
{
	auto *s = (struct face_tracker_ptz*)data;

	if (s->hotkey_pause != OBS_INVALID_HOTKEY_PAIR_ID)
		obs_hotkey_pair_unregister(s->hotkey_pause);
	if (s->hotkey_reset != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(s->hotkey_reset);

	delete s->ftm;
	bfree(s->ptz_type);
	if (s->debug_data_tracker)
		fclose(s->debug_data_tracker);
	if (s->debug_data_error)
		fclose(s->debug_data_error);
	if (s->debug_data_control)
		fclose(s->debug_data_control);
	bfree(s->debug_data_tracker_last);
	bfree(s->debug_data_error_last);
	bfree(s->debug_data_control_last);

	bfree(s);
}

static bool ftptz_reset_tracking(obs_properties_t *, obs_property_t *, void *data)
{
	auto *s = (struct face_tracker_ptz*)data;

	s->detect_err = f3(0, 0, 0);
	s->filter_int = f3(0, 0, 0);
	s->filter_lpf = f3(0, 0, 0);
	s->ftm->reset_requested = true;

	return true;
}

static void set_properties_visible(obs_properties_t *props, const char **names, bool visible)
{
	for (; *names; names++) {
		obs_property_t *prop = obs_properties_get(props, *names);
		if (prop) obs_property_set_visible(prop, visible);
	}
}

static bool ptz_type_modified(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	const char *ptz_type = obs_data_get_string(settings, "ptz-type");

	const char *props_obsptz[] = {
		"ptz-obsptz-device_id",
		NULL
	};
	set_properties_visible(props, props_obsptz, !strcmp(ptz_type, "obsptz"));

	const char *props_viscaip[] = {
		"ptz-viscaip-address",
		"ptz-viscaip-port",
		NULL
	};
	bool en = false;
#ifdef WITH_PTZ_TCP
	if (!strcmp(ptz_type, "visca-over-tcp"))
		en = true;
#endif // WITH_PTZ_TCP
	set_properties_visible(props, props_viscaip, en);

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
		obs_properties_add_float(pp, "track_z", obs_module_text("Zoom"), 0.1, 2.0, 0.05);
		obs_properties_add_float(pp, "track_x", obs_module_text("X"), -1.0, +1.0, 0.05);
		obs_properties_add_float(pp, "track_y", obs_module_text("Y"), -1.0, +1.0, 0.05);
		obs_properties_add_group(props, "track", obs_module_text("Tracking target location"), OBS_GROUP_NORMAL, pp);
	}

	{
		obs_properties_t *pp = obs_properties_create();
		obs_property_t *p;
		p = obs_properties_add_float(pp, "Kp_x_db", "Track Kp (X)",  -40.0, +80.0, 1.0);
		obs_property_float_set_suffix(p, " dB");
		p = obs_properties_add_float(pp, "Kp_y_db", "Track Kp (Y)", -40.0, +80.0, 1.0);
		obs_property_float_set_suffix(p, " dB");
		p = obs_properties_add_float(pp, "Kp_z_db", "Track Kp (Z)", -40.0, +60.0, 1.0);
		obs_property_float_set_suffix(p, " dB");
		obs_properties_add_float(pp, "Ki_x", "Track Ki (X)", 0.0, 5.0, 0.01);
		obs_properties_add_float(pp, "Ki_y", "Track Ki (Y)", 0.0, 5.0, 0.01);
		obs_properties_add_float(pp, "Ki_z", "Track Ki (Z)", 0.0, 5.0, 0.01);
		obs_properties_add_float(pp, "Td_x", "Track Td (X)", 0.0, 5.0, 0.01);
		obs_properties_add_float(pp, "Td_y", "Track Td (Y)", 0.0, 5.0, 0.01);
		obs_properties_add_float(pp, "Td_z", "Track Td (Z)", 0.0, 5.0, 0.01);
		obs_properties_add_float(pp, "Tdlpf", "Track LPF for Td (X, Y)", 0.0, 10.0, 0.1);
		obs_properties_add_float(pp, "Tdlpf_z", "Track LPF for Td (Z)", 0.0, 10.0, 0.1);
		obs_properties_add_float(pp, "e_deadband_x", "Dead band (X)", 0.0, 50, 0.1);
		obs_properties_add_float(pp, "e_deadband_y", "Dead band (Y)", 0.0, 50, 0.1);
		obs_properties_add_float(pp, "e_deadband_z", "Dead band (Z)", 0.0, 50, 0.1);
		obs_properties_add_float(pp, "e_nonlinear_x", "Nonlinear band (X)", 0.0, 50, 0.1);
		obs_properties_add_float(pp, "e_nonlinear_y", "Nonlinear band (Y)", 0.0, 50, 0.1);
		obs_properties_add_float(pp, "e_nonlinear_z", "Nonlinear band (Z)", 0.0, 50, 0.1);
		obs_properties_add_float(pp, "Tatt_int", "Attenuation time for lost face", 0.0, 4.0, 0.5);
		obs_properties_add_group(props, "ctrl", obs_module_text("Tracking response"), OBS_GROUP_NORMAL, pp);
	}

	{
		obs_properties_t *pp = obs_properties_create();
		obs_property_t *p = obs_properties_add_list(pp, "ptz-type", obs_module_text("PTZ Type"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
		obs_property_list_add_string(p, obs_module_text("None"), "dummy");
		obs_property_list_add_string(p, obs_module_text("through PTZ Controls"), "obsptz");
#ifdef WITH_PTZ_TCP
		obs_property_list_add_string(p, obs_module_text("VISCA over TCP"), "visca-over-tcp");
#endif // WITH_PTZ_TCP
		obs_property_set_modified_callback(p, ptz_type_modified);
		obs_properties_add_int(pp, "ptz-obsptz-device_id", obs_module_text("Device ID"), 0, 99, 1);
		obs_properties_add_text(pp, "ptz-viscaip-address", obs_module_text("IP address"), OBS_TEXT_DEFAULT);
		obs_properties_add_int(pp, "ptz-viscaip-port", obs_module_text("Port"), 1, 65535, 1);
		obs_properties_add_int_slider(pp, "ptz_max_x", "Max control (pan)",  0, PTZ_MAX_X, 1);
		obs_properties_add_int_slider(pp, "ptz_max_y", "Max control (tilt)", 0, PTZ_MAX_Y, 1);
		obs_properties_add_int_slider(pp, "ptz_max_z", "Max control (zoom)", 0, PTZ_MAX_Z, 1);
		obs_properties_add_bool(pp, "invert_x", obs_module_text("Invert control (Pan)"));
		obs_properties_add_bool(pp, "invert_y", obs_module_text("Invert control (Tilt)"));
		obs_properties_add_bool(pp, "invert_z", obs_module_text("Invert control (Zoom)"));
		obs_properties_add_group(props, "output", obs_module_text("Output"), OBS_GROUP_NORMAL, pp);
	}

	{
		obs_properties_t *pp = obs_properties_create();
		obs_properties_add_bool(pp, "debug_faces", "Show face detection results");
		obs_properties_add_bool(pp, "debug_always_show", "Always show information (useful for demo)");
#ifdef ENABLE_DEBUG_DATA
		obs_properties_add_path(pp, "debug_data_tracker", "Save correlation tracker data to file",
				OBS_PATH_FILE_SAVE, DEBUG_DATA_PATH_FILTER, NULL );
		obs_properties_add_path(pp, "debug_data_error", "Save calculated error data to file",
				OBS_PATH_FILE_SAVE, DEBUG_DATA_PATH_FILTER, NULL );
		obs_properties_add_path(pp, "debug_data_control", "Save control data to file",
				OBS_PATH_FILE_SAVE, DEBUG_DATA_PATH_FILTER, NULL );
#endif
		obs_properties_add_group(props, "debug", obs_module_text("Debugging"), OBS_GROUP_NORMAL, pp);
	}

	return props;
}

static void ftptz_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "preset_mask_track", true);
	obs_data_set_default_bool(settings, "preset_mask_control", true);
	face_tracker_manager::get_defaults(settings);
	obs_data_set_default_double(settings, "tracking_th_dB", -40.0); // overwrite the default from face_tracker_manager
	obs_data_set_default_double(settings, "track_z",  0.25); // Smaller is preferable for PTZ not to lose the face.
	obs_data_set_default_double(settings, "track_y", +0.00); // +0.00 +0.10 +0.30

	obs_data_set_default_double(settings, "Kp_x_db", 50.0);
	obs_data_set_default_double(settings, "Kp_y_db", 50.0);
	obs_data_set_default_double(settings, "Kp_z_db", 40.0);
	obs_data_set_default_double(settings, "Ki_x", 0.3);
	obs_data_set_default_double(settings, "Ki_y", 0.3);
	obs_data_set_default_double(settings, "Ki_z", 0.1);
	obs_data_set_default_double(settings, "Td_x", 0.42);
	obs_data_set_default_double(settings, "Td_y", 0.42);
	obs_data_set_default_double(settings, "Td_z", 0.14);
	obs_data_set_default_double(settings, "Tdlpf", 2.0);
	obs_data_set_default_double(settings, "Tdlpf_z", 6.0);
	obs_data_set_default_double(settings, "Tatt_int", 2.0);

	obs_data_t *presets = obs_data_create();
	obs_data_set_default_obj(settings, "presets", presets);
	obs_data_release(presets);

	obs_data_set_default_string(settings, "ptz-type", "obsptz");
	obs_data_set_default_int(settings, "ptz-viscaip-port", 1259);
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
	if (x<0.0f) return -tilt_flt2raw(-x);
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

static inline int zoom_flt2raw(float x, int u)
{
	if (std::abs(x - (float)u) < 0.75f)
		return u;
	return (int)roundf(x);
}

static bool hotkey_cb_pause(void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	auto *s = (struct face_tracker_ptz*)data;
	if (!pressed)
		return false;
	if (s->is_paused)
		return false;
	s->is_paused = true;
	emit_state_changed(s);
	return true;
}

static bool hotkey_cb_pause_resume(void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	auto *s = (struct face_tracker_ptz*)data;
	if (!pressed)
		return false;
	if (!s->is_paused)
		return false;
	s->is_paused = false;
	emit_state_changed(s);
	return true;
}

static void hotkey_cb_reset(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (pressed)
		ftptz_reset_tracking(NULL, NULL, data);
}

static void register_hotkeys(struct face_tracker_ptz *s, obs_source_t *target)
{
	if (!target)
		return;

	if (s->hotkey_pause == OBS_INVALID_HOTKEY_PAIR_ID) {
		s->hotkey_pause = obs_hotkey_pair_register_source(target,
				"face-tracker.pause",
				obs_module_text("Pause Face Tracker PTZ"),
				"face-tracker.pause_resume",
				obs_module_text("Resume Face Tracker PTZ"),
				hotkey_cb_pause, hotkey_cb_pause_resume, s, s);
	}

	if (s->hotkey_reset == OBS_INVALID_HOTKEY_ID) {
		s->hotkey_reset = obs_hotkey_register_source(target,
				"face-tracker.reset",
				obs_module_text("Reset Face Tracker PTZ"),
				hotkey_cb_reset, s);
	}
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
		if (second * s->ki.v[i] > 1.0e-10) {
			if (s->filter_int.v[i] < 0.0f && e.v[i] > 0.0f)
				e_int.v[i] = std::min(e.v[i], -s->filter_int.v[i] / (second * s->ki.v[i]));
			else if (s->filter_int.v[i] > 0.0f && e.v[i] < 0.0f)
				e_int.v[i] = std::max(e.v[i], -s->filter_int.v[i] / (second * s->ki.v[i]));
			else
				e_int.v[i] = x;
		}
		e.v[i] = x;
	}

	f3 filter_lpf_prev = s->filter_lpf;
	for (int i=0; i<3; i++)
		s->filter_int.v[i] += e_int.v[i] * s->ki.v[i] * second;
	if (!s->face_found) {
		float x = second * s->f_att_int;
		if (x < 1.0f)
			s->filter_int += s->filter_int * -x;
		else
			s->filter_int = f3(0.0f, 0.0f, 0.0f);
	}
	for (int i=0; i<3; i++)
		s->filter_lpf.v[i] = (s->filter_lpf.v[i] * s->tlpf.v[i] + e.v[i] * second) / (s->tlpf.v[i] + second);
	f3 uf (0.0f, 0.0f, 0.0f);
	if (s->face_found && s->face_found_last) {
		for (int i=0; i<3; i++)
			uf.v[i] = (e.v[i] + s->filter_int.v[i]) * second + (s->filter_lpf.v[i] - filter_lpf_prev.v[i]) * s->klpf.v[i];
	}
	s->face_found_last = s->face_found;
	const int u_max[3] = {s->ptz_max_x, s->ptz_max_y, s->ptz_max_z};
	const float kp_zoom = raw2zoomfactor(s->ptz_query[2]);
	const float kp[3] = {
		s->kp_x / srwh / kp_zoom,
		s->kp_y / srwh / kp_zoom,
		s->kp_z / srwh
	};
	for (int i=0; i<3; i++) {
		float x = uf.v[i] * kp[i];
		int n = s->u[i];
		switch(i) {
			case 0:  n = pan_flt2raw(x); break;
			case 1:  n = tilt_flt2raw(x); break;
			default: n = zoom_flt2raw(x, n); break;
		}
		if      (n < -u_max[i]) n = -u_max[i];
		else if (n > +u_max[i]) n = +u_max[i];
		s->u[i] = n;
	}

	if (s->debug_data_control) {
		fprintf(s->debug_data_control, "%f\t%f\t%f\t%f\t%d\t%d\t%d\n",
				os_gettime_ns() * 1e-9,
				uf.v[0], uf.v[1], uf.v[2],
				s->u[0], s->u[1], s->u[2] );
	}
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
	for (int i=0; i<2 && s->ftm->dev && s->ftm->can_send_ptz_cmd(); i++) {
		if (s->ftm->ptz_last_cmd != ptz_cmd_state_pantilt) {
			s->ftm->dev->set_pantilt_speed(s->u[0], s->u[1]);
			s->ftm->ptz_last_cmd = ptz_cmd_state_pantilt;
		}
		else {
			s->ftm->dev->set_zoom_speed(s->u[2]);
			s->ftm->ptz_last_cmd = ptz_cmd_state_zoom;
		}
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

	if (
			s->hotkey_pause == OBS_INVALID_HOTKEY_PAIR_ID ||
			s->hotkey_reset == OBS_INVALID_HOTKEY_ID
	   )
		register_hotkeys(s, obs_filter_get_parent(s->context));

	s->known_width = obs_source_get_base_width(target);
	s->known_height = obs_source_get_base_height(target);

	if (s->known_width<=0 || s->known_height<=0)
		return;

	if (was_rendered) {
		if (!s->is_paused)
			calculate_error(s);
		else {
			s->face_found = false;
			s->detect_err = f3(0, 0, 0);
		}

		tick_filter(s, second);
		send_ptz_cmd_immediate(s);
	}

	if (s->ftm && s->ftm->dev) {
		s->ftm->dev->tick();
		s->ptz_query[2] = s->ftm->dev->get_zoom();
	}
}

static inline void render_target(struct face_tracker_ptz *s, obs_source_t *, obs_source_t *)
{
	s->ftm->release_cvtex();
}

static inline void calculate_error(struct face_tracker_ptz *s)
{
	f3 e_tot(0.0f, 0.0f, 0.0f);
	float sc_tot = 0.0f;
	bool found = false;
	auto &tracker_rects = s->ftm->tracker_rects;
	for (size_t i = 0; i < tracker_rects.size(); i++) {
		f3 r (tracker_rects[i].rect);
		float score = tracker_rects[i].rect.score;

		if (s->ftm->landmark_detection_data) {
			pointf_s center = landmark_center(tracker_rects[i].landmark);
			float area = landmark_area(tracker_rects[i].landmark);
			if (area <= 0.0f)
				continue;

			r.v[0] = center.x;
			r.v[1] = center.y;
			r.v[2] = sqrtf(area * (float)(4.0f / M_PI));
		}

		if (s->debug_data_tracker) {
			fprintf(s->debug_data_tracker, "%f\t%f\t%f\t%f\t%f\n",
					os_gettime_ns() * 1e-9,
					r.v[0], r.v[1], r.v[2], score );
		}

		r.v[0] -= get_width(tracker_rects[i].crop_rect) * s->track_x;
		r.v[1] += get_height(tracker_rects[i].crop_rect) * s->track_y;
		r.v[2] /= s->track_z;
		f3 w (tracker_rects[i].crop_rect);

		f3 e = (r-w) * score;
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
	s->face_found = found;

	if (s->debug_data_error) {
		fprintf(s->debug_data_error, "%f\t%f\t%f\t%f\n",
				os_gettime_ns() * 1e-9,
				s->detect_err.v[0], s->detect_err.v[1], s->detect_err.v[2] );
	}
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

static void draw_frame_info(struct face_tracker_ptz *s, bool landmark_only = false)
{
	bool draw_det = !landmark_only;
	bool draw_trk = !landmark_only;
	bool draw_lmk = true;
	bool draw_ref = !landmark_only;
	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_SOLID);
	while (gs_effect_loop(effect, "Solid")) {
		if (draw_det) {
			gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"), 0xFF0000FF);
			for (size_t i = 0; i < s->ftm->detect_rects.size(); i++)
				draw_rect_upsize(s->ftm->detect_rects[i], s->ftm->upsize_l, s->ftm->upsize_r, s->ftm->upsize_t, s->ftm->upsize_b);
		}

		gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"), 0xFF00FF00);
		for (size_t i = 0; i < s->ftm->tracker_rects.size(); i++) {
			const auto &tr = s->ftm->tracker_rects[i];
			if (draw_trk)
				draw_rect_upsize(tr.rect);
			if (draw_lmk && tr.landmark.size())
				draw_landmark(tr.landmark);
		}

		if (draw_ref) {
			gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"), 0xFFFFFF00); // amber
			gs_render_start(false);
			const float srwhr2 = sqrtf((float)s->known_width * s->known_height) * 0.5f;
			const float rcx = (float)s->known_width*(0.5f + s->track_x);
			const float rcy = (float)s->known_height*(0.5f - s->track_y);
			gs_vertex2f(rcx-srwhr2*s->track_z, rcy);
			gs_vertex2f(rcx+srwhr2*s->track_z, rcy);
			gs_vertex2f(rcx, rcy-srwhr2*s->track_z);
			gs_vertex2f(rcx, rcy+srwhr2*s->track_z);
			gs_render_stop(GS_LINES);

			if (s->ftm->landmark_detection_data) {
				gs_render_start(false);
				float r = srwhr2 * s->track_z;
				for (int i=0; i<=32; i++)
					gs_vertex2f(rcx + r * sinf(M_PI * i / 8), rcy + r * cosf(M_PI * i / 8));
				gs_render_stop(GS_LINESTRIP);
			}
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

static void cb_render_info(void *data, calldata_t *cd)
{
	auto *s = (struct face_tracker_ptz*)data;
	bool landmark_only = false;
	calldata_get_bool(cd, "landmark_only", &landmark_only);

	draw_frame_info(s, landmark_only);
}

static void cb_get_state(void *data, calldata_t *cd)
{
	auto *s = (struct face_tracker_ptz*)data;
	calldata_set_bool(cd, "paused", s->is_paused);
}

static void cb_set_state(void *data, calldata_t *cd)
{
	auto *s = (struct face_tracker_ptz*)data;
	bool is_paused = s->is_paused;
	calldata_get_bool(cd, "paused", &is_paused);
	if (is_paused != s->is_paused) {
		s->is_paused = is_paused;
		emit_state_changed(s);
	}

	bool reset = false;
	calldata_get_bool(cd, "reset", &reset);
	if (reset)
		ftptz_reset_tracking(NULL, NULL, s);
}

static void emit_state_changed(struct face_tracker_ptz *s)
{
	struct calldata cd;
	uint8_t stack[128];

	calldata_init_fixed(&cd, stack, sizeof(stack));
	calldata_set_ptr(&cd, "source", s->context);
	cb_get_state(s, &cd);

	signal_handler_t *sh = obs_source_get_signal_handler(s->context);
	signal_handler_signal(sh, "state_changed", &cd);
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
