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

	f3 kp;
	float ki;
	f3 klpf;
	f3 tlpf;
	f3 e_deadband, e_nonlinear; // deadband and nonlinear amount for error input
	f3 filter_int_out;
	f3 filter_int;
	f3 filter_lpf;
	f3 u_last;
	int aspect_x, aspect_y;

	// face tracker source
	char *target_name;
	obs_weak_source_t *target_ref;

	bool debug_faces;
	bool debug_notrack;
	bool debug_always_show;
	FILE *debug_data_tracker;
	FILE *debug_data_error;
	FILE *debug_data_control;
	char *debug_data_tracker_last;
	char *debug_data_error_last;
	char *debug_data_control_last;

	bool is_paused;
	obs_hotkey_pair_id hotkey_pause;
	obs_hotkey_id hotkey_reset;
};
