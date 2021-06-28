#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include "plugin-macros.generated.h"
#include "face-tracker-base.h"

class face_tracker_dlib : public face_tracker_base
{
	struct face_tracker_dlib_private_s *p;

	virtual void track_main();
	public:
		face_tracker_dlib();
		virtual ~face_tracker_dlib();

		void set_texture(texture_object *) override;
		virtual void set_position(const rect_s &rect);
		virtual bool get_face(struct rect_s &);
};
