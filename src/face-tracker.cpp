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
#include "face-tracker.hpp"
#include "face-tracker-preset.h"
#include "face-tracker-manager.hpp"

// #define debug_track(fmt, ...) blog(LOG_INFO, fmt, __VA_ARGS__)
#define debug_track(fmt, ...)

static gs_effect_t *effect_ft = NULL;

static inline void scale_texture(struct face_tracker_filter *s, float scale);
static inline int stage_to_surface(struct face_tracker_filter *s, float scale);
static inline class texture_object *surface_to_cvtex(struct face_tracker_filter *s, float scale);

class ft_manager_for_ftf : public face_tracker_manager
{
	public:
		struct face_tracker_filter *ctx;

	public:
		ft_manager_for_ftf(struct face_tracker_filter *ctx_) {
			ctx = ctx_;
		}

		~ft_manager_for_ftf()
		{
			release_cvtex();
		}

		inline void release_cvtex()
		{
		}

		class texture_object *get_cvtex() override
		{
			if (scale<1.0f) scale = 1.0f;
			scale_texture(ctx, scale);
			if (stage_to_surface(ctx, scale))
				return NULL;
			return surface_to_cvtex(ctx, scale);
		};
};

static const char *ftf_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Face Tracker");
}

static inline void get_aspect_from_str(struct face_tracker_filter *s, const char *str)
{
	if (sscanf(str, "%d:%d", &s->aspect_x, &s->aspect_y)==2)
		return;
	if (sscanf(str, "%dx%d", &s->aspect_x, &s->aspect_y)==2)
		return;
	s->aspect_x = 0;
	s->aspect_y = 0;
}

static void ftf_update(void *data, obs_data_t *settings)
{
	auto *s = (struct face_tracker_filter*)data;

	s->ftm->update(settings);
	s->track_z = obs_data_get_double(settings, "track_z");
	s->track_x = obs_data_get_double(settings, "track_x");
	s->track_y = obs_data_get_double(settings, "track_y");
	s->scale_max = obs_data_get_double(settings, "scale_max");

	double kp = obs_data_get_double(settings, "Kp");
	float ki = (float)obs_data_get_double(settings, "Ki");
	double td = obs_data_get_double(settings, "Td");
	double att2 = from_dB(obs_data_get_double(settings, "att2_dB"));
	s->kp.v[0] = s->kp.v[1] = (float)kp;
	s->kp.v[2] = (float)(att2 * kp);
	s->ki = ki;
	s->klpf = s->kp * td;
	s->tlpf.v[0] = s->tlpf.v[1] = (float)obs_data_get_double(settings, "Tdlpf");
	s->tlpf.v[2] = (float)obs_data_get_double(settings, "Tdlpf_z");
	s->e_deadband.v[0] = (float)obs_data_get_double(settings, "e_deadband_x") * 1e-2;
	s->e_deadband.v[1] = (float)obs_data_get_double(settings, "e_deadband_y") * 1e-2;
	s->e_deadband.v[2] = (float)obs_data_get_double(settings, "e_deadband_z") * 1e-2;
	s->e_nonlinear.v[0] = (float)obs_data_get_double(settings, "e_nonlinear_x") * 1e-2;
	s->e_nonlinear.v[1] = (float)obs_data_get_double(settings, "e_nonlinear_y") * 1e-2;
	s->e_nonlinear.v[2] = (float)obs_data_get_double(settings, "e_nonlinear_z") * 1e-2;

	get_aspect_from_str(s, obs_data_get_string(settings, "aspect"));

	s->debug_faces = obs_data_get_bool(settings, "debug_faces");
	s->debug_notrack = obs_data_get_bool(settings, "debug_notrack");
	s->debug_always_show = obs_data_get_bool(settings, "debug_always_show");
}

