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

	f4 detect_err;
	f4 range_min, range_max, range_min_out;

	class ft_manager_for_ftf *ftm;

	float track_z, track_x, track_y;
	float scale_max;

	f4 kp;
	float ki;
	f4 klpf;
	f4 tlpf;
	f4 e_deadband, e_nonlinear; // deadband and nonlinear amount for error input
	f4 filter_int_out;
	f4 filter_int;
	f4 filter_lpf;
	f4 u_last;
	int aspect_x, aspect_y;
	bool rotate = false;

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
