#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include "plugin-macros.generated.h"
#include "face-detector-dlib.h"

#include <dlib/image_processing/frontal_face_detector.h>

#define SCALE 2

struct face_detector_dlib_private_s
{
	dlib::array2d<unsigned char> img;
	std::vector<rect_s> rects;
	dlib::frontal_face_detector *detector;
	face_detector_dlib_private_s()
	{
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
	delete p;
}

void face_detector_dlib::set_texture(uint8_t *data, uint32_t linesize, uint32_t width, uint32_t height)
{
	p->img.set_size(height/SCALE, width/SCALE);
	for (int i=0; i<height/SCALE; i++) {
		auto row = p->img[i];
		uint8_t *line = data+(i*SCALE)*linesize;
		for (int j=0; j<width/SCALE; j++) {
			int r = line[j*SCALE*4+0];
			int g = line[j*SCALE*4+1];
			int b = line[j*SCALE*4+2];
			row[j] = (+306*r +601*g +117*b)/1024; // BT.601
		}
	}
}

void face_detector_dlib::detect_main()
{
	if (p->img.nc()<80 || p->img.nr()<80)
		return;

	if (!p->detector)
		p->detector = new dlib::frontal_face_detector(dlib::get_frontal_face_detector());

	std::vector<dlib::rectangle> dets = (*p->detector)(p->img);
	p->rects.resize(dets.size());
	for (size_t i=0; i<dets.size(); i++) {
		rect_s &r = p->rects[i];
		r.x0 = dets[i].left() * SCALE;
		r.y0 = dets[i].top() * SCALE;
		r.x1 = dets[i].right() * SCALE;
		r.y1 = dets[i].bottom() * SCALE;
		r.score = 1.0; // TODO: implement me
	}
}

void face_detector_dlib::get_faces(std::vector<struct rect_s> &rects)
{
	rects = p->rects;
}
