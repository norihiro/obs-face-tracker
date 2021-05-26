#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include "plugin-macros.generated.h"
#include "face-tracker-base.h"
#ifndef _WIN32
#include <sys/time.h>
#include <sys/resource.h>
#endif // ! _WIN32

face_tracker_base::face_tracker_base()
{
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);
	stop_requested = 0;
}

face_tracker_base::~face_tracker_base()
{
	pthread_cond_destroy(&cond);
	pthread_mutex_destroy(&mutex);
}

void *face_tracker_base::thread_routine(void *p)
{
	face_tracker_base *base = (face_tracker_base*)p;
#ifndef _WIN32
	setpriority(PRIO_PROCESS, 0, 19);
#endif // ! _WIN32
	os_set_thread_name("face-trk");

	base->lock();
	while(!base->stop_requested) {
		blog(LOG_INFO, "face_tracker_base: calling detect_main...");
		if (!base->suspend_requested)
			base->track_main();
		blog(LOG_INFO, "face_tracker_base: waiting next signal...");
		pthread_cond_wait(&base->cond, &base->mutex);
	}
	base->stopped = 1;
	base->unlock();
	return NULL;
}

void face_tracker_base::start()
{
	blog(LOG_INFO, "face_tracker_base: starting the thread.");
	stop_requested = 0;
	stopped = 0;
	suspend_requested = 0;
	if (!thread)
		pthread_create(&thread, NULL, thread_routine, (void*)this);
	else {
		lock();
		signal();
		unlock();
	}
}

void face_tracker_base::stop()
{
	blog(LOG_INFO, "face_tracker_base: stopping the thread...");
	lock();
	stop_requested = 1;
	signal();
	unlock();
	pthread_join(thread, NULL);
	thread = 0;
	blog(LOG_INFO, "face_tracker_base: stopped the thread...");
}

void face_tracker_base::request_stop()
{
	lock();
	stop_requested = 1;
	signal();
	unlock();
}

bool face_tracker_base::is_stopped()
{
	if (stopped) {
		pthread_join(thread, NULL);
		thread = 0;
		return 1;
	}
	else
		return 0;
}

void face_tracker_base::request_suspend()
{
	suspend_requested = true;
}
