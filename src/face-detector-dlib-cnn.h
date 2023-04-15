#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include "plugin-macros.generated.h"
#include "face-detector-base.h"

class face_detector_dlib_cnn : public face_detector_base
{
	struct private_s *p;

	void detect_main() override;
	public:
		face_detector_dlib_cnn();
		virtual ~face_detector_dlib_cnn();
		void set_texture(std::shared_ptr<texture_object> &, int crop_l, int crop_r, int crop_t, int crop_b) override;
		void get_faces(std::vector<struct rect_s> &) override;

		void set_model(const char *filename);
};
