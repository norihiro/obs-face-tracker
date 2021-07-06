#include <obs-module.h>
#include "plugin-macros.generated.h"
#include "face-tracker-manager.hpp"
#include "face-detector-dlib.h"
#include "face-tracker-dlib.h"
#include "texture-object.h"
#include "helper.hpp"

// #define debug_track(fmt, ...) blog(LOG_INFO, fmt, __VA_ARGS__)
// #define debug_detect(fmt, ...) blog(LOG_INFO, fmt, __VA_ARGS__)
#define debug_track(fmt, ...)
#define debug_detect(fmt, ...)

face_tracker_manager::face_tracker_manager()
{
	upsize_l = upsize_r = upsize_t = upsize_b = 0.0f;
	scale = 0.0f;
	crop_cur.x0 = crop_cur.x1 = crop_cur.y0 = crop_cur.y1 = 0.0f;
	tick_cnt = detect_tick = next_tick_stage_to_detector = 0;
	detector_in_progress = false;
	detect = new face_detector_dlib();
	detect->start();
}

face_tracker_manager::~face_tracker_manager()
{
	detect->stop();
	delete detect;
}

inline void face_tracker_manager::retire_tracker(int ix)
{
	trackers_idlepool.push_back(trackers[ix]);
	trackers[ix].tracker->request_suspend();
	trackers.erase(trackers.begin()+ix);
}

inline void face_tracker_manager::attenuate_tracker()
{
	for (int j=0; j<detect_rects.size(); j++) {
		rect_s r = detect_rects[j];
		int a0 = (r.x1 - r.x0) * (r.y1 - r.y0);
		int a_overlap_sum = 0;
		for (int i=trackers.size()-1; i>=0; i--) {
			if (trackers[i].state != tracker_inst_s::tracker_state_available)
				continue;
			int a = common_area(r, trackers[i].rect);
			a_overlap_sum += a;
			if (a*10>a0 && a_overlap_sum*2 > a0)
				retire_tracker(i);
		}
	}

	for (int i=0; i<trackers.size(); i++) {
		if (trackers[i].state != tracker_inst_s::tracker_state_available)
			continue;
		struct tracker_inst_s &t = trackers[i];

		int a1 = (t.rect.x1 - t.rect.x0) * (t.rect.y1 - t.rect.y0);
		float amax = (float)a1*0.1f;
		for (int j=0; j<detect_rects.size(); j++) {
			rect_s r = detect_rects[j];
			float a = (float)common_area(r, t.rect);
			if (a > amax) amax = a;
		}

		t.att *= powf(amax / a1, 0.1f); // if no faces, remove the tracker
	}

	float score_max = 1e-17f;
	for (int i=0; i<trackers.size(); i++) {
		if (trackers[i].state == tracker_inst_s::tracker_state_available) {
			float s = trackers[i].att * trackers[i].rect.score;
			if (s > score_max) score_max = s;
		}
	}

	for (int i=0; i<trackers.size(); i++) {
		if (trackers[i].state != tracker_inst_s::tracker_state_available)
			continue;
		if (trackers[i].att * trackers[i].rect.score > 1e-2f * score_max)
			continue;

		retire_tracker(i);
		i--;
	}
}

inline void face_tracker_manager::copy_detector_to_tracker()
{
	int i_tracker;
	for (i_tracker=0; i_tracker < trackers.size(); i_tracker++)
		if (
				trackers[i_tracker].tick_cnt == detect_tick &&
				trackers[i_tracker].state==tracker_inst_s::tracker_state_e::tracker_state_reset_texture )
			break;
	if (i_tracker >= trackers.size())
		return;

	if (detect_rects.size()<=0) {
		trackers.erase(trackers.begin() + i_tracker);
		return;
	}

	struct tracker_inst_s &t = trackers[i_tracker];

	struct rect_s r = detect_rects[0];
	int w = r.x1-r.x0;
	int h = r.y1-r.y0;
	r.x0 -= w * upsize_l;
	r.x1 += w * upsize_r;
	r.y0 -= h * upsize_t;
	r.y1 += h * upsize_b;
	t.tracker->set_position(r); // TODO: consider how to track two or more faces.
	t.tracker->start();
	t.state = tracker_inst_s::tracker_state_constructing;
}

inline void face_tracker_manager::stage_to_detector()
{
	if (detect->trylock())
		return;

	// get previous results
	if (detector_in_progress) {
		detect->get_faces(detect_rects);
		for (int i=0; i<detect_rects.size(); i++)
			debug_detect("stage_to_detector: detect_rects %d %d %d %d %d %f", i,
					detect_rects[i].x0, detect_rects[i].y0, detect_rects[i].x1, detect_rects[i].y1, detect_rects[i].score );
		attenuate_tracker();
		copy_detector_to_tracker();
		detector_in_progress = false;
	}

	if ((next_tick_stage_to_detector - tick_cnt) > 0) {
		detect->unlock();
		return;
	}

	if (class texture_object *cvtex = get_cvtex()) {
		detect->set_texture(cvtex);
		detect->signal();
		detector_in_progress = true;
		detect_tick = tick_cnt;

		struct tracker_inst_s t;
		if (trackers_idlepool.size() > 0) {
			t.tracker = trackers_idlepool[0].tracker;
			trackers_idlepool[0].tracker = NULL;
			trackers_idlepool.pop_front();
		}
		else
			t.tracker = new face_tracker_dlib();
		t.crop_tracker = crop_cur;
		t.state = tracker_inst_s::tracker_state_e::tracker_state_reset_texture;
		t.tick_cnt = tick_cnt;
		t.tracker->set_texture(cvtex);
		trackers.push_back(t);
	}

	detect->unlock();
}

