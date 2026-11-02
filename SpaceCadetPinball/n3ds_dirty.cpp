#include "pch.h"
#include "n3ds_dirty.h"

#include "gdrv.h"

uint8_t n3ds_dirty::tiles[MaxTilesY][MaxTilesX];
int n3ds_dirty::tilesX = 0, n3ds_dirty::tilesY = 0;
const gdrv_bitmap8 *n3ds_dirty::screen = nullptr;

void n3ds_dirty::Init(gdrv_bitmap8 *s)
{
	screen = s;
	tilesX = std::min((s->Width + 7) / 8, MaxTilesX);
	tilesY = std::min((s->Height + 7) / 8, MaxTilesY);
	memset(tiles, 0x3, sizeof tiles); // both buffers start empty: upload everything once
}

void n3ds_dirty::MarkRect(int x, int y, int w, int h)
{
	int x0 = std::max(x, 0) / 8;
	int y0 = std::max(y, 0) / 8;
	int x1 = std::min((x + w - 1) / 8, tilesX - 1);
	int y1 = std::min((y + h - 1) / 8, tilesY - 1);
	for (int ty = y0; ty <= y1; ty++)
		for (int tx = x0; tx <= x1; tx++)
			tiles[ty][tx] = 0x3;
}
