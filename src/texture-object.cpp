#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <dlib/array2d/array2d_kernel.h>
#include "plugin-macros.generated.h"
#include "texture-object.h"

struct texture_object_private_s
{
	dlib::array2d<unsigned char> dlib_img;
};

texture_object::texture_object()
{
	ref = 1;
	data = new texture_object_private_s;
}

texture_object::~texture_object()
{
	delete data;
}

void texture_object::set_texture_y(uint8_t *data_, uint32_t linesize, uint32_t width, uint32_t height)
{
	data->dlib_img.set_size(height, width);
	for (uint32_t i=0; i<height; i++) {
		auto row = data->dlib_img[i];
		uint8_t *line = data_+i*linesize;
		for (uint32_t j=0; j<width; j++)
			row[j] = line[j];
	}
}

const dlib::array2d<unsigned char> &texture_object::get_dlib_img()
{
	return data->dlib_img;
}
