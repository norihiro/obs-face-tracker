#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include "plugin-macros.generated.h"
#include "face-detector-base.h"

class face_detector_dlib : public face_detector_base
{
	struct face_detector_dlib_private_s *p;

	virtual void detect_main();
	public:
		face_detector_dlib();
		virtual ~face_detector_dlib();
		virtual void set_texture(uint8_t *data, uint32_t linesize, uint32_t width, uint32_t height);
		virtual void get_faces(std::vector<struct rect_s> &);
};
