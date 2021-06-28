#pragma once

#include <vector>
#include <deque>
#include "helper.hpp"

struct face_tracker_filter
{
	obs_source_t *context;
	gs_texrender_t *texrender;
	gs_texrender_t *texrender_scaled;
	gs_stagesurf_t *stagesurface;
	uint32_t known_width;
	uint32_t known_height;
	uint32_t width_with_aspect;
	uint32_t height_with_aspect;
	bool target_valid;
	bool rendered;
	bool is_active;

	f3 detect_err;
	f3 range_min, range_max, range_min_out;

	class ft_manager_for_ftf *ftm;

	float track_z, track_x, track_y;
	float scale_max;

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
