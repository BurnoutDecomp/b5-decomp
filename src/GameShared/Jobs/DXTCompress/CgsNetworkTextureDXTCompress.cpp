// CgsNetwork::NetworkTextureDXTCompress
//
// Two job lanes, DXT compression and DXT decode, over two pairs of image buffers. A new image
// is copied into the buffer miWriteToSource names; Update hands it to the lane's job and turns
// the ping-pong indices over, so the next image can be queued while a job runs. When a job
// finishes, Update fires the lane's completion callback with the buffer the job wrote.

#include "GameShared/Jobs/DXTCompress/CgsNetworkTextureDXTCompress.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStream (buffer-size assert)
#include "SDKs/EATech/eajobs/entry_point.h"
#include "ppmalloc/EAGeneralAllocator.h"

#include <cstring>   // memcpy

namespace CgsNetwork
{

// Clear both buffer pairs and both jobs and reset the lane state. The two "read" indices start
// on the second buffer.
void NetworkTextureDXTCompress::Construct()
{
    for (s32 liBufferIndex = 0; liBufferIndex < KI_NUM_IMAGE_BUFFERS; ++liBufferIndex)
    {
        mapUncompressedBuffers[liBufferIndex] = nullptr;
        mapCompressedBuffers[liBufferIndex]   = nullptr;
    }

    mDXTCompressJob.Clear();
    mDXTDecodeJob.Clear();

    miWriteToSource         = 0;
    miJobWriteToTexture     = 0;
    mbRunningCompressionJob = false;
    mbNewImageToCompress    = false;
    miJobReadFromSource     = 1;
    miReadFromTexture       = 1;
    mbRunningDecodeJob      = false;
    mbNewImageToDecode      = false;

    mCompressionCompleteCallback = nullptr;
    mpCompressionCompleteData    = nullptr;
    mDecodeCompleteCallback      = nullptr;
    mpDecodeCompleteData         = nullptr;
    miUncompressedBufferSize     = 0;
    miCompressedBufferSize       = 0;
}

// The same reset as Construct, in the console's store order.
void NetworkTextureDXTCompress::Destruct()
{
    mCompressionCompleteCallback = nullptr;
    mpCompressionCompleteData    = nullptr;
    mDecodeCompleteCallback      = nullptr;
    mpDecodeCompleteData         = nullptr;
    mbNewImageToCompress         = false;
    mbRunningCompressionJob      = false;
    mbRunningDecodeJob           = false;
    mbNewImageToDecode           = false;

    mDXTCompressJob.Clear();
    mDXTDecodeJob.Clear();

    miJobWriteToTexture = 0;
    miWriteToSource     = 0;
    miReadFromTexture   = 1;
    miJobReadFromSource = 1;

    for (s32 liBufferIndex = 0; liBufferIndex < KI_NUM_IMAGE_BUFFERS; ++liBufferIndex)
    {
        mapUncompressedBuffers[liBufferIndex] = nullptr;
        mapCompressedBuffers[liBufferIndex]   = nullptr;
    }

    miUncompressedBufferSize = 0;
    miCompressedBufferSize   = 0;
}

// Take the heap and allocate both buffer pairs from it (128-byte aligned), validating the heap
// around every allocation.
bool NetworkTextureDXTCompress::Prepare(CgsMemory::HeapMalloc* lpHeapMalloc,
                                        s32 liUncompressedBufferSize,
                                        s32 liCompressedBufferSize)
{
    CGS_ASSERT(lpHeapMalloc, "lpHeapMalloc");

    mpHeapMalloc = lpHeapMalloc;

    for (s32 liBufferIndex = 0; liBufferIndex < KI_NUM_IMAGE_BUFFERS; ++liBufferIndex)
    {
        CGS_ASSERT(mpHeapMalloc->GetAllocator()->ValidateHeap(EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
                   "mpHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)");
        mapUncompressedBuffers[liBufferIndex] =
            static_cast<char*>(mpHeapMalloc->Malloc(liUncompressedBufferSize, 128));
        CGS_ASSERT(mpHeapMalloc->GetAllocator()->ValidateHeap(EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
                   "mpHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)");
        CGS_ASSERT(mapUncompressedBuffers[liBufferIndex], "mapUncompressedBuffers[ liBufferIndex ]");

        CGS_ASSERT(mpHeapMalloc->GetAllocator()->ValidateHeap(EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
                   "mpHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)");
        mapCompressedBuffers[liBufferIndex] =
            static_cast<char*>(mpHeapMalloc->Malloc(liCompressedBufferSize, 128));
        CGS_ASSERT(mpHeapMalloc->GetAllocator()->ValidateHeap(EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
                   "mpHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)");
        CGS_ASSERT(mapCompressedBuffers[liBufferIndex], "mapCompressedBuffers[ liBufferIndex ]");
    }

    miUncompressedBufferSize = liUncompressedBufferSize;
    miCompressedBufferSize   = liCompressedBufferSize;
    return true;
}

// ---------------------------------------------------------------------------
// Release
//
// Hand both double-buffer pairs back to the heap (compressed slot first, then the
// uncompressed one, per buffer index), then drop the heap and the buffer sizes.
// ---------------------------------------------------------------------------
bool NetworkTextureDXTCompress::Release()
{
    for (s32 liBufferIndex = 0; liBufferIndex < KI_NUM_IMAGE_BUFFERS; ++liBufferIndex)
    {
        if (mapCompressedBuffers[liBufferIndex])
        {
            mpHeapMalloc->Free(mapCompressedBuffers[liBufferIndex]);
            mapCompressedBuffers[liBufferIndex] = nullptr;
        }

        if (mapUncompressedBuffers[liBufferIndex])
        {
            mpHeapMalloc->Free(mapUncompressedBuffers[liBufferIndex]);
            mapUncompressedBuffers[liBufferIndex] = nullptr;
        }
    }

    mpHeapMalloc             = nullptr;
    miUncompressedBufferSize = 0;
    miCompressedBufferSize   = 0;
    return true;
}

// Queue a source image for compression: copy it into the write buffer and record its shape
// in the job data block. Update submits the job.
void NetworkTextureDXTCompress::SetNewTextureToCompress(
    char*            lpNewSourcePixels,
    s32              liNewSourcePixelsSize,
    s32              liTextureWidth,
    s32              liTextureHeight,
    s32              liSrcPitch,
    s32              liCmpPitch,
    s32              liQuality,
    s32              leSourceFormat,
    s8               lbInputIsUncompressedYUYV,
    CompressionCompleteCallback lCompressionCompleteCallback,
    void*            lpCompressionCompleteData)
{
    CGS_ASSERT(lpNewSourcePixels, "lpNewSourcePixels");
    CGS_ASSERT(miWriteToSource < KI_NUM_IMAGE_BUFFERS, "miWriteToSource < KI_NUM_IMAGE_BUFFERS");
    CGS_ASSERT(miWriteToSource >= 0, "miWriteToSource >= 0");
    CGS_ASSERT(mCompressionCompleteCallback == nullptr, "mCompressionCompleteCallback == NULL");
    CGS_ASSERT(miUncompressedBufferSize >= liNewSourcePixelsSize,
               "miUncompressedBufferSize >= liNewSourcePixelsSize");

    memcpy(mapUncompressedBuffers[miWriteToSource], lpNewSourcePixels,
           static_cast<size_t>(liNewSourcePixelsSize));

    mDXTCompressData.miSrcWidth                = liTextureWidth;
    mDXTCompressData.miSrcHeight               = liTextureHeight;
    mDXTCompressData.miSrcPitch                = liSrcPitch;
    mDXTCompressData.miDstPitch                = liCmpPitch;
    mDXTCompressData.miQuality                 = liQuality;
    mDXTCompressData.mePixelFormat             = static_cast<renderengine::PixelFormat>(leSourceFormat);
    mDXTCompressData.mbInputIsUncompressedYUYV = lbInputIsUncompressedYUYV != 0;

    mCompressionCompleteCallback = lCompressionCompleteCallback;
    mpCompressionCompleteData    = lpCompressionCompleteData;
    mbNewImageToCompress         = true;
}

// Queue a DXT1 image for decoding: copy it into the write buffer and record its shape in the
// job data block (the decoded image is eight times the compressed size). Update submits the
// job.
void NetworkTextureDXTCompress::SetNewTextureToDecompress(
    char*            lpCompressedPixels,
    s32              liCompressedPixelSize,
    s32              liCompressedWidth,
    s32              liCompressedHeight,
    CompressionCompleteCallback lDecodeCompleteCallback,
    void*            lpDecodeCompleteData)
{
    CGS_ASSERT(lpCompressedPixels, "lpCompressedPixels");
    CGS_ASSERT(miWriteToSource < KI_NUM_IMAGE_BUFFERS, "miWriteToSource < KI_NUM_IMAGE_BUFFERS");
    CGS_ASSERT(miWriteToSource >= 0, "miWriteToSource >= 0");
    CGS_ASSERT(mDecodeCompleteCallback == nullptr, "mDecodeCompleteCallback == NULL");
    CGS_ASSERT(miCompressedBufferSize >= liCompressedPixelSize,
               "miCompressedBufferSize >= liCompressedPixelSize");

    const s32 liDecodedSize = liCompressedPixelSize << 3;
    if (!(miUncompressedBufferSize >= liDecodedSize))
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << "Uncompressed buffer is not large enough. DXT1 textures are 8 times smaller than their 32-bit ARGB equivalent\n";
        CGS_ASSERT(miUncompressedBufferSize >= liDecodedSize, lStrStream.GetBuffer());
    }

    memcpy(mapCompressedBuffers[miWriteToSource], lpCompressedPixels,
           static_cast<size_t>(liCompressedPixelSize));

    mDXTDecodeData.miCompressedSize = liCompressedPixelSize;
    mDXTDecodeData.miDecodedSize    = liDecodedSize;
    mDXTDecodeData.miDecodedWidth   = liCompressedWidth;
    mDXTDecodeData.miDecodedHeight  = liCompressedHeight;

    mDecodeCompleteCallback = lDecodeCompleteCallback;
    mpDecodeCompleteData    = lpDecodeCompleteData;
    mbNewImageToDecode      = true;
}

// Service the compression lane, then the decode lane. Submitting a job or reaping a
// finished compression job ends the tick; the decode lane is serviced only while the
// compression lane has nothing to do. A submit moves the image to the job's read buffer and
// advances the write buffer; a finished job moves its output buffer to the read side, advances
// the job's write buffer and fires (and clears) the lane's callback.
//
// [PC platform layer] THE DISPATCH, AND ONLY THE DISPATCH. The console submits each job to the
// process-wide EA::Jobs::JobScheduler (gJobManager), which does not exist on this build. As
// RelocatorEntry, TrafficJobEntry, the tint blend and the collision generators already do, the
// job's own entry point runs here instead, over the same data block SetData attached. The job
// wiring (Clear, SetCode, SetData, SetName), the running latch and the reaping are the
// console's: Job::IsDone reports a never-submitted job as done, so the next Update reaps the
// finished job and fires the callback one tick later, as it would after a worker finished.
// The cost is that the compression (or decode) runs on the calling thread.
void NetworkTextureDXTCompress::Update()
{
    // ---- Compression lane ----
    if (!mbRunningCompressionJob)
    {
        if (mbNewImageToCompress)
        {
            const s32 liSource = miWriteToSource;
            mbRunningCompressionJob = true;
            miJobReadFromSource     = liSource;
            mbNewImageToCompress    = false;
            miWriteToSource         = (liSource + 1) % KI_NUM_IMAGE_BUFFERS;

            mDXTCompressData.mpSrcPixels = mapUncompressedBuffers[liSource];
            mDXTCompressData.mpDstPixels = mapCompressedBuffers[miJobWriteToTexture];

            mDXTCompressJob.Clear();
            mDXTCompressJob.mEntryPoint.SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
                                                reinterpret_cast<const void*>(&DXTCompressEntry), 0);
            mDXTCompressJob.SetData(&mDXTCompressData, 128);
            mDXTCompressJob.mEntryPoint.SetName("DXTCompressJob");

            DXTCompressEntry(EA::Jobs::Param(), EA::Jobs::Param(static_cast<void*>(&mDXTCompressData)),
                             EA::Jobs::Param(), EA::Jobs::Param());
            return;
        }
    }
    else if (mDXTCompressJob.IsDone())
    {
        const s32 liTexture = miJobWriteToTexture;
        miReadFromTexture       = liTexture;
        mbRunningCompressionJob = false;
        miJobWriteToTexture     = (liTexture + 1) % KI_NUM_IMAGE_BUFFERS;

        const CompressionCompleteCallback lCallback = mCompressionCompleteCallback;
        if (lCallback)
        {
            void* const lpData = mpCompressionCompleteData;
            mCompressionCompleteCallback = nullptr;
            mpCompressionCompleteData    = nullptr;
            lCallback(mapCompressedBuffers[miReadFromTexture], lpData);
        }
        return;
    }

    // ---- Decode lane ----
    if (!mbRunningDecodeJob)
    {
        if (mbNewImageToDecode)
        {
            const s32 liSource = miWriteToSource;
            mbRunningDecodeJob  = true;
            miJobReadFromSource = liSource;
            mbNewImageToDecode  = false;
            miWriteToSource     = (liSource + 1) % KI_NUM_IMAGE_BUFFERS;

            mDXTDecodeData.mpCompressedPixels = mapCompressedBuffers[liSource];
            mDXTDecodeData.mpDecodedPixels    = mapUncompressedBuffers[miJobWriteToTexture];

            mDXTDecodeJob.Clear();
            mDXTDecodeJob.mEntryPoint.SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
                                              reinterpret_cast<const void*>(&DXTDecodeEntry), 0);
            mDXTDecodeJob.SetData(&mDXTDecodeData, 128);
            mDXTDecodeJob.mEntryPoint.SetName("DXTDecodeJob");

            DXTDecodeEntry(EA::Jobs::Param(), EA::Jobs::Param(static_cast<void*>(&mDXTDecodeData)),
                           EA::Jobs::Param(), EA::Jobs::Param());
        }
        return;
    }

    if (mDXTDecodeJob.IsDone())
    {
        const s32 liTexture = miJobWriteToTexture;
        miReadFromTexture   = liTexture;
        mbRunningDecodeJob  = false;
        miJobWriteToTexture = (liTexture + 1) % KI_NUM_IMAGE_BUFFERS;

        const CompressionCompleteCallback lCallback = mDecodeCompleteCallback;
        if (lCallback)
        {
            void* const lpData = mpDecodeCompleteData;
            mDecodeCompleteCallback = nullptr;
            mpDecodeCompleteData    = nullptr;
            lCallback(mapUncompressedBuffers[miReadFromTexture], lpData);
        }
    }
}

} // namespace CgsNetwork
