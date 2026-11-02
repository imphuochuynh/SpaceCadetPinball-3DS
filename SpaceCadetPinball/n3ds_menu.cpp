#include "pch.h"
#include "n3ds_menu.h"

#include <3ds.h>
#include <citro2d.h>

#include "high_score.h"
#include "n3ds_controls.h"
#include "n3ds_font.h"
#include "options.h"
#include "pb.h"

namespace
{
	enum class Page { Main, Controls, ConfirmReset, ConfirmQuit };

	enum ButtonId
	{
		// Main
		BtnResume, BtnNewGame, BtnPlayers, BtnCamera, BtnSound, BtnMusic,
		BtnControls, BtnDemo, BtnResetScores, BtnQuit,
		// Controls (one per action, then these)
		BtnAction0 = 100,
		BtnDefaults = 200, BtnBack,
		// Confirm pages
		BtnCancel = 300, BtnConfirm,
	};

	struct Button
	{
		int id;
		float x, y, w, h;
	};

	constexpr int MaxButtons = 12;

	struct Layout
	{
		Button buttons[MaxButtons];
		int count = 0;

		void Add(int id, float x, float y, float w, float h)
		{
			buttons[count++] = {id, x, y, w, h};
		}

		int IndexOf(int id) const
		{
			for (int i = 0; i < count; i++)
				if (buttons[i].id == id)
					return i;
			return 0;
		}
	};

	bool open = false;
	Page page = Page::Main;
	int highlight = 0;          // index into the current page's layout
	int capturing = -1;         // controls page: action waiting for a button
	bool playersChanged = false;

	const u32 ColorBg = C2D_Color32(0x10, 0x14, 0x18, 0xFF);
	const u32 ColorButton = C2D_Color32(0x1B, 0x21, 0x30, 0xFF);
	const u32 ColorButtonHot = C2D_Color32(0x24, 0x30, 0x49, 0xFF);
	const u32 ColorAccent = C2D_Color32(0x8F, 0xB8, 0xFF, 0xFF);
	const u32 ColorRule = C2D_Color32(0x2C, 0x32, 0x40, 0xFF);
	const u32 ColorDanger = C2D_Color32(0xFF, 0x50, 0x50, 0xFF);
	const u32 ColorChanged = C2D_Color32(0xFF, 0xD2, 0x4A, 0xFF);
	const u32 ColorDim = C2D_Color32(0x00, 0x00, 0x00, 0xA8);

	Layout BuildLayout(Page p)
	{
		Layout l;
		switch (p)
		{
		case Page::Main:
		{
			// 2 columns x 5 rows of big touch targets
			const float m = 8.0f, gap = 6.0f, w = (320.0f - 2 * m - gap) / 2, h = 40.0f;
			const int ids[10] = {BtnResume, BtnNewGame, BtnPlayers, BtnCamera, BtnSound,
			                     BtnMusic, BtnControls, BtnDemo, BtnResetScores, BtnQuit};
			for (int i = 0; i < 10; i++)
				l.Add(ids[i], m + (i % 2) * (w + gap), m + (i / 2) * (h + gap), w, h);
			break;
		}
		case Page::Controls:
			for (int i = 0; i < n3ds_controls::ActionCount; i++)
				l.Add(BtnAction0 + i, 8.0f, 30.0f + i * 28.0f, 304.0f, 25.0f);
			l.Add(BtnDefaults, 8.0f, 202.0f, 149.0f, 32.0f);
			l.Add(BtnBack, 163.0f, 202.0f, 149.0f, 32.0f);
			break;
		case Page::ConfirmReset:
		case Page::ConfirmQuit:
			l.Add(BtnCancel, 24.0f, 150.0f, 130.0f, 40.0f);
			l.Add(BtnConfirm, 166.0f, 150.0f, 130.0f, 40.0f);
			break;
		}
		return l;
	}

	void GoTo(Page p, int highlightId)
	{
		page = p;
		capturing = -1;
		highlight = BuildLayout(p).IndexOf(highlightId);
	}

	// Move the highlight to the nearest button in a direction.
	void Navigate(const Layout &l, float dx, float dy)
	{
		const Button &from = l.buttons[highlight];
		float fx = from.x + from.w / 2, fy = from.y + from.h / 2;
		int best = -1;
		float bestScore = 1e9f;
		for (int i = 0; i < l.count; i++)
		{
			if (i == highlight)
				continue;
			const Button &b = l.buttons[i];
			float vx = b.x + b.w / 2 - fx, vy = b.y + b.h / 2 - fy;
			float along = vx * dx + vy * dy;
			if (along <= 1.0f)
				continue;
			float across = fabsf(vx * dy - vy * dx);
			float score = along + across * 2.0f;
			if (score < bestScore)
			{
				bestScore = score;
				best = i;
			}
		}
		if (best >= 0)
			highlight = best;
	}

	void SaveSettings()
	{
		options::uninit(); // copies sound/music/players into the settings
		n3ds_controls::Save();
		options::save();
	}

