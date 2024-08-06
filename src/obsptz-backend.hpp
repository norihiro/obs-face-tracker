#pragma once
#include "ptz-backend.hpp"

class obsptz_backend : public ptz_backend
{
	uint64_t available_ns = 0;
	int device_id = -1;
	proc_handler_t *ptz_ph = NULL;
	proc_handler_t *get_ptz_ph();
	int prev_pan = 0;
	int prev_tilt = 0;
	int prev_zoom = 0;
	int same_pantilt_cnt = 0;
	int same_zoom_cnt = 0;
public:
	obsptz_backend();
	~obsptz_backend() override;

	void set_config(struct obs_data *data) override;
	bool can_send() override;
	void tick() override;
	void set_pantilt_speed(int pan, int tilt) override;
	void set_zoom_speed(int zoom) override;
	void recall_preset(int preset) override;
	float get_zoom() override;

	static bool ptz_type_modified(obs_properties_t *group_output, obs_data_t *settings);
};
