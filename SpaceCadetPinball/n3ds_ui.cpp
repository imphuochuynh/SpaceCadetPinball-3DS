#include "pch.h"
#include "n3ds_ui.h"

#include <cctype>
#include <citro2d.h>

#include "high_score.h"
#include "n3ds_font.h"

static const u32 ColorRule = C2D_Color32(0x2C, 0x32, 0x40, 0xFF);
static const u32 ColorIcon = C2D_Color32(0x78, 0x58, 0xDC, 0xFF); // the font's lit-dot purple

void n3ds_ui::Initialize()
{
}

void n3ds_ui::Dispose()
{
}

static void FormatScore(char *out, size_t size, int score)
{
	char digits[16];
	snprintf(digits, sizeof digits, "%d", score);
	size_t len = strlen(digits), o = 0;
	for (size_t i = 0; i < len && o + 2 < size; i++)
	{
		if (i > 0 && (len - i) % 3 == 0)
			out[o++] = ',';
		out[o++] = digits[i];
	}
	out[o] = 0;
}

void n3ds_ui::DrawBottom(C3D_RenderTarget *target, float x, const high_score_struct *table,
                         const char *cameraMode, const char *perfLine)
{
	C2D_Prepare();
	C2D_SceneBegin(target);

	const float left = x + 6.0f;
	const float right = ScreenWidth - 6.0f;
	const float width = right - left;

	n3ds_font::Draw("TOP 3", left, 6.0f);
	C2D_DrawRectSolid(left, 30.0f, 0.5f, width, 1.0f, ColorRule);

	// Two lines per entry: rank + initials, then the score right-aligned.
	// The full top 5 is on the menu's High Scores page.
	float y = 36.0f;
	for (int i = 0; i < 3; i++, y += 48.0f)
	{
		bool empty = table[i].Score <= 0;

		char rank[4];
		snprintf(rank, sizeof rank, "%d", i + 1);
		n3ds_font::Draw(rank, left, y, 1.0f, n3ds_font::Left, 0.0f, 0.6f);
		n3ds_font::Draw(empty || !table[i].Name[0] ? "---" : table[i].Name, left + 16.0f, y, 1.0f,
		                n3ds_font::Left, width - 16.0f, empty ? 0.6f : 1.0f);

		char score[24];
		if (empty)
			strcpy(score, "---");
		else
			FormatScore(score, sizeof score, table[i].Score);
		n3ds_font::Draw(score, right, y + 22.0f, 1.0f, n3ds_font::Right, width, empty ? 0.6f : 1.0f);

		if (i < 2)
			C2D_DrawRectSolid(left, y + 44.0f, 0.5f, width, 1.0f, ColorRule);
	}

	// Footer: current view (Select switches it) and the menu icon (tap or Start)
	C2D_DrawRectSolid(left, 186.0f, 0.5f, width, 1.0f, ColorRule);

	char view[16];
	snprintf(view, sizeof view, "%s", cameraMode);
	for (char *c = view; *c; c++)
		*c = static_cast<char>(toupper(static_cast<unsigned char>(*c)));
	n3ds_font::Draw(view, left, 192.0f, 1.0f, n3ds_font::Left, width, 0.6f);
	if (perfLine)
		n3ds_font::Draw(perfLine, left, 214.0f, 1.0f, n3ds_font::Left, width - 22.0f, 1.0f);
	else
		n3ds_font::Draw("SELECT", left, 214.0f, 1.0f, n3ds_font::Left, width - 22.0f, 0.35f);

	for (int k = 0; k < 3; k++)
		C2D_DrawRectSolid(right - 16.0f, 218.0f + k * 6.0f, 0.5f, 16.0f, 3.0f, ColorIcon);
}
