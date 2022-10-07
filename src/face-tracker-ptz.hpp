#pragma once

#include <vector>
#include <deque>
#include "helper.hpp"

struct face_tracker_ptz
{
	obs_source_t *context;
	uint32_t known_width;
	uint32_t known_height;
	bool rendered;
	bool is_active;

	f3 detect_err;
	bool face_found, face_found_last;

	class ft_manager_for_ftptz *ftm;

	float track_z, track_x, track_y;

	float kp_x, kp_y, kp_z;
	f3 ki;
	f3 klpf;
	f3 tlpf;
	f3 e_deadband, e_nonlinear; // deadband and nonlinear amount for error input
	f3 filter_int;
	f3 filter_lpf;
	float f_att_int;
	int u[3];
	int ptz_query[3];
	uint64_t face_found_last_ns;

	int face_lost_timeout_ms;
	int face_lost_ptz_preset;

	bool debug_faces;
	bool debug_notrack;
	bool debug_always_show;
	FILE *debug_data_tracker;
	FILE *debug_data_error;
	FILE *debug_data_control;
	char *debug_data_tracker_last;
	char *debug_data_error_last;
	char *debug_data_control_last;

	char *ptz_type;
	int ptz_max_x, ptz_max_y, ptz_max_z;

	bool is_paused;
	obs_hotkey_pair_id hotkey_pause;
	obs_hotkey_id hotkey_reset;
};
