#include "face-tracker-dlib.h"
#include "face-tracker-manager.hpp"
#include "texture-object.h"

#include <cstdio>
#include <cstdlib>
#include <random>
#include <semaphore>
#include <thread>

// The manager's settings helpers reference module entry points. This test never loads a module.
obs_module_t *obs_current_module()
{
	return nullptr;
}

const char *obs_module_text(const char *text)
{
	return text;
}

namespace {

using tracker_state = face_tracker_manager::tracker_inst_s::tracker_state_e;

void require(bool condition, const char *message)
{
	if (!condition) {
		std::fprintf(stderr, "%s\n", message);
		std::exit(EXIT_FAILURE);
	}
}

std::shared_ptr<texture_object> make_frame(int scale)
{
	constexpr int size = 192;
	auto *frame = obs_source_frame_create(VIDEO_FORMAT_BGRX, size, size);
	std::mt19937 pixels(1234);
	for (int y = 0; y < size; y++) {
		auto *line = frame->data[0] + y * frame->linesize[0];
		for (int x = 0; x < size; x++) {
			const auto value = static_cast<uint8_t>(pixels());
			line[4 * x] = line[4 * x + 1] = line[4 * x + 2] = value;
			line[4 * x + 3] = 255;
		}
	}

	auto texture = std::make_shared<texture_object>();
	texture->scale = scale;
	texture->set_texture_obsframe(frame, scale);
	obs_source_frame_destroy(frame);
	return texture;
}

class fixture_detector : public face_detector_base {
	void detect_main() override {}

public:
	void set_texture(std::shared_ptr<texture_object> &, int, int, int, int) override {}
	void get_faces(std::vector<rect_s> &faces) override { faces = {{56, 56, 136, 136, 1.0f}}; }
};

class controlled_tracker : public face_tracker_dlib {
	std::binary_semaphore entered{0}, proceed{0}, completed{0};
	bool has_frame = false;
	bool delay = false;

	void track_main() override
	{
		if (!has_frame)
			return;
		has_frame = false;
		if (delay) {
			delay = false;
			entered.release();
			proceed.acquire();
		}
		face_tracker_dlib::track_main();
		completed.release();
	}

public:
	void set_texture(std::shared_ptr<texture_object> &texture) override
	{
		face_tracker_dlib::set_texture(texture);
		has_frame = true;
	}

	void delay_next_update()
	{
		lock();
		delay = true;
		unlock();
	}

	void wait_until_delayed() { entered.acquire(); }
	void resume() { proceed.release(); }

	void wait_for_update()
	{
		completed.acquire();
		// The completion signal precedes the worker's release of its mutex.
		lock();
		unlock();
	}
};

class fixture_manager : public face_tracker_manager {
	std::shared_ptr<texture_object> frame;

	std::shared_ptr<texture_object> get_cvtex() override { return frame; }

public:
	explicit fixture_manager(controlled_tracker *worker)
	{
		detect = new fixture_detector();
		detector_crop_l = detector_crop_r = detector_crop_t = detector_crop_b = 0;
		reset_requested = false;
		trackers_idlepool.push_back({});
		trackers_idlepool.back().tracker = worker;
	}

	void render(std::shared_ptr<texture_object> texture = {})
	{
		frame = std::move(texture);
		post_render();
		frame.reset();
	}
};

void initialize_tracker(fixture_manager &manager, controlled_tracker &worker, int scale)
{
	manager.render(make_frame(scale));
	require(manager.trackers.size() == 1 && manager.trackers.front().tracker == &worker,
		"Detector cycle should reserve one tracker from the idle pool");
	require(manager.trackers_idlepool.empty(), "Reserved tracker should leave the idle pool");
	manager.tick(1.0f);
	// Deliver the detector result without a new frame so start_track runs on the detector frame.
	manager.render();
	worker.wait_for_update();
}

void invalidate_tracker(fixture_manager &manager, controlled_tracker &worker, int scale)
{
	worker.delay_next_update();
	auto texture = make_frame(scale);
	std::weak_ptr<texture_object> rejected_frame = texture;
	manager.render(std::move(texture));
	worker.wait_until_delayed();
	manager.render(make_frame(scale));
	require(manager.trackers.size() == 1 &&
			manager.trackers.front().state == tracker_state::tracker_state_first_track,
		"An in-flight first update should stay pending");
	require(!rejected_frame.expired(), "The worker should hold the pending frame");

	worker.resume();
	worker.wait_for_update();
	auto next_texture = make_frame(scale);
	std::weak_ptr<texture_object> next_frame = next_texture;
	manager.render(std::move(next_texture));
	require(manager.trackers.empty(), "A scale mismatch on the first update should retire the active tracker");
	require(rejected_frame.expired(), "The worker should release the rejected frame");
	require(next_frame.expired(), "The retired tracker should receive no next frame");

	manager.render(make_frame(scale));
	require(manager.trackers.empty(), "Subsequent frames should leave the invalidated tracker idle");
	require(manager.trackers_idlepool.size() == 1 && manager.trackers_idlepool.front().tracker == &worker,
		"Repeated invalidations should retain only one reusable worker");
	manager.tick(1.0f);
}

void recovers_at_new_scale()
{
	auto *worker = new controlled_tracker();
	fixture_manager manager(worker);
	initialize_tracker(manager, *worker, 1);
	invalidate_tracker(manager, *worker, 2);

	initialize_tracker(manager, *worker, 2);
	manager.render(make_frame(2));
	worker->wait_for_update();
	manager.render();
	require(manager.trackers.size() == 1 &&
			manager.trackers.front().state == tracker_state::tracker_state_available,
		"The next detector result should produce an available tracker after a same-scale update");
	manager.tick(1.0f);
	require(manager.tracker_rects.size() == 1 && manager.tracker_rects.front().rect.score > 0.0f,
		"Tracking should publish a face at the new scale");
}

void repeated_invalidations_reuse_worker()
{
	auto *worker = new controlled_tracker();
	fixture_manager manager(worker);
	for (int cycle = 0; cycle < 3; cycle++) {
		initialize_tracker(manager, *worker, 1);
		invalidate_tracker(manager, *worker, 2);
	}
}

void reuses_ready_worker_behind_busy_worker()
{
	auto *busy = new controlled_tracker();
	auto *ready = new controlled_tracker();
	fixture_manager manager(busy);
	manager.trackers_idlepool.push_back({.tracker = ready});

	std::binary_semaphore locked{0}, release{0};
	std::thread lock_holder([&] {
		busy->lock();
		locked.release();
		release.acquire();
		busy->unlock();
	});
	locked.acquire();
	manager.render(make_frame(1));
	release.release();
	lock_holder.join();

	require(manager.trackers.size() == 1 && manager.trackers.front().tracker == ready,
		"The detector should reuse a ready worker behind a busy worker");
	require(manager.trackers_idlepool.size() == 1 && manager.trackers_idlepool.front().tracker == busy,
		"The busy worker should remain in the idle pool");
}

} // namespace

int main()
{
	recovers_at_new_scale();
	repeated_invalidations_reuse_worker();
	reuses_ready_worker_behind_busy_worker();
}
