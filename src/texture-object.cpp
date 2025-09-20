#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <util/bmem.h>
#include <dlib/array2d/array2d_kernel.h>
#include "plugin-macros.generated.h"
#include "texture-object.h"

static uint32_t formats_found = 0;
#define TEST_FORMAT(f) (0 <= (uint32_t)(f) && (uint32_t)(f) < 32 && !(formats_found & (1 << (uint32_t)(f))))
#define SET_FORMAT(f) (0 <= (uint32_t)(f) && (uint32_t)(f) < 32 && (formats_found |= (1 << (uint32_t)(f))))

struct texture_object_private_s
{
	struct obs_source_frame *obs_frame = NULL;
	int scale = 0;
};

texture_object::texture_object()
{
	data = new texture_object_private_s;
	data->obs_frame = NULL;
}

texture_object::~texture_object()
{
	obs_source_frame_destroy(data->obs_frame);
	delete data;
}

static void obsframe2dlib_bgrx(dlib::matrix<dlib::rgb_pixel> &img, const struct obs_source_frame *frame, int scale,
			       int size = 4)
{
	const int nr = img.nr();
	const int nc = img.nc();
	const int inc = size * scale;
	for (int i = 0; i < nr; i++) {
		uint8_t *line = frame->data[0] + frame->linesize[0] * scale * i;
		for (int j = 0, js = 0; j < nc; j++, js += inc) {
			img(i, j).red = line[js + 2];
			img(i, j).green = line[js + 1];
			img(i, j).blue = line[js + 0];
		}
	}
}

static void obsframe2dlib_rgbx(dlib::matrix<dlib::rgb_pixel> &img, const struct obs_source_frame *frame, int scale)
{
	const int nr = img.nr();
	const int nc = img.nc();
	for (int i = 0; i < nr; i++) {
		uint8_t *line = frame->data[0] + frame->linesize[0] * scale * i;
		for (int j = 0, js = 0; j < nc; j++, js += 4 * scale) {
			img(i, j).red = line[js + 0];
			img(i, j).green = line[js + 1];
			img(i, j).blue = line[js + 2];
		}
	}
}

static bool need_allocate_frame(const struct obs_source_frame *dst, const struct obs_source_frame *src)
{
	if (!dst)
		return true;

	if (dst->format != src->format)
		return true;

	if (dst->width != src->width || dst->height != src->height)
		return true;

	return false;
}

void texture_object::set_texture_obsframe(const struct obs_source_frame *frame, int scale)
{
	if (need_allocate_frame(data->obs_frame, frame)) {
		obs_source_frame_destroy(data->obs_frame);
		data->obs_frame = obs_source_frame_create(frame->format, frame->width, frame->height);
	}

	obs_source_frame_copy(data->obs_frame, frame);
	data->scale = scale;
}

bool texture_object::get_dlib_rgb_image(dlib::matrix<dlib::rgb_pixel> &img) const
{
	if (!data->obs_frame)
		return false;

	const auto *frame = data->obs_frame;
	const int scale = data->scale;
	if (TEST_FORMAT(frame->format))
		blog(LOG_INFO, "received frame format=%d", frame->format);
	img.set_size(frame->height / scale, frame->width / scale);
	switch (frame->format) {
	case VIDEO_FORMAT_BGRX:
	case VIDEO_FORMAT_BGRA:
		obsframe2dlib_bgrx(img, frame, scale);
		break;
	case VIDEO_FORMAT_BGR3:
		obsframe2dlib_bgrx(img, frame, scale, 3);
		break;
	case VIDEO_FORMAT_RGBA:
		obsframe2dlib_rgbx(img, frame, scale);
		break;
	default:
		if (TEST_FORMAT(frame->format))
			blog(LOG_ERROR, "Frame format %d has to be RGB", (int)frame->format);
	}
	SET_FORMAT(frame->format);

	return true;
}
