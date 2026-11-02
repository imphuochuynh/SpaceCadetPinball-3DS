// Native harness: prints where the table quad's corners land on the top screen
// for each camera mode, using the exact on-device camera code.
// Build: see tools/camera_preview/README.md
#include <cstdio>
#include "../../SpaceCadetPinball/n3ds_camera.h"
using namespace n3ds_camera;

static void corners(const char *label, Mode mode, float focus)
{
	State s; s.focusRow = focus;
	View v; Compute(mode, s, 0, 0, &v);
	printf("%s", label);
	const float q[4][2] = {{0, 1}, {1, 1}, {1, 0}, {0, 0}}; // TL TR BR BL
	for (auto &p : q)
	{
		C3D_FVec w = Mtx_MultiplyFVec4(&v.modelView, FVec4_New(p[0], p[1], 0, 1));
		C3D_FVec c = Mtx_MultiplyFVec4(&v.projection, w);
		printf(" %.2f %.2f", (1 - c.y / c.w) * 200, 240 - (1 + c.x / c.w) * 120); // y down for images
	}
	printf("\n");
}

int main()
{
	for (int m = 0; m < (int)Mode::Count; m++)
	{
		float lo, hi; FocusLimits((Mode)m, &lo, &hi);
		fprintf(stderr, "%s focus rows %.1f..%.1f\n", ModeName((Mode)m), lo, hi);
		char buf[64];
		snprintf(buf, sizeof buf, "%s_top", ModeName((Mode)m)); corners(buf, (Mode)m, lo);
		snprintf(buf, sizeof buf, "%s_bottom", ModeName((Mode)m)); corners(buf, (Mode)m, hi);
	}
}
