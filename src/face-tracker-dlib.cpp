#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include "plugin-macros.generated.h"
#include "face-tracker-dlib.h"

#include <dlib/image_processing/scan_fhog_pyramid.h>
#include <dlib/image_processing/correlation_tracker.h>

#define SCALE 2

struct face_tracker_dlib_private_s
{
	dlib::array2d<unsigned char> img;
	rect_s rect;
	dlib::correlation_tracker *tracker;
	float score0;
	float pslr_max, pslr_min;
	bool need_restart;
	uint64_t last_ns;
	face_tracker_dlib_private_s()
	{
		tracker = NULL;
		need_restart = false;
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

void face_tracker_dlib::set_texture(uint8_t *data, uint32_t linesize, uint32_t width, uint32_t height)
{
	p->img.set_size(height/SCALE, width/SCALE);
	for (int i=0; i<height/SCALE; i++) {
		auto row = p->img[i];
		uint8_t *line = data+(i*SCALE)*linesize;
		for (int j=0; j<width/SCALE; j++) {
			int r = line[j*SCALE*4+0];
			int g = line[j*SCALE*4+1];
			int b = line[j*SCALE*4+2];
			row[j] = (+306*r +601*g +117*b)/1024; // BT.601
		}
	}
	blog(LOG_INFO, "face_tracker_dlib: got image %dx%d", width, height);
}

void face_tracker_dlib::set_position(const rect_s &rect)
{
	p->rect = rect;
	p->need_restart = true;
}

void face_tracker_dlib::track_main()
{
	uint64_t ns = os_gettime_ns();
	if (p->need_restart) {
		if (!p->tracker)
			p->tracker = new dlib::correlation_tracker();

		dlib::rectangle r (p->rect.x0/SCALE, p->rect.y0/SCALE, p->rect.x1/SCALE, p->rect.y1/SCALE);
		p->tracker->start_track(p->img, r);
		p->score0 = p->rect.score;
		p->need_restart = false;
		blog(LOG_INFO, "face_tracker_dlib::track_main: %p starting correlation_tracker score0=%f", this, p->score0);
		p->pslr_max = 0.0f;
		p->pslr_min = 1e9f;
	}
	else {
		float s = p->tracker->update(p->img);
		if (s>p->pslr_max) p->pslr_max = s;
		if (s<p->pslr_min) p->pslr_min = s;
		dlib::rectangle r = p->tracker->get_position();
		p->rect.x0 = r.left() * SCALE;
		p->rect.y0 = r.top() * SCALE;
		p->rect.x1 = r.right() * SCALE;
		p->rect.y1 = r.bottom() * SCALE;
		s = p->pslr_max / p->pslr_min * ((ns-p->last_ns)*1e-9f);
		p->rect.score = (p->rect.score /*+ 0.0f*s */) / (1.0f + s);
	}
	p->last_ns = ns;
}

void face_tracker_dlib::get_face(struct rect_s &rect)
{
	rect = p->rect;
}
