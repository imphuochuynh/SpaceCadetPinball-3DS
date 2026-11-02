#include "n3ds_camera.h"

namespace n3ds_camera
{
	// ---- Tuning -----------------------------------------------------------------

	// Follow: table drawn this tall on the 240px screen (1.75x the Classic view),
	// which still fits the full table width on the 400px screen.
	constexpr float FollowTableHeight = 420.0f;

	// Camera motion is a critically damped spring: it eases toward the target
	// without overshoot or sudden speed changes. Smaller = snappier.
	constexpr float SmoothTime = 0.16f; // seconds
	constexpr float FrameTime = 1.0f / 60.0f;

	// Look ahead of a falling ball so the flippers come into view early.
	// Velocity is smoothed first; raw frame-to-frame deltas are too noisy.
	constexpr float LookaheadFrames = 12.0f;
	constexpr float VelocitySmoothing = 0.15f;

	// ---- Helpers ----------------------------------------------------------------

	static float Clamp(float v, float lo, float hi)
	{
		return v < lo ? lo : (v > hi ? hi : v);
	}

	static float DrawnHeight(Mode mode)
	{
		return mode == Mode::Follow ? FollowTableHeight : ScreenHeight;
	}

	// ---- Public -----------------------------------------------------------------

	const char *ModeName(Mode mode)
	{
		switch (mode)
		{
		case Mode::Classic: return "Classic";
		case Mode::Follow: return "Follow";
		default: return "";
		}
	}

	Mode NextMode(Mode mode)
	{
		return static_cast<Mode>((static_cast<int>(mode) + 1) % static_cast<int>(Mode::Count));
	}

	void FocusLimits(Mode mode, float *minRow, float *maxRow)
	{
		// Half the screen height, in table rows. Centering the camera any closer
		// to an edge than this would show past the end of the table.
		float halfView = (ScreenHeight * 0.5f) * (TableHeight / DrawnHeight(mode));
		if (halfView >= TableHeight * 0.5f)
		{
			*minRow = *maxRow = TableHeight * 0.5f; // whole table fits
			return;
		}
		*minRow = halfView;
		*maxRow = TableHeight - halfView; // table bottom on the screen bottom: flippers in view
	}

	void Reset(State *state, Mode mode)
	{
		float lo, hi;
		FocusLimits(mode, &lo, &hi);
		state->focusRow = hi; // start on the flippers and plunger
		state->focusVelocity = 0.0f;
		state->lastBallRow = -1.0f;
		state->ballVelocity = 0.0f;
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
			state->ballVelocity = 0.0f;
		}
		else
		{
			if (state->lastBallRow >= 0.0f)
			{
				float raw = ballRow - state->lastBallRow;
				state->ballVelocity += (raw - state->ballVelocity) * VelocitySmoothing;
			}
			state->lastBallRow = ballRow;
			// Only lead a falling ball (positive = toward the flippers)
			float lead = state->ballVelocity > 0.0f ? state->ballVelocity * LookaheadFrames : 0.0f;
			target = ballRow + lead;
		}
		target = Clamp(target, lo, hi);

		// Critically damped spring step (as in Game Programming Gems 4, 1.10)
		float omega = 2.0f / SmoothTime;
		float x = omega * FrameTime;
		float decay = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
		float change = state->focusRow - target;
		float temp = (state->focusVelocity + omega * change) * FrameTime;
		state->focusVelocity = (state->focusVelocity - omega * temp) * decay;
		float next = target + (change + temp) * decay;

		if (next < lo || next > hi)
		{
			next = Clamp(next, lo, hi);
			state->focusVelocity = 0.0f; // stop at the table edge instead of pushing into it
		}
		state->focusRow = next;
	}

	void Compute(Mode mode, const State &state, float shakeX, float shakeY, View *out)
	{
		Mtx_OrthoTilt(&out->projection, 0.0f, ScreenWidth, 0.0f, ScreenHeight, 0.1f, 1.0f, true);
		Mtx_Identity(&out->modelView);

		float h = DrawnHeight(mode);
		float w = h * (TableWidth / TableHeight);
		float pxScale = h / TableHeight; // screen pixels per game pixel
		float x = (ScreenWidth - w) * 0.5f;
		// Put the focus row on the screen's vertical center (y runs up).
		float y = mode == Mode::Classic ? 0.0f : ScreenHeight * 0.5f - h * (1.0f - state.focusRow / TableHeight);

		Mtx_Translate(&out->modelView, x + shakeX * pxScale, y - shakeY * pxScale, 0.5f, true);
		Mtx_Scale(&out->modelView, w, h, 1.0f);
	}
}
