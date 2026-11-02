#include "n3ds_initials.h"

#include <citro2d.h>

#include "n3ds_font.h"
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

static const u32 ColorDim = C2D_Color32(0x00, 0x00, 0x00, 0xA8);
static const u32 ColorBox = C2D_Color32(0x10, 0x14, 0x18, 0xFF);
static const u32 ColorRule = C2D_Color32(0x2C, 0x32, 0x40, 0xFF);
static const u32 ColorHeader = C2D_Color32(0x8F, 0xB8, 0xFF, 0xFF);

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

static void FormatScore(char *out, int value)
{
	char digits[16];
	snprintf(digits, sizeof digits, "%d", value);
	size_t len = strlen(digits), o = 0;
	for (size_t i = 0; i < len; i++)
	{
		if (i > 0 && (len - i) % 3 == 0)
			out[o++] = ',';
		out[o++] = digits[i];
	}
	out[o] = 0;
}

void n3ds_initials::DrawTop(C3D_RenderTarget *target)
{
	if (!active)
		return;
	C2D_Prepare();
	C2D_SceneBegin(target);
	C2D_DrawRectSolid(0.0f, 0.0f, 0.5f, 400.0f, 240.0f, ColorDim);
	n3ds_font::Draw("NEW HIGH SCORE", 200.0f, 98.0f, 2.0f, n3ds_font::Center, 380.0f);
}

void n3ds_initials::DrawBottom(C3D_RenderTarget *target)
{
	if (!active)
		return;
	C2D_Prepare();
	C2D_SceneBegin(target);

	const float cx = 160.0f;
	C2D_DrawRectSolid(0.0f, 0.0f, 0.5f, 320.0f, 240.0f, ColorBox);

	n3ds_font::Draw("NEW HIGH SCORE", cx, 10.0f, 1.0f, n3ds_font::Center);

	char line[64];
	snprintf(line, sizeof line, "%s   RANK %d", label, entryRank);
	n3ds_font::Draw(line, cx, 38.0f, 1.0f, n3ds_font::Center, 300.0f, 0.6f);

	char score[24];
	FormatScore(score, entryScore);
	n3ds_font::Draw(score, cx, 64.0f, 1.0f, n3ds_font::Center);

	// Three letter slots at 2x
	const float slotW = 44.0f, gap = 18.0f;
	const float slotsX = cx - (3 * slotW + 2 * gap) * 0.5f;
	const float letterY = 112.0f;
	for (int i = 0; i < 3; i++)
	{
		float sx = slotsX + i * (slotW + gap);
		float mid = sx + slotW * 0.5f;
		bool selected = i == slot;

		char ch[2] = {Charset[letters[i]], 0};
		if (ch[0] != ' ')
			n3ds_font::Draw(ch, mid, letterY, 2.0f, n3ds_font::Center, 0.0f, selected ? 1.0f : 0.75f);
		C2D_DrawRectSolid(sx, letterY + 48.0f, 0.5f, slotW, 3.0f, selected ? ColorHeader : ColorRule);

		if (selected)
		{
			float ay = letterY - 5.0f;
			C2D_DrawTriangle(mid, ay - 9.0f, ColorHeader, mid - 8.0f, ay, ColorHeader, mid + 8.0f, ay, ColorHeader, 0.5f);
			float by = letterY + 56.0f;
			C2D_DrawTriangle(mid - 8.0f, by, ColorHeader, mid + 8.0f, by, ColorHeader, mid, by + 9.0f, ColorHeader, 0.5f);
		}
	}

	n3ds_font::Draw("A: NEXT    B: BACK", cx, 206.0f, 1.0f, n3ds_font::Center, 300.0f, 0.6f);
}
