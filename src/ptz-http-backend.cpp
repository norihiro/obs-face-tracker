#include <string>
#include <vector>
#include <obs-module.h>
#include <obs.hpp>
#include <util/util.hpp>
#include <util/platform.h>
#include <curl/curl.h>
#include "plugin-macros.generated.h"
#include "ptz-http-backend.hpp"

#define CAMERA_MODELS_FILE "ptz/ptz-http-backend-camera-models.json"

static obs_data_array_t *get_camera_models()
{
	BPtr<char> path = obs_module_file(CAMERA_MODELS_FILE);
	OBSDataAutoRelease root = obs_data_create_from_json_file(path);
	return obs_data_get_array(root, "camera-models");
}

static obs_data_t *get_camera_model(const char *ptz_http_id)
{
	if (!ptz_http_id)
		return nullptr;

	OBSDataArrayAutoRelease cameras = get_camera_models();
	for (size_t i = 0, n = obs_data_array_count(cameras); i < n; i++) {
		obs_data_t *camera = obs_data_array_item(cameras, i);
		const char *id = obs_data_get_string(camera, "id");

		if (strcmp(id, ptz_http_id) == 0)
			return camera;

		obs_data_release(camera);
	}

	return nullptr;
}

static void append_data_value(std::string &ret, obs_data *data, const char *name)
{
	char buf[64] = {0};
	obs_data_item_t *item = obs_data_item_byname(data, name);
	if (!item)
		return;

	switch (obs_data_item_gettype(item)) {
	case OBS_DATA_STRING:
		ret += obs_data_item_get_string(item);
		break;
	case OBS_DATA_NUMBER:
		switch (obs_data_item_numtype(item)) {
		case OBS_DATA_NUM_INT:
			snprintf(buf, sizeof(buf) - 1, "%lld", obs_data_item_get_int(item));
			ret += buf;
			break;
		case OBS_DATA_NUM_DOUBLE:
			snprintf(buf, sizeof(buf) - 1, "%f", obs_data_item_get_double(item));
			ret += buf;
			break;
		case OBS_DATA_NUM_INVALID:
			break;
		}
	       break;
	default:
		blog(LOG_ERROR, "Cannot convert camera settings '%s'", name);
	}

	obs_data_item_release(&item);
}

static std::string replace_placeholder(const char *str, obs_data *data)
{
	/**
	 * Replaces `{name}` in `str` with the actual value in `data`.
	 * Also replaces `{{}` in `str` with `{` as an escape.
	 */

	std::string ret;

	while (*str) {
		if (strncmp(str, "{{}", 3) == 0) {
			ret += '{';
			str += 3;
		}
		else if (*str == '{') {
			str++;
			int end;
			for (end = 0; str[end] && str[end] != '}'; end++);
			if (str[end] == '}') {
				std::string name(str, str + end);
				append_data_value(ret, data, name.c_str());
				str += end + 1;
			}
		}
		else
			ret += *str++;
	}

	return ret;
}

struct control_change_s
{
	int u_int = 0;
	bool is_int = false;

	bool update(float u, obs_data_t *control_function, const char *name);
};

bool control_change_s::update(float u, obs_data_t *control_function, const char *name)
{
	OBSDataAutoRelease func = obs_data_get_obj(control_function, name);
	const char *type = obs_data_get_string(func, "type");
	if (strcmp(type, "linear-int") == 0) {
		double k1 = obs_data_get_double(func, "k1");
		double k0 = obs_data_get_double(func, "k0");
		int max = (int)obs_data_get_int(func, "max");
		int u_int_next = (int)(k1 * u + k0);
		if (u_int_next > max)
			u_int_next = max;
		else if (u_int_next < -max)
			u_int_next = -max;

		if (!is_int || u_int_next != u_int) {
			is_int = true;
			u_int = u_int_next;

			blog(LOG_INFO, "control_change_s::update: k1=%f k0=%f u=%f max=%d u_int=%d",
					k1, k0, u, max, u_int_next);

			return true;
		}
		return false;
	}

	return false;
}

static void add_control_value(obs_data_t *data, const char *name, control_change_s &u)
{
	if (u.is_int) {
		obs_data_set_int(data, name, u.u_int);
	} else {
		blog(LOG_WARNING, "control data for '%s' is not defined", name);
	}
}

struct ptz_http_backend_data_s
{
	std::mutex mutex;

	/* User input data such as
	 * "id"
	 * "host"
	 */
	OBSData user_data;

	std::atomic<bool> data_changed;
	std::atomic<bool> preset_changed;
	std::atomic<float> p_next, t_next, z_next;
};

ptz_http_backend::ptz_http_backend()
{
	data = new ptz_http_backend_data_s;

	add_ref();
	pthread_t thread;
	pthread_create(&thread, NULL, ptz_http_backend::thread_main, (void*)this);
	pthread_detach(thread);
}

