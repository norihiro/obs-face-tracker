#pragma once

#include "ptz-backend.hpp"

class dummy_backend : public ptz_backend {
	int prev_pan = 0;
	int prev_tilt = 0;
	int prev_zoom = 0;

public:
	dummy_backend();
	~dummy_backend() override;

	void set_config(struct obs_data *data) override;
	void set_pantilt_speed(int pan, int tilt) override;
	void set_zoom_speed(int zoom) override;
	void recall_preset(int preset) override;
	float get_zoom() override;
};
