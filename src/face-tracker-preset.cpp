#include <obs-module.h>
#include "plugin-macros.generated.h"
#include "face-tracker.hpp"
#include "face-tracker-preset.h"

static void copy_data_double(obs_data_t *dst, obs_data_t *src, const char *name)
{
	blog(LOG_INFO, "copying %s as double", name);
	double v = obs_data_get_double(src, name);
	obs_data_set_double(dst, name, v);
}

#define preset_mask_track 1
#define preset_mask_control 2
static const struct {
	const uint32_t flag;
	const char *name;
} preset_mask_flags[] = {
	{preset_mask_track, "preset_mask_track"},
	{preset_mask_control, "preset_mask_control"},
	{0, NULL}
};

static uint32_t prop_to_preset_mask(obs_data_t *prop)
{
	uint32_t mask = 0;
	for (auto *p = preset_mask_flags; p->flag; p++) {
		if (obs_data_get_bool(prop, p->name))
			mask |= p->flag;
	}
	return mask;
}

static const struct {
	const char *name;
	void(*func)(obs_data_t *, obs_data_t *, const char *);
	uint32_t mask;
} preset_property_list[] = {
	{ "upsize_l",        copy_data_double, preset_mask_track },
	{ "upsize_r",        copy_data_double, preset_mask_track },
	{ "upsize_t",        copy_data_double, preset_mask_track },
	{ "upsize_b",        copy_data_double, preset_mask_track },
	{ "scale_max",       copy_data_double, preset_mask_track },
	{ "track_z",         copy_data_double, preset_mask_track },
	{ "track_x",         copy_data_double, preset_mask_track },
	{ "track_y",         copy_data_double, preset_mask_track },
	{ "Kp",              copy_data_double, preset_mask_control },
	{ "Ki",              copy_data_double, preset_mask_control },
	{ "Kd",              copy_data_double, preset_mask_control },
	{ "Tdlpf",           copy_data_double, preset_mask_control },
	{ "e_deadband_x",    copy_data_double, preset_mask_control },
	{ "e_deadband_y",    copy_data_double, preset_mask_control },
	{ "e_deadband_z",    copy_data_double, preset_mask_control },
	{ "e_nonlineaeer_x", copy_data_double, preset_mask_control },
	{ "e_nonlineaeer_y", copy_data_double, preset_mask_control },
	{ "e_nonlineaeer_z", copy_data_double, preset_mask_control },
	// specific to face_tracker_ptz
	{ "Kp_x_db",         copy_data_double, preset_mask_control },
	{ "Kp_y_db",         copy_data_double, preset_mask_control },
	{ "Kp_z_db",         copy_data_double, preset_mask_control },
	{ "Ki_x",            copy_data_double, preset_mask_control },
	{ "Ki_y",            copy_data_double, preset_mask_control },
	{ "Ki_z",            copy_data_double, preset_mask_control },
	{ "Td_x",            copy_data_double, preset_mask_control },
	{ "Td_y",            copy_data_double, preset_mask_control },
	{ "Td_z",            copy_data_double, preset_mask_control },
	{ "Tdlpf_z",         copy_data_double, preset_mask_control },
	{ "Tatt_int",        copy_data_double, preset_mask_control },
	{ NULL, NULL, 0 }
};

static void copy_preset(obs_data_t *dst, obs_data_t *src, uint32_t mask)
{
	for (auto *p = preset_property_list; p->name && p->func; p++) {
		if (!(p->mask & mask))
			continue;
		if (!obs_data_has_user_value(src, p->name) && !obs_data_has_default_value(src, p->name))
			continue;
		p->func(dst, src, p->name);
	}
}

void ftf_preset_item_to_list(obs_property_t *p, obs_data_t *settings)
{
	obs_data_t *presets = obs_data_get_obj(settings, "presets");
	if (!presets)
		return;

	for (obs_data_item_t *item = obs_data_first(presets); item; obs_data_item_next(&item)) {
		const char *name = obs_data_item_get_name(item);
		obs_property_list_add_string(p, name, name);
	}

	obs_data_release(presets);
}

