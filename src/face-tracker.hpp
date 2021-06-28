#pragma once

#include <vector>
#include <deque>
#include "helper.hpp"

struct tracker_inst_s
{
	class face_tracker_base *tracker;
	rect_s rect;
	rectf_s crop_tracker; // crop corresponding to current processing image
	rectf_s crop_rect; // crop corresponding to rect
	float att;
	enum tracker_state_e {
		tracker_state_init = 0,
		tracker_state_reset_texture, // texture has been set, position is not set.
		tracker_state_constructing, // texture and positions have been set, starting to construct correlation_tracker.
		tracker_state_first_track, // correlation_tracker has been prepared, running 1st tracking
		tracker_state_available, // 1st tracking was done, `rect` is available, can accept next frame.
		tracker_state_ending,
	} state;
	int tick_cnt;
};

struct face_tracker_filter
{
	obs_source_t *context;
	gs_texrender_t *texrender;
	gs_texrender_t *texrender_scaled;
	gs_stagesurf_t *stagesurface;
	class texture_object *cvtex;
	uint32_t known_width;
	uint32_t known_height;
	uint32_t width_with_aspect;
	uint32_t height_with_aspect;
	int tick_cnt;
	int next_tick_stage_to_detector;
	bool target_valid;
	bool rendered;
	bool is_active;
	bool detector_in_progress;

	class face_detector_base *detect;
	std::vector<rect_s> *rects;
	int detect_tick;

	std::deque<struct tracker_inst_s> *trackers;
	std::deque<struct tracker_inst_s> *trackers_idlepool;

	rectf_s crop_cur;
	f3 detect_err;
	f3 range_min, range_max, range_min_out;

	float upsize_l, upsize_r, upsize_t, upsize_b;
	float track_z, track_x, track_y;
	float scale_max;
	volatile float scale;

	float kp;
	float ki;
	float klpf;
	float tlpf;
	f3 e_deadband, e_nonlinear; // deadband and nonlinear amount for error input
	f3 filter_int_out;
	f3 filter_int;
	f3 filter_lpf;
	int aspect_x, aspect_y;

	bool debug_faces;
	bool debug_notrack;
};
