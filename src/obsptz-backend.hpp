#pragma once
#include "ptz-backend.hpp"

class obsptz_backend : public ptz_backend
{
	uint64_t available_ns = 0;
	int device_id = -1;
public:
	obsptz_backend();
	~obsptz_backend() override;

	void set_config(struct obs_data *data) override;
	bool can_send() override;
	void tick() override;
	void set_pantilt_speed(int pan, int tilt) override;
	void set_zoom_speed(int zoom) override;
	int get_zoom() override;
};
