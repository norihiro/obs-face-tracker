/*
 * A video source for the integration tests in tests/integration: an
 * asynchronous one, as the face tracker's filters are for that kind only and
 * no source OBS has can be one without a camera, that shows the BGRA frame in
 * its frame_file setting, of width by height pixels, if it has one.
 */

#include <obs-module.h>
#include <util/threading.h>
#include <util/platform.h>
#include <errno.h>
#include <stdio.h>

struct test_video_source
{
	obs_source_t *source;
	pthread_mutex_t mutex;
	pthread_t thread;
	os_event_t *stop;
	uint8_t *data;
	uint32_t width, height;
};

static const char *test_video_source_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "Face Tracker Test Video Source";
}

static void *test_video_source_thread(void *data)
{
	struct test_video_source *v = data;

	while (os_event_timedwait(v->stop, 50) == ETIMEDOUT) {
		pthread_mutex_lock(&v->mutex);
		if (v->data) {
			struct obs_source_frame frame = {0};
			frame.data[0] = v->data;
			frame.linesize[0] = v->width * 4;
			frame.width = v->width;
			frame.height = v->height;
			frame.format = VIDEO_FORMAT_BGRA;
			frame.timestamp = os_gettime_ns();
			obs_source_output_video(v->source, &frame);
		}
		pthread_mutex_unlock(&v->mutex);
	}
	return NULL;
}

static void test_video_source_update(void *data, obs_data_t *settings)
{
	struct test_video_source *v = data;
	const char *file = obs_data_get_string(settings, "frame_file");
	uint32_t width = (uint32_t)obs_data_get_int(settings, "width");
	uint32_t height = (uint32_t)obs_data_get_int(settings, "height");
	uint8_t *buf = NULL;

	if (*file && width && height) {
		size_t size = (size_t)width * height * 4;
		FILE *f = fopen(file, "rb");
		if (f) {
			buf = bmalloc(size);
			if (fread(buf, 1, size, f) != size) {
				bfree(buf);
				buf = NULL;
			}
			fclose(f);
		}
		if (!buf)
			blog(LOG_WARNING, "[face-tracker-test-sources] test video source: cannot read %ux%u BGRA from '%s'", width, height, file);
	}

	pthread_mutex_lock(&v->mutex);
	bfree(v->data);
	v->data = buf;
	v->width = width;
	v->height = height;
	pthread_mutex_unlock(&v->mutex);
}

static void *test_video_source_create(obs_data_t *settings, obs_source_t *source)
{
	struct test_video_source *v = bzalloc(sizeof(struct test_video_source));
	v->source = source;
	pthread_mutex_init(&v->mutex, NULL);
	os_event_init(&v->stop, OS_EVENT_TYPE_MANUAL);
	test_video_source_update(v, settings);
	pthread_create(&v->thread, NULL, test_video_source_thread, v);
	return v;
}

static void test_video_source_destroy(void *data)
{
	struct test_video_source *v = data;
	os_event_signal(v->stop);
	pthread_join(v->thread, NULL);
	os_event_destroy(v->stop);
	pthread_mutex_destroy(&v->mutex);
	bfree(v->data);
	bfree(v);
}

void register_test_video_source(void)
{
	struct obs_source_info info = {
		.id = "face_tracker_test_video_source",
		.type = OBS_SOURCE_TYPE_INPUT,
		.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_ASYNC,
		.get_name = test_video_source_name,
		.create = test_video_source_create,
		.destroy = test_video_source_destroy,
		.update = test_video_source_update,
	};
	obs_register_source(&info);
}
