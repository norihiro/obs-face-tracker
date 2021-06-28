#pragma once

void ftf_preset_item_to_list(obs_property_t *p, obs_data_t *settings);
bool ftf_preset_load(obs_properties_t *props, obs_property_t *, void *ctx_data);
bool ftf_preset_save(obs_properties_t *props, obs_property_t *, void *ctx_data);
bool ftf_preset_delete(obs_properties_t *props, obs_property_t *, void *ctx_data);
