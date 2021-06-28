#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include "plugin-macros.generated.h"
#include "texture-object.h"
#include "face-tracker-dlib.h"

#include <dlib/image_processing/scan_fhog_pyramid.h>
#include <dlib/image_processing/correlation_tracker.h>

struct face_tracker_dlib_private_s
{
	texture_object *tex;
	rect_s rect;
	dlib::correlation_tracker *tracker;
	float score0;
	float pslr_max, pslr_min;
	bool need_restart;
	uint64_t last_ns;
	float scale_orig;
	int n_track;
	face_tracker_dlib_private_s()
	{
		tracker = NULL;
		need_restart = false;
		tex = NULL;
		rect.score = 0.0f;
	}
};

face_tracker_dlib::face_tracker_dlib()
{
	p = new face_tracker_dlib_private_s;
}

face_tracker_dlib::~face_tracker_dlib()
{
	if (p->tracker) delete p->tracker;
	delete p;
}

void face_tracker_dlib::set_texture(class texture_object *tex)
{
	if (p->tex) p->tex->release();
	tex->addref();
	p->tex = tex;
	p->n_track = 0;
}

void face_tracker_dlib::set_position(const rect_s &rect)
{
	p->rect.x0 = rect.x0 / p->tex->scale;
	p->rect.y0 = rect.y0 / p->tex->scale;
	p->rect.x1 = rect.x1 / p->tex->scale;
	p->rect.y1 = rect.y1 / p->tex->scale;
	p->rect.score = 1.0f;
	p->need_restart = true;
	p->n_track = 0;
}

void face_tracker_dlib::track_main()
{
	if (!p->tex)
		return;

	uint64_t ns = os_gettime_ns();
	if (p->need_restart) {
		if (!p->tracker)
			p->tracker = new dlib::correlation_tracker();

		dlib::rectangle r (p->rect.x0, p->rect.y0, p->rect.x1, p->rect.y1);
		p->tracker->start_track(p->tex->get_dlib_img(), r);
		p->score0 = p->rect.score;
		p->need_restart = false;
		p->pslr_max = 0.0f;
		p->pslr_min = 1e9f;
		p->scale_orig = p->tex->scale;
	}
	else if (p->tex->scale != p->scale_orig) {
		p->rect.score = 0.0f;
	}
	else {
		float s = p->tracker->update(p->tex->get_dlib_img());
		if (s>p->pslr_max) p->pslr_max = s;
		if (s<p->pslr_min) p->pslr_min = s;
		dlib::rectangle r = p->tracker->get_position();
		p->rect.x0 = r.left() * p->tex->scale;
		p->rect.y0 = r.top() * p->tex->scale;
		p->rect.x1 = r.right() * p->tex->scale;
		p->rect.y1 = r.bottom() * p->tex->scale;
		s = p->pslr_max / p->pslr_min * ((ns-p->last_ns)*1e-9f);
		p->rect.score = (p->rect.score /*+ 0.0f*s */) / (1.0f + s);
		p->n_track += 1;
	}
	p->last_ns = ns;

	if (p->tex) p->tex->release();
	p->tex = NULL;
}

bool face_tracker_dlib::get_face(struct rect_s &rect)
{
	if (p->n_track>0) {
		rect = p->rect;
		return true;
	}
	else
		return false;
}
