#pragma once

#include <deque>
#include "face-tracker-base.h"

class face_tracker_manager
{
	public:
		struct tracker_rect_s {
			rect_s rect;
			rectf_s crop_rect;
		};

		struct tracker_inst_s
		{
			class face_tracker_base *tracker;
			rect_s rect;
			rectf_s crop_tracker; // crop corresponding to current processing image
			rectf_s crop_rect; // crop corresponding to rect
			float att;
			float score_first;
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

	public: // properties
		float upsize_l, upsize_r, upsize_t, upsize_b;
		volatile float scale;
		volatile bool reset_requested;
		float tracking_threshold;
		int detector_crop_l, detector_crop_r, detector_crop_t, detector_crop_b;

	public: // realtime status
		rectf_s crop_cur;
		int tick_cnt;

	public: // results
		std::vector<rect_s> detect_rects;
		std::vector<tracker_rect_s> tracker_rects;

	public: /* not sure they are necessary to be public */
		class face_detector_base *detect;
		int detect_tick;

		std::deque<struct tracker_inst_s> trackers;
		std::deque<struct tracker_inst_s> trackers_idlepool;

	private:
		int next_tick_stage_to_detector;
		bool detector_in_progress;

	public:
		face_tracker_manager();
		~face_tracker_manager();
		void tick(float second);
		void post_render();
		void update(obs_data_t *settings);
		static void get_properties(obs_properties_t *);
		static void get_defaults(obs_data_t *settings);

	protected:
		virtual class texture_object *get_cvtex() = 0;

	private:
		inline void retire_tracker(int ix);
		inline bool is_low_confident(const tracker_inst_s &t, float th1);
		void remove_duplicated_tracker();
		void attenuate_tracker();
		void copy_detector_to_tracker();
		void stage_to_detector();
		int stage_surface_to_tracker(struct tracker_inst_s &t);
		void stage_to_trackers();
};
