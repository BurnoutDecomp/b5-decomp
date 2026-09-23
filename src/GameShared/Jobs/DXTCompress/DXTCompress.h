#pragma once

// The DXT compression job: its data block, the per-worker job objects and the job entry point.
//
// NetworkTextureDXTCompress fills a DXTCompressData block, hands it to its EA::Jobs::Job with
// SetData (128 bytes) and submits the job. DXTCompressEntry picks this worker's DXTCompressJob
// out of the six-entry array and runs it over the block: Execute walks the source image four
// scan lines at a time and hands each band to the fdxt block compressor.

#include "types.hpp"
#include "pc/gcm/renderengine/pixelformat.h"   // renderengine::PixelFormat (mePixelFormat)
#include "SDKs/EATech/eajobs/job_types.h"      // EA::Jobs::Param

// The data block the compression job reads (+0x700 inside NetworkTextureDXTCompress, handed to
// the job with SetData size 128, hence the 128-byte alignment).
struct alignas(128) DXTCompressData
{
    char* mpSrcPixels;                         // +0x00 uncompressed source image
    char* mpDstPixels;                         // +0x04 compressed destination
    s32 miSrcWidth;                            // +0x08
    s32 miSrcHeight;                           // +0x0C
    s32 miSrcPitch;                            // +0x10 bytes per source scan line
    s32 miDstPitch;                            // +0x14 bytes per row of compressed blocks
    s32 miQuality;                             // +0x18 endpoint-search effort (0..100)
    renderengine::PixelFormat mePixelFormat;   // +0x1C source format
    bool mbInputIsUncompressedYUYV;            // +0x20
};

// The job-thread context array bound.
const s32 KI_NUM_DXT_COMPRESS_JOBS = 6;

// One per job thread. Execute latches the data block and compresses the whole image it names.
class DXTCompressJob
{
public:
    void Execute(DXTCompressData* lpData);

    DXTCompressData* mpData;   // +0x00 the block the entry point handed us
};

extern DXTCompressJob gaDXTCompressJobs[KI_NUM_DXT_COMPRESS_JOBS];

// The EA::Jobs entry point NetworkTextureDXTCompress wires its compression job to; the second
// parameter carries the DXTCompressData block.
void DXTCompressEntry(EA::Jobs::Param lParam0, EA::Jobs::Param lParam1,
                      EA::Jobs::Param lParam2, EA::Jobs::Param lParam3);
