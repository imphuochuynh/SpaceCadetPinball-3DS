#pragma once

// Top-screen camera for the pinball table.
//
// Pure math on top of citro3d's matrix helpers (no GPU calls), so it can be
// compiled natively and previewed off-device (see tools/camera_preview).
//
// The table is drawn as one textured unit quad: quad (0,0) is the bottom-left
// of the table art and quad (0,1) the top-left. Table rows run 0 (top) to
// TableHeight (bottom), matching the game's own screen coordinates.

// c3d/maths.h has no C++ guard of its own (citro3d.h normally provides it).
extern "C"
{
#include <c3d/maths.h>
}

namespace n3ds_camera
{
	enum class Mode : int
	{
		Classic = 0, // whole table, centered
		Follow = 1,  // zoomed in, tracks the ball
		Count = 2,
	};

	constexpr float ScreenWidth = 400.0f;  // top screen, landscape
	constexpr float ScreenHeight = 240.0f;
	constexpr float TableWidth = 360.0f;   // table area of the game's screen, in pixels
	constexpr float TableHeight = 416.0f;

	struct View
	{
		C3D_Mtx projection;
		C3D_Mtx modelView;
	};

	struct State
	{
		float focusRow = TableHeight; // smoothed table row the camera centers on
		float lastBallRow = -1.0f;    // for the ball's vertical velocity
	};

	const char *ModeName(Mode mode);
	Mode NextMode(Mode mode);

	// Allowed range for the focus row, so the camera never shows past the table
	// edges and, at the low end, keeps the flippers fully in view.
	void FocusLimits(Mode mode, float *minRow, float *maxRow);

	// Advance the camera one frame (60 fps). ballRow < 0 means no ball in play,
	// which parks the camera on the flippers and plunger.
	void Update(State *state, Mode mode, float ballRow);

	// Snap straight to the right spot, e.g. after switching modes or a new game.
	void Reset(State *state, Mode mode);

	// Build projection and model-view matrices for drawing the table quad.
	// shakeX/shakeY: table bump offset in game pixels.
	void Compute(Mode mode, const State &state, float shakeX, float shakeY, View *out);
}