inline int face_tracker_manager::stage_surface_to_tracker(struct tracker_inst_s &t)
{
	if (class texture_object *cvtex = get_cvtex()) {
		t.tracker->set_texture(cvtex);
		t.crop_tracker = crop_cur;
		t.tracker->signal();
	}
	else
		return 1;
	return 0;
}

inline void face_tracker_manager::stage_to_trackers()
{
	for (int i=0; i<trackers.size(); i++) {
		struct tracker_inst_s &t = trackers[i];
		if (t.state == tracker_inst_s::tracker_state_constructing) {
			if (!t.tracker->trylock()) {
				if (!stage_surface_to_tracker(t)) {
					t.crop_tracker = crop_cur;
					t.state = tracker_inst_s::tracker_state_first_track;
				}
				t.tracker->unlock();
				t.state = tracker_inst_s::tracker_state_first_track;
			}
		}
		else if (t.state == tracker_inst_s::tracker_state_first_track) {
			if (!t.tracker->trylock()) {
				bool ret = t.tracker->get_face(t.rect);
				t.crop_rect = t.crop_tracker;
				debug_track("tracker_state_first_track %p %d %d %d %d %f", t.tracker, t.rect.x0, t.rect.y0, t.rect.x1, t.rect.y1, t.rect.score);
				t.att = 1.0f;
				stage_surface_to_tracker(t);
				t.tracker->signal();
				t.tracker->unlock();
				if (ret)
					t.state = tracker_inst_s::tracker_state_available;
			}
		}
		else if (t.state == tracker_inst_s::tracker_state_available) {
			if (!t.tracker->trylock()) {
				t.tracker->get_face(t.rect);
				t.crop_rect = t.crop_tracker;
				debug_track("tracker_state_available %p %d %d %d %d %f", t.tracker, t.rect.x0, t.rect.y0, t.rect.x1, t.rect.y1, t.rect.score);
				stage_surface_to_tracker(t);
				t.tracker->signal();
				t.tracker->unlock();
			}
		}
	}
}

static inline void make_tracker_rects(face_tracker_manager *ftm)
{
	ftm->tracker_rects.resize(0);
	auto &trackers = ftm->trackers;
	for (int i=0; i<trackers.size(); i++) {
		if (trackers[i].state != face_tracker_manager::tracker_inst_s::tracker_state_available)
			continue;
		face_tracker_manager::tracker_rect_s r;
		r.rect = trackers[i].rect;
		r.rect.score *= trackers[i].att;
		r.crop_rect = trackers[i].crop_rect;
		if (r.rect.score<=0.0f || isnan(r.rect.score))
			continue;
		ftm->tracker_rects.push_back(r);
	}
}

void face_tracker_manager::tick(float second)
{
	if (detect_tick==tick_cnt)
		next_tick_stage_to_detector = tick_cnt + (int)(2.0f/second); // detect for each _ second(s).

	tick_cnt += 1;

	make_tracker_rects(this);
}

void face_tracker_manager::post_render()
{
	stage_to_detector();
	stage_to_trackers();
}

void face_tracker_manager::update(obs_data_t *settings)
{
	upsize_l = obs_data_get_double(settings, "upsize_l");
	upsize_r = obs_data_get_double(settings, "upsize_r");
	upsize_t = obs_data_get_double(settings, "upsize_t");
	upsize_b = obs_data_get_double(settings, "upsize_b");
	scale = obs_data_get_double(settings, "scale");
}

void face_tracker_manager::get_properties(obs_properties_t *pp)
{
	obs_properties_add_float(pp, "upsize_l", obs_module_text("Left"), -0.4, 4.0, 0.2);
	obs_properties_add_float(pp, "upsize_r", obs_module_text("Right"), -0.4, 4.0, 0.2);
	obs_properties_add_float(pp, "upsize_t", obs_module_text("Top"), -0.4, 4.0, 0.2);
	obs_properties_add_float(pp, "upsize_b", obs_module_text("Bottom"), -0.4, 4.0, 0.2);
	obs_properties_add_float(pp, "scale", obs_module_text("Scale image"), 1.0, 16.0, 1.0);
}

void face_tracker_manager::get_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, "upsize_l", 0.2);
	obs_data_set_default_double(settings, "upsize_r", 0.2);
	obs_data_set_default_double(settings, "upsize_t", 0.3);
	obs_data_set_default_double(settings, "upsize_b", 0.1);
	obs_data_set_default_double(settings, "scale", 2.0);
}
