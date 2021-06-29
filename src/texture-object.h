#pragma once
#include <obs-module.h>
#include <util/threading.h>
#include <vector>
#include <dlib/array2d/array2d_kernel.h>
#include "plugin-macros.generated.h"

class texture_object
{
	volatile long ref;
	struct texture_object_private_s *data;
public:
	texture_object();
	~texture_object();
	void addref() { os_atomic_inc_long(&ref); }
	void release() { if (os_atomic_dec_long(&ref)<=0) delete this; }

	void set_texture_y(uint8_t *data, uint32_t linesize, uint32_t width, uint32_t height);
	void set_texture_obsframe_scale(const struct obs_source_frame *frame, int scale);
	const dlib::array2d<unsigned char> &get_dlib_img();

public:
	int tick;
	float scale;
};
