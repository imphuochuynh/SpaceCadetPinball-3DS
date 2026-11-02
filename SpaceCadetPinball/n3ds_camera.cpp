#include "n3ds_camera.h"

#include <cmath>

namespace n3ds_camera
{
	// ---- Tuning -----------------------------------------------------------------

	// Follow: table drawn this tall on the 240px screen (1.75x the Classic view),
	// which still fits the full table width on the 400px screen.
	constexpr float FollowTableHeight = 420.0f;

	// Tilted: table leaned back by TiltDegrees and viewed through a perspective
	// camera. Distance is set so the focus row spans TiltWidthFill of the screen.
	constexpr float TiltDegrees = 28.0f;
	constexpr float TiltFovDegrees = 35.0f; // vertical field of view
	constexpr float TiltWidthFill = 0.82f; // keeps the near (flipper) end on screen

	// Camera smoothing per 60fps frame. The camera chases a falling ball faster
	// than a rising one, so the flippers come into view before the ball arrives.
	constexpr float SmoothingDown = 0.20f;
	constexpr float SmoothingUp = 0.08f;
	constexpr float LookaheadFrames = 10.0f;

	// Keep this much table below the bottom edge of the flippers' area visible.
	constexpr float BottomMargin = 0.0f;

	// ---- Helpers ----------------------------------------------------------------

	static float Clamp(float v, float lo, float hi)
	{
		return v < lo ? lo : (v > hi ? hi : v);
	}

	static float TiltDistance()
	{
		// Visible half-width at distance d is d * tan(fov/2) * aspect.
		float halfWidthPerUnit = tanf(C3D_AngleFromDegrees(TiltFovDegrees) * 0.5f) * C3D_AspectRatioTop;
		return (TableWidth * 0.5f) / (halfWidthPerUnit * TiltWidthFill);
	}

	// Screen position (x right 0..400, y up 0..240) of a point on the table quad.
	static bool ProjectToScreen(const View &view, float qx, float qy, float *sx, float *sy)
	{
		C3D_FVec v = Mtx_MultiplyFVec4(&view.modelView, FVec4_New(qx, qy, 0.0f, 1.0f));
		C3D_FVec c = Mtx_MultiplyFVec4(&view.projection, v);
		if (c.w <= 0.0f)
			return false;
		// The 3DS screens are rotated: NDC y runs across the screen, NDC x up it.
		*sx = (1.0f - c.y / c.w) * 0.5f * ScreenWidth;
		*sy = (1.0f + c.x / c.w) * 0.5f * ScreenHeight;
		return true;
	}

	// Screen y of the table's top (edge = 1) or bottom (edge = 0) at a focus row.
	static float EdgeScreenY(Mode mode, float focusRow, float edge)
	{
		State s;
		s.focusRow = focusRow;
		View view;
		Compute(mode, s, 0.0f, 0.0f, &view);
		float sx, sy;
		if (!ProjectToScreen(view, 0.5f, edge, &sx, &sy))
			return edge > 0.5f ? 1e6f : -1e6f;
		return sy;
	}

	// Find the focus row where an edge of the table lands exactly on a screen
	// edge. Screen y of an edge grows with focusRow, so bisection works.
	static float SolveFocus(Mode mode, float edge, float targetY)
	{
		float lo = 0.0f, hi = TableHeight;
		for (int i = 0; i < 40; i++)
		{
			float mid = (lo + hi) * 0.5f;
			if (EdgeScreenY(mode, mid, edge) < targetY)
				lo = mid;
			else
				hi = mid;
		}
		return (lo + hi) * 0.5f;
	}

	// ---- Public -----------------------------------------------------------------

	const char *ModeName(Mode mode)
	{
		switch (mode)
		{
		case Mode::Classic: return "Classic";
		case Mode::Follow: return "Follow";
		case Mode::Tilted: return "Tilted";
		default: return "";
		}
	}

