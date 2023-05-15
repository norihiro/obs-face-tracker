#pragma once
#include <obs-module.h>
#include <util/threading.h>
#include <vector>
#include <dlib/array2d/array2d_kernel.h>
#include "plugin-macros.generated.h"

class texture_object
{
	struct texture_object_private_s *data;
public:
	texture_object();
	~texture_object();

	void set_texture_obsframe(const struct obs_source_frame *frame, int scale);
	bool get_dlib_rgb_image(dlib::matrix<dlib::rgb_pixel> &img) const;

public:
	int tick;
	float scale;
};
