#include "pch.h"
#include "n3ds_font.h"

#include <citro2d.h>

#include "gdrv.h"
#include "score.h"

namespace
{
	constexpr int FirstChar = 32, LastChar = 127;
	constexpr int AtlasWidth = 256;

	bool available = false;
	int gap = 0;
	C3D_Tex atlas;
	Tex3DS_SubTexture glyphs[128];
	int glyphWidth[128];
	C2D_TextBuf fallbackBuf = nullptr;

	// PICA textures are stored in 8x8 tiles, Morton order inside each tile,
	// first tile row at the top of the image (same layout winmain uses).
	inline uint32_t TexelIndex(int x, int y, int width)
	{
		uint32_t tile = (y / 8) * (width / 8) + x / 8;
		uint32_t m = (x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3);
		return tile * 64 + m;
	}

	uint16_t NextPow2(int v)
	{
		uint16_t p = 8;
		while (p < v)
			p <<= 1;
		return p;
	}
}

void n3ds_font::Initialize()
{
	fallbackBuf = C2D_TextBufNew(512);

	auto font = score::msg_fontp;
	if (!font || font->Height <= 0)
		return;
	gap = font->GapWidth;
	int h = font->Height;

	// Lay glyphs out in rows, 1px apart
	int x = 0, y = 0;
	int posX[128] = {}, posY[128] = {};
	for (int c = FirstChar; c <= LastChar; c++)
	{
		auto bmp = font->Chars[c];
		int w = bmp ? bmp->Width : 0;
		if (x + w > AtlasWidth)
		{
			x = 0;
			y += h + 1;
		}
		posX[c] = x;
		posY[c] = y;
		glyphWidth[c] = w;
		x += w + 1;
	}
	uint16_t atlasHeight = NextPow2(y + h);

	if (!C3D_TexInit(&atlas, AtlasWidth, atlasHeight, GPU_RGBA8))
		return;
	C3D_TexSetFilter(&atlas, GPU_NEAREST, GPU_NEAREST); // keep the dots crisp when scaled
	C3D_TexSetWrap(&atlas, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

	auto texels = static_cast<uint32_t *>(atlas.data);
	memset(texels, 0, atlas.size);
	for (int c = FirstChar; c <= LastChar; c++)
	{
		auto bmp = font->Chars[c];
		if (!bmp)
			continue;
		for (int gy = 0; gy < bmp->Height && gy < h; gy++)
			for (int gx = 0; gx < bmp->Width; gx++)
			{
				// Color 0 is the transparent background, as in the game's own blit
				uint32_t color = bmp->BmpBufPtr1[gy * bmp->Stride + gx].Color;
				texels[TexelIndex(posX[c] + gx, posY[c] + gy, AtlasWidth)] = color;
			}

		auto &sub = glyphs[c];
		sub.width = static_cast<u16>(bmp->Width);
		sub.height = static_cast<u16>(h);
		sub.left = posX[c] / static_cast<float>(AtlasWidth);
		sub.right = (posX[c] + bmp->Width) / static_cast<float>(AtlasWidth);
		sub.top = 1.0f - posY[c] / static_cast<float>(atlasHeight);
		sub.bottom = 1.0f - (posY[c] + h) / static_cast<float>(atlasHeight);
	}
	C3D_TexFlush(&atlas);
	available = true;
}

void n3ds_font::Dispose()
{
	if (available)
		C3D_TexDelete(&atlas);
	available = false;
	if (fallbackBuf)
		C2D_TextBufDelete(fallbackBuf);
	fallbackBuf = nullptr;
}

void n3ds_font::BeginFrame()
{
	if (fallbackBuf)
		C2D_TextBufClear(fallbackBuf);
}

static int GlyphIndex(char ch)
{
	int c = static_cast<unsigned char>(ch);
	return (c >= FirstChar && c <= LastChar) ? c : '?';
}

float n3ds_font::Width(const char *text, float scale)
{
	if (!available)
	{
		C2D_Text t;
		C2D_TextParse(&t, fallbackBuf, text);
		float w, h;
		C2D_TextGetDimensions(&t, 0.6f * scale, 0.6f * scale, &w, &h);
		return w;
	}
	float w = 0;
	int n = 0;
	for (const char *p = text; *p; p++, n++)
		w += glyphWidth[GlyphIndex(*p)];
	if (n > 1)
		w += gap * (n - 1);
	return w * scale;
}

void n3ds_font::Draw(const char *text, float x, float y, float scale, Align align,
                     float maxWidth, float alpha, uint32_t tint, float tintBlend)
{
	float w = Width(text, scale);
	float sx = scale;
	if (maxWidth > 0 && w > maxWidth)
	{
		sx = scale * maxWidth / w;
		w = maxWidth;
	}
	if (align == Center)
		x -= w * 0.5f;
	else if (align == Right)
		x -= w;
	x = floorf(x + 0.5f);
	y = floorf(y + 0.5f);

	if (!available)
	{
		C2D_Text t;
		C2D_TextParse(&t, fallbackBuf, text);
		C2D_TextOptimize(&t);
		uint32_t color = tint ? tint : C2D_Color32(0x94, 0xCC, 0xF4, 0xFF);
		color = (color & 0x00FFFFFF) | (static_cast<uint32_t>(alpha * 255.0f) << 24);
		C2D_DrawText(&t, C2D_WithColor, x, y, 0.5f, 0.6f * sx, 0.6f * scale, color);
		return;
	}

	C2D_ImageTint imageTint;
	C2D_PlainImageTint(&imageTint, tint ? tint : C2D_Color32(0, 0, 0, 0xFF), tint ? tintBlend : 0.0f);
	for (int corner = 0; corner < 4; corner++)
		imageTint.corners[corner].color = (imageTint.corners[corner].color & 0x00FFFFFF) |
		                                  (static_cast<uint32_t>(alpha * 255.0f) << 24);

	for (const char *p = text; *p; p++)
	{
		int c = GlyphIndex(*p);
		if (glyphWidth[c] > 0 && *p != ' ')
		{
			C2D_Image img = {&atlas, &glyphs[c]};
			C2D_DrawImageAt(img, x, y, 0.5f, &imageTint, sx, scale);
		}
		x += (glyphWidth[c] + gap) * sx;
	}
}