	Mode NextMode(Mode mode)
	{
		return static_cast<Mode>((static_cast<int>(mode) + 1) % static_cast<int>(Mode::Count));
	}

	void FocusLimits(Mode mode, float *minRow, float *maxRow)
	{
		static bool cached[static_cast<int>(Mode::Count)] = {};
		static float cachedMin[static_cast<int>(Mode::Count)], cachedMax[static_cast<int>(Mode::Count)];
		int m = static_cast<int>(mode);

		if (!cached[m])
		{
			if (mode == Mode::Classic)
			{
				cachedMin[m] = cachedMax[m] = TableHeight * 0.5f;
			}
			else
			{
				// Lowest focus: table top touches the screen top.
				// Highest focus: table bottom touches the screen bottom.
				cachedMin[m] = SolveFocus(mode, 1.0f, ScreenHeight);
				cachedMax[m] = SolveFocus(mode, 0.0f, -BottomMargin);
				if (cachedMin[m] > cachedMax[m]) // whole table already fits
					cachedMin[m] = cachedMax[m] = (cachedMin[m] + cachedMax[m]) * 0.5f;
			}
			cached[m] = true;
		}
		*minRow = cachedMin[m];
		*maxRow = cachedMax[m];
	}

	void Reset(State *state, Mode mode)
	{
		float lo, hi;
		FocusLimits(mode, &lo, &hi);
		state->focusRow = hi; // start on the flippers and plunger
		state->lastBallRow = -1.0f;
	}

	void Update(State *state, Mode mode, float ballRow)
	{
		float lo, hi;
		FocusLimits(mode, &lo, &hi);

		float target;
		if (ballRow < 0.0f)
		{
			target = hi;
			state->lastBallRow = -1.0f;
		}
		else
		{
			float velocity = state->lastBallRow >= 0.0f ? ballRow - state->lastBallRow : 0.0f;
			state->lastBallRow = ballRow;
			target = ballRow + velocity * LookaheadFrames;
		}

		target = Clamp(target, lo, hi);
		float k = target > state->focusRow ? SmoothingDown : SmoothingUp;
		state->focusRow = Clamp(state->focusRow + (target - state->focusRow) * k, lo, hi);
	}

	void Compute(Mode mode, const State &state, float shakeX, float shakeY, View *out)
	{
		Mtx_Identity(&out->modelView);

		if (mode == Mode::Tilted)
		{
			Mtx_PerspTilt(&out->projection, C3D_AngleFromDegrees(TiltFovDegrees), C3D_AspectRatioTop, 1.0f, 4000.0f, false);

			// Focus row at the origin, table leaned back about it, pushed away from the camera.
			float focusFromBottom = TableHeight - state.focusRow;
			Mtx_Translate(&out->modelView, shakeX, -shakeY, -TiltDistance(), true);
			Mtx_RotateX(&out->modelView, -C3D_AngleFromDegrees(TiltDegrees), true);
			Mtx_Translate(&out->modelView, -TableWidth * 0.5f, -focusFromBottom, 0.0f, true);
			Mtx_Scale(&out->modelView, TableWidth, TableHeight, 1.0f);
			return;
		}

		Mtx_OrthoTilt(&out->projection, 0.0f, ScreenWidth, 0.0f, ScreenHeight, 0.1f, 1.0f, true);

		float h = mode == Mode::Follow ? FollowTableHeight : ScreenHeight;
		float w = h * (TableWidth / TableHeight);
		float pxScale = h / TableHeight; // screen pixels per game pixel
		float x = (ScreenWidth - w) * 0.5f;
		// Put the focus row on the screen's vertical center.
		float y = ScreenHeight * 0.5f - h * (1.0f - state.focusRow / TableHeight);
		if (mode == Mode::Classic)
			y = 0.0f;

		Mtx_Translate(&out->modelView, x + shakeX * pxScale, y - shakeY * pxScale, 0.5f, true);
		Mtx_Scale(&out->modelView, w, h, 1.0f);
	}
}
