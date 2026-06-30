#pragma once

// CgsNetwork::NetworkTextureDXTCompress
//
// Manages two parallel EA::Jobs job lanes — one for DXT compression (CPU→GPU)
// and one for DXT decode (GPU→CPU) — backed by a double-buffer scheme over a
// caller-supplied HeapMalloc.  Each lane has its own EA::Jobs::Job slot and a
// paired 128-byte descriptor struct that is committed to the job via SetData.
//
// LAYOUT AUTHORITY: X360 ARTIST asm + DWARF (CgsNetworkTextureDXTCompress.cpp)
//   sizeof(NetworkTextureDXTCompress) = approx. 0x840+ bytes on X360;
//   members below are accessed by name per quality-gate policy.

#include "types.hpp"
#include "SDKs/EATech/eajobs/job.h"          // EA::Jobs::Job (embedded, sizeof=848)
#include "SDKs/EATech/eajobs/job_scheduler.h" // EA::Jobs::JobScheduler (AddJobs)

namespace CgsMemory { class HeapMalloc; }

// ---------------------------------------------------------------------------
// Job descriptor structs (128-byte X360 layout, named-member access only).
// ---------------------------------------------------------------------------

// Descriptor for the DXT compression job (written at X360 offset +1792).
struct DXTCompressData
{
    char* lpUncompressedPixels;    // +0x00  source pixel buffer
    char* lpCompressedPixels;      // +0x04  destination compressed buffer
    s32 miTextureWidth;            // +0x08
    s32 miTextureHeight;           // +0x0C
    s32 miSrcPitch;                // +0x10
    s32 miCmpPitch;                // +0x14
    s32 miQuality;                 // +0x18
    s32 leSourceFormat;            // +0x1C  renderengine::PixelFormat (stored as s32)
    s8  lbInputIsUncompressedYUYV; // +0x20
};

// Descriptor for the DXT decode job (written at X360 offset +1920).
struct DXTDecodeData
{
    char* lpCompressedPixels;   // +0x00  source compressed buffer
    char* lpUncompressedPixels; // +0x04  destination uncompressed buffer
    s32 miCompressedPixelSize;  // +0x08
    s32 miUncompressedSize;     // +0x0C
    s32 miCompressedWidth;      // +0x10
    s32 miCompressedHeight;     // +0x14
};

namespace CgsNetwork
{

class NetworkTextureDXTCompress
{
public:
    typedef void (*CompressCallback)(char*, void*);

    void Construct();
    void Destruct();

    bool Prepare(CgsMemory::HeapMalloc* lpHeapMalloc,
                 s32 liUncompressedBufferSize,
                 s32 liCompressedBufferSize);

    void SetNewTextureToCompress(char*           lpNewSourcePixels,
                                  s32             liNewSourcePixelsSize,
                                  s32             liTextureWidth,
                                  s32             liTextureHeight,
                                  s32             liSrcPitch,
                                  s32             liCmpPitch,
                                  s32             liQuality,
                                  s32             leSourceFormat,
                                  s8              lbInputIsUncompressedYUYV,
                                  CompressCallback lCompressionCompleteCallback,
                                  void*            lpCompressionCompleteData);

    void SetNewTextureToDecompress(char*           lpCompressedPixels,
                                    s32             liCompressedPixelSize,
                                    s32             liCompressedWidth,
                                    s32             liCompressedHeight,
                                    CompressCallback lDecodeCompleteCallback,
                                    void*            lpDecodeCompleteData);

    void Update();

private:
    // Two EA::Jobs job slots (each sizeof=848 on X360).
    EA::Jobs::Job mDXTCompressJob;    // +0x000 on X360
    EA::Jobs::Job mDXTDecodeJob;      // +0x350 on X360

    // Job descriptors (at +0x700 / +0x780 on X360; gap between jobs and data
    // is a platform layout artifact not reflected in named-member access).
    DXTCompressData mDXTCompressData; // +0x700 on X360
    DXTDecodeData   mDXTDecodeData;   // +0x780 on X360

    // Double-buffer index state (four s32 ping-pong indices).
    s32 miWriteToSource;              // +0x800 on X360
    s32 miJobReadFromSource;          // +0x804 on X360
    s32 miJobWriteToTexture;          // +0x808 on X360
    s32 miReadFromTexture;            // +0x80C on X360

    // Double-buffer pointers (2 uncompressed + 2 compressed slots).
    char* mapUncompressedBuffers[2];  // +0x810 on X360
    char* mapCompressedBuffers[2];    // +0x818 on X360

    // Per-lane run / new-image flags.
    bool mbRunningCompressionJob;     // +0x820 on X360
    bool mbNewImageToCompress;        // +0x821 on X360
    bool mbRunningDecodeJob;          // +0x822 on X360
    bool mbNewImageToDecode;          // +0x823 on X360

    CgsMemory::HeapMalloc* mpHeapMalloc; // +0x824 on X360

    CompressCallback mCompressionCompleteCallback; // +0x828 on X360
    void*            mpCompressionCompleteData;    // +0x82C on X360

    CompressCallback mDecodeCompleteCallback;      // +0x830 on X360
    void*            mpDecodeCompleteData;         // +0x834 on X360

    s32 miUncompressedBufferSize;     // +0x838 on X360
    s32 miCompressedBufferSize;       // +0x83C on X360
};

} // namespace CgsNetwork
