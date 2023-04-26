#include "plugin-macros.generated.h"
#include <iostream>
#include <dlib/image_processing/frontal_face_detector.h>

int main()
{
	std::cout << dlib::get_serialized_frontal_faces();
	return 0;
}

