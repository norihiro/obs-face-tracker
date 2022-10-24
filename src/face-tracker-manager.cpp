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
#define debug_track_thread(fmt, ...) // blog(LOG_INFO, fmt, __VA_ARGS__)

face_tracker_manager::face_tracker_manager()
{
	upsize_l = upsize_r = upsize_t = upsize_b = 0.0f;
	scale = 0.0f;
	tracking_threshold = 1e-2f;
	landmark_detection_data = NULL;
	crop_cur.x0 = crop_cur.x1 = crop_cur.y0 = crop_cur.y1 = 0.0f;
	tick_cnt = detect_tick = next_tick_stage_to_detector = 0;
	detector_in_progress = false;
	detect = new face_detector_dlib();
	detect->start();
}

face_tracker_manager::~face_tracker_manager()
{
	for (auto &t : trackers_idlepool) {
		if (t.tracker) {
			t.tracker->stop();
			delete t.tracker;
			t.tracker = NULL;
		}
	}
	for (auto &t : trackers) {
		if (t.tracker) {
			t.tracker->stop();
			delete t.tracker;
			t.tracker = NULL;
		}
	}
	detect->stop();
	bfree(landmark_detection_data);
	delete detect;
}

inline void face_tracker_manager::retire_tracker(int ix)
{
	debug_track_thread("%p retire_tracker(%d %p)", this, ix, trackers[ix].tracker);
	trackers_idlepool.push_back(trackers[ix]);
	trackers[ix].tracker->request_suspend();
	trackers.erase(trackers.begin()+ix);
}

inline bool face_tracker_manager::is_low_confident(const tracker_inst_s &t, float th1)
{
	if (t.att * t.rect.score <= th1)
		return true;

	if (t.att * t.rect.score <= tracking_threshold * t.score_first)
		return true;

	return false;
}

void face_tracker_manager::remove_duplicated_tracker()
{
	for (size_t i = 0; i < trackers.size(); i++) {
		if (trackers[i].state != tracker_inst_s::tracker_state_available)
			continue;

		rect_s r = trackers[i].rect;
		int a0 = (r.x1 - r.x0) * (r.y1 - r.y0);
		int a_overlap_sum = 0;
		bool to_remove = false;
		for (size_t j = i + 1; j < trackers.size() && !to_remove; j++) {
			if (trackers[j].state != tracker_inst_s::tracker_state_available)
				continue;
			int a = common_area(r, trackers[j].rect);
			a_overlap_sum += a;
			if (a*10>a0 && a_overlap_sum*2 > a0)
				to_remove = true;
		}

		if (to_remove) {
			retire_tracker(i);
			i--;
		}
	}
}

inline void face_tracker_manager::attenuate_tracker()
{
	for (size_t i = 0; i < trackers.size(); i++) {
		if (trackers[i].state != tracker_inst_s::tracker_state_available)
			continue;
		struct tracker_inst_s &t = trackers[i];

		int a1 = (t.rect.x1 - t.rect.x0) * (t.rect.y1 - t.rect.y0);
		float amax = (float)a1*0.1f;
		for (size_t j = 0; j < detect_rects.size(); j++) {
			rect_s r = detect_rects[j];
			float a = (float)common_area(r, t.rect);
			if (a > amax) amax = a;
		}

		t.att *= powf(amax / a1, 0.1f); // if no faces, remove the tracker
	}

	float score_max = 1e-17f;
	for (size_t i = 0; i < trackers.size(); i++) {
		if (trackers[i].state == tracker_inst_s::tracker_state_available) {
			float s = trackers[i].att * trackers[i].rect.score;
			if (s > score_max) score_max = s;
		}
	}

	for (size_t i = 0; i < trackers.size(); i++) {
		if (trackers[i].state != tracker_inst_s::tracker_state_available)
			continue;
		if (!is_low_confident(trackers[i], 1e-2f * score_max))
			continue;

		retire_tracker(i);
		i--;
	}
}

