// CgsNetwork::NetworkTextureDXTCompress
//
// Two job lanes, DXT compression and DXT decode, over two pairs of image buffers. A new image
// is copied into the buffer miWriteToSource names; Update hands it to the lane's job and turns
// the ping-pong indices over, so the next image can be queued while a job runs. When a job
// finishes, Update fires the lane's completion callback with the buffer the job wrote.

#include "GameShared/Jobs/DXTCompress/CgsNetworkTextureDXTCompress.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::StrStream (buffer-size assert)
#include "GameShared/GameClasses/System/CgsHardwareInit.h"
#include "SDKs/EATech/eajobs/job_scheduler.h"
#include "ppmalloc/EAGeneralAllocator.h"

#include <xtl.h>

static const char KAC_ARTIST_ASSERT_FILE[] =
    "..\\..\\..\\GameShared\\Jobs\\DXTCompress/CgsNetworkTextureDXTCompress.cpp";

namespace CgsNetwork
{

// Clear both buffer pairs and both jobs and reset the lane state. The two "read" indices start
// on the second buffer.
void NetworkTextureDXTCompress::Construct()
{
    for (s32 liBufferIndex = 0; liBufferIndex < KI_NUM_IMAGE_BUFFERS; ++liBufferIndex)
    {
        mapUncompressedBuffers[liBufferIndex] = 0;
        mapCompressedBuffers[liBufferIndex]   = 0;
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

    mCompressionCompleteCallback = 0;
    mpCompressionCompleteData    = 0;
    mDecodeCompleteCallback      = 0;
    mpDecodeCompleteData         = 0;
    miUncompressedBufferSize     = 0;
    miCompressedBufferSize       = 0;
}

// The same reset as Construct, in the console's store order.
void NetworkTextureDXTCompress::Destruct()
{
    mCompressionCompleteCallback = 0;
    mpCompressionCompleteData    = 0;
    mDecodeCompleteCallback      = 0;
    mpDecodeCompleteData         = 0;
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
        mapUncompressedBuffers[liBufferIndex] = 0;
        mapCompressedBuffers[liBufferIndex]   = 0;
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
    CGS_ASSERT_AT(lpHeapMalloc,
        "lpHeapMalloc", KAC_ARTIST_ASSERT_FILE, 134 /* ARTIST 0x8287EAAC */);

    mpHeapMalloc = lpHeapMalloc;

    for (s32 liBufferIndex = 0; liBufferIndex < KI_NUM_IMAGE_BUFFERS; ++liBufferIndex)
    {
        CGS_ASSERT_AT(mpHeapMalloc->GetAllocator()->ValidateHeap(EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
            "mpHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)",
                KAC_ARTIST_ASSERT_FILE, 142 /* ARTIST 0x8287EB04 */);

        mapUncompressedBuffers[liBufferIndex] =
            static_cast<char*>(mpHeapMalloc->Malloc(liUncompressedBufferSize, 128));
        CGS_ASSERT_AT(mpHeapMalloc->GetAllocator()->ValidateHeap(EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
            "mpHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)",
                KAC_ARTIST_ASSERT_FILE, 144 /* ARTIST 0x8287EB4C */);

        CGS_ASSERT_AT(mapUncompressedBuffers[liBufferIndex],
            "mapUncompressedBuffers[ liBufferIndex ]",
                KAC_ARTIST_ASSERT_FILE, 145 /* ARTIST 0x8287EB70 */);


        CGS_ASSERT_AT(mpHeapMalloc->GetAllocator()->ValidateHeap(EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
            "mpHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)",
                KAC_ARTIST_ASSERT_FILE, 147 /* ARTIST 0x8287EBA4 */);

        mapCompressedBuffers[liBufferIndex] =
            static_cast<char*>(mpHeapMalloc->Malloc(liCompressedBufferSize, 128));
        CGS_ASSERT_AT(mpHeapMalloc->GetAllocator()->ValidateHeap(EA::Allocator::GeneralAllocator::kHeapValidationLevelFull),
            "mpHeapMalloc->GetAllocator()->ValidateHeap(rw::core::GeneralAllocator::kHeapValidationLevelFull)",
                KAC_ARTIST_ASSERT_FILE, 149 /* ARTIST 0x8287EBEC */);

        CGS_ASSERT_AT(mapCompressedBuffers[liBufferIndex],
            "mapCompressedBuffers[ liBufferIndex ]",
                KAC_ARTIST_ASSERT_FILE, 150 /* ARTIST 0x8287EC10 */);

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
            mapCompressedBuffers[liBufferIndex] = 0;
        }

        if (mapUncompressedBuffers[liBufferIndex])
        {
            mpHeapMalloc->Free(mapUncompressedBuffers[liBufferIndex]);
            mapUncompressedBuffers[liBufferIndex] = 0;
        }
    }

    mpHeapMalloc             = 0;
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
    renderengine::PixelFormat leSourceFormat,
    bool             lbInputIsUncompressedYUYV,
    CompressionCompleteCallback lCompressionCompleteCallback,
    void*            lpCompressionCompleteData)
{
    CGS_ASSERT_AT(lpNewSourcePixels,
                  "lpNewSourcePixels", KAC_ARTIST_ASSERT_FILE, 338);
    CGS_ASSERT_AT(miWriteToSource < KI_NUM_IMAGE_BUFFERS,
                  "miWriteToSource < KI_NUM_IMAGE_BUFFERS", KAC_ARTIST_ASSERT_FILE, 339);
    CGS_ASSERT_AT(miWriteToSource >= 0,
                  "miWriteToSource >= 0", KAC_ARTIST_ASSERT_FILE, 340);
    CGS_ASSERT_AT(mCompressionCompleteCallback == 0,
                  "mCompressionCompleteCallback == NULL", KAC_ARTIST_ASSERT_FILE, 349);
    CGS_ASSERT_AT(miUncompressedBufferSize >= liNewSourcePixelsSize,
                  "miUncompressedBufferSize >= liNewSourcePixelsSize", KAC_ARTIST_ASSERT_FILE, 353);

    XMemCpy(mapUncompressedBuffers[miWriteToSource], lpNewSourcePixels,
           static_cast<size_t>(liNewSourcePixelsSize));

    mDXTCompressData.miSrcWidth                = liTextureWidth;
    mDXTCompressData.miSrcHeight               = liTextureHeight;
    mDXTCompressData.miSrcPitch                = liSrcPitch;
    mDXTCompressData.miDstPitch                = liCmpPitch;
    mDXTCompressData.miQuality                 = liQuality;
    mDXTCompressData.mePixelFormat             = leSourceFormat;
    mDXTCompressData.mbInputIsUncompressedYUYV = lbInputIsUncompressedYUYV;

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
    CGS_ASSERT_AT(lpCompressedPixels,
                  "lpCompressedPixels", KAC_ARTIST_ASSERT_FILE, 423);
    CGS_ASSERT_AT(miWriteToSource < KI_NUM_IMAGE_BUFFERS,
                  "miWriteToSource < KI_NUM_IMAGE_BUFFERS", KAC_ARTIST_ASSERT_FILE, 424);
    CGS_ASSERT_AT(miWriteToSource >= 0,
                  "miWriteToSource >= 0", KAC_ARTIST_ASSERT_FILE, 425);
    CGS_ASSERT_AT(mDecodeCompleteCallback == 0,
                  "mDecodeCompleteCallback == NULL", KAC_ARTIST_ASSERT_FILE, 434);
    CGS_ASSERT_AT(miCompressedBufferSize >= liCompressedPixelSize,
                  "miCompressedBufferSize >= liCompressedPixelSize", KAC_ARTIST_ASSERT_FILE, 438);

    const s32 liDecodedSize = liCompressedPixelSize << 3;
    CGS_ASSERT_STREAM_AT(miUncompressedBufferSize >= liDecodedSize,
        "Uncompressed buffer is not large enough. DXT1 textures are 8 times smaller than their 32-bit ARGB equivalent\n",
        KAC_ARTIST_ASSERT_FILE, 441);

    XMemCpy(mapCompressedBuffers[miWriteToSource], lpCompressedPixels,
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
void NetworkTextureDXTCompress::Update()
{
    if (!mbRunningCompressionJob && mbNewImageToCompress)
    {
        mbRunningCompressionJob = true;
        miJobReadFromSource = miWriteToSource;
        mbNewImageToCompress = false;
        miWriteToSource = (miWriteToSource + 1) % KI_NUM_IMAGE_BUFFERS;

        mDXTCompressData.mpSrcPixels = mapUncompressedBuffers[miJobReadFromSource];
        mDXTCompressData.mpDstPixels = mapCompressedBuffers[miJobWriteToTexture];
        mDXTCompressJob.Clear();
        mDXTCompressJob.GetEntryPoint().SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
                                               reinterpret_cast<const void*>(&DXTCompressEntry), 0);
        mDXTCompressJob.SetData(&mDXTCompressData, sizeof(mDXTCompressData));
        mDXTCompressJob.GetEntryPoint().SetName("DXTCompressJob");
        CgsSystem::JobManager()->AddJobs(&mDXTCompressJob, 1);
        return;
    }

    if (mbRunningCompressionJob && mDXTCompressJob.IsDone())
    {
        miReadFromTexture = miJobWriteToTexture;
        mbRunningCompressionJob = false;
        miJobWriteToTexture = (miJobWriteToTexture + 1) % KI_NUM_IMAGE_BUFFERS;

        if (mCompressionCompleteCallback)
        {
            const CompressionCompleteCallback lCompressionCompleteCallback = mCompressionCompleteCallback;
            void* const lpUserData = mpCompressionCompleteData;
            mCompressionCompleteCallback = 0;
            mpCompressionCompleteData = 0;
            lCompressionCompleteCallback(mapCompressedBuffers[miReadFromTexture], lpUserData);
        }
        return;
    }

    if (!mbRunningDecodeJob && mbNewImageToDecode)
    {
        mbRunningDecodeJob = true;
        miJobReadFromSource = miWriteToSource;
        mbNewImageToDecode = false;
        miWriteToSource = (miWriteToSource + 1) % KI_NUM_IMAGE_BUFFERS;

        mDXTDecodeData.mpCompressedPixels = mapCompressedBuffers[miJobReadFromSource];
        mDXTDecodeData.mpDecodedPixels = mapUncompressedBuffers[miJobWriteToTexture];
        mDXTDecodeJob.Clear();
        mDXTDecodeJob.GetEntryPoint().SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
                                             reinterpret_cast<const void*>(&DXTDecodeEntry), 0);
        mDXTDecodeJob.SetData(&mDXTDecodeData, sizeof(mDXTDecodeData));
        mDXTDecodeJob.GetEntryPoint().SetName("DXTDecodeJob");
        CgsSystem::JobManager()->AddJobs(&mDXTDecodeJob, 1);
        return;
    }

    if (mbRunningDecodeJob && mDXTDecodeJob.IsDone())
    {
        miReadFromTexture = miJobWriteToTexture;
        mbRunningDecodeJob = false;
        miJobWriteToTexture = (miJobWriteToTexture + 1) % KI_NUM_IMAGE_BUFFERS;

        if (mDecodeCompleteCallback)
        {
            const CompressionCompleteCallback lDecodeCompleteCallback = mDecodeCompleteCallback;
            void* const lpDecodeCompleteData = mpDecodeCompleteData;
            mDecodeCompleteCallback = 0;
            mpDecodeCompleteData = 0;
            lDecodeCompleteCallback(mapUncompressedBuffers[miReadFromTexture], lpDecodeCompleteData);
        }
    }
}

} // namespace CgsNetwork