ptz_http_backend::~ptz_http_backend()
{
	delete data;
}

void *ptz_http_backend::thread_main(void *data)
{
	auto *p = (ptz_http_backend*)data;
	p->thread_loop();
	p->release();
	return NULL;
}

static void call_url(const char *method, const char *url, const char *payload)
{
	blog(LOG_INFO, "call_url(method='%s', url='%s', payload='%s')", method, url, payload);

	CURL *const c = curl_easy_init();
	if (!c)
		return;

	curl_easy_setopt(c, CURLOPT_URL, url);

	// TODO: Implement method, payload, etc.

	char error[CURL_ERROR_SIZE];
	curl_easy_setopt(c, CURLOPT_ERRORBUFFER, error);

	CURLcode code = curl_easy_perform(c);
	if (code != CURLE_OK) {
		blog(LOG_WARNING, "Failed method='%s' url='%s' %s",
				method, url,
				strlen(error) ? error : curl_easy_strerror(code));
	}

	curl_easy_cleanup(c);
}

static bool send_ptz(obs_data_t *user_data, obs_data_t *camera_settings)
{
	const char *method = obs_data_get_string(camera_settings, "ptz-method");
	const char *url_t = obs_data_get_string(camera_settings, "ptz-url");
	const char *payload_t = obs_data_get_string(camera_settings, "ptz-payload");
	if (!method || !url_t)
		return false;

	std::string url = replace_placeholder(url_t, user_data);
	std::string payload = replace_placeholder(payload_t, user_data);

	blog(LOG_INFO, "send_ptz: url_t='%s' url='%s' payload_t='%s' payload='%s'",
			url_t, url.c_str(), payload_t, payload.c_str());

	call_url(method, url.c_str(), payload.c_str());

	return true;
}

void ptz_http_backend::thread_loop()
{
	bool p_changed = true, t_changed = true, z_changed = true;
	control_change_s up, ut, uz;

	OBSData user_data;
	BPtr<char> ptz_http_id;

	/* Camera settings such as
	 * "ptz-method"
	 * "ptz-url" and "ptz-payload"
	 */
	OBSDataAutoRelease camera_settings;

	/* Camera control function including these objects.
	 * "p", "t", "z"
	 */
	OBSDataAutoRelease control_function;

	while (get_ref() > 1) {
		if (data->data_changed.exchange(false)) {
			blog(LOG_INFO, "%s: data_changed was true", __func__);

			{
				std::lock_guard<std::mutex> lock(data->mutex);
				/* Assuming `data->user_data` won't be touched by the other thread. */
				user_data = data->user_data.Get();
				blog(LOG_INFO, "got user_data=%p", user_data.Get());
			}

			for (obs_data_item_t *item = obs_data_first(user_data); item; obs_data_item_next(&item)) {
				const char *name = obs_data_item_get_name(item);
				switch (obs_data_item_gettype(item)) {
					case OBS_DATA_STRING:
						blog(LOG_INFO, "ptz_http_backend::thread_loop: name='%s' value='%s'", name, obs_data_item_get_string(item));
						break;
					default:
						blog(LOG_INFO, "ptz_http_backend::thread_loop: name='%s'", name);
				}
			}

			const char *ptz_http_id_new = obs_data_get_string(user_data, "id");
			if (!ptz_http_id_new)
				continue;
			if (!ptz_http_id || strcmp(ptz_http_id_new, ptz_http_id) != 0) {
				ptz_http_id = bstrdup(ptz_http_id_new);
				OBSDataAutoRelease camera = get_camera_model(ptz_http_id_new);
				if (!camera) {
					blog(LOG_ERROR, "Camera model for '%s' was not found", ptz_http_id.Get());
					continue;
				}

				camera_settings = obs_data_get_obj(camera, "settings");
				control_function = obs_data_get_obj(camera, "control-function");

				blog(LOG_INFO, "loaded camera model '%s' %p %p", ptz_http_id_new, camera_settings.Get(), control_function.Get());
			}
		}

		if (!user_data || !ptz_http_id || !camera_settings || !control_function) {
			blog(LOG_INFO, "%s: user_data=%p ptz_http_id=%p camera_settings=%p control_function=%p",
					__func__, user_data.Get(), ptz_http_id.Get(), camera_settings.Get(), control_function.Get());
			os_sleep_ms(500);
			continue;
		}

		p_changed |= up.update(data->p_next, control_function, "p");
		t_changed |= ut.update(data->t_next, control_function, "t");
		z_changed |= uz.update(data->z_next, control_function, "z");

		add_control_value(user_data, "p", up);
		add_control_value(user_data, "t", ut);
		add_control_value(user_data, "z", uz);

		if (p_changed || t_changed || z_changed) {
			if (send_ptz(user_data, camera_settings)) {
				p_changed = t_changed = z_changed = false;
			}
		}

		// TODO: If send_ptz failed, try other variant such as send_pt and send_z.
	}
}

