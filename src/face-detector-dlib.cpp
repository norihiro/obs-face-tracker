#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include "plugin-macros.generated.h"
#include "face-detector-dlib.h"
#include "texture-object.h"

#include <dlib/image_processing/frontal_face_detector.h>

struct face_detector_dlib_private_s
{
	texture_object *tex;
	std::vector<rect_s> rects;
	dlib::frontal_face_detector *detector;
	face_detector_dlib_private_s()
	{
		tex = NULL;
	}
};

face_detector_dlib::face_detector_dlib()
{
	p = new face_detector_dlib_private_s;
	p->detector = NULL;
}

face_detector_dlib::~face_detector_dlib()
{
	if (p->detector) delete p->detector;
	if (p->tex) p->tex->release();
	delete p;
}

void face_detector_dlib::set_texture(texture_object *tex)
{
	if (p->tex) p->tex->release();
	tex->addref();
	p->tex = tex;
}

void face_detector_dlib::detect_main()
{
	if (!p->tex)
		return;
	const dlib::array2d<unsigned char> &img = p->tex->get_dlib_img();
	if (img.nc()<80 || img.nr()<80)
		return;

	if (!p->detector)
		p->detector = new dlib::frontal_face_detector(dlib::get_frontal_face_detector());

	std::vector<dlib::rectangle> dets = (*p->detector)(img);
	p->rects.resize(dets.size());
	for (size_t i=0; i<dets.size(); i++) {
		rect_s &r = p->rects[i];
		r.x0 = dets[i].left() * p->tex->scale;
		r.y0 = dets[i].top() * p->tex->scale;
		r.x1 = dets[i].right() * p->tex->scale;
		r.y1 = dets[i].bottom() * p->tex->scale;
		r.score = 1.0; // TODO: implement me
	}

	if (p->tex) p->tex->release();
	p->tex = NULL;
}

void face_detector_dlib::get_faces(std::vector<struct rect_s> &rects)
{
	rects = p->rects;
}
