#pragma once

#include <citro3d.h>

struct high_score_struct;

// Bottom-screen text drawn with citro2d (system font), next to the game's own
// score panel: the top-5 high scores and the current camera mode.
class n3ds_ui
{
public:
	static constexpr float ScreenWidth = 320.0f;
	static constexpr float ScreenHeight = 240.0f;

	static void Initialize();
	static void Dispose();

	// x: left edge of the text column; everything right of it belongs to the UI.
	static void DrawBottom(C3D_RenderTarget *target, float x, const high_score_struct *table,
	                       const char *cameraMode);
};
