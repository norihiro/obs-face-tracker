#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include "plugin-macros.generated.h"
#include "face-detector-dlib-hog.h"
#include "texture-object.h"

#include <dlib/image_processing/frontal_face_detector.h>

#define MAX_ERROR 2

struct face_detector_dlib_private_s
{
	std::shared_ptr<texture_object> tex;
	std::vector<rect_s> rects;
	dlib::frontal_face_detector *detector;
	int crop_l = 0, crop_r = 0, crop_t = 0, crop_b = 0;
	int n_error = 0;
	face_detector_dlib_private_s()
	{
		detector = NULL;
	}
	~face_detector_dlib_private_s()
	{
		if (detector)
			delete detector;
	}
};

face_detector_dlib_hog::face_detector_dlib_hog()
{
	p = new face_detector_dlib_private_s;
}

face_detector_dlib_hog::~face_detector_dlib_hog()
{
	delete p;
}

void face_detector_dlib_hog::set_texture(std::shared_ptr<texture_object> &tex, int crop_l, int crop_r, int crop_t, int crop_b)
{
	p->tex = tex;
	p->crop_l = crop_l;
	p->crop_r = crop_r;
	p->crop_t = crop_t;
	p->crop_b = crop_b;
}

void face_detector_dlib_hog::detect_main()
{
	if (!p->tex)
		return;
	const auto *img = &p->tex->get_dlib_rgb_image();
	int x0 = 0, y0 = 0;
	dlib::array2d<dlib::rgb_pixel> img_crop;
	if (p->crop_l > 0 || p->crop_r > 0 || p->crop_t > 0 || p->crop_b > 0) {
		x0 = (int)(p->crop_l / p->tex->scale);
		int x1 = img->nc() - (int)(p->crop_r / p->tex->scale);
		y0 = (int)(p->crop_t / p->tex->scale);
		int y1 = img->nr() - (int)(p->crop_b / p->tex->scale);
		if (x1 - x0 < 80 || y1 - y0 < 80) {
			if (p->n_error++ < MAX_ERROR)
				blog(LOG_ERROR, "too small image: %dx%d cropped left=%d right=%d top=%d bottom=%d",
						(int)img->nc(), (int)img->nr(),
						p->crop_l, p->crop_r, p->crop_t, p->crop_b );
			return;
		}
		else if (p->n_error) {
			p->n_error--;
		}
		img_crop.set_size(y1 - y0, x1 - x0);
		for (int y = y0; y < y1; y++) {
			for (int x = x0; x < x1; x++) {
				img_crop[y-y0][x-x0] = (*img)[y][x];
			}
		}
		img = &img_crop;
	}
	if (img->nc()<80 || img->nr()<80) {
		if (p->n_error++ < MAX_ERROR)
			blog(LOG_ERROR, "too small image: %dx%d", (int)img->nc(), (int)img->nr());
		return;
	}
	else if (p->n_error) {
		p->n_error--;
	}

	if (!p->detector)
		p->detector = new dlib::frontal_face_detector(dlib::get_frontal_face_detector());

	std::vector<dlib::rectangle> dets = (*p->detector)(*img);
	p->rects.resize(dets.size());
	for (size_t i=0; i<dets.size(); i++) {
		rect_s &r = p->rects[i];
		r.x0 = (dets[i].left() + x0) * p->tex->scale;
		r.y0 = (dets[i].top() + y0) * p->tex->scale;
		r.x1 = (dets[i].right() + x0) * p->tex->scale;
		r.y1 = (dets[i].bottom() + y0) * p->tex->scale;
		r.score = 1.0; // TODO: implement me
	}

	p->tex.reset();
}

void face_detector_dlib_hog::get_faces(std::vector<struct rect_s> &rects)
{
	rects = p->rects;
}
