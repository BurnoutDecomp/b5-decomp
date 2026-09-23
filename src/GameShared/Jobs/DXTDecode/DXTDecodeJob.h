#pragma once

// DXTDecodeJob - the body of the decode job NetworkTextureDXTCompress submits. One instance per
// job thread lives in gaDXTDecodeJobs; DXTDecodeEntry picks its own and hands it the job data
// block. Execute latches the block and runs the dxt_decode codec over it.

#include "types.hpp"
#include "GameShared/Jobs/DXTDecode/DXTDecode.h"   // DXTDecodeData

// dxt_decode - decode a DXT1 image into 32-bit A,R,G,B pixels (the image's width x 4 bytes per
// scan line), block by block in rows of four scan lines. Returns 0.
s32 dxt_decode(char* lpDecodedPixels, const char* lpCompressedPixels, s32 liWidth, s32 liHeight);

// The job-thread context array bound.
const s32 KI_NUM_DXT_DECODE_JOBS = 6;

struct DXTDecodeJob
{
    // Latch the data block, then run the codec on it.
    s32 Execute(const DXTDecodeData* lpData);

    const DXTDecodeData* mpData; // +0x00 the block the entry point handed us
};

extern DXTDecodeJob gaDXTDecodeJobs[KI_NUM_DXT_DECODE_JOBS];
