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

	class ft_manager_for_ftptz *ftm;

	float track_z, track_x, track_y;

	float kp_x, kp_y, kp_z;
	float ki;
	float klpf;
	float tlpf;
	f3 e_deadband, e_nonlinear; // deadband and nonlinear amount for error input
	f3 filter_int;
	f3 filter_lpf;
	int u[3];
	int u_prev[3];
	int u_prev1[3];
	int ptz_query[3];
	bool ptz_request_reset;

	bool debug_faces;
	bool debug_notrack;
	bool debug_always_show;

	char *ptz_type;
	int ptz_max_x, ptz_max_y, ptz_max_z;
};