static void *ftf_create(obs_data_t *settings, obs_source_t *context)
{
	auto *s = (struct face_tracker_filter*)bzalloc(sizeof(struct face_tracker_filter));
	s->ftm = new ft_manager_for_ftf(s);
	s->ftm->crop_cur.x1 = s->ftm->crop_cur.y1 = -2;
	s->context = context;
	s->ftm->scale = 2.0f;
	s->hotkey_pause = OBS_INVALID_HOTKEY_PAIR_ID;
	s->hotkey_reset = OBS_INVALID_HOTKEY_ID;

	obs_enter_graphics();
	if (!effect_ft) {
		char *f = obs_module_file("face-tracker.effect");
		effect_ft = gs_effect_create_from_file(f, NULL);
		if (!effect_ft)
			blog(LOG_ERROR, "Cannot load '%s' (face-tracker.effect)", f);
		bfree(f);
	}
	obs_leave_graphics();

	obs_source_update(context, settings);
	return s;
}

static void ftf_destroy(void *data)
{
	auto *s = (struct face_tracker_filter*)data;

	if (s->hotkey_pause != OBS_INVALID_HOTKEY_PAIR_ID)
		obs_hotkey_pair_unregister(s->hotkey_pause);
	if (s->hotkey_reset != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(s->hotkey_reset);

	obs_enter_graphics();
	gs_texrender_destroy(s->texrender);
	s->texrender = NULL;
	gs_texrender_destroy(s->texrender_scaled);
	s->texrender_scaled = NULL;
	gs_stagesurface_destroy(s->stagesurface);
	s->stagesurface = NULL;
	obs_leave_graphics();

	delete s->ftm;

	bfree(s);
}

static bool ftf_reset_tracking(obs_properties_t *, obs_property_t *, void *data)
{
	auto *s = (struct face_tracker_filter*)data;

	float w = s->known_width;
	float h = s->known_height;
	float z = sqrtf(s->width_with_aspect * s->height_with_aspect);
	s->detect_err = f3(0, 0, 0);
	s->filter_int_out = f3(w*0.5f, h*0.5f, z);
	s->filter_int = f3(0, 0, 0);
	s->filter_lpf = f3(0, 0, 0);
	s->ftm->reset_requested = true;

	return true;
}

static obs_properties_t *ftf_properties(void *data)
{
	auto *s = (struct face_tracker_filter*)data;
	obs_properties_t *props;
	props = obs_properties_create();

	obs_properties_add_button(props, "ftf_reset_tracking", obs_module_text("Reset tracking"), ftf_reset_tracking);

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
		obs_properties_add_float(pp, "Tdlpf", "Track LPF for Td (X, Y)", 0.0, 10.0, 0.1);
		obs_properties_add_float(pp, "Tdlpf_z", "Track LPF for Td (Z)", 0.0, 10.0, 0.1);
		obs_properties_add_float(pp, "att2_dB", "Attenuation (Z)", -60.0, 10.0, 1.0);
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
		obs_property_t *p = obs_properties_add_list(pp, "aspect", obs_module_text("Aspect"), OBS_COMBO_TYPE_EDITABLE, OBS_COMBO_FORMAT_STRING);
		const char *aspects[] = {
			"16:9",
			"4:3",
			"1:1",
			"3:4",
			"9:16",
			NULL
		};
		obs_property_list_add_string(p, obs_module_text("same as the source"), "");
		for (int i=0; aspects[i]; i++)
			obs_property_list_add_string(p, aspects[i], aspects[i]);
		obs_properties_add_group(props, "output", obs_module_text("Output"), OBS_GROUP_NORMAL, pp);
	}

	{
		obs_properties_t *pp = obs_properties_create();
		obs_properties_add_bool(pp, "debug_faces", "Show face detection results");
		obs_properties_add_bool(pp, "debug_notrack", "Stop tracking faces");
		obs_properties_add_bool(pp, "debug_always_show", "Always show information (useful for demo)");
		obs_properties_add_group(props, "debug", obs_module_text("Debugging"), OBS_GROUP_NORMAL, pp);
	}

	return props;
}

static void ftf_get_defaults(obs_data_t *settings)
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
	obs_data_set_default_double(settings, "Tdlpf_z", 6.0);
	obs_data_set_default_double(settings, "att2_dB", -10);

	obs_data_t *presets = obs_data_create();
	obs_data_set_default_obj(settings, "presets", presets);
	obs_data_release(presets);
}

