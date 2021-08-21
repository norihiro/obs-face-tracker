#pragma once

#include <util/threading.h>
// #include "libvisca.h"

class libvisca_thread
{
	pthread_mutex_t mutex;
	pthread_t thread;
	struct _VISCA_interface *iface;
	struct _VISCA_camera *camera;
	volatile long ref;
	struct obs_data *data;
	volatile bool data_changed;
	volatile long pan_rsvd, tilt_rsvd, zoom_rsvd;
	volatile long zoom_got;

	static void *thread_main(void *);
	void thread_connect();
	void thread_loop();

public:
	libvisca_thread();
	~libvisca_thread();
	void add_ref() { os_atomic_inc_long(&ref); }
	void release();

	void set_config(struct obs_data *data); // and attempt to connect

	void set_pantilt_speed(int pan, int tilt) {
		os_atomic_set_long(&pan_rsvd, pan);
		os_atomic_set_long(&tilt_rsvd, tilt);
	}
	void set_zoom_speed(int zoom) { os_atomic_set_long(&zoom_rsvd, zoom); }
	int get_zoom() { return os_atomic_load_long(&zoom_got); }
};
