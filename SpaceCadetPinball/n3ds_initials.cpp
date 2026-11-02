#include "n3ds_initials.h"

#include <citro2d.h>
#include <cctype>
#include <cstdio>
#include <cstring>

static const char Charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789. ";
static const int CharsetSize = sizeof Charset - 1;

// Hold Up/Down: first repeat after RepeatDelay frames, then every RepeatRate.
static const int RepeatDelay = 18;
static const int RepeatRate = 4;

static bool active = false;
static int entryScore = 0;
static int entryRank = 0;
static char label[32] = {};
static int letters[3] = {};
static int slot = 0;
static int holdFrames = 0;
static char result[4] = {};
static C2D_TextBuf textBuf = nullptr;

static const u32 ColorDim = C2D_Color32(0x00, 0x00, 0x00, 0xA8);
static const u32 ColorBox = C2D_Color32(0x10, 0x14, 0x18, 0xF4);
static const u32 ColorRule = C2D_Color32(0x2C, 0x32, 0x40, 0xFF);
static const u32 ColorHeader = C2D_Color32(0x8F, 0xB8, 0xFF, 0xFF);
static const u32 ColorText = C2D_Color32(0xE8, 0xE8, 0xF0, 0xFF);
static const u32 ColorMuted = C2D_Color32(0x7A, 0x80, 0x90, 0xFF);
static const u32 ColorWhite = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);

static int IndexOf(char c)
{
	c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
	const char *p = strchr(Charset, c);
	return p && c ? static_cast<int>(p - Charset) : 0;
}

bool n3ds_initials::Active()
{
	return active;
}

void n3ds_initials::Begin(int score, int rank, const char *playerLabel, const char *startInitials)
{
	active = true;
	entryScore = score;
	entryRank = rank;
	snprintf(label, sizeof label, "%s", playerLabel);
	for (char *c = label; *c; c++)
		*c = static_cast<char>(toupper(static_cast<unsigned char>(*c)));

	size_t len = startInitials ? strlen(startInitials) : 0;
	for (int i = 0; i < 3; i++)
		letters[i] = i < static_cast<int>(len) ? IndexOf(startInitials[i]) : 0;
	slot = 0;
	holdFrames = 0;
}

int n3ds_initials::Score()
{
	return entryScore;
}

const char *n3ds_initials::Initials()
{
	return result;
}

static void Confirm()
{
	for (int i = 0; i < 3; i++)
		result[i] = Charset[letters[i]];
	result[3] = 0;
	// Trailing spaces don't count ("AL " -> "AL"); all blank becomes "???".
	for (int i = 2; i >= 0 && result[i] == ' '; i--)
		result[i] = 0;
	if (!result[0])
		strcpy(result, "???");
	active = false;
}

bool n3ds_initials::Update(uint32_t down, uint32_t held)
{
	if (!active)
		return false;

	// Up/Down with hold-to-repeat
	int step = 0;
	uint32_t vertical = held & (KEY_UP | KEY_DOWN);
	if (vertical)
	{
		bool fire = (down & (KEY_UP | KEY_DOWN)) ||
		            (holdFrames >= RepeatDelay && (holdFrames - RepeatDelay) % RepeatRate == 0);
		if (fire)
			step = (vertical & KEY_UP) ? 1 : -1;
		holdFrames++;
	}
	else
	{
		holdFrames = 0;
	}
	if (step)
		letters[slot] = (letters[slot] + step + CharsetSize) % CharsetSize;

	if (down & KEY_LEFT)
		slot = slot > 0 ? slot - 1 : 0;
	if (down & KEY_RIGHT)
		slot = slot < 2 ? slot + 1 : 2;
	if (down & KEY_B)
		slot = slot > 0 ? slot - 1 : 0;

	if (down & KEY_START)
	{
		Confirm();
		return true;
	}
	if (down & KEY_A)
	{
		if (slot < 2)
		{
			slot++;
		}
		else
		{
			Confirm();
			return true;
		}
	}
	return false;
}

