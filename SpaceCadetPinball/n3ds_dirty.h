#pragma once

#include <cstdint>

struct gdrv_bitmap8;

// Tracks which 8x8 tiles of the game's screen buffer changed, so only those
// are re-uploaded to the GPU. Two bits per tile: one per texture buffer.
// The game's drawing primitives (gdrv/zdrv) call Mark on every write.
class n3ds_dirty
{
public:
	static constexpr int MaxTilesX = 128; // 1024px texture width
	static constexpr int MaxTilesY = 64;  // 512px texture height

	static void Init(gdrv_bitmap8 *screen);

	static inline void Mark(const gdrv_bitmap8 *dst, int x, int y, int w, int h)
	{
		if (dst == screen && w > 0 && h > 0)
			MarkRect(x, y, w, h);
	}

	// Bit 0 = texture buffer 0, bit 1 = buffer 1.
	static uint8_t tiles[MaxTilesY][MaxTilesX];
	static int tilesX, tilesY;

private:
	static const gdrv_bitmap8 *screen;
	static void MarkRect(int x, int y, int w, int h);
};
