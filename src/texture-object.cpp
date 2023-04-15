#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <util/bmem.h>
#include <dlib/array2d/array2d_kernel.h>
#include "plugin-macros.generated.h"
#include "texture-object.h"

static uint32_t formats_found = 0;
#define TEST_FORMAT(f) (0<=(uint32_t)(f) && (uint32_t)(f)<32 && !(formats_found&(1<<(uint32_t)(f))))
#define SET_FORMAT(f) (0<=(uint32_t)(f) && (uint32_t)(f)<32 && (formats_found|=(1<<(uint32_t)(f))))

struct texture_object_private_s
{
	dlib::matrix<dlib::rgb_pixel> dlib_rgb_image;
	void *leak_test;
};

texture_object::texture_object()
{
	data = new texture_object_private_s;
	data->leak_test = bmalloc(1);
}

texture_object::~texture_object()
{
	bfree(data->leak_test);
	delete data;
}

static void obsframe2dlib_bgrx(dlib::matrix<dlib::rgb_pixel> &img, const struct obs_source_frame *frame, int scale, int size=4)
{
	const int nr = img.nr();
	const int nc = img.nc();
	const int inc = size * scale;
	for (int i=0; i<nr; i++) {
		uint8_t *line = frame->data[0] + frame->linesize[0] * scale * i;
		for (int j=0, js=0; j<nc; j++, js+=inc) {
			img(i,j).red = line[js+2];
			img(i,j).green = line[js+1];
			img(i,j).blue = line[js+0];
		}
	}
}

static void obsframe2dlib_rgbx(dlib::matrix<dlib::rgb_pixel> &img, const struct obs_source_frame *frame, int scale)
{
	const int nr = img.nr();
	const int nc = img.nc();
	for (int i=0; i<nr; i++) {
		uint8_t *line = frame->data[0] + frame->linesize[0] * scale * i;
		for (int j=0, js=0; j<nc; j++, js+=4*scale) {
			img(i,j).red = line[js+0];
			img(i,j).green = line[js+1];
			img(i,j).blue = line[js+2];
		}
	}
}

void texture_object::set_texture_obsframe_scale(const struct obs_source_frame *frame, int scale)
{
	if (TEST_FORMAT(frame->format))
		blog(LOG_INFO, "received frame format=%d", frame->format);
	data->dlib_rgb_image.set_size(frame->height/scale, frame->width/scale);
	switch(frame->format) {
		case VIDEO_FORMAT_BGRX:
		case VIDEO_FORMAT_BGRA:
			obsframe2dlib_bgrx(data->dlib_rgb_image, frame, scale);
			break;
		case VIDEO_FORMAT_BGR3:
			obsframe2dlib_bgrx(data->dlib_rgb_image, frame, scale, 3);
			break;
		case VIDEO_FORMAT_RGBA:
			obsframe2dlib_rgbx(data->dlib_rgb_image, frame, scale);
			break;
		default:
			if (TEST_FORMAT(frame->format))
				blog(LOG_ERROR, "Frame format %d has to be RGB", (int)frame->format);
	}
	SET_FORMAT(frame->format);
}

const dlib::matrix<dlib::rgb_pixel> &texture_object::get_dlib_rgb_image()
{
	return data->dlib_rgb_image;
}