static void Text(const char *str, float x, float y, float scale, u32 color)
{
	C2D_Text text;
	C2D_TextParse(&text, textBuf, str);
	C2D_TextOptimize(&text);
	C2D_DrawText(&text, C2D_WithColor | C2D_AlignCenter, x, y, 0.5f, scale, scale, color);
}

void n3ds_initials::Draw(C3D_RenderTarget *topTarget)
{
	if (!active)
		return;
	if (!textBuf)
		textBuf = C2D_TextBufNew(256);

	C2D_TextBufClear(textBuf);
	C2D_Prepare();
	C2D_SceneBegin(topTarget);

	const float cx = 200.0f;
	const float boxX = 80.0f, boxY = 36.0f, boxW = 240.0f, boxH = 168.0f;

	// Dim the frozen table, then the panel
	C2D_DrawRectSolid(0.0f, 0.0f, 0.5f, 400.0f, 240.0f, ColorDim);
	C2D_DrawRectSolid(boxX, boxY, 0.5f, boxW, boxH, ColorBox);
	C2D_DrawRectSolid(boxX, boxY, 0.5f, boxW, 1.0f, ColorRule);
	C2D_DrawRectSolid(boxX, boxY + boxH - 1.0f, 0.5f, boxW, 1.0f, ColorRule);
	C2D_DrawRectSolid(boxX, boxY, 0.5f, 1.0f, boxH, ColorRule);
	C2D_DrawRectSolid(boxX + boxW - 1.0f, boxY, 0.5f, 1.0f, boxH, ColorRule);

	Text("NEW HIGH SCORE", cx, boxY + 8.0f, 0.6f, ColorHeader);

	char line[64];
	snprintf(line, sizeof line, "%s  \xC2\xB7  RANK %d", label, entryRank);
	Text(line, cx, boxY + 30.0f, 0.42f, ColorMuted);

	char digits[16], score[24];
	snprintf(digits, sizeof digits, "%d", entryScore);
	size_t len = strlen(digits), o = 0;
	for (size_t i = 0; i < len; i++)
	{
		if (i > 0 && (len - i) % 3 == 0)
			score[o++] = ',';
		score[o++] = digits[i];
	}
	score[o] = 0;
	Text(score, cx, boxY + 46.0f, 0.7f, ColorWhite);

	// Three letter slots
	const float slotW = 36.0f, gap = 14.0f;
	const float slotsX = cx - (3 * slotW + 2 * gap) * 0.5f;
	const float letterY = boxY + 90.0f;
	for (int i = 0; i < 3; i++)
	{
		float sx = slotsX + i * (slotW + gap);
		float mid = sx + slotW * 0.5f;
		bool selected = i == slot;

		char ch[2] = {Charset[letters[i]], 0};
		Text(ch, mid, letterY, 1.1f, selected ? ColorWhite : ColorText);
		C2D_DrawRectSolid(sx, letterY + 36.0f, 0.5f, slotW, 2.0f, selected ? ColorHeader : ColorRule);

		if (selected)
		{
			// Up arrow above, down arrow below
			float ay = letterY - 6.0f;
			C2D_DrawTriangle(mid, ay - 7.0f, ColorHeader, mid - 7.0f, ay, ColorHeader, mid + 7.0f, ay, ColorHeader, 0.5f);
			float by = letterY + 44.0f;
			C2D_DrawTriangle(mid - 7.0f, by, ColorHeader, mid + 7.0f, by, ColorHeader, mid, by + 7.0f, ColorHeader, 0.5f);
		}
	}

	// U+E006 / U+E000 / U+E001: the system font's D-pad, A and B button glyphs
	Text("\uE006 Letter     \uE000 Next     \uE001 Back", cx, boxY + boxH - 18.0f, 0.42f, ColorMuted);
}
