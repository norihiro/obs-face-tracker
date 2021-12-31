#include <obs-module.h>
#include "plugin-macros.generated.h"
#include "helper.hpp"

void draw_rect_upsize(rect_s r, float upsize_l, float upsize_r, float upsize_t, float upsize_b)
{
	if (r.x0>=r.x1 || r.y0>=r.y1)
		return;
	int w = r.x1-r.x0;
	int h = r.y1-r.y0;
	float dx0 = w * upsize_l;
	float dx1 = w * upsize_r;
	float dy0 = h * upsize_t;
	float dy1 = h * upsize_b;

	gs_render_start(false);

	if (std::abs(dx0)>=0.5f || std::abs(dy1)>=0.5f || std::abs(dx1)>=0.5f || std::abs(dy0)>=0.5f) {
		gs_vertex2f((float)r.x0, (float)r.y0); gs_vertex2f((float)r.x0, (float)r.y1);
		gs_vertex2f((float)r.x0, (float)r.y1); gs_vertex2f((float)r.x1, (float)r.y1);
		gs_vertex2f((float)r.x1, (float)r.y1); gs_vertex2f((float)r.x1, (float)r.y0);
		gs_vertex2f((float)r.x1, (float)r.y0); gs_vertex2f((float)r.x0, (float)r.y0);
	}
	r.x0 -= (int)dx0;
	r.x1 += (int)dx1;
	r.y0 -= (int)dy0;
	r.y1 += (int)dy1;
	gs_vertex2f((float)r.x0, (float)r.y0); gs_vertex2f((float)r.x0, (float)r.y1);
	gs_vertex2f((float)r.x0, (float)r.y1); gs_vertex2f((float)r.x1, (float)r.y1);
	gs_vertex2f((float)r.x1, (float)r.y1); gs_vertex2f((float)r.x1, (float)r.y0);
	gs_vertex2f((float)r.x1, (float)r.y0); gs_vertex2f((float)r.x0, (float)r.y0);

	gs_render_stop(GS_LINES);
}

float landmark_area(const std::vector<pointf_s> &landmark)
{
	// TODO: implement area calculation for other models
	// Maybe, use the area of the maximum convex polygon.

	float ret = 0.0f;

	const static int ii5[] = {
		1, // center
		// 0, 4, 2, 3, 1,
		0, 1, 3, 2, 4,
		0, -1 };

	const static int ii68[] = {
		30, // center
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
		10, 11, 12, 13, 14, 15, 16,
		26, 25, 24, 23, 22,
		21, 20, 19, 18, 17,
		0, -1 };

	const int *ii =
		landmark.size() == 68 ? ii68 :
		landmark.size() == 5 ? ii5 :
		NULL;

	if (!ii) return 0.0f;

	pointf_s c = landmark[ii[0]]; // center to minimize calculation errors
	for (int i=1; ii[i+1] >= 0; i++) {
		float x1 = landmark[ii[i]].x - c.x;
		float y1 = landmark[ii[i]].y - c.y;
		float x2 = landmark[ii[i+1]].x - c.x;
		float y2 = landmark[ii[i+1]].y - c.y;
		ret += (x2 * y1 - x1 * y2) * 0.5f;
	}

	return ret;
}

pointf_s landmark_center(const std::vector<pointf_s> &landmark)
{
	pointf_s ret = {0.0f, 0.0f};

	for (size_t i=0; i<landmark.size(); i++) {
		ret.x += landmark[i].x;
		ret.y += landmark[i].y;
	}

	ret.x /= landmark.size();
	ret.y /= landmark.size();

	return ret;
}

void draw_landmark(const std::vector<pointf_s> &landmark)
{
	if (landmark.size() < 2)
		return;

	gs_render_start(false);

	if (landmark.size()==5) {
			gs_vertex2f(landmark[0].x, landmark[0].y);
			gs_vertex2f(landmark[1].x, landmark[1].y);

			gs_vertex2f(landmark[1].x, landmark[1].y);
			gs_vertex2f(landmark[3].x, landmark[3].y);

			gs_vertex2f(landmark[3].x, landmark[3].y);
			gs_vertex2f(landmark[2].x, landmark[2].y);

			gs_vertex2f(landmark[2].x, landmark[2].y);
			gs_vertex2f(landmark[4].x, landmark[4].y);

			gs_vertex2f(landmark[4].x, landmark[4].y);
			gs_vertex2f(landmark[0].x, landmark[0].y);
	}
	else for (size_t i=0; i<landmark.size(); i++) {
		// points should not be duplicated: 0, 16, 17, 21, 22, 26, 27, 30, 31, 35, 36, 41, 42, 47, 48, 59, 60, 57
		if (i==42) {
			gs_vertex2f(landmark[36].x, landmark[36].y);
		}
		else if (i==48) {
			gs_vertex2f(landmark[42].x, landmark[42].y);
		}
		else if (i==60) {
			gs_vertex2f(landmark[48].x, landmark[48].y);
		}
		else if (i==67) {
			gs_vertex2f(landmark[60].x, landmark[60].y);
		}
		else if (i==0 || i==16 || i==17 || i==21 || i==22 || i==26 || i==27 || i==30 || i==31 || i==35 || i==36) {
		}
		else
			gs_vertex2f(landmark[i].x, landmark[i].y);

		gs_vertex2f(landmark[i].x, landmark[i].y);
	}

	gs_render_stop(GS_LINES);
}

void debug_data_open(FILE **dest, char **last_name, obs_data_t *settings, const char *name)
{
	const char *debug_data = obs_data_get_string(settings, name);

	// If the file name is not changed, just return.
	if (*last_name && debug_data && strcmp(*last_name, debug_data) == 0)
		return;

	// If both file names are empty, just return.
	if (!*last_name && (!debug_data || !*debug_data))
		return;

	if (*dest)
		fclose(*dest);
	*dest = NULL;

	if (*last_name)
		bfree(*last_name);
	*last_name = NULL;

	if (debug_data && *debug_data) {
		*dest = fopen(debug_data, "a");
		if (!*dest) {
			blog(LOG_ERROR, "%s: Failed to open file \"%s\"", name, debug_data);
		}
		*last_name = bstrdup(debug_data);
	}
}
