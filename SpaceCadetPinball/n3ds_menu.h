#pragma once

#include <citro3d.h>
#include <cstdint>

// Pause menu on the bottom screen. Works with touch, or D-pad + A/B.
// Start opens it (and pauses the game); Start or B on the main page closes it.
class n3ds_menu
{
public:
	enum class Command
	{
		None,
		Close,         // resume play
		NewGame,       // close and start a new game
		ToggleCamera,  // stays open
		ToggleDemo,    // close and start/stop demo
		Quit,
	};

	static bool IsOpen();
	static void Open();

	// Handles input while open. Settings the menu owns (sound, music,
	// players, controls, high score reset) are applied here directly.
	static Command Update(uint32_t buttonsDown, uint32_t buttonsHeld);

	static void DrawTop(C3D_RenderTarget *target);
	static void DrawBottom(C3D_RenderTarget *target, const char *cameraName, bool demoActive);
};
