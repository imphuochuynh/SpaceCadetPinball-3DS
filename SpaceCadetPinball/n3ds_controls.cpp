#include "pch.h"
#include "n3ds_controls.h"

#include <3ds.h>

#include "options.h"

// Bits in the libctru key mask (KEY_A == BIT(0) and so on).
static const int DefaultBits[n3ds_controls::ActionCount] =
{
	9, // L         left flipper
	8, // R         right flipper
	0, // A         plunger
	5, // D-pad L   nudge left
	4, // D-pad R   nudge right
	6, // D-pad U   nudge up
};

// Stored under new names: the game's own "... key" settings hold PC keycodes.
static const char *SettingNames[n3ds_controls::ActionCount] =
{
	"3DS Left Flipper", "3DS Right Flipper", "3DS Plunger",
	"3DS Nudge Left", "3DS Nudge Right", "3DS Nudge Up",
};

static const char *ActionNames[n3ds_controls::ActionCount] =
{
	"Left Flipper", "Right Flipper", "Plunger",
	"Nudge Left", "Nudge Right", "Nudge Up",
};

static int bits[n3ds_controls::ActionCount];

static const char *BitName(int bit)
{
	switch (bit)
	{
	case 0: return "A";
	case 1: return "B";
	case 4: return "D-Pad Right";
	case 5: return "D-Pad Left";
	case 6: return "D-Pad Up";
	case 7: return "D-Pad Down";
	case 8: return "R";
	case 9: return "L";
	case 10: return "X";
	case 11: return "Y";
	case 14: return "ZL";
	case 15: return "ZR";
	case 24: return "C-Stick Right";
	case 25: return "C-Stick Left";
	case 26: return "C-Stick Up";
	case 27: return "C-Stick Down";
	case 28: return "C-Pad Right";
	case 29: return "C-Pad Left";
	case 30: return "C-Pad Up";
	case 31: return "C-Pad Down";
	default: return "?";
	}
}

uint32_t n3ds_controls::ReservedMask()
{
	return KEY_START | KEY_SELECT | KEY_TOUCH;
}

static bool Allowed(int bit)
{
	if (bit < 0 || bit > 31)
		return false;
	uint32_t mask = 1u << bit;
	return !(mask & n3ds_controls::ReservedMask()) && BitName(bit)[0] != '?';
}

void n3ds_controls::Load()
{
	for (int i = 0; i < ActionCount; i++)
	{
		int bit = options::get_int(SettingNames[i], DefaultBits[i]);
		bits[i] = Allowed(bit) ? bit : DefaultBits[i];
	}
	// A hand-edited settings file could bind one button twice; fall back.
	for (int i = 0; i < ActionCount; i++)
		for (int j = i + 1; j < ActionCount; j++)
			if (bits[i] == bits[j])
			{
				ResetDefaults();
				return;
			}
}

void n3ds_controls::Save()
{
	for (int i = 0; i < ActionCount; i++)
		options::set_int(SettingNames[i], bits[i]);
}

void n3ds_controls::ResetDefaults()
{
	for (int i = 0; i < ActionCount; i++)
		bits[i] = DefaultBits[i];
}

uint32_t n3ds_controls::Mask(Action action)
{
	return 1u << bits[action];
}

const char *n3ds_controls::ActionName(Action action)
{
	return ActionNames[action];
}

const char *n3ds_controls::KeyName(Action action)
{
	return BitName(bits[action]);
}

bool n3ds_controls::AssignFromButtons(Action action, uint32_t buttons)
{
	for (int bit = 0; bit < 32; bit++)
	{
		if (!(buttons & (1u << bit)) || !Allowed(bit))
			continue;

		for (int other = 0; other < ActionCount; other++)
			if (other != action && bits[other] == bit)
				bits[other] = bits[action]; // swap
		bits[action] = bit;
		return true;
	}
	return false;
}
