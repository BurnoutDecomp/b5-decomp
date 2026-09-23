#pragma once

// CgsNetwork::NetworkTextureDXTCompress
//
// Manages two parallel EA::Jobs job lanes — one for DXT compression (CPU→GPU)
// and one for DXT decode (GPU→CPU) — backed by a double-buffer scheme over a
// caller-supplied HeapMalloc.  Each lane has its own EA::Jobs::Job slot and a
// paired 128-byte descriptor struct that is committed to the job via SetData.
//
// LAYOUT AUTHORITY: X360 ARTIST asm + DWARF (CgsNetworkTextureDXTCompress.cpp)
//
// Console layout: the two jobs at +0x000 / +0x350, the two job descriptors at +0x700 /
// +0x780, the buffer indices from +0x800, the heap at +0x824, the callbacks +0x828..+0x834,
// the buffer sizes +0x838 / +0x83C. The descriptors are the data blocks handed to the job
// (SetData size 128) and sit on 128-byte boundaries, which leaves the gap after the second
// job and rounds the object to 0x880 -- exactly the span BrnNetworkManager reserves at
// +0x86200 (itself a 128-byte boundary). _AssertLayout pins this in a 32-bit build.

#include <cstddef>                             // offsetof (_AssertLayout)
#include "types.hpp"
#include "SDKs/EATech/eajobs/job.h"          // EA::Jobs::Job (embedded, sizeof=848)

namespace CgsMemory { class HeapMalloc; }

// ---------------------------------------------------------------------------
// Job descriptor structs (128-byte X360 layout, named-member access only).
// ---------------------------------------------------------------------------

// Descriptor for the DXT compression job (+0x700), a 128-byte aligned job data block.
struct alignas(128) DXTCompressData
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

// Descriptor for the DXT decode job (+0x780), a 128-byte aligned job data block.
struct alignas(128) DXTDecodeData
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
    // Fired when a compress / decode job completes: (the finished pixel buffer, the caller's
    // user data).
    typedef void (*CompressionCompleteCallback)(void*, void*);

    static const s32 KI_NUM_IMAGE_BUFFERS = 2;

    // Both jobs start empty and unnamed (the owner's constructor builds them as Job(0)).
    NetworkTextureDXTCompress() : mDXTCompressJob(0), mDXTDecodeJob(0) {}

    void Construct();
    void Destruct();

    bool Prepare(CgsMemory::HeapMalloc* lpHeapMalloc,
                 s32 liUncompressedBufferSize,
                 s32 liCompressedBufferSize);

    // Free both double-buffer pairs back to the heap and drop the heap + sizes.
    bool Release();

    void SetNewTextureToCompress(char*           lpNewSourcePixels,
                                  s32             liNewSourcePixelsSize,
                                  s32             liTextureWidth,
                                  s32             liTextureHeight,
                                  s32             liSrcPitch,
                                  s32             liCmpPitch,
                                  s32             liQuality,
                                  s32             leSourceFormat,
                                  s8              lbInputIsUncompressedYUYV,
                                  CompressionCompleteCallback lCompressionCompleteCallback,
                                  void*            lpCompressionCompleteData);

    void SetNewTextureToDecompress(char*           lpCompressedPixels,
                                    s32             liCompressedPixelSize,
                                    s32             liCompressedWidth,
                                    s32             liCompressedHeight,
                                    CompressionCompleteCallback lDecodeCompleteCallback,
                                    void*            lpDecodeCompleteData);

    void Update();

private:
    // Console offsets pinned in a 32-bit build (inert on the host).
    static void _AssertLayout();

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

    CompressionCompleteCallback mCompressionCompleteCallback; // +0x828
    void*            mpCompressionCompleteData;    // +0x82C on X360

    CompressionCompleteCallback mDecodeCompleteCallback;      // +0x830
    void*            mpDecodeCompleteData;         // +0x834 on X360

    s32 miUncompressedBufferSize;     // +0x838 on X360
    s32 miCompressedBufferSize;       // +0x83C on X360
};

inline void NetworkTextureDXTCompress::_AssertLayout()
{
#define CGS_DXT_AT(member, off) \
    static_assert(sizeof(void*) != 4 || offsetof(NetworkTextureDXTCompress, member) == off, #member " @ " #off)
    CGS_DXT_AT(mDXTCompressData,         0x700);
    CGS_DXT_AT(mDXTDecodeData,           0x780);
    CGS_DXT_AT(miWriteToSource,          0x800);
    CGS_DXT_AT(mapUncompressedBuffers,   0x810);
    CGS_DXT_AT(mapCompressedBuffers,     0x818);
    CGS_DXT_AT(mbRunningCompressionJob,  0x820);
    CGS_DXT_AT(mpHeapMalloc,             0x824);
    CGS_DXT_AT(mCompressionCompleteCallback, 0x828);
    CGS_DXT_AT(mDecodeCompleteCallback,  0x830);
    CGS_DXT_AT(miCompressedBufferSize,   0x83C);
    static_assert(sizeof(void*) != 4 || sizeof(NetworkTextureDXTCompress) == 0x880,
                  "sizeof(NetworkTextureDXTCompress) == 0x880");
#undef CGS_DXT_AT
}

} // namespace CgsNetwork
