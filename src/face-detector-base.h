#pragma once
#include <obs-module.h>
#include <util/threading.h>
#include <vector>
#include "plugin-macros.generated.h"
#include "helper.hpp"

class face_detector_base
{
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	bool running;
	volatile bool request_stop;
	void *leak_test;

	static void* thread_routine(void *);
	virtual void detect_main() = 0;

	public:
		face_detector_base();
		virtual ~face_detector_base();

		int lock() { return pthread_mutex_lock(&mutex); }
		int trylock() { return pthread_mutex_trylock(&mutex); }
		int unlock() { return pthread_mutex_unlock(&mutex); }
		int signal() { return pthread_cond_signal(&cond); }

		virtual void set_texture(class texture_object *) = 0;
		virtual void get_faces(std::vector<struct rect_s> &) = 0;

		void start();
		void stop();
};
