#pragma once

#include "types.hpp"

// fdxt - compress one band of four scan lines into DXT blocks.
//
//   lpDst        destination; 8 bytes per block, 16 when an alpha block precedes the colour block
//   lpSrc        first pixel of the band
//   liSrcPitch   bytes per source scan line
//   liNumBlocks  blocks across the band
//   luFlags      0x100 explicit (4-bit) alpha block, 0x200 interpolated alpha block, 0x10 skip
//                the colour block; 0 compresses colour only
//   liQuality    endpoint-search effort, 0..100 (0 takes the colour bounding box as is)
//   leFormat     renderengine::PixelFormat of the source (A1R5G5B5, the 16-bit 8_8 YUY2 plane,
//                or 32-bit ARGB)
//   lbInputIsUncompressedYUYV  32-bit source whose bytes carry (-, Y, U, V) per pixel
//
// The block bytes are written the way the texture unit reads them: every 16-bit word of the
// block is byte-swapped after encoding.
void fdxt(u8* lpDst, const u8* lpSrc, s32 liSrcPitch, s32 liNumBlocks, u32 luFlags,
          s32 liQuality, s32 leFormat, bool lbInputIsUncompressedYUYV);
