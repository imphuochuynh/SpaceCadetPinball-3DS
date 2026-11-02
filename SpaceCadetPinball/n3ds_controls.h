#pragma once

#include <cstdint>

// Remappable game buttons. Any button except Start, Select and the touch
// screen can be used; each action has exactly one button.
class n3ds_controls
{
public:
	enum Action
	{
		LeftFlipper,
		RightFlipper,
		Plunger,
		NudgeLeft,
		NudgeRight,
		NudgeUp,
		ActionCount
	};

	static void Load();
	static void Save();
	static void ResetDefaults();

	static uint32_t Mask(Action action);
	static const char *ActionName(Action action);
	static const char *KeyName(Action action);

	// Bind the first allowed button in `buttons` to an action. If another
	// action already uses it, the two swap. Returns false if none was allowed.
	static bool AssignFromButtons(Action action, uint32_t buttons);

	// Buttons that can never be bound (they open the menu / switch camera).
	static uint32_t ReservedMask();
};
