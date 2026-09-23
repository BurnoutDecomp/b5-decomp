#pragma once

// The DXT decode job's data block and entry point.
//
// NetworkTextureDXTCompress fills a DXTDecodeData block, hands it to its EA::Jobs::Job with
// SetData (128 bytes) and submits the job. DXTDecodeEntry picks this worker's DXTDecodeJob out
// of the six-entry array and runs it over the block.

#include "types.hpp"
#include "SDKs/EATech/eajobs/job_types.h"   // EA::Jobs::Param

// The data block the decode job reads (+0x780 inside NetworkTextureDXTCompress, handed to the
// job with SetData size 128, hence the 128-byte alignment).
struct alignas(128) DXTDecodeData
{
    char* mpCompressedPixels;   // +0x00 DXT1 source
    char* mpDecodedPixels;      // +0x04 32-bit destination
    s32 miCompressedSize;       // +0x08 source bytes
    s32 miDecodedSize;          // +0x0C destination bytes (8 x the source)
    s32 miDecodedWidth;         // +0x10
    s32 miDecodedHeight;        // +0x14
};

// The EA::Jobs entry point NetworkTextureDXTCompress wires its decode job to; the second
// parameter carries the DXTDecodeData block.
void DXTDecodeEntry(EA::Jobs::Param lParam0, EA::Jobs::Param lParam1,
                    EA::Jobs::Param lParam2, EA::Jobs::Param lParam3);