static void tick_filter(struct face_tracker_filter *s, float second)
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

	s->filter_int_out += (e + s->filter_int).hp(s->kp * second);
	s->filter_int += e_int * (second * s->ki);
	for (int i=0; i<3; i++)
		s->filter_lpf.v[i] = (s->filter_lpf.v[i] * s->tlpf.v[i] + e.v[i] * second) / (s->tlpf.v[i] + second);

	f3 u = s->filter_int_out + s->filter_lpf.hp(s->klpf);

	for (int i=0; i<3; i++) {
		if (isnan(u.v[i]))
			u.v[i] = s->range_min_out.v[i];
		else if (u.v[i] < s->range_min_out.v[i]) {
			u.v[i] = s->range_min_out.v[i];
			if (s->filter_int_out.v[i] < s->range_min_out.v[i])
				s->filter_int_out.v[i] = s->range_min_out.v[i];
		}
		else if (u.v[i] > s->range_max.v[i]) {
			u.v[i] = s->range_max.v[i];
			if (s->filter_int_out.v[i] > s->range_max.v[i])
				s->filter_int_out.v[i] = s->range_max.v[i];
		}
	}

	s->ftm->crop_cur = f3_to_rectf(u, s->width_with_aspect, s->height_with_aspect);
}

static void ftf_activate(void *data)
{
	auto *s = (struct face_tracker_filter*)data;
	s->is_active = true;
}

static void ftf_deactivate(void *data)
{
	auto *s = (struct face_tracker_filter*)data;
	s->is_active = false;
}

static inline void calculate_error(struct face_tracker_filter *s);

static void calculate_aspect(struct face_tracker_filter *s)
{
	if (s->aspect_y<=0 || s->aspect_x<=0) {
		s->width_with_aspect = s->known_width;
		s->height_with_aspect = s->known_height;
	}
	else if (s->known_width * s->aspect_y >= s->known_height * s->aspect_x) {
		s->height_with_aspect = s->known_height;
		s->width_with_aspect = s->aspect_x * s->known_height / s->aspect_y;
	}
	else {
		s->width_with_aspect = s->known_width;
		s->height_with_aspect = s->known_width * s->aspect_y / s->aspect_x;
	}
}

static bool hotkey_cb_pause(void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	auto *s = (struct face_tracker_filter*)data;
	if (!pressed)
		return false;
	if (s->is_paused)
		return false;
	s->is_paused = true;
	return true;
}

static bool hotkey_cb_pause_resume(void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	auto *s = (struct face_tracker_filter*)data;
	if (!pressed)
		return false;
	if (!s->is_paused)
		return false;
	s->is_paused = false;
	return true;
}

static void hotkey_cb_reset(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	if (pressed)
		ftf_reset_tracking(NULL, NULL, data);
}

static void ftf_tick(void *data, float second)
{
	auto *s = (struct face_tracker_filter*)data;
	const bool was_rendered = s->rendered;
	s->ftm->tick(second);

	obs_source_t *target = obs_filter_get_target(s->context);
	if (!target)
		goto err;

	if (s->hotkey_pause == OBS_INVALID_HOTKEY_PAIR_ID) {
		obs_source_t *parent = obs_filter_get_parent(s->context);
		s->hotkey_pause = obs_hotkey_pair_register_source(parent,
				"face-tracker.pause",
				obs_module_text("Pause"),
				"face-tracker.pause_resume",
				obs_module_text("Resume"),
				hotkey_cb_pause, hotkey_cb_pause_resume, s, s);
	}
	if (s->hotkey_reset == OBS_INVALID_HOTKEY_ID) {
		obs_source_t *parent = obs_filter_get_parent(s->context);
		s->hotkey_reset = obs_hotkey_register_source(parent,
				"face-tracker.reset",
				obs_module_text("Reset tracking"),
				hotkey_cb_reset, s);
	}

	s->rendered = false;

	s->known_width = obs_source_get_base_width(target);
	s->known_height = obs_source_get_base_height(target);

	if (s->known_width<=0 || s->known_height<=0)
		goto err;

	calculate_aspect(s);

	if (s->ftm->crop_cur.x1<-1 || s->ftm->crop_cur.y1<-1) {
		ftf_reset_tracking(NULL, NULL, s);
		s->ftm->crop_cur = f3_to_rectf(s->filter_int_out, s->width_with_aspect, s->height_with_aspect);
	}
	else if (was_rendered && !s->is_paused) {
		s->range_min.v[0] = get_width(s->ftm->crop_cur) * 0.5f;
		s->range_max.v[0] = s->known_width - get_width(s->ftm->crop_cur) * 0.5f;
		s->range_min.v[1] = get_height(s->ftm->crop_cur) * 0.5f;
		s->range_max.v[1] = s->known_height - get_height(s->ftm->crop_cur) * 0.5f;
		s->range_min.v[2] = sqrtf(s->known_width*s->known_height) / s->scale_max;
		s->range_max.v[2] = sqrtf(s->width_with_aspect * s->height_with_aspect);
		s->range_min_out = s->range_min;
		s->range_min_out.v[2] = std::min(s->range_min.v[2], s->filter_int_out.v[2]);
		calculate_error(s);
		tick_filter(s, second);
	}

	s->target_valid = true;
	return;
err:
	s->target_valid = false;
}

