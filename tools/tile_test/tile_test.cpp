// Checks n3ds_tiles against the original port's swizzle (GenerateMortonSwizzle
// + SwizzleAndCopy, copied verbatim below) on a random 600x416 image.
// Build: g++ -std=c++17 -O2 tools/tile_test/tile_test.cpp -o /tmp/tile_test
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include "../../SpaceCadetPinball/n3ds_tiles.h"

constexpr int W = 600, H = 416, TW = 1024, TH = 512;

// ---- original code, adapted only to take the image size as constants ----
static uint8_t **GenerateMortonSwizzle(uint8_t *renderTextureData, const uint16_t renderTextureWidth, const uint32_t screenTexturePixelCount)
{
	uint8_t **swizzle = new uint8_t *[screenTexturePixelCount >> 1];
	uint32_t dstOffset = 0, widthCount = 0;
	constexpr uint32_t tileSize = 8, subTileSize1 = 4, subTileSize2 = 2;
	uint32_t widthBytes = W * tileSize * 4;
	for (int32_t y = 0; y < H; y += tileSize)
		for (int32_t x = 0; x < W; x += tileSize)
			for (uint8_t ty = 0; ty < tileSize; ty += subTileSize1)
				for (uint8_t tx = 0; tx < tileSize; tx += subTileSize1)
					for (uint8_t sty1 = 0; sty1 < subTileSize1; sty1 += subTileSize2)
						for (uint8_t stx1 = 0; stx1 < subTileSize1; stx1 += subTileSize2)
							for (uint8_t sty2 = 0; sty2 < subTileSize2; sty2++)
							{
								uint32_t index = (y + ty + sty1 + sty2) * W + (x + tx + stx1);
								swizzle[index >> 1] = &renderTextureData[dstOffset + 0];
								dstOffset += 8;
								widthCount += 8;
								if (widthCount == widthBytes)
								{
									dstOffset += (renderTextureWidth - W) << 5;
									widthCount = 0;
								}
							}
	return swizzle;
}

static void SwizzleAndCopy(const uint32_t *image, uint8_t **swizzle, const uint32_t screenTexturePixelCount)
{
	for (uint32_t i = 0; i < screenTexturePixelCount; i += 16)
	{
		const uint32_t *src = &image[i];
		uint8_t **dst = &swizzle[i >> 1];
		for (int k = 0; k < 8; k++)
			memcpy(dst[k], &src[k * 2], 8);
	}
}
// -------------------------------------------------------------------------

int main()
{
	std::vector<uint32_t> image(W * H);
	srand(1234);
	for (auto &p : image)
		p = (uint32_t)rand() * 2654435761u;

	std::vector<uint32_t> oldTex(TW * TH, 0), newTex(TW * TH, 0);
	uint8_t **sw = GenerateMortonSwizzle((uint8_t *)oldTex.data(), TW, W * H);
	SwizzleAndCopy(image.data(), sw, W * H);

	static uint8_t flags[64][128];
	memset(flags, 0x3, sizeof flags);
	int first, last;
	int n = n3ds_tiles::CopyDirtyTiles<128>(newTex.data(), TW, image.data(), W, flags, W / 8, H / 8, 1, &first, &last);

	bool same = memcmp(oldTex.data(), newTex.data(), oldTex.size() * 4) == 0;
	printf("tiles copied %d (expect %d), rows %d..%d, layout %s\n", n, (W / 8) * (H / 8), first, last, same ? "IDENTICAL" : "DIFFERENT");

	// Partial update: change a small rect, mark it, copy only those tiles
	for (int y = 100; y < 120; y++)
		for (int x = 300; x < 330; x++)
			image[y * W + x] ^= 0xDEADBEEF;
	SwizzleAndCopy(image.data(), sw, W * H);
	memset(flags, 0, sizeof flags);
	for (int ty = 100 / 8; ty <= 119 / 8; ty++)
		for (int tx = 300 / 8; tx <= 329 / 8; tx++)
			flags[ty][tx] = 1;
	n = n3ds_tiles::CopyDirtyTiles<128>(newTex.data(), TW, image.data(), W, flags, W / 8, H / 8, 1, &first, &last);
	same = memcmp(oldTex.data(), newTex.data(), oldTex.size() * 4) == 0;
	printf("partial: tiles copied %d, rows %d..%d, layout %s\n", n, first, last, same ? "IDENTICAL" : "DIFFERENT");
	return same ? 0 : 1;
}
