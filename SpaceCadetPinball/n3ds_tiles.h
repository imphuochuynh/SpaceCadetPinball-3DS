#pragma once

#include <cstdint>

// Copy 8x8 tiles of a linear 32-bit image into a PICA texture (8x8 tiles,
// Morton order inside a tile, first tile row at the top of the image).
// Pure code so it can be checked natively (tools/tile_test).
namespace n3ds_tiles
{
	// Texel offset inside a tile for pixel (x, y) at [y * 8 + x]: the bits of
	// x and y interleaved (x0 y0 x1 y1 x2 y2).
	static const uint8_t MortonOffset[64] =
	{
		 0,  1,  4,  5, 16, 17, 20, 21,
		 2,  3,  6,  7, 18, 19, 22, 23,
		 8,  9, 12, 13, 24, 25, 28, 29,
		10, 11, 14, 15, 26, 27, 30, 31,
		32, 33, 36, 37, 48, 49, 52, 53,
		34, 35, 38, 39, 50, 51, 54, 55,
		40, 41, 44, 45, 56, 57, 60, 61,
		42, 43, 46, 47, 58, 59, 62, 63
	};

	inline void CopyTile(uint32_t *texture, int textureWidth, const uint32_t *image, int imageStride, int tileX, int tileY)
	{
		uint32_t *dst = texture + (tileY * (textureWidth / 8) + tileX) * 64;
		const uint32_t *src = image + tileY * 8 * imageStride + tileX * 8;
		for (int y = 0; y < 8; y++, src += imageStride)
		{
			const uint8_t *row = &MortonOffset[y * 8];
			for (int x = 0; x < 8; x++)
				dst[row[x]] = src[x];
		}
	}

	// Copies every tile whose flags contain `bit`, clears that bit, and reports
	// the range of tile rows touched (for cache flushing). Returns tiles copied.
	template <int MaxTilesX>
	int CopyDirtyTiles(uint32_t *texture, int textureWidth, const uint32_t *image, int imageStride,
	                   uint8_t (*flags)[MaxTilesX], int tilesX, int tilesY, uint8_t bit,
	                   int *firstRow, int *lastRow)
	{
		int copied = 0;
		*firstRow = -1;
		*lastRow = -1;
		for (int ty = 0; ty < tilesY; ty++)
		{
			uint8_t *row = flags[ty];
			for (int tx = 0; tx < tilesX; tx++)
			{
				if (!(row[tx] & bit))
					continue;
				row[tx] &= static_cast<uint8_t>(~bit);
				CopyTile(texture, textureWidth, image, imageStride, tx, ty);
				copied++;
				if (*firstRow < 0)
					*firstRow = ty;
				*lastRow = ty;
			}
		}
		return copied;
	}
}
