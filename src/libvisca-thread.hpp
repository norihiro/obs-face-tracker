#pragma once

#include <util/threading.h>
// #include "libvisca.h"
#include "ptz-backend.hpp"

class libvisca_thread : public ptz_backend
{
	pthread_mutex_t mutex;
	pthread_t thread;
	struct _VISCA_interface *iface;
	struct _VISCA_camera *camera;
	struct obs_data *data;
	volatile bool data_changed;
	volatile long pan_rsvd, tilt_rsvd, zoom_rsvd;
	volatile long zoom_got;

	static void *thread_main(void *);
	void thread_connect();
	void thread_loop();

public:
	libvisca_thread();
	~libvisca_thread() override;

	void set_config(struct obs_data *data) override; // and attempt to connect

	void set_pantiltzoom_speed(int pan, int tilt, int zoom) override {
		os_atomic_set_long(&pan_rsvd, pan);
		os_atomic_set_long(&tilt_rsvd, tilt);
		os_atomic_set_long(&zoom_rsvd, zoom);
	}
	int get_zoom() override { return os_atomic_load_long(&zoom_got); }
};
