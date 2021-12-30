#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include "plugin-macros.generated.h"
#include "face-tracker-base.h"

class face_tracker_dlib : public face_tracker_base
{
	struct face_tracker_dlib_private_s *p;

	void track_main() override;
	public:
		face_tracker_dlib();
		virtual ~face_tracker_dlib();

		void set_texture(texture_object *) override;
		void set_position(const rect_s &rect) override;
		void set_upsize_info(const rectf_s &upsize) override;
		void set_landmark_detection(const char *data_file_path) override;
		bool get_face(struct rect_s &) override;
		bool get_landmark(std::vector<pointf_s> &) override;
};
