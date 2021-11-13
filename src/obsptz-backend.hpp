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
public:
	obsptz_backend();
	~obsptz_backend() override;

	void set_config(struct obs_data *data) override;
	bool can_send() override;
	void tick() override;
	void set_pantiltzoom_speed(int pan, int tilt, int zoom) override;
	int get_zoom() override;
};
