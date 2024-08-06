#pragma once

#include <util/threading.h>
// #include "libvisca.h"
#include "ptz-backend.hpp"

class libvisca_thread : public ptz_backend
{
	pthread_mutex_t mutex;
	struct _VISCA_interface *iface;
	struct _VISCA_camera *camera;
	struct obs_data *data;
	volatile bool data_changed;
	volatile bool preset_changed;
	volatile long pan_rsvd, tilt_rsvd, zoom_rsvd;
	volatile int preset_rsvd;
	volatile long zoom_got;

	static void *thread_main(void *);
	void thread_connect();
	void thread_loop();
	float raw2zoomfactor(int);

public:
	libvisca_thread();
	~libvisca_thread() override;

	void set_config(struct obs_data *data) override; // and attempt to connect

	void set_pantilt_speed(int pan, int tilt) override {
		os_atomic_set_long(&pan_rsvd, pan);
		os_atomic_set_long(&tilt_rsvd, tilt);
	}
	void set_zoom_speed(int zoom) override { os_atomic_set_long(&zoom_rsvd, zoom); }
	void recall_preset(int preset) override {
		preset_rsvd = preset;
		os_atomic_set_bool(&preset_changed, true);
	}
	float get_zoom() override { return raw2zoomfactor(os_atomic_load_long(&zoom_got)); }

	inline static bool check_data(obs_data_t *data)
	{
		if (!obs_data_get_string(data, "address"))
			return false;
		if (obs_data_get_int(data, "port") <= 0)
			return false;
		return true;
	}
	static bool ptz_type_modified(obs_properties_t *group_output, obs_data_t *settings);
};
