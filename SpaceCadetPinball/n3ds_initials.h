#pragma once

#include <citro3d.h>
#include <cstdint>

// Arcade-style high score entry: three letters on the bottom screen in the
// game's dot-matrix font, picked with the D-pad. The top screen dims the table.
//   Up/Down     change the letter (hold to scroll)
//   Left/Right  move between letters
//   A           next letter, or confirm on the last one
//   B           previous letter
//   Start       confirm
class n3ds_initials
{
public:
	static bool Active();
	static void Begin(int score, int rank, const char *playerLabel, const char *startInitials);

	// Returns true once confirmed; the result is then in Initials().
	static bool Update(uint32_t buttonsDown, uint32_t buttonsHeld);

	static int Score();
	static const char *Initials();

	static void DrawTop(C3D_RenderTarget *target);
	static void DrawBottom(C3D_RenderTarget *target);
};
