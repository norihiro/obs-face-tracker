#pragma once

#include <atomic>
#include <mutex>
#include "ptz-backend.hpp"

class ptz_http_backend : public ptz_backend
{
	struct ptz_http_backend_data_s *data;

	static void *thread_main(void *);
	void thread_loop();

public:
	ptz_http_backend();
	~ptz_http_backend() override;

	void set_config(struct obs_data *data) override;

	void set_pantilt_speed(int, int) override { }
	void set_zoom_speed(int) override { }
	void set_pantiltzoom_speed(float pan, float tilt, float zoom) override;
	void recall_preset(int) override { }
	float get_zoom() override {
		// TODO: Implement if available
		return 1.0f;
	}

public:
	static bool ptz_type_modified(obs_properties_t *group_output, obs_data_t *settings);
};