inline void face_tracker_manager::copy_detector_to_tracker()
{
	size_t i_tracker;
	for (i_tracker=0; i_tracker < trackers.size(); i_tracker++)
		if (
				trackers[i_tracker].tick_cnt == detect_tick &&
				trackers[i_tracker].state==tracker_inst_s::tracker_state_e::tracker_state_reset_texture )
			break;
	if (i_tracker >= trackers.size())
		return;

	if (detect_rects.size()<=0) {
		retire_tracker(i_tracker);
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
	t.tracker->set_upsize_info(rectf_s{upsize_l, upsize_t, upsize_r, upsize_b});
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
		for (size_t i = 0; i < detect_rects.size(); i++)
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
		detect->set_texture(cvtex,
				detector_crop_l, detector_crop_r,
				detector_crop_t, detector_crop_b );
		detect->signal();
		detector_in_progress = true;
		detect_tick = tick_cnt;

		struct tracker_inst_s t;
		if (trackers_idlepool.size() > 0) {
			t.tracker = trackers_idlepool[0].tracker;
			trackers_idlepool[0].tracker = NULL;
			trackers_idlepool.pop_front();
		}
		else {
			debug_track_thread("%p No available idle tracker, creating new tracker thread. There are %d existing thread.", this, trackers.size());
			t.tracker = new face_tracker_dlib();
			for (size_t i = 0; i < trackers.size(); i++) {
				debug_track_thread("%p existing tracker[%d]: state=%d", this, i, (int)trackers[i].state);
			}
		}
		t.crop_tracker = crop_cur;
		t.state = tracker_inst_s::tracker_state_e::tracker_state_reset_texture;
		t.tick_cnt = tick_cnt;
		t.tracker->set_texture(cvtex);
		t.tracker->set_landmark_detection(landmark_detection_data);
		if (!landmark_detection_data)
			t.landmark.clear();
		trackers.push_back(t);

		cvtex->release();
	}

	detect->unlock();
}

inline int face_tracker_manager::stage_surface_to_tracker(struct tracker_inst_s &t)
{
	if (class texture_object *cvtex = get_cvtex()) {
		t.tracker->set_texture(cvtex);
		t.crop_tracker = crop_cur;
		t.tracker->signal();
		cvtex->release();
	}
	else
		return 1;
	return 0;
}

inline void face_tracker_manager::stage_to_trackers()
{
	bool have_new_tracker = false;
	for (size_t i = 0; i < trackers.size(); i++) {
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
				t.score_first = t.rect.score;
				if (!ret || !landmark_detection_data || !t.tracker->get_landmark(t.landmark))
					t.landmark.resize(0);
				stage_surface_to_tracker(t);
				t.tracker->signal();
				t.tracker->unlock();
				if (ret) {
					t.state = tracker_inst_s::tracker_state_available;
					have_new_tracker = true;
				}
			}
		}
		else if (t.state == tracker_inst_s::tracker_state_available) {
			if (!t.tracker->trylock()) {
				bool ret = t.tracker->get_face(t.rect);
				t.crop_rect = t.crop_tracker;
				debug_track("tracker_state_available %p %d %d %d %d %f landmark=%d", t.tracker, t.rect.x0, t.rect.y0, t.rect.x1, t.rect.y1, t.rect.score, t.landmark.size());
				if (!ret || !landmark_detection_data || !t.tracker->get_landmark(t.landmark))
					t.landmark.resize(0);
				stage_surface_to_tracker(t);
				t.tracker->signal();
				t.tracker->unlock();
			}
		}
	}

	if (have_new_tracker)
		remove_duplicated_tracker();
}

static inline void make_tracker_rects(
		std::vector<face_tracker_manager::tracker_rect_s> &tracker_rects,
		const std::deque<face_tracker_manager::tracker_inst_s> &trackers )
{
	size_t n = 0;
	for (size_t i = 0; i < trackers.size(); i++) {
		if (trackers[i].state != face_tracker_manager::tracker_inst_s::tracker_state_available)
			continue;

		float score = trackers[i].rect.score * trackers[i].att;

		if (score<=0.0f || isnan(score))
			continue;

		if (tracker_rects.size() <= n)
			tracker_rects.resize(n+1);
		auto &r = tracker_rects[n++];

		r.rect = trackers[i].rect;
		r.rect.score = score;
		r.crop_rect = trackers[i].crop_rect;
		r.landmark = trackers[i].landmark;
	}

	if (tracker_rects.size() > n)
		tracker_rects.resize(n);
}

