#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <util/bmem.h>
#include "plugin-macros.generated.h"
#include "face-detector-base.h"
#ifndef _WIN32
#include <sys/time.h>
#include <sys/resource.h>
#else // _WIN32
#include <windows.h>
#endif // _WIN32

face_detector_base::face_detector_base()
{
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);
	request_stop = 0;
	running = 0;
	leak_test = bmalloc(1);
}

face_detector_base::~face_detector_base()
{
	bfree(leak_test);
	pthread_cond_destroy(&cond);
	pthread_mutex_destroy(&mutex);
}

void *face_detector_base::thread_routine(void *p)
{
	face_detector_base *base = (face_detector_base*)p;
#ifndef _WIN32
	setpriority(PRIO_PROCESS, 0, 19);
#else // _WIN32
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
#endif // _WIN32
	os_set_thread_name("face-det");


	base->lock();
	while(!base->request_stop) {
		try {
			base->detect_main();
		}
		catch (std::exception &e) {
			blog(LOG_ERROR, "detect_main: exception %s", e.what());
		}
		catch (...) {
			blog(LOG_ERROR, "detect_main: unknown exception");
		}
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
	running = 1;
}

void face_detector_base::stop()
{
	blog(LOG_INFO, "face_detector_base: stopping the thread...");
	lock();
	request_stop = 1;
	signal();
	unlock();
	if (running) {
		pthread_join(thread, NULL);
		running = 0;
	}
	blog(LOG_INFO, "face_detector_base: stopped the thread...");
}