static inline void render_target(struct face_tracker_filter *s, obs_source_t *target, obs_source_t *parent)
{
	if (!s->texrender)
		s->texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	const uint32_t cx = s->known_width, cy = s->known_height;
	gs_texrender_reset(s->texrender);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

	if (gs_texrender_begin(s->texrender, cx, cy)) {
		uint32_t parent_flags = obs_source_get_output_flags(target);
		bool custom_draw = (parent_flags & OBS_SOURCE_CUSTOM_DRAW) != 0;
		bool async = (parent_flags & OBS_SOURCE_ASYNC) != 0;
		struct vec4 clear_color;

		vec4_zero(&clear_color);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
		gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);

		if (target == parent && !custom_draw && !async)
			obs_source_default_render(target);
		else
			obs_source_video_render(target);

		gs_texrender_end(s->texrender);
	}

	gs_blend_state_pop();

	s->ftm->release_cvtex();
}

static inline f3 ensure_range(f3 u, const struct face_tracker_filter *s)
{
	if (isnan(u.v[2]))
		u.v[2] = s->range_min.v[2];
	else if (u.v[2] < s->range_min.v[2])
		u.v[2] = s->range_min.v[2];
	else if (u.v[2] > s->range_max.v[2])
		u.v[2] = s->range_max.v[2];

	rectf_s r = f3_to_rectf(u, s->width_with_aspect, s->height_with_aspect);

	if (r.x0 < 0)
		u.v[0] += -r.x0;
	else if (r.x1 > s->known_width)
		u.v[0] -= r.x1 - s->known_width;

	if (r.y0 < 0)
		u.v[1] += -r.y0;
	else if (r.y1 > s->known_height)
		u.v[1] -= r.y1 - s->known_height;

	return u;
}

static inline void calculate_error(struct face_tracker_filter *s)
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
		r = ensure_range(r, s);
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

static inline void draw_sprite_crop(float width, float height, float x0, float y0, float x1, float y1);

static inline void scale_texture(struct face_tracker_filter *s, float scale)
{
	if (!s->texrender_scaled)
		s->texrender_scaled = gs_texrender_create(GS_R8, GS_ZS_NONE);
	const uint32_t cx = s->known_width / scale, cy = s->known_height / scale;
	gs_texrender_reset(s->texrender_scaled);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
	if (gs_texrender_begin(s->texrender_scaled, cx, cy)) {
		gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);
		gs_texture_t *tex = gs_texrender_get_texture(s->texrender);
		if (tex && effect_ft) {
			gs_eparam_t *image = gs_effect_get_param_by_name(effect_ft, "image");
			gs_effect_set_texture(image, tex);
			while (gs_effect_loop(effect_ft, "DrawY"))
				draw_sprite_crop(cx, cy, 0, 0, 1, 1);
		}
		gs_texrender_end(s->texrender_scaled);
	}
	gs_blend_state_pop();
}

