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

void texture_object::set_texture(uint8_t *data_, uint32_t linesize, uint32_t width, uint32_t height)
{
	// TODO: monochromed by GPU
	data->dlib_img.set_size(height, width);
	for (int i=0; i<height; i++) {
		auto row = data->dlib_img[i];
		uint8_t *line = data_+i*linesize;
		for (int j=0; j<width; j++) {
			// TODO: row[j] = line[j];
			int r = line[j*4+0];
			int g = line[j*4+1];
			int b = line[j*4+2];
			row[j] = (+306*r +601*g +117*b)/1024; // BT.601 // TODO
		}
	}
}

const dlib::array2d<unsigned char> &texture_object::get_dlib_img()
{
	return data->dlib_img;
}
