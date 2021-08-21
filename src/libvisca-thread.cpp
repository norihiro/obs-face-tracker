#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <graphics/vec2.h>
#include <graphics/graphics.h>
#include <climits>
#include <cstdlib>
#include "plugin-macros.generated.h"
#include "libvisca-thread.hpp"
#include "libvisca.h"

#define debug(...) blog(LOG_INFO, __VA_ARGS__)

libvisca_thread::libvisca_thread()
{
	debug("libvisca_thread::libvisca_thread");
	iface = NULL;
	camera = NULL;
	ref = 1;
	data = NULL;
	pan_rsvd = 0;
	tilt_rsvd = 0;
	zoom_rsvd = 0;
	zoom_got = 0;
	pthread_mutex_init(&mutex, 0);

	pthread_create(&thread, NULL, libvisca_thread::thread_main, (void*)this);
	pthread_detach(thread);
}

libvisca_thread::~libvisca_thread()
{
	if (iface) {
		VISCA_close(iface);
		bfree(iface);
	}
	if (camera)
		bfree(camera);
	if (data)
		obs_data_release(data);
	pthread_mutex_destroy(&mutex);
}

void libvisca_thread::release()
{
	if (os_atomic_dec_long(&ref) == 0)
		delete this;
}

void libvisca_thread::thread_connect()
{
	pthread_mutex_lock(&mutex);

	const char *address = obs_data_get_string(data, "address");
	int port = (int)obs_data_get_int(data, "port");
	auto *iface_new = (struct _VISCA_interface*)bzalloc(sizeof(struct _VISCA_interface));
	debug("libvisca_thread::thread_connect connecting to address=%s port=%d...", address, port);
	if (VISCA_open_tcp(iface_new, address, port) != VISCA_SUCCESS) {
		blog(LOG_ERROR, "failed to connect %s:%d", address, port);
		bfree(iface_new);
		iface_new = NULL;
	}
	debug("libvisca_thread::thread_connect connected.");
	pthread_mutex_unlock(&mutex);
	if (!iface_new)
		return;

	if (iface) {
		VISCA_close(iface);
		bfree(iface);
	}
	iface = iface_new;
	if (!camera)
		camera = (VISCACamera_t*)bzalloc(sizeof(VISCACamera_t));
	data_changed = false;

	debug("libvisca_thread::thread_connect sending VISCA_clear...");
	camera->address = 1;
	VISCA_clear(iface, camera);
	debug("libvisca_thread::thread_connect exiting successfully");
}

void *libvisca_thread::thread_main(void *data)
{
	auto *visca = (libvisca_thread*)data;
	visca->add_ref();
	visca->thread_loop();
	visca->release();
	return NULL;
}

static inline void send_pantilt(struct _VISCA_interface *iface, struct _VISCA_camera *camera, int pan, int tilt)
{
	debug("send_pantilt moving pan=%d tilt=%d", pan, tilt);
	int pan_a = abs(pan);
	int tilt_a = abs(tilt);
	if (pan_a>127) pan_a = 127;
	if (tilt_a>127) tilt_a = 127;
	if      (tilt<0 && pan<0) // 1=up, 1=left
		VISCA_set_pantilt_upleft(iface, camera, pan_a, tilt_a);
	else if (tilt<0 && pan==0) // 1=up
		VISCA_set_pantilt_up(iface, camera, pan_a, tilt_a);
	else if (tilt<0 && pan>0) // 1=up, 2=right
		VISCA_set_pantilt_upright(iface, camera, pan_a, tilt_a);
	else if (tilt==0 && pan<0) // 1=left
		VISCA_set_pantilt_left(iface, camera, pan_a, tilt_a);
	else if (tilt==0 && pan==0)
		VISCA_set_pantilt_stop(iface, camera, pan_a, tilt_a);
	else if (tilt==0 && pan>0) // 2=right
		VISCA_set_pantilt_right(iface, camera, pan_a, tilt_a);
	else if (tilt>0 && pan<0) // 2=down, 1=left
		VISCA_set_pantilt_downleft(iface, camera, pan_a, tilt_a);
	else if (tilt>0 && pan==0) // 2=down
		VISCA_set_pantilt_down(iface, camera, pan_a, tilt_a);
	else if (tilt>0 && pan>0)
		VISCA_set_pantilt_downright(iface, camera, pan_a, tilt_a);
}

static inline void send_zoom(struct _VISCA_interface *iface, struct _VISCA_camera *camera, int zoom)
{
	debug("send_zoom moving zoom=%d", zoom);
	int zoom_a = std::abs(zoom);
	if (zoom_a > 7) zoom_a = 7;
	// zoom>0 : wide
	if (zoom>0)
		VISCA_set_zoom_wide_speed(iface, camera, zoom_a);
	else if (zoom<0)
		VISCA_set_zoom_tele_speed(iface, camera, zoom_a);
	else
		VISCA_set_zoom_stop(iface, camera);
}

void libvisca_thread::thread_loop()
{
	int pan_prev=INT_MIN, tilt_prev=INT_MIN, zoom_prev=INT_MIN;

	while (os_atomic_load_long(&ref) > 1) {
		if (data_changed) {
			thread_connect();
			pan_prev=INT_MIN;
			tilt_prev=INT_MIN;
			zoom_prev=INT_MIN;
		}
		if (!iface) {
			os_sleep_ms(50);
			continue;
		}
		int pan = os_atomic_load_long(&pan_rsvd);
		int tilt = os_atomic_load_long(&tilt_rsvd);
		int zoom = os_atomic_load_long(&zoom_rsvd);
		bool ptz_changed = false;
		if (pan!=pan_prev || tilt!=tilt_prev) {
			send_pantilt(iface, camera, pan, tilt);
			pan_prev = pan;
			tilt_prev = tilt;
			ptz_changed = true;
		}
		if (zoom!=zoom_prev) {
			send_zoom(iface, camera, zoom);
			zoom_prev = zoom;
			ptz_changed = true;
		}

		uint16_t zoom_cur = 0;
		if (VISCA_get_zoom_value(iface, camera, &zoom_cur) == VISCA_SUCCESS) {
			os_atomic_set_long(&zoom_got, (long)zoom_cur);
			debug("libvisca_thread::thread_loop got zoom=%d", (int)zoom_cur);
		}

		if (!ptz_changed)
			os_sleep_ms(50);
	}
}

void libvisca_thread::set_config(struct obs_data *data_)
{
	pthread_mutex_lock(&mutex);

	obs_data_addref(data_);
	if (data) {
		obs_data_release(data);
		const char *address_old = obs_data_get_string(data, "address");
		int port_old = (int)obs_data_get_int(data, "port");
		const char *address_new = obs_data_get_string(data_, "address");
		int port_new = (int)obs_data_get_int(data_, "port");
		if (strcmp(address_old, address_new))
			data_changed = true;
		if (port_old != port_new)
			data_changed = true;
	}
	else
		data_changed = true;
	data = data_;


	pthread_mutex_unlock(&mutex);
}
