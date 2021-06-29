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

// #define debug_track(fmt, ...) blog(LOG_INFO, fmt, __VA_ARGS__)
#define debug_track(fmt, ...)

static gs_effect_t *effect_ft = NULL;

class ft_manager_for_ftptz : public face_tracker_manager
{
	public:
		struct face_tracker_ptz *ctx;
		class texture_object *cvtex_cache;

	public:
		ft_manager_for_ftptz(struct face_tracker_ptz *ctx_) {
			ctx = ctx_;
			cvtex_cache = NULL;
		}

		~ft_manager_for_ftptz()
		{
			release_cvtex();
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

static void ftptz_update(void *data, obs_data_t *settings)
{
	auto *s = (struct face_tracker_ptz*)data;

	s->ftm->update(settings);
	s->ftm->scale = roundf(s->ftm->scale);
	s->track_z = obs_data_get_double(settings, "track_z");
	s->track_x = obs_data_get_double(settings, "track_x");
	s->track_y = obs_data_get_double(settings, "track_y");

	double kp = obs_data_get_double(settings, "Kp");
	float ki = (float)obs_data_get_double(settings, "Ki");
	double td = obs_data_get_double(settings, "Td");
	s->kp = (float)kp;
	s->ki = ki;
	s->klpf = (float)(td * kp);
	s->tlpf = (float)obs_data_get_double(settings, "Tdlpf");
	s->e_deadband.v[0] = (float)obs_data_get_double(settings, "e_deadband_x") * 1e-2;
	s->e_deadband.v[1] = (float)obs_data_get_double(settings, "e_deadband_y") * 1e-2;
	s->e_deadband.v[2] = (float)obs_data_get_double(settings, "e_deadband_z") * 1e-2;
	s->e_nonlinear.v[0] = (float)obs_data_get_double(settings, "e_nonlinear_x") * 1e-2;
	s->e_nonlinear.v[1] = (float)obs_data_get_double(settings, "e_nonlinear_y") * 1e-2;
	s->e_nonlinear.v[2] = (float)obs_data_get_double(settings, "e_nonlinear_z") * 1e-2;

	s->debug_faces = obs_data_get_bool(settings, "debug_faces");
	s->debug_notrack = obs_data_get_bool(settings, "debug_notrack");
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

	bfree(s);
}

static bool ftptz_reset_tracking(obs_properties_t *, obs_property_t *, void *data)
{
	auto *s = (struct face_tracker_ptz*)data;

	s->detect_err = f3(0, 0, 0);
	s->filter_int = f3(0, 0, 0);
	s->filter_lpf = f3(0, 0, 0);

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
		obs_properties_add_float(pp, "scale_max", obs_module_text("Scale max"), 1.0, 20.0, 1.0);
		obs_properties_add_group(props, "track", obs_module_text("Tracking target location"), OBS_GROUP_NORMAL, pp);
	}

	{
		obs_properties_t *pp = obs_properties_create();
		obs_properties_add_float(pp, "Kp", "Track Kp", 0.01, 10.0, 0.1);
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
		obs_properties_add_bool(pp, "debug_notrack", "Stop tracking faces");
		obs_properties_add_group(props, "debug", obs_module_text("Debugging"), OBS_GROUP_NORMAL, pp);
	}

	return props;
}

static void ftptz_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, "preset_mask_track", true);
	obs_data_set_default_bool(settings, "preset_mask_control", true);
	face_tracker_manager::get_defaults(settings);
	obs_data_set_default_double(settings, "track_z",  0.70); //  1.00  0.50  0.35
	obs_data_set_default_double(settings, "track_y", +0.00); // +0.00 +0.10 +0.30
	obs_data_set_default_double(settings, "scale_max", 10.0);

	obs_data_set_default_double(settings, "Kp", 0.95);
	obs_data_set_default_double(settings, "Ki", 0.3);
	obs_data_set_default_double(settings, "Td", 0.42);
	obs_data_set_default_double(settings, "Tdlpf", 2.0);

	obs_data_t *presets = obs_data_create();
	obs_data_set_default_obj(settings, "presets", presets);
	obs_data_release(presets);
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

	// TODO: s->filter_int_out += (e + s->filter_int) * (second * s->kp);
	s->filter_int += e_int * (second * s->ki);
	s->filter_lpf = (s->filter_lpf * s->tlpf + e * second) * (1.f/(s->tlpf + second));

	// TODO: f3 u = s->filter_int_out + s->filter_lpf * s->klpf;
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

	else if (was_rendered) {
		calculate_error(s);
		tick_filter(s, second);
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

	if (s->debug_faces && !s->is_active)
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