static inline int stage_to_surface(struct face_tracker_filter *s, float scale)
{
	uint32_t width = s->known_width / scale;
	uint32_t height = s->known_height / scale;
	if (width<=0 || height<=0)
		return 1;

	gs_texture_t *tex = gs_texrender_get_texture(s->texrender_scaled);
	if (!tex)
		return 2;

	if (!s->stagesurface ||
			width != gs_stagesurface_get_width(s->stagesurface) ||
			height != gs_stagesurface_get_height(s->stagesurface) ) {
		gs_stagesurface_destroy(s->stagesurface);
		s->stagesurface = gs_stagesurface_create(width, height, GS_R8);
	}

	gs_stage_texture(s->stagesurface, tex);

	return 0;
}

static inline class texture_object *surface_to_cvtex(struct face_tracker_filter *s, float scale)
{
	texture_object *cvtex = NULL;
	uint8_t *video_data = NULL;
	uint32_t video_linesize;
	if (gs_stagesurface_map(s->stagesurface, &video_data, &video_linesize)) {
		uint32_t width = gs_stagesurface_get_width(s->stagesurface);
		uint32_t height = gs_stagesurface_get_height(s->stagesurface);

		cvtex = new texture_object();
		cvtex->scale = scale;
		cvtex->tick = s->ftm->tick_cnt;
		cvtex->set_texture_y(video_data, video_linesize, width, height);

		gs_stagesurface_unmap(s->stagesurface);
	}

	return cvtex;
}

static inline void draw_sprite_crop(float width, float height, float x0, float y0, float x1, float y1)
{
	gs_render_start(false);
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(width, 0.0f);
	gs_vertex2f(0.0f, height);
	gs_vertex2f(width, height);
	struct vec2 tv;
	vec2_set(&tv, x0, y0); gs_texcoord2v(&tv, 0);
	vec2_set(&tv, x1, y0); gs_texcoord2v(&tv, 0);
	vec2_set(&tv, x0, y1); gs_texcoord2v(&tv, 0);
	vec2_set(&tv, x1, y1); gs_texcoord2v(&tv, 0);
	gs_render_stop(GS_TRISTRIP);
}

static inline void draw_frame_texture(struct face_tracker_filter *s)
{
	uint32_t width = s->width_with_aspect;
	uint32_t height = s->height_with_aspect;
	const rectf_s &crop_cur = s->ftm->crop_cur;
	const float scale = sqrtf((float)(width*height) / ((crop_cur.x1-crop_cur.x0) * (crop_cur.y1-crop_cur.y0)));
	const bool debug_notrack = s->debug_notrack && (!s->is_active || s->debug_always_show);

	// TODO: linear_srgb, 27 only?

	if (width<=0 || height<=0)
		return;

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_eparam_t *image = gs_effect_get_param_by_name(effect, "image");
	gs_texture_t *tex = gs_texrender_get_texture(s->texrender);
	if (!tex)
		return;
	gs_effect_set_texture(image, tex);

	while (gs_effect_loop(effect, "Draw")) {
		if (debug_notrack)
			gs_draw_sprite(tex, 0, s->known_width, s->known_height);
		else
			draw_sprite_crop(
					width, height,
					crop_cur.x0/s->known_width, crop_cur.y0/s->known_height,
					crop_cur.x1/s->known_width, crop_cur.y1/s->known_height );
	}
}

