#include "pch.h"
#include "n3ds_ui.h"

#include <citro2d.h>

#include "high_score.h"

static C2D_TextBuf textBuf = nullptr;

// Palette for the dark panel
static const u32 ColorHeader = C2D_Color32(0x8F, 0xB8, 0xFF, 0xFF);
static const u32 ColorName = C2D_Color32(0xE8, 0xE8, 0xF0, 0xFF);
static const u32 ColorScore = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
static const u32 ColorDim = C2D_Color32(0x7A, 0x80, 0x90, 0xFF);
static const u32 ColorRule = C2D_Color32(0x2C, 0x32, 0x40, 0xFF);
static const u32 ColorPaused = C2D_Color32(0xFF, 0xD2, 0x4A, 0xFF);

void n3ds_ui::Initialize()
{
	textBuf = C2D_TextBufNew(1024);
}

void n3ds_ui::Dispose()
{
	if (textBuf)
		C2D_TextBufDelete(textBuf);
	textBuf = nullptr;
}

static void FormatScore(char *out, size_t size, int score)
{
	char digits[16];
	snprintf(digits, sizeof digits, "%d", score);
	size_t len = strlen(digits), o = 0;
	for (size_t i = 0; i < len && o + 1 < size; i++)
	{
		if (i > 0 && (len - i) % 3 == 0 && o + 1 < size)
			out[o++] = ',';
		out[o++] = digits[i];
	}
	out[o] = 0;
}

// Draw text, shrinking it if it would be wider than maxWidth.
static void DrawText(const char *str, float x, float y, float scale, u32 color, float maxWidth, bool alignRight = false)
{
	C2D_Text text;
	C2D_TextParse(&text, textBuf, str);
	C2D_TextOptimize(&text);

	float w, h;
	C2D_TextGetDimensions(&text, scale, scale, &w, &h);
	float sx = scale;
	if (maxWidth > 0 && w > maxWidth)
		sx = scale * (maxWidth / w);

	u32 flags = C2D_WithColor | (alignRight ? C2D_AlignRight : 0);
	C2D_DrawText(&text, flags, x, y, 0.5f, sx, scale, color);
}

void n3ds_ui::DrawBottom(C3D_RenderTarget *target, float x, const high_score_struct *table,
                         const char *cameraMode, bool paused)
{
	C2D_TextBufClear(textBuf);
	C2D_Prepare();
	C2D_SceneBegin(target);

	const float left = x + 6.0f;
	const float right = ScreenWidth - 6.0f;
	const float width = right - left;

	DrawText("HIGH SCORES", left, 6.0f, 0.5f, ColorHeader, width);
	C2D_DrawRectSolid(left, 25.0f, 0.5f, width, 1.0f, ColorRule);

	// One row per score: rank, initials, score (right-aligned)
	float y = 34.0f;
	for (int i = 0; i < 5; i++, y += 32.0f)
	{
		char rank[4];
		snprintf(rank, sizeof rank, "%d", i + 1);
		DrawText(rank, left, y, 0.45f, ColorDim, 0);

		bool empty = table[i].Score <= 0;
		const char *name = empty || !table[i].Name[0] ? "---" : table[i].Name;
		DrawText(name, left + 11.0f, y, 0.5f, empty ? ColorDim : ColorName, 34.0f);

		char score[24];
		if (empty)
			strcpy(score, "---");
		else
			FormatScore(score, sizeof score, table[i].Score);
		DrawText(score, right, y, 0.5f, empty ? ColorDim : ColorScore, width - 50.0f, true);
	}

	C2D_DrawRectSolid(left, 201.0f, 0.5f, width, 1.0f, ColorRule);
	if (paused)
		DrawText("PAUSED", left, 204.0f, 0.5f, ColorPaused, width);

	char cam[48];
	snprintf(cam, sizeof cam, "SELECT  %s", cameraMode);
	DrawText(cam, left, 222.0f, 0.42f, ColorDim, width);
}