	n3ds_menu::Command CloseWith(n3ds_menu::Command command)
	{
		open = false;
		capturing = -1;
		SaveSettings();
		if (playersChanged && command == n3ds_menu::Command::Close)
			command = n3ds_menu::Command::NewGame;
		playersChanged = false;
		return command;
	}

	n3ds_menu::Command Activate(int id)
	{
		using C = n3ds_menu::Command;
		switch (id)
		{
		case BtnResume: return CloseWith(C::Close);
		case BtnNewGame:
			playersChanged = false;
			return CloseWith(C::NewGame);
		case BtnPlayers:
		{
			int players = options::Options.Players % 4 + 1;
			options::toggle(static_cast<Menu1>(static_cast<int>(Menu1::OnePlayer) + players - 1));
			playersChanged = true;
			return C::None;
		}
		case BtnCamera: return C::ToggleCamera;
		case BtnSound: options::toggle(Menu1::Sounds); return C::None;
		case BtnMusic: options::toggle(Menu1::Music); return C::None;
		case BtnControls: GoTo(Page::Controls, BtnAction0); return C::None;
		case BtnDemo:
			playersChanged = false;
			return CloseWith(C::ToggleDemo);
		case BtnResetScores: GoTo(Page::ConfirmReset, BtnCancel); return C::None;
		case BtnQuit: GoTo(Page::ConfirmQuit, BtnCancel); return C::None;

		case BtnDefaults: n3ds_controls::ResetDefaults(); return C::None;
		case BtnBack: GoTo(Page::Main, BtnControls); return C::None;

		case BtnCancel:
			GoTo(Page::Main, page == Page::ConfirmQuit ? BtnQuit : BtnResetScores);
			return C::None;
		case BtnConfirm:
			if (page == Page::ConfirmQuit)
			{
				open = false;
				SaveSettings();
				return C::Quit;
			}
			high_score::clear_table(pb::highscore_table);
			high_score::write(pb::highscore_table);
			options::save();
			GoTo(Page::Main, BtnResetScores);
			return C::None;

		default:
			if (id >= BtnAction0 && id < BtnAction0 + n3ds_controls::ActionCount)
				capturing = id - BtnAction0;
			return C::None;
		}
	}

	n3ds_menu::Command Back()
	{
		switch (page)
		{
		case Page::Main: return CloseWith(n3ds_menu::Command::Close);
		case Page::Controls: GoTo(Page::Main, BtnControls); break;
		case Page::ConfirmReset: GoTo(Page::Main, BtnResetScores); break;
		case Page::ConfirmQuit: GoTo(Page::Main, BtnQuit); break;
		}
		return n3ds_menu::Command::None;
	}

	// ---- Drawing --------------------------------------------------------------

	void Frame(float x, float y, float w, float h, u32 color, float t)
	{
		C2D_DrawRectSolid(x, y, 0.5f, w, t, color);
		C2D_DrawRectSolid(x, y + h - t, 0.5f, w, t, color);
		C2D_DrawRectSolid(x, y, 0.5f, t, h, color);
		C2D_DrawRectSolid(x + w - t, y, 0.5f, t, h, color);
	}

	// tint: 0 for the font's own colors, or a color to blend the label toward
	void DrawButton(const Button &b, bool hot, const char *label, u32 tint = 0)
	{
		C2D_DrawRectSolid(b.x, b.y, 0.5f, b.w, b.h, hot ? ColorButtonHot : ColorButton);
		Frame(b.x, b.y, b.w, b.h, hot ? ColorAccent : ColorRule, hot ? 2.0f : 1.0f);
		n3ds_font::Draw(label, b.x + b.w / 2, b.y + (b.h - n3ds_font::Height) / 2, 1.0f, n3ds_font::Center,
		                b.w - 12.0f, 1.0f, tint, tint ? 0.6f : 0.0f);
	}
}

bool n3ds_menu::IsOpen()
{
	return open;
}

void n3ds_menu::Open()
{
	open = true;
	playersChanged = false;
	GoTo(Page::Main, BtnResume);
}

n3ds_menu::Command n3ds_menu::Update(uint32_t down, uint32_t held)
{
	if (!open)
		return Command::None;

	Layout l = BuildLayout(page);

	// Waiting for a button to bind: Start cancels, a tap cancels.
	if (capturing >= 0)
	{
		if (down & (KEY_START | KEY_TOUCH))
		{
			capturing = -1;
			return Command::None;
		}
		if (n3ds_controls::AssignFromButtons(static_cast<n3ds_controls::Action>(capturing), down))
			capturing = -1;
		return Command::None;
	}

	if (down & KEY_TOUCH)
	{
		touchPosition touch;
		hidTouchRead(&touch);
		for (int i = 0; i < l.count; i++)
		{
			const Button &b = l.buttons[i];
			if (touch.px >= b.x && touch.px < b.x + b.w && touch.py >= b.y && touch.py < b.y + b.h)
			{
				highlight = i;
				return Activate(b.id);
			}
		}
		return Command::None;
	}

	if (down & KEY_START)
		return CloseWith(Command::Close);
	if (down & KEY_B)
		return Back();
	if (down & KEY_A)
		return Activate(l.buttons[highlight].id);

	if (down & KEY_UP) Navigate(l, 0, -1);
	if (down & KEY_DOWN) Navigate(l, 0, 1);
	if (down & KEY_LEFT) Navigate(l, -1, 0);
	if (down & KEY_RIGHT) Navigate(l, 1, 0);
	return Command::None;
}