void ptz_http_backend::set_config(struct obs_data *user_data)
{
	blog(LOG_INFO, "%s: got user_data=%p", __func__, user_data);
	for (obs_data_item_t *item = obs_data_first(user_data); item; obs_data_item_next(&item)) {
		const char *name = obs_data_item_get_name(item);
		switch (obs_data_item_gettype(item)) {
		case OBS_DATA_STRING:
			blog(LOG_INFO, "ptz_http_backend::set_config: name='%s' value='%s'", name, obs_data_item_get_string(item));
			break;
		default:
			blog(LOG_INFO, "ptz_http_backend::set_config: name='%s'", name);
		}
	}

	std::lock_guard<std::mutex> lock(data->mutex);
	data->user_data = user_data;
	data->data_changed = true;
}

void ptz_http_backend::set_pantiltzoom_speed(float pan, float tilt, float zoom)
{
	data->p_next = pan;
	data->t_next = tilt;
	data->z_next = zoom;
}

static void remove_id_specific_props(obs_properties_t *group)
{
	std::vector<const char *> names;

	for (obs_property_t *prop = obs_properties_first(group); prop; obs_property_next(&prop)) {
		const char *name = obs_property_name(prop);
		if (strncmp(name, "ptz.http.", 9) != 0)
			continue;
		if (strcmp(name, "ptz.http.id") == 0)
			continue;
		names.push_back(name);
	}

	for (const char *name : names) {
		obs_properties_remove_by_name(group, name);
	}
}

static void add_id_specific_props(obs_properties_t *group, const char *ptz_http_id)
{
	if (!ptz_http_id)
		return;

	OBSDataAutoRelease camera = get_camera_model(ptz_http_id);
	if (!camera) {
		blog(LOG_ERROR, "Cannot find camera model '%s'", ptz_http_id);
		return;
	}

	OBSDataAutoRelease properties = obs_data_get_obj(camera, "properties");
	for (obs_data_item_t *item = obs_data_first(properties); item; obs_data_item_next(&item)) {
		const char *name = obs_data_item_get_name(item);
		OBSDataAutoRelease obj = obs_data_item_get_obj(item);
		const char *type = obs_data_get_string(obj, "type");
		const char *description = obs_data_get_string(obj, "description");
		const char *long_description = obs_data_get_string(obj, "long-description");
		obs_property_t *prop;

		if (!type || !description) {
			blog(LOG_ERROR, "camera model '%s' has invalid property '%s'", ptz_http_id, name);
			continue;
		}

		std::string prop_name = "ptz.http.";
		prop_name += ptz_http_id;
		prop_name += ".";
		prop_name += name;

		if (strcmp(type, "string") == 0) {
			prop = obs_properties_add_text(group, prop_name.c_str(), description, OBS_TEXT_DEFAULT);
		}
		else {
			blog(LOG_ERROR, "camera model '%s': property '%s': invalid type '%s'", ptz_http_id, name, type);
			continue;
		}

		if (long_description)
			obs_property_set_long_description(prop, long_description);
	}
}

static bool id_modified(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	obs_property_t *group_prop = obs_properties_get(props, "output");
	obs_properties_t *group = obs_property_group_content(group_prop);

	remove_id_specific_props(group);

	add_id_specific_props(group, obs_data_get_string(settings, "ptz.http.id"));

	return true;
}

static void init_ptz_http_id(obs_properties_t *group_output, obs_property_t *prop, obs_data_t *settings)
{
	obs_property_set_modified_callback(prop, id_modified);

	OBSDataArrayAutoRelease cameras = get_camera_models();
	for (size_t i = 0, n = obs_data_array_count(cameras); i < n; i++) {
		OBSDataAutoRelease camera = obs_data_array_item(cameras, i);

		const char *id = obs_data_get_string(camera, "id");
		const char *name = obs_data_get_string(camera, "name");

		obs_property_list_add_string(prop, name, id);
	}

	if (obs_data_has_user_value(settings, "ptz.http.id"))
		id_modified(obs_properties_get_parent(group_output), prop, settings);
}

bool ptz_http_backend::ptz_type_modified(obs_properties_t *group_output, obs_data_t *settings)
{
	if (obs_properties_get(group_output, "ptz.http.id"))
		return false;

	obs_property_t *prop = obs_properties_add_list(group_output, "ptz.http.id", obs_module_text("Camera Model"),
			OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	init_ptz_http_id(group_output, prop, settings);

	obs_properties_add_text(group_output, "ptz.http.host", obs_module_text("Host"), OBS_TEXT_DEFAULT);
	// TODO: Also consider to add basic authentication, if necessary.

	return true;
}
