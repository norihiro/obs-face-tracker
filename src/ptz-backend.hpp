#pragma once

#include <util/threading.h>

class ptz_backend
{
	volatile long ref;

public:
	ptz_backend();
	virtual ~ptz_backend();
	void add_ref() { os_atomic_inc_long(&ref); }
	void release();
	inline long get_ref() { return os_atomic_load_long(&ref); }

	virtual void set_config(struct obs_data *data) = 0;

	virtual bool can_send() { return true; }
	virtual void tick() {}
	virtual void set_pantiltzoom_speed(int pan, int tilt, int zoom) = 0;
	virtual int get_zoom() = 0;
};
