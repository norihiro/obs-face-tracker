#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include "plugin-macros.generated.h"
#include "face-detector-base.h"
#ifndef _WIN32
#include <sys/time.h>
#include <sys/resource.h>
#endif // ! _WIN32

face_detector_base::face_detector_base()
{
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);
	request_stop = 0;
}

face_detector_base::~face_detector_base()
{
	pthread_cond_destroy(&cond);
	pthread_mutex_destroy(&mutex);
}

void *face_detector_base::thread_routine(void *p)
{
	face_detector_base *base = (face_detector_base*)p;
#ifndef _WIN32
	setpriority(PRIO_PROCESS, 0, 19);
#endif // ! _WIN32
	os_set_thread_name("face-det");


	base->lock();
	while(!base->request_stop) {
		blog(LOG_INFO, "face_detector_base: calling detect_main...");
		base->detect_main();
		blog(LOG_INFO, "face_detector_base: waiting next signal...");
		pthread_cond_wait(&base->cond, &base->mutex);
	}
	base->unlock();
	return NULL;
}

void face_detector_base::start()
{
	blog(LOG_INFO, "face_detector_base: starting the thread.");
	request_stop = 0;
	pthread_create(&thread, NULL, thread_routine, (void*)this);
}

void face_detector_base::stop()
{
	blog(LOG_INFO, "face_detector_base: stopping the thread...");
	lock();
	request_stop = 1;
	signal();
	unlock();
	pthread_join(thread, NULL);
	thread = 0;
	blog(LOG_INFO, "face_detector_base: stopped the thread...");
}