void n3ds_menu::DrawTop(C3D_RenderTarget *target)
{
	if (!open)
		return;
	C2D_Prepare();
	C2D_SceneBegin(target);

	C2D_DrawRectSolid(0, 0, 0.5f, 400, 240, ColorDim);
	n3ds_font::Draw("PAUSED", 200, 98, 2.0f, n3ds_font::Center);
}

void n3ds_menu::DrawBottom(C3D_RenderTarget *target, const char *cameraName, bool demoActive)
{
	if (!open)
		return;
	C2D_Prepare();
	C2D_SceneBegin(target);
	C2D_DrawRectSolid(0, 0, 0.5f, 320, 240, ColorBg);

	Layout l = BuildLayout(page);
	char label[64];

	switch (page)
	{
	case Page::Main:
		for (int i = 0; i < l.count; i++)
		{
			const Button &b = l.buttons[i];
			u32 tint = 0;
			switch (b.id)
			{
			case BtnResume: snprintf(label, sizeof label, "Resume"); break;
			case BtnNewGame: snprintf(label, sizeof label, "New Game"); break;
			case BtnPlayers:
				snprintf(label, sizeof label, "Players: %d", options::Options.Players);
				if (playersChanged)
					tint = ColorChanged; // takes effect as a new game on resume
				break;
			case BtnCamera: snprintf(label, sizeof label, "View: %s", cameraName); break;
			case BtnSound: snprintf(label, sizeof label, "Sound: %s", options::Options.Sounds ? "On" : "Off"); break;
			case BtnMusic: snprintf(label, sizeof label, "Music: %s", options::Options.Music ? "On" : "Off"); break;
			case BtnControls: snprintf(label, sizeof label, "Controls"); break;
			case BtnDemo: snprintf(label, sizeof label, demoActive ? "Stop Demo" : "Demo"); break;
			case BtnResetScores: snprintf(label, sizeof label, "Reset Scores"); break;
			case BtnQuit: snprintf(label, sizeof label, "Quit"); tint = ColorDanger; break;
			}
			DrawButton(b, i == highlight, label, tint);
		}
		break;

	case Page::Controls:
		n3ds_font::Draw("CONTROLS", 12, 4, 1.0f);
		n3ds_font::Draw(capturing >= 0 ? "START: CANCEL" : "TAP TO CHANGE", 308, 4, 1.0f, n3ds_font::Right, 0.0f, 0.6f);
		for (int i = 0; i < l.count; i++)
		{
			const Button &b = l.buttons[i];
			bool hot = i == highlight;
			if (b.id >= BtnAction0 && b.id < BtnAction0 + n3ds_controls::ActionCount)
			{
				auto action = static_cast<n3ds_controls::Action>(b.id - BtnAction0);
				bool waiting = capturing == static_cast<int>(action);
				C2D_DrawRectSolid(b.x, b.y, 0.5f, b.w, b.h, hot || waiting ? ColorButtonHot : ColorButton);
				if (hot || waiting)
					Frame(b.x, b.y, b.w, b.h, ColorAccent, waiting ? 2.0f : 1.0f);
				float ty = b.y + (b.h - n3ds_font::Height) / 2;
				n3ds_font::Draw(n3ds_controls::ActionName(action), b.x + 8, ty, 1.0f);
				n3ds_font::Draw(waiting ? "Press a button" : n3ds_controls::KeyName(action),
				                b.x + b.w - 8, ty, 1.0f, n3ds_font::Right, 150.0f, waiting ? 1.0f : 0.6f,
				                waiting ? ColorChanged : 0, waiting ? 0.5f : 0.0f);
			}
			else
			{
				DrawButton(b, hot, b.id == BtnDefaults ? "Defaults" : "Back");
			}
		}
		break;

	case Page::ConfirmReset:
	case Page::ConfirmQuit:
	{
		bool quit = page == Page::ConfirmQuit;
		n3ds_font::Draw(quit ? "Quit Space Cadet?" : "Reset all high scores?", 160, 72, 1.0f, n3ds_font::Center, 300.0f);
		n3ds_font::Draw(quit ? "Scores and settings are saved" : "This can't be undone", 160, 104, 1.0f, n3ds_font::Center, 300.0f, 0.6f);
		for (int i = 0; i < l.count; i++)
		{
			const Button &b = l.buttons[i];
			bool confirm = b.id == BtnConfirm;
			DrawButton(b, i == highlight, confirm ? (quit ? "Quit" : "Reset") : "Cancel", confirm ? ColorDanger : 0);
		}
		break;
	}
	}
}
