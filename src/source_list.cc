#include <vector>
#include <string>
#include <algorithm>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include "plugin-macros.generated.h"
#include "source_list.h"

struct add_sources_s
{
	obs_source_t *self;
	std::vector<std::string> source_names;
};

static bool add_sources(void *data, obs_source_t *source)
{
	auto &ctx = *(add_sources_s *)data;

	if (source == ctx.self)
		return true;

	uint32_t caps = obs_source_get_output_flags(source);
	if (~caps & OBS_SOURCE_VIDEO)
		return true;

	if (obs_source_is_group(source))
		return true;

	const char *name = obs_source_get_name(source);
	ctx.source_names.push_back(name);
	return true;
}

void property_list_add_sources(obs_property_t *prop, obs_source_t *self)
{
	// scenes, same order as the scene list
	obs_frontend_source_list sceneList = {};
	obs_frontend_get_scenes(&sceneList);
	for (size_t i = 0; i < sceneList.sources.num; i++) {
		obs_source_t *source = sceneList.sources.array[i];
		const char *c_name = obs_source_get_name(source);
		std::string name = obs_module_text("Scene: ");
		name += c_name;
		obs_property_list_add_string(prop, name.c_str(), c_name);
	}
	obs_frontend_source_list_free(&sceneList);

	// sources, alphabetical order
	add_sources_s ctx;
	ctx.self = self;
	obs_enum_sources(add_sources, &ctx);

	std::sort(ctx.source_names.begin(), ctx.source_names.end());

	for (size_t i = 0; i < ctx.source_names.size(); i++) {
		const std::string name = obs_module_text("Source: ") + ctx.source_names[i];
		obs_property_list_add_string(prop, name.c_str(), ctx.source_names[i].c_str());
	}
}