void face_tracker_manager::tick(float second)
{
	if (reset_requested) {
		for (int i=trackers.size()-1; i>=0; i--)
			trackers[i].att = 0.0f;
		detect_rects.clear();
		reset_requested = false;
	}

	if (detect_tick==tick_cnt)
		next_tick_stage_to_detector = tick_cnt + (int)(2.0f/second); // detect for each _ second(s).

	tick_cnt += 1;

	make_tracker_rects(tracker_rects, trackers);
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
	detector_crop_l = obs_data_get_int(settings, "detector_crop_l");
	detector_crop_r = obs_data_get_int(settings, "detector_crop_r");
	detector_crop_t = obs_data_get_int(settings, "detector_crop_t");
	detector_crop_b = obs_data_get_int(settings, "detector_crop_b");
	bool landmark_detection = obs_data_get_bool(settings, "landmark_detection");
	bfree(landmark_detection_data);
	landmark_detection_data = NULL;
	if (landmark_detection)
		landmark_detection_data = bstrdup(obs_data_get_string(settings, "landmark_detection_data"));
	if (obs_data_get_bool(settings, "tracking_th_en"))
		tracking_threshold = from_dB(obs_data_get_double(settings, "tracking_th_dB"));
	else
		tracking_threshold = 0.0;
}

static bool tracking_th_en_modified(obs_properties_t *props, obs_property_t *, obs_data_t *settings)
{
	bool tracking_th_en = obs_data_get_bool(settings, "tracking_th_en");
	obs_property_t *tracking_th_dB = obs_properties_get(props, "tracking_th_dB");
	obs_property_set_visible(tracking_th_dB, tracking_th_en);
	return true;
}

void face_tracker_manager::get_properties(obs_properties_t *pp)
{
	obs_property_t *p;
	obs_properties_add_float(pp, "upsize_l", obs_module_text("Left"), -0.4, 4.0, 0.2);
	obs_properties_add_float(pp, "upsize_r", obs_module_text("Right"), -0.4, 4.0, 0.2);
	obs_properties_add_float(pp, "upsize_t", obs_module_text("Top"), -0.4, 4.0, 0.2);
	obs_properties_add_float(pp, "upsize_b", obs_module_text("Bottom"), -0.4, 4.0, 0.2);
	obs_properties_add_float(pp, "scale", obs_module_text("Scale image"), 1.0, 16.0, 1.0);
	obs_properties_add_int(pp, "detector_crop_l", obs_module_text("Crop left for detector"), 0, 1920, 1);
	obs_properties_add_int(pp, "detector_crop_r", obs_module_text("Crop right for detector"), 0, 1920, 1);
	obs_properties_add_int(pp, "detector_crop_t", obs_module_text("Crop top for detector"), 0, 1080, 1);
	obs_properties_add_int(pp, "detector_crop_b", obs_module_text("Crop bottom for detector"), 0, 1080, 1);
	obs_properties_add_bool(pp, "landmark_detection", obs_module_text("Enable landmark detection"));
	p = obs_properties_add_path(pp, "landmark_detection_data", obs_module_text("Landmark detection data"),
			OBS_PATH_FILE,
			"Data Files (*.dat);;"
			"All Files (*.*)",
			obs_get_module_data_path(obs_current_module()) );
	obs_property_set_long_description(p, obs_module_text(
				"You can get the shape_predictor_68_face_landmarks.dat file from: "
				"http://dlib.net/files/shape_predictor_68_face_landmarks.dat.bz2" ));
	p = obs_properties_add_bool(pp, "tracking_th_en", obs_module_text("Set tracking threshold"));
	obs_property_set_modified_callback(p, tracking_th_en_modified);
	p = obs_properties_add_float(pp, "tracking_th_dB", obs_module_text("Tracking threshold"), -120.0, -20.0, 5.0);
	obs_property_float_set_suffix(p, " dB");
}

void face_tracker_manager::get_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, "upsize_l", 0.2);
	obs_data_set_default_double(settings, "upsize_r", 0.2);
	obs_data_set_default_double(settings, "upsize_t", 0.3);
	obs_data_set_default_double(settings, "upsize_b", 0.1);
	obs_data_set_default_double(settings, "scale", 2.0);
	obs_data_set_default_bool(settings, "tracking_th_en", true);
	obs_data_set_default_double(settings, "tracking_th_dB", -80.0);

	if (char *f = obs_module_file("shape_predictor_5_face_landmarks.dat")) {
		obs_data_set_default_string(settings, "landmark_detection_data", f);
		bfree(f);
	}
	else {
		blog(LOG_ERROR, "shape_predictor_5_face_landmarks.dat is not found in the data directory.");
	}
}
