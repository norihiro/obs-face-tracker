#pragma once
#include <obs-module.h>
#include <util/threading.h>
#include <vector>
#include "plugin-macros.generated.h"
#include "face-detector-base.h"

class face_tracker_base
{
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	bool running;
	volatile bool stop_requested;
	volatile bool stopped;
	volatile bool suspend_requested;
	void *leak_test;

	static void* thread_routine(void *);
	virtual void track_main() = 0;

	public:
		face_tracker_base();
		virtual ~face_tracker_base();

		int lock() { return pthread_mutex_lock(&mutex); }
		int trylock() { return pthread_mutex_trylock(&mutex); }
		int unlock() { return pthread_mutex_unlock(&mutex); }
		int signal() { return pthread_cond_signal(&cond); }

		virtual void set_texture(class texture_object *) = 0;
		virtual void set_position(const rect_s &rect) = 0;
		virtual void set_upsize_info(const rectf_s &upsize) = 0;
		virtual void set_landmark_detection(const char *data_file_path) = 0;
		virtual bool get_face(struct rect_s &) = 0;
		virtual bool get_landmark(std::vector<pointf_s> &) = 0;

		void start();
		void stop();
		void request_stop();
		void request_suspend();
		bool is_stopped();
};
