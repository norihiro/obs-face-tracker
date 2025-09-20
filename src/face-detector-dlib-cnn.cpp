#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <string>
#include "plugin-macros.generated.h"
#include "face-detector-dlib-cnn.h"
#include "texture-object.h"

#include <dlib/dnn.h>
#include <dlib/data_io.h>
#include <dlib/image_processing.h>
#include <dlib/array2d/array2d_kernel.h>

#define MAX_ERROR 2

using namespace dlib;
template<long num_filters, typename SUBNET> using con5d = con<num_filters, 5, 5, 2, 2, SUBNET>;
template<long num_filters, typename SUBNET> using con5 = con<num_filters, 5, 5, 1, 1, SUBNET>;
template<typename SUBNET>
using downsampler = relu<affine<con5d<32, relu<affine<con5d<32, relu<affine<con5d<16, SUBNET>>>>>>>>>;
template<typename SUBNET> using rcon5 = relu<affine<con5<45, SUBNET>>>;
using net_type =
	loss_mmod<con<1, 9, 9, 1, 1, rcon5<rcon5<rcon5<downsampler<input_rgb_image_pyramid<pyramid_down<6>>>>>>>>;
typedef dlib::matrix<dlib::rgb_pixel> image_t;

struct private_s
{
	std::shared_ptr<texture_object> tex;
	std::vector<rect_s> rects;
	std::string model_filename;
	net_type net;
	bool net_loaded = false;
	bool has_error = false;
	int crop_l = 0, crop_r = 0, crop_t = 0, crop_b = 0;
	int n_error = 0;
};

face_detector_dlib_cnn::face_detector_dlib_cnn()
{
	p = new private_s;
}

face_detector_dlib_cnn::~face_detector_dlib_cnn()
{
	delete p;
}

void face_detector_dlib_cnn::set_texture(std::shared_ptr<texture_object> &tex, int crop_l, int crop_r, int crop_t,
					 int crop_b)
{
	p->tex = tex;
	p->crop_l = crop_l;
	p->crop_r = crop_r;
	p->crop_t = crop_t;
	p->crop_b = crop_b;
}

void face_detector_dlib_cnn::detect_main()
{
	if (!p->tex)
		return;

	dlib::matrix<dlib::rgb_pixel> img;
	if (!p->tex->get_dlib_rgb_image(img))
		return;

	int x0 = 0, y0 = 0;
	if (p->crop_l > 0 || p->crop_r > 0 || p->crop_t > 0 || p->crop_b > 0) {
		image_t img_crop;
		x0 = (int)(p->crop_l / p->tex->scale);
		int x1 = img.nc() - (int)(p->crop_r / p->tex->scale);
		y0 = (int)(p->crop_t / p->tex->scale);
		int y1 = img.nr() - (int)(p->crop_b / p->tex->scale);
		if (x1 - x0 < 80 || y1 - y0 < 80) {
			if (p->n_error++ < MAX_ERROR)
				blog(LOG_ERROR, "too small image: %dx%d cropped left=%d right=%d top=%d bottom=%d",
				     (int)img.nc(), (int)img.nr(), p->crop_l, p->crop_r, p->crop_t, p->crop_b);
			return;
		} else if (p->n_error) {
			p->n_error--;
		}
		img_crop.set_size(y1 - y0, x1 - x0);
		for (int y = y0; y < y1; y++) {
			for (int x = x0; x < x1; x++) {
				img_crop(y - y0, x - x0) = img(y, x);
			}
		}
		img = img_crop;
	}
	if (img.nc() < 80 || img.nr() < 80) {
		if (p->n_error++ < MAX_ERROR)
			blog(LOG_ERROR, "too small image: %dx%d", (int)img.nc(), (int)img.nr());
		return;
	} else if (p->n_error) {
		p->n_error--;
	}

	if (!p->net_loaded) {
		p->net_loaded = true;
		try {
			blog(LOG_INFO, "loading file '%s'", p->model_filename.c_str());
			deserialize(p->model_filename.c_str()) >> p->net;
			p->has_error = false;
		} catch (...) {
			blog(LOG_ERROR, "failed to load file '%s'", p->model_filename.c_str());
			p->has_error = true;
		}
	}

	if (p->has_error)
		return;

	auto dets = p->net(img);
	p->rects.resize(dets.size());
	for (size_t i = 0; i < dets.size(); i++) {
		auto &det = dets[i];
		rect_s &r = p->rects[i];
		r.x0 = (det.rect.left() + x0) * p->tex->scale;
		r.y0 = (det.rect.top() + y0) * p->tex->scale;
		r.x1 = (det.rect.right() + x0) * p->tex->scale;
		r.y1 = (det.rect.bottom() + y0) * p->tex->scale;
		r.score = det.detection_confidence;
	}

	p->tex.reset();
}

void face_detector_dlib_cnn::get_faces(std::vector<struct rect_s> &rects)
{
	rects = p->rects;
}

void face_detector_dlib_cnn::set_model(const char *filename)
{
	if (p->model_filename != filename) {
		p->model_filename = filename;
		p->net_loaded = false;
	}
}
