#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include "plugin-macros.generated.h"
#include "texture-object.h"
#include "face-tracker-dlib.h"

#include <dlib/image_processing/scan_fhog_pyramid.h>
#include <dlib/image_processing/correlation_tracker.h>
#include <dlib/image_processing.h>

struct face_tracker_dlib_private_s
{
	texture_object *tex;
	rect_s rect;
	dlib::correlation_tracker *tracker;
	int tracker_nc, tracker_nr;
	dlib::shape_predictor sp;
	dlib::full_object_detection shape;
	float last_scale;
	float score0;
	float pslr_max, pslr_min;
	bool need_restart;
	uint64_t last_ns;
	float scale_orig;
	int n_track;
	rectf_s upsize;
	char *landmark_detection_data;
	bool landmark_detection_data_updated;

	face_tracker_dlib_private_s()
	{
		tracker = NULL;
		need_restart = false;
		tex = NULL;
		rect.score = 0.0f;
		landmark_detection_data = NULL;
		landmark_detection_data_updated = false;
	}
};

face_tracker_dlib::face_tracker_dlib()
{
	p = new face_tracker_dlib_private_s;
}

face_tracker_dlib::~face_tracker_dlib()
{
	bfree(p->landmark_detection_data);
	if (p->tracker) delete p->tracker;
	if (p->tex) p->tex->release();
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

void face_tracker_dlib::set_upsize_info(const rectf_s &upsize)
{
	p->upsize = upsize;
}

void face_tracker_dlib::set_landmark_detection(const char *data_file_path)
{
	if (p->landmark_detection_data && data_file_path && strcmp(p->landmark_detection_data, data_file_path) == 0)
		return;

	bfree(p->landmark_detection_data);
	p->landmark_detection_data = NULL;
	if (data_file_path) {
		p->landmark_detection_data = bstrdup(data_file_path);
		p->landmark_detection_data_updated = true;
	}
}

template <typename Tx, typename Ta>
inline Tx internal_division(Tx x0, Tx x1, Ta a0, Ta a1)
{
	return (x0 * a1 + x1 * a0) / (a0 + a1);
}

void face_tracker_dlib::track_main()
{
	if (!p->tex)
		return;

	uint64_t ns = os_gettime_ns();
	if (p->need_restart) {
		if (!p->tracker)
			p->tracker = new dlib::correlation_tracker();

		auto &img = p->tex->get_dlib_img();
		dlib::rectangle r (p->rect.x0, p->rect.y0, p->rect.x1, p->rect.y1);
		p->tracker->start_track(img, r);
		p->tracker_nc = img.nc();
		p->tracker_nr = img.nr();
		p->score0 = p->rect.score;
		p->need_restart = false;
		p->pslr_max = 0.0f;
		p->pslr_min = 1e9f;
		p->scale_orig = p->tex->scale;
		p->shape = dlib::full_object_detection();
	}
	else if (p->tex->scale != p->scale_orig) {
		p->rect.score = 0.0f;
	}
	else {
		auto &img = p->tex->get_dlib_img();
		if (img.nc() != p->tracker_nc || img.nr() != p->tracker_nr) {
			blog(LOG_ERROR, "face_tracker_dlib::track_main: cannot run correlation-tracker with different image size %dx%d, expected %dx%d",
					img.nc(), img.nr(),
					p->tracker_nc, p->tracker_nr );
			p->rect.score = 0;
			p->n_track += 1; // to return score=0
			return;
		}

		float s = p->tracker->update(img);
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

		if (p->landmark_detection_data) {
			if (p->landmark_detection_data_updated) {
				p->landmark_detection_data_updated = false;
				blog(LOG_INFO, "loading file %s", p->landmark_detection_data);
				try {
					dlib::deserialize(p->landmark_detection_data) >> p->sp;
				} catch (...) {
					blog(LOG_ERROR, "Failed to load file %s", p->landmark_detection_data);
				}
			}

			dlib::rectangle r_face (
					internal_division(r.left(), r.right(), p->upsize.x0, p->upsize.x1 + 1.0f),
					internal_division(r.top(), r.bottom(), p->upsize.y0, p->upsize.y1 + 1.0f),
					internal_division(r.left(), r.right(), p->upsize.x0 + 1.0f, p->upsize.x1),
					internal_division(r.top(), r.bottom(), p->upsize.y0 + 1.0f, p->upsize.y1) );

			p->shape = p->sp(p->tex->get_dlib_img(), r_face);
			p->last_scale = p->tex->scale;
		}
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

bool face_tracker_dlib::get_landmark(std::vector<pointf_s> &results)
{
	if (p->shape.num_parts() > 0) {
		const auto &shape = p->shape;
		results.resize(shape.num_parts());

		for (unsigned long i=0; i<shape.num_parts(); i++) {
			const dlib::point pnt =shape.part(i);
			results[i].x = (float)pnt.x() * p->last_scale;
			results[i].y = (float)pnt.y() * p->last_scale;
		}

		return true;
	}
	else
		return false;
}
