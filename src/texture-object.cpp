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
	dlib::array2d<unsigned char> dlib_img;
	void *leak_test;
};

texture_object::texture_object()
{
	ref = 1;
	data = new texture_object_private_s;
	data->leak_test = bmalloc(1);
}

texture_object::~texture_object()
{
	bfree(data->leak_test);
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

static void obsframe2dlib_bgrx(dlib::array2d<unsigned char> &img, const struct obs_source_frame *frame, int scale)
{
	const int nr = img.nr();
	const int nc = img.nc();
	for (int i=0; i<nr; i++) {
		auto row = img[i];
		uint8_t *line = frame->data[0] + frame->linesize[0] * scale * i;
		for (int j=0, js=0; j<nc; j++, js+=4*scale) {
			int r = line[js+2];
			int g = line[js+1];
			int b = line[js+0];
			row[j] = (+306*r +601*g +117*b)/1024; // BT.601
		}
	}
}

static void obsframe2dlib_rgbx(dlib::array2d<unsigned char> &img, const struct obs_source_frame *frame, int scale)
{
	const int nr = img.nr();
	const int nc = img.nc();
	for (int i=0; i<nr; i++) {
		auto row = img[i];
		uint8_t *line = frame->data[0] + frame->linesize[0] * scale * i;
		for (int j=0, js=0; j<nc; j++, js+=4*scale) {
			int r = line[js+0];
			int g = line[js+1];
			int b = line[js+2];
			row[j] = (+306*r +601*g +117*b)/1024; // BT.601
		}
	}
}

static void obsframe2dlib_packed_y2(dlib::array2d<unsigned char> &img, const struct obs_source_frame *frame, int scale, int offset)
{
	const int nr = img.nr();
	const int nc = img.nc();
	for (int i=0; i<nr; i++) {
		auto row = img[i];
		uint8_t *line = frame->data[0] + frame->linesize[0] * scale * i + offset;
		for (int j=0, js=0; j<nc; j++, js+=2*scale) {
			row[j] = line[js];
		}
	}
}

static void obsframe2dlib_y(dlib::array2d<unsigned char> &img, const struct obs_source_frame *frame, int scale)
{
	const int nr = img.nr();
	const int nc = img.nc();
	for (int i=0; i<nr; i++) {
		auto row = img[i];
		uint8_t *line = frame->data[0] + frame->linesize[0] * scale * i;
		for (int j=0, js=0; j<nc; j++, js+=scale) {
			row[j] = line[js];
		}
	}
}

void texture_object::set_texture_obsframe_scale(const struct obs_source_frame *frame, int scale)
{
	if (TEST_FORMAT(frame->format))
		blog(LOG_INFO, "received frame format=%d", frame->format);
	data->dlib_img.set_size(frame->height/scale, frame->width/scale);
	switch(frame->format) {
		case VIDEO_FORMAT_BGRX:
		case VIDEO_FORMAT_BGRA:
			obsframe2dlib_bgrx(data->dlib_img, frame, scale);
			break;
		case VIDEO_FORMAT_RGBA:
			obsframe2dlib_rgbx(data->dlib_img, frame, scale);
			break;
		case VIDEO_FORMAT_YVYU:
		case VIDEO_FORMAT_YUY2:
			obsframe2dlib_packed_y2(data->dlib_img, frame, scale, 0);
			break;
		case VIDEO_FORMAT_UYVY:
			obsframe2dlib_packed_y2(data->dlib_img, frame, scale, 1);
			break;
		case VIDEO_FORMAT_I420:
		case VIDEO_FORMAT_I422:
		case VIDEO_FORMAT_I444:
		case VIDEO_FORMAT_I40A:
		case VIDEO_FORMAT_I42A:
		case VIDEO_FORMAT_YUVA:
		case VIDEO_FORMAT_Y800:
		case VIDEO_FORMAT_NV12:
			obsframe2dlib_y(data->dlib_img, frame, scale);
			break;
		default:
			if (TEST_FORMAT(frame->format))
				blog(LOG_ERROR, "unsupported frame format %d", frame->format);
	}
	SET_FORMAT(frame->format);
}

const dlib::array2d<unsigned char> &texture_object::get_dlib_img()
{
	return data->dlib_img;
}