static inline void draw_frame_info(struct face_tracker_filter *s)
{
	const rectf_s &crop_cur = s->ftm->crop_cur;

	const bool debug_notrack = s->debug_notrack && (!s->is_active || s->debug_always_show);
	if (!debug_notrack) {
		uint32_t width = s->width_with_aspect;
		uint32_t height = s->height_with_aspect;
		const float scale = sqrtf((float)(width*height) / ((crop_cur.x1-crop_cur.x0) * (crop_cur.y1-crop_cur.y0)));

		gs_matrix_push();
		struct matrix4 tr;
		matrix4_identity(&tr);
		matrix4_translate3f(&tr, &tr, -(crop_cur.x0+crop_cur.x1)*0.5f, -(crop_cur.y0+crop_cur.y1)*0.5f, 0.0f);
		matrix4_scale3f(&tr, &tr, scale, scale, 1.0f);
		matrix4_translate3f(&tr, &tr, width/2, height/2, 0.0f);
		gs_matrix_mul(&tr);
	}

	gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_SOLID);
	while (gs_effect_loop(effect, "Solid")) {
		gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"), 0xFF0000FF);
		for (int i=0; i<s->ftm->detect_rects.size(); i++)
			draw_rect_upsize(s->ftm->detect_rects[i], s->ftm->upsize_l, s->ftm->upsize_r, s->ftm->upsize_t, s->ftm->upsize_b);

		gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"), 0xFF00FF00);
		for (int i=0; i<s->ftm->tracker_rects.size(); i++) {
			draw_rect_upsize(s->ftm->tracker_rects[i].rect);
		}
		if (debug_notrack) {
			gs_effect_set_color(gs_effect_get_param_by_name(effect, "color"), 0xFFFFFF00); // amber
			const rectf_s &r = s->ftm->crop_cur;
			gs_render_start(false);
			gs_vertex2f(r.x0, r.y0);
			gs_vertex2f(r.x0, r.y1);
			gs_vertex2f(r.x0, r.y1);
			gs_vertex2f(r.x1, r.y1);
			gs_vertex2f(r.x1, r.y1);
			gs_vertex2f(r.x1, r.y0);
			gs_vertex2f(r.x1, r.y0);
			gs_vertex2f(r.x0, r.y0);
			const float srwhr2 = sqrtf((r.x1-r.x0) * (r.y1-r.y0)) * 0.5f;
			const float rcx = (r.x0+r.x1)*0.5f + (r.x1-r.x0)*s->track_x;
			const float rcy = (r.y0+r.y1)*0.5f - (r.y1-r.y0)*s->track_y;
			gs_vertex2f(rcx-srwhr2*s->track_z, rcy);
			gs_vertex2f(rcx+srwhr2*s->track_z, rcy);
			gs_vertex2f(rcx, rcy-srwhr2*s->track_z);
			gs_vertex2f(rcx, rcy+srwhr2*s->track_z);
			gs_render_stop(GS_LINES);
		}
	}

	if (!debug_notrack)
		gs_matrix_pop();
}

static inline void draw_frame(struct face_tracker_filter *s)
{
	draw_frame_texture(s);

	if (s->debug_faces && (!s->is_active || s->debug_always_show))
		draw_frame_info(s);
}

static void ftf_render(void *data, gs_effect_t *)
{
	auto *s = (struct face_tracker_filter*)data;
	if (!s->target_valid) {
		obs_source_skip_video_filter(s->context);
		return;
	}
	obs_source_t *target = obs_filter_get_target(s->context);
	obs_source_t *parent = obs_filter_get_parent(s->context);

	if (!target || !parent) {
		obs_source_skip_video_filter(s->context);
		return;
	}

	if (!s->rendered) {
		render_target(s, target, parent);
		if (!s->is_paused)
			s->ftm->post_render();
		s->rendered = true;
	}

	draw_frame(s);
}

static uint32_t ftf_width(void *data)
{
	auto *s = (struct face_tracker_filter*)data;
	const bool debug_notrack = s->debug_notrack && (!s->is_active || s->debug_always_show);
	return debug_notrack ? s->known_width : s->width_with_aspect;
}

static uint32_t ftf_height(void *data)
{
	auto *s = (struct face_tracker_filter*)data;
	const bool debug_notrack = s->debug_notrack && (!s->is_active || s->debug_always_show);
	return debug_notrack ? s->known_height : s->height_with_aspect;
}

extern "C"
void register_face_tracker_filter()
{
	struct obs_source_info info = {};
	info.id = "face_tracker_filter";
	info.type = OBS_SOURCE_TYPE_FILTER;
	info.output_flags = OBS_SOURCE_VIDEO;
	info.get_name = ftf_get_name;
	info.create = ftf_create;
	info.destroy = ftf_destroy;
	info.update = ftf_update;
	info.get_properties = ftf_properties;
	info.get_defaults = ftf_get_defaults;
	info.activate = ftf_activate,
	info.deactivate = ftf_deactivate,
	info.video_tick = ftf_tick;
	info.video_render = ftf_render;
	info.get_width = ftf_width;
	info.get_height = ftf_height;
	obs_register_source(&info);
}
