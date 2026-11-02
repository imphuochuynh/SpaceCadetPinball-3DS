#pragma once

#include <cstdint>

// Copy 8x8 tiles of a linear 32-bit image into a PICA texture (8x8 tiles,
// Morton order inside a tile, first tile row at the top of the image).
// Pure code so it can be checked natively (tools/tile_test).
namespace n3ds_tiles
{
	// Texel offset inside a tile for pixel (x, y), 0 <= x, y < 8
	struct MortonTable
	{
		uint8_t offset[64];
		constexpr MortonTable() : offset()
		{
			for (int y = 0; y < 8; y++)
				for (int x = 0; x < 8; x++)
					offset[y * 8 + x] = static_cast<uint8_t>(
						(x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3));
		}
	};
	constexpr MortonTable Morton{};

	inline void CopyTile(uint32_t *texture, int textureWidth, const uint32_t *image, int imageStride, int tileX, int tileY)
	{
		uint32_t *dst = texture + (tileY * (textureWidth / 8) + tileX) * 64;
		const uint32_t *src = image + tileY * 8 * imageStride + tileX * 8;
		for (int y = 0; y < 8; y++, src += imageStride)
		{
			const uint8_t *row = &Morton.offset[y * 8];
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
