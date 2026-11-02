#pragma once

#include <cstdint>

// Draws text with the game's own dot-matrix message font (the one used for
// "Awaiting Deployment"), packed into a GPU texture at startup. Falls back to
// the 3DS system font if the table data has no bitmap font.
//
// Call between C2D_Prepare/C2D_SceneBegin and the end of the frame.
class n3ds_font
{
public:
	enum Align { Left, Center, Right };

	static constexpr float Height = 22.0f; // glyph height at scale 1

	static void Initialize(); // after pb::init (needs the loaded font)
	static void Dispose();
	static void BeginFrame(); // once per frame, before any Draw

	static float Width(const char *text, float scale = 1.0f);

	// maxWidth > 0 squeezes the text horizontally to fit.
	// alpha dims it; tint (0xRRGGBBAA as C2D_Color32) blends toward a color.
	static void Draw(const char *text, float x, float y, float scale = 1.0f, Align align = Left,
	                 float maxWidth = 0.0f, float alpha = 1.0f, uint32_t tint = 0, float tintBlend = 0.0f);
};