bool ftf_preset_load(obs_properties_t *, obs_property_t *, void *ctx_data)
{
	auto *s = (struct face_tracker_filter*)ctx_data;

	obs_data_t *settings = NULL;
	obs_data_t *presets = NULL;
	obs_data_t *preset_data = NULL;
	const char *preset_name;

	settings = obs_source_get_settings(s->context);
	if (!settings) goto err;
	preset_name = obs_data_get_string(settings, "preset_name");
	blog(LOG_INFO, "ftf_preset_load: loading preset %s", preset_name);
	presets = obs_data_get_obj(settings, "presets");
	if (!presets) goto err;

	preset_data = obs_data_get_obj(presets, preset_name);
	if (!preset_data) {
		blog(LOG_ERROR, "ftf_preset_load: preset %s does not exist", preset_name);
		goto err;
	}
	copy_preset(settings, preset_data, prop_to_preset_mask(settings));
	obs_source_update(s->context, settings);

err:
	obs_data_release(preset_data);
	obs_data_release(presets);
	obs_data_release(settings);
	return true;
}

static inline void list_insert_string(obs_property_t *p, const char *name)
{
	size_t count = obs_property_list_item_count(p);
	for (size_t i=0; i<count; i++) {
		const char *s = obs_property_list_item_name(p, i);
		if (s && strcmp(s, name)==0)
			return;
		if (!s || strcmp(s, name)>0) {
			obs_property_list_insert_string(p, i, name, name);
			return;
		}
	}
	obs_property_list_add_string(p, name, name);
}

static inline void list_delete_string(obs_property_t *p, const char *name)
{
	size_t count = obs_property_list_item_count(p);
	for (size_t i=0; i<count; i++) {
		const char *s = obs_property_list_item_name(p, i);
		if (s && strcmp(s, name)==0) {
			obs_property_list_item_remove(p, i);
			return;
		}
	}
}

bool ftf_preset_save(obs_properties_t *props, obs_property_t *, void *ctx_data)
{
	auto *s = (struct face_tracker_filter*)ctx_data;

	obs_data_t *settings = NULL;
	obs_data_t *presets = NULL;
	obs_data_t *preset_data = NULL;
	const char *preset_name;

	settings = obs_source_get_settings(s->context);
	if (!settings) {
		blog(LOG_ERROR, "cannot get settings for %p", s->context);
		goto err;
	}
	preset_name = obs_data_get_string(settings, "preset_name");
	blog(LOG_INFO, "ftf_preset_save: saving preset %s", preset_name);
	presets = obs_data_get_obj(settings, "presets");
	if (!presets)
		presets = obs_data_create();

	preset_data = obs_data_create();
	copy_preset(preset_data, settings, prop_to_preset_mask(settings));
	obs_data_set_obj(presets, preset_name, preset_data);

	obs_data_set_obj(settings, "presets", presets);

	if (obs_property_t *p = obs_properties_get(props, "preset_name"))
		list_insert_string(p, preset_name);

err:
	obs_data_release(preset_data);
	obs_data_release(presets);
	obs_data_release(settings);
	return true;
}

bool ftf_preset_delete(obs_properties_t *props, obs_property_t *, void *ctx_data)
{
	auto *s = (struct face_tracker_filter*)ctx_data;

	obs_data_t *settings = NULL;
	obs_data_t *presets = NULL;
	const char *preset_name;

	settings = obs_source_get_settings(s->context);
	if (!settings) {
		blog(LOG_ERROR, "cannot get settings for %p", s->context);
		goto err;
	}
	preset_name = obs_data_get_string(settings, "preset_name");
	blog(LOG_INFO, "ftf_preset_save: deleting preset %s", preset_name);
	if (obs_property_t *p = obs_properties_get(props, "preset_name"))
		list_delete_string(p, preset_name);
	presets = obs_data_get_obj(settings, "presets");
	if (!presets)
		goto err;

	obs_data_unset_user_value(presets, preset_name);

	obs_data_set_obj(settings, "presets", presets);

err:
	obs_data_release(presets);
	obs_data_release(settings);
	return true;
}

