#include <obs-module.h>
#include <obs.hpp>
#include "plugin-macros.generated.h"


struct face_tracker_monitor
{
	obs_source_t *context;

	// properties
	char *source_name;
	char *filter_name;
	bool notrack;
	bool nosource;

	obs_weak_source_t *source_ref;
	obs_weak_source_t *filter_ref;
};

static const char *ftmon_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Face Tracker Monitor");
}

static void *ftmon_create(obs_data_t *settings, obs_source_t *context)
{
	auto *s = (struct face_tracker_monitor*)bzalloc(sizeof(struct face_tracker_monitor));
	s->context = context;

	obs_source_update(context, settings);

	return s;
}

static void ftmon_destroy(void *data)
{
	auto *s = (struct face_tracker_monitor*)data;

	bfree(s->source_name);
	bfree(s->filter_name);
	obs_weak_source_release(s->filter_ref);
	obs_weak_source_release(s->source_ref);

	bfree(s);
}

static void ftmon_update(void *data, obs_data_t *settings)
{
	auto *s = (struct face_tracker_monitor*)data;

	const char *source_name = obs_data_get_string(settings, "source_name");
	const char *filter_name = obs_data_get_string(settings, "filter_name");
	if (source_name && (!s->source_name || strcmp(source_name, s->source_name))) {
		bfree(s->source_name);
		s->source_name = bstrdup(source_name);
	}

	if (!filter_name || !*filter_name) {
		bfree(s->filter_name);
		s->filter_name = NULL;
	} else if (!s->filter_name || strcmp(filter_name, s->filter_name)) {
		bfree(s->filter_name);
		s->filter_name = bstrdup(filter_name);
	}

	s->notrack = obs_data_get_bool(settings, "notrack");

	s->nosource = obs_data_get_bool(settings, "nosource");
}

static obs_properties_t *ftmon_properties(void *data)
{
	auto *s = (struct face_tracker_monitor*)data;
	obs_properties_t *props;
	props = obs_properties_create();

	// TODO: use obs_properties_add_list
	obs_properties_add_text(props, "source_name", obs_module_text("Source name"), OBS_TEXT_DEFAULT);
	obs_properties_add_text(props, "filter_name", obs_module_text("Filter name"), OBS_TEXT_DEFAULT);

	obs_properties_add_bool(props, "notrack", "Display original source");

	obs_properties_add_bool(props, "nosource", "Overlay only");

	return props;
}

static void ftmon_get_defaults(obs_data_t *settings)
{
}

static obs_source_t *get_source(struct face_tracker_monitor *s)
{
	if (!s->source_ref)
		return NULL;
	return obs_weak_source_get_source(s->source_ref);
}

static obs_source_t *get_filter(struct face_tracker_monitor *s)
{
	if (!s->filter_ref)
		return NULL;
	return obs_weak_source_get_source(s->filter_ref);
}

static obs_source_t *get_target(struct face_tracker_monitor *s)
{
	if (!s->filter_name || !*s->filter_name)
		return get_source(s);
	return get_filter(s);
}

static inline void tick_source(struct face_tracker_monitor *s, obs_weak_source_t *&source_ref, const char *source_name,
		obs_source_t *(*get_source)(struct face_tracker_monitor *))
{
	if (!source_name || !*source_name)
		return;

	bool fail = false;
	obs_source_t *src = get_filter(s);
	if (!src)
		fail = true;
	const char *name = src ? obs_source_get_name(src) : NULL;
	if (!name || source_name)
		fail = true;
	else if (strcmp(name, source_name))
		fail = true;

	obs_source_release(src);

	if (fail) {
		obs_weak_source_release(source_ref);
		src = get_source(s);
		source_ref = obs_source_get_weak_source(src);
		obs_source_release(src);
	}
}

obs_source_t *get_source_by_name(struct face_tracker_monitor *s)
{
	return obs_get_source_by_name(s->source_name);
}

