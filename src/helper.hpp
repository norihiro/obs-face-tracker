#pragma once
#include <cmath>
#include <vector>

#ifdef _WIN32
#define DEBUG_DATA_PATH_FILTER "TSV Files (*.tsv);;Data Files (*.dat);;All Files (*.*)"
#else
#define DEBUG_DATA_PATH_FILTER "Data Files (*.dat);;TSV Files (*.tsv);;All Files (*.*)"
#endif

#define CALLDATA_FIXED_DECL(cd, size) calldata_t cd; uint8_t calldata_##cd##_stack[128]; calldata_init_fixed(&cd, calldata_##cd##_stack, sizeof(calldata_##cd##_stack));

struct pointf_s
{
	float x;
	float y;
};

struct rect_s
{
	int x0;
	int y0;
	int x1;
	int y1;
	float score;
};

struct rectf_s
{
	float x0;
	float y0;
	float x1;
	float y1;
};

struct f3
{
	float v[3];

	f3 () {}
	f3 (const f3 &a) {*this=a;}
	f3 (float a, float b, float c) { v[0]=a; v[1]=b; v[2]=c; }
	f3 (const rect_s &a) { v[0]=(float)(a.x0+a.x1)*0.5f; v[1]=(float)(a.y0+a.y1)*0.5f; v[2]=sqrtf((float)(a.x1-a.x0)*(float)(a.y1-a.y0)); }
	f3 (const rectf_s &a) { v[0]=(a.x0+a.x1)*0.5f; v[1]=(a.y0+a.y1)*0.5f; v[2]=sqrtf((a.x1-a.x0)*(a.y1-a.y0)); }
	f3 operator + (const f3 &a) { return f3 (v[0]+a.v[0], v[1]+a.v[1], v[2]+a.v[2]); }
	f3 operator - (const f3 &a) { return f3 (v[0]-a.v[0], v[1]-a.v[1], v[2]-a.v[2]); }
	f3 operator * (float a) { return f3 (v[0]*a, v[1]*a, v[2]*a); }
	f3 & operator += (const f3 &a) { return *this = f3 (v[0]+a.v[0], v[1]+a.v[1], v[2]+a.v[2]); }
	f3 & operator = (const f3 &a) { v[0]=a.v[0]; v[1]=a.v[1]; v[2]=a.v[2]; return *this; }

	f3 hp (const f3 &a) const { return f3 (v[0]*a.v[0], v[1]*a.v[1], v[2]*a.v[2]); }
};

static inline bool isnan(const f3 &a) { return isnan(a.v[0]) || isnan(a.v[1]) || isnan(a.v[2]); }

static inline int get_width (const rect_s &r) { return r.x1 - r.x0; }
static inline int get_height(const rect_s &r) { return r.y1 - r.y0; }
static inline float get_width (const rectf_s &r) { return r.x1 - r.x0; }
static inline float get_height(const rectf_s &r) { return r.y1 - r.y0; }

static inline int common_length(int a0, int a1, int b0, int b1)
{
	// assumes a0 < a1, b0 < b1
	// if (a1 <= b0) return 0; // a0 < a1 < b0 < b1
	if (a0 <= b0 && b0 <= a1 && a1 <= b1) return a1 - b0; // a0 < b0 < a1 < b1
	if (a0 <= b0 && b1 <= a1) return b1 - b0; // a0 < b0 < b1 < a1
	if (b0 <= a0 && a1 <= b1) return a1 - a0; // b0 < a0 < a1 < b1
	if (b0 <= a0 && a0 <= b1 && a0 <= b1) return b1 - a0; // b0 < a0 < b1 < a1
	// if (b1 <= a0) return 0; // b0 < b1 < a0 < a1
	return 0;
}

static inline int common_area(const rect_s &a, const rect_s &b)
{
	return common_length(a.x0, a.x1, b.x0, b.x1) * common_length(a.y0, a.y1, b.y0, b.y1);
}

template <typename T> static inline bool samesign(const T &a, const T &b)
{
	if (a>0 && b>0)
		return true;
	if (a<0 && b<0)
		return true;
	return false;
}

static inline float sqf(float x) { return x*x; }

static inline rectf_s f3_to_rectf(const f3 &u, float w, float h)
{
	const float srwh = sqrtf(w * h);
	const float s2h = h / srwh;
	const float s2w = w / srwh;
	rectf_s r;
	r.x0 = u.v[0] - s2w * u.v[2] * 0.5f;
	r.x1 = u.v[0] + s2w * u.v[2] * 0.5f;
	r.y0 = u.v[1] - s2h * u.v[2] * 0.5f;
	r.y1 = u.v[1] + s2h * u.v[2] * 0.5f;
	return r;
}

void draw_rect_upsize(rect_s r, float upsize_l=0.0f, float upsize_r=0.0f, float upsize_t=0.0f, float upsize_b=0.0f);
void draw_landmark(const std::vector<pointf_s> &landmark);
float landmark_area(const std::vector<pointf_s> &landmark);
pointf_s landmark_center(const std::vector<pointf_s> &landmark);

struct render_reference_line
{
	float rcx, rcy;
	float gmwh; // geometric mean of width and height
	float r;
	f3 e_deadband;
	f3 e_nonlinear;
	bool has_landmark;

	void render() const;
};

inline double from_dB(double x)
{
	return exp(x * (M_LN10/20));
}

void debug_data_open(FILE **dest, char **last_name, obs_data_t *settings, const char *name);