obs_source_t *get_filter_by_name(struct face_tracker_monitor *s)
{
	obs_source_t *src = get_source(s);
	obs_source_t *ret = obs_source_get_filter_by_name(src, s->filter_name);
	obs_source_release(src);
	return ret;
}

static void ftmon_tick(void *data, float second)
{
	auto *s = (struct face_tracker_monitor*)data;

	bool source_specified = s->source_name && *s->source_name;
	bool filter_specified = s->filter_name && *s->filter_name;

	if (source_specified)
		tick_source(s, s->source_ref, s->source_name, get_source_by_name);
	if (filter_specified)
		tick_source(s, s->filter_ref, s->filter_name, get_filter_by_name);

	if (source_specified && !s->source_ref) {
		blog(LOG_INFO, "failed to get source \"%s\"", s->source_name);
	}
	else if (filter_specified && !s->filter_ref) {
		blog(LOG_INFO, "failed to get filter \"%s\"", s->filter_name);
	}
}

static uint32_t ftmon_get_width(void *data)
{
	auto *s = (struct face_tracker_monitor*)data;

	if (s->notrack) {
		OBSSource target(get_target(s));
		obs_source_release(target);

		proc_handler_t *ph = obs_source_get_proc_handler(target);
		if (!ph)
			return 0;

		calldata_t cd = {0};
		if (proc_handler_call(ph, "get_target_size", &cd)) {
			long long ret;
			if (calldata_get_int(&cd, "width", &ret)) {
				calldata_free(&cd);
				return (int32_t)ret;
			}
		}
		calldata_free(&cd);
	}

	OBSSource source(get_source(s));
	obs_source_release(source);
	if (!source)
		return 0;
	return obs_source_get_width(source);
}

static uint32_t ftmon_get_height(void *data)
{
	auto *s = (struct face_tracker_monitor*)data;

	if (s->notrack) {
		OBSSource target(get_target(s));
		obs_source_release(target);

		proc_handler_t *ph = obs_source_get_proc_handler(target);
		if (!ph)
			return 0;

		calldata_t cd = {0};
		if (proc_handler_call(ph, "get_target_size", &cd)) {
			long long ret;
			if (calldata_get_int(&cd, "height", &ret)) {
				calldata_free(&cd);
				return (int32_t)ret;
			}
		}
		calldata_free(&cd);
	}

	OBSSource source(get_source(s));
	obs_source_release(source);
	if (!source)
		return 0;
	return obs_source_get_height(source);
}

static void ftmon_video_render(void *data, gs_effect_t *)
{
	auto *s = (struct face_tracker_monitor*)data;

	OBSSource target(get_target(s));
	obs_source_release(target);
	if (!target)
		return;

	proc_handler_t *ph = obs_source_get_proc_handler(target);
	if (!ph)
		return;

	calldata_t cd = {0};
	calldata_set_bool(&cd, "notrack", s->notrack);

	if (!s->nosource) {
		if (!proc_handler_call(ph, "render_frame", &cd)) {
			OBSSource src(get_source(s));
			obs_source_release(src);
			obs_source_video_render(src);
		}
	}

	proc_handler_call(ph, "render_info", &cd);

	calldata_free(&cd);
}

extern "C"
void register_face_tracker_monitor()
{
	struct obs_source_info info = {};
	info.id = "face_tracker_monitor";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags =
#ifndef ENABLE_MONITOR_USER
		OBS_SOURCE_CAP_DISABLED |
#endif
		OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_DO_NOT_DUPLICATE;
	info.get_name = ftmon_get_name;
	info.create = ftmon_create;
	info.destroy = ftmon_destroy;
	info.update = ftmon_update;
	info.get_properties = ftmon_properties;
	info.get_defaults = ftmon_get_defaults;
	info.get_width = ftmon_get_width;
	info.get_height = ftmon_get_height;
	info.video_tick = ftmon_tick;
	info.video_render = ftmon_video_render;
	obs_register_source(&info);
}
