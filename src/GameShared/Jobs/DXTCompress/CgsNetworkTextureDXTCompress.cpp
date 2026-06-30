// CgsNetwork::NetworkTextureDXTCompress
//
// Manages two parallel EA::Jobs job lanes for DXT compression and DXT decode.
// Each lane owns an EA::Jobs::Job slot and a 128-byte descriptor (DXTCompressData /
// DXTDecodeData) committed via Job::SetData.  The double-buffer scheme lets a new
// image be queued while the previous job is still in flight.
//
// X360 function addresses:
//   Construct              @ 0x8287E938
//   Destruct               @ 0x8287E9D8
//   Prepare                @ 0x8287EA78
//   Update                 @ 0x8287ECC0
//   SetNewTextureToCompress   @ 0x8287EF60
//   SetNewTextureToDecompress @ 0x8287F0B8

#include "GameShared/Jobs/DXTCompress/CgsNetworkTextureDXTCompress.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"
#include "GameShared/GameClasses/Containers/CgsPriorityQueue.h"
#include "SDKs/EATech/eajobs/entry_point.h"
#include "ppmalloc/EAGeneralAllocator.h"

#include <cstring>   // memset / memcpy

// ---------------------------------------------------------------------------
// HIDWORD — lvalue macro for the high 32 bits of a 64-bit local.  Used in the
// StrStreamBase-on-stack pattern inside the overflow assert block.
// ---------------------------------------------------------------------------
#define HIDWORD(x) (*reinterpret_cast<u32*>(reinterpret_cast<u8*>(&(x)) + 4u))

// ---------------------------------------------------------------------------
// File-scope externs
// ---------------------------------------------------------------------------

// Process-wide job scheduler (unk_830EA650); defined in CgsHardwareInitPS3.cpp.
extern EA::Jobs::JobScheduler gJobManager;

// StrStreamBase vtable pointers (X360 .rodata) used in the overflow assert path.
extern u32 off_82000D00;
extern u32 off_82000D08;

// Job entry-point functions defined in the DXTCompress / DXTDecode codec TUs.
extern void DXTCompressEntry(EA::Jobs::Param, EA::Jobs::Param,
                              EA::Jobs::Param, EA::Jobs::Param);
extern void DXTDecodeEntry(EA::Jobs::Param, EA::Jobs::Param,
                            EA::Jobs::Param, EA::Jobs::Param);

// ---------------------------------------------------------------------------
static const char KPC_SRC[] =
    "d:\\p4\\b5_main\\burnout\\main\\code\\GameShared\\Jobs\\DXTCompress\\"
    "CgsNetworkTextureDXTCompress.cpp";

namespace CgsNetwork
{

// ---------------------------------------------------------------------------
// Construct @ 0x8287E938
//
// Zero-initialises all buffer pointers, clears both job slots, and zeroes the
// remaining state fields.
// ---------------------------------------------------------------------------
void NetworkTextureDXTCompress::Construct()
{
    for (s32 li = 0; li < 2; ++li)
    {
        mapUncompressedBuffers[li] = nullptr;
        mapCompressedBuffers[li]   = nullptr;
    }

    mDXTCompressJob.EA::Jobs::Job::Clear();
    mDXTDecodeJob.EA::Jobs::Job::Clear();

    miWriteToSource       = 0;
    miJobReadFromSource   = 0;
    miJobWriteToTexture   = 0;
    miReadFromTexture     = 0;

    mbRunningCompressionJob = false;
    mbNewImageToCompress    = false;
    mbRunningDecodeJob      = false;
    mbNewImageToDecode      = false;

    mpHeapMalloc                   = nullptr;
    mCompressionCompleteCallback   = nullptr;
    mpCompressionCompleteData      = nullptr;
    mDecodeCompleteCallback        = nullptr;
    mpDecodeCompleteData           = nullptr;
    miUncompressedBufferSize       = 0;
    miCompressedBufferSize         = 0;
    return;
}

// ---------------------------------------------------------------------------
// Destruct @ 0x8287E9D8
//
// Clears both job slots and zeroes all state, mirroring Construct.
// ---------------------------------------------------------------------------
void NetworkTextureDXTCompress::Destruct()
{
    for (s32 li = 0; li < 2; ++li)
    {
        mapUncompressedBuffers[li] = nullptr;
        mapCompressedBuffers[li]   = nullptr;
    }

    mDXTCompressJob.EA::Jobs::Job::Clear();
    mDXTDecodeJob.EA::Jobs::Job::Clear();

    miWriteToSource       = 0;
    miJobReadFromSource   = 0;
    miJobWriteToTexture   = 0;
    miReadFromTexture     = 0;

    mbRunningCompressionJob = false;
    mbNewImageToCompress    = false;
    mbRunningDecodeJob      = false;
    mbNewImageToDecode      = false;

    mpHeapMalloc                   = nullptr;
    mCompressionCompleteCallback   = nullptr;
    mpCompressionCompleteData      = nullptr;
    mDecodeCompleteCallback        = nullptr;
    mpDecodeCompleteData           = nullptr;
    miUncompressedBufferSize       = 0;
    miCompressedBufferSize         = 0;
    return;
}

// ---------------------------------------------------------------------------
// Prepare @ 0x8287EA78
//
// Stores the HeapMalloc and buffer-size parameters, then allocates two
// uncompressed and two compressed pixel buffers (double-buffered, 128-byte
// aligned).  Each allocation is bracketed by heap-validation asserts and
// followed by a non-null assert.
// ---------------------------------------------------------------------------
bool NetworkTextureDXTCompress::Prepare(CgsMemory::HeapMalloc* lpHeapMalloc,
                                         s32 liUncompressedBufferSize,
                                         s32 liCompressedBufferSize)
{
    if (!lpHeapMalloc)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpHeapMalloc != NULL", KPC_SRC, 78);
        CgsDev::Assert::EndAssert();
    }

    mpHeapMalloc             = lpHeapMalloc;
    miUncompressedBufferSize = liUncompressedBufferSize;
    miCompressedBufferSize   = liCompressedBufferSize;

    EA::Allocator::GeneralAllocator* const lpAllocator = mpHeapMalloc->GetAllocator();

    for (s32 liBufferIndex = 0; liBufferIndex < 2; ++liBufferIndex)
    {
        if (!lpAllocator->EA::Allocator::GeneralAllocator::ValidateHeap(
                EA::Allocator::GeneralAllocator::kHeapValidationLevelFull))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "mpHeapMalloc->GetAllocator()->ValidateHeap(kHeapValidationLevelFull)",
                KPC_SRC, 95);
            CgsDev::Assert::EndAssert();
        }

        mapUncompressedBuffers[liBufferIndex] = static_cast<char*>(
            mpHeapMalloc->CgsMemory::HeapMalloc::Malloc(miUncompressedBufferSize, 128));

        if (!lpAllocator->EA::Allocator::GeneralAllocator::ValidateHeap(
                EA::Allocator::GeneralAllocator::kHeapValidationLevelFull))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "mpHeapMalloc->GetAllocator()->ValidateHeap(kHeapValidationLevelFull)",
                KPC_SRC, 101);
            CgsDev::Assert::EndAssert();
        }

        if (!mapUncompressedBuffers[liBufferIndex])
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "mapUncompressedBuffers[liBufferIndex] != NULL", KPC_SRC, 106);
            CgsDev::Assert::EndAssert();
        }

        if (!lpAllocator->EA::Allocator::GeneralAllocator::ValidateHeap(
                EA::Allocator::GeneralAllocator::kHeapValidationLevelFull))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "mpHeapMalloc->GetAllocator()->ValidateHeap(kHeapValidationLevelFull)",
                KPC_SRC, 113);
            CgsDev::Assert::EndAssert();
        }

        mapCompressedBuffers[liBufferIndex] = static_cast<char*>(
            mpHeapMalloc->CgsMemory::HeapMalloc::Malloc(miCompressedBufferSize, 128));

        if (!lpAllocator->EA::Allocator::GeneralAllocator::ValidateHeap(
                EA::Allocator::GeneralAllocator::kHeapValidationLevelFull))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "mpHeapMalloc->GetAllocator()->ValidateHeap(kHeapValidationLevelFull)",
                KPC_SRC, 119);
            CgsDev::Assert::EndAssert();
        }

        if (!mapCompressedBuffers[liBufferIndex])
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "mapCompressedBuffers[liBufferIndex] != NULL", KPC_SRC, 124);
            CgsDev::Assert::EndAssert();
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// SetNewTextureToCompress @ 0x8287EF60
//
// Validates the source pointer, the write-index bounds, that no callback is
// already pending, and that the source size fits the allocated buffer.  Then
// DMA-copies the source pixels into the current write-slot and records the job
// parameters for the next Update tick.
// ---------------------------------------------------------------------------
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
    CompressCallback lCompressionCompleteCallback,
    void*            lpCompressionCompleteData)
{
    if (!lpNewSourcePixels)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpNewSourcePixels != NULL", KPC_SRC, 175);
        CgsDev::Assert::EndAssert();
    }

    if (miWriteToSource >= 2)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "miWriteToSource < KI_NUM_IMAGE_BUFFERS", KPC_SRC, 180);
        CgsDev::Assert::EndAssert();
    }

    if (miWriteToSource < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("miWriteToSource >= 0", KPC_SRC, 185);
        CgsDev::Assert::EndAssert();
    }

    if (mCompressionCompleteCallback)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "mCompressionCompleteCallback == NULL", KPC_SRC, 190);
        CgsDev::Assert::EndAssert();
    }

    if (liNewSourcePixelsSize > miUncompressedBufferSize)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liNewSourcePixelsSize <= miUncompressedBufferSize", KPC_SRC, 195);
        CgsDev::Assert::EndAssert();
    }

    memcpy(mapUncompressedBuffers[miWriteToSource],
           lpNewSourcePixels,
           liNewSourcePixelsSize);

    mDXTCompressData.lpUncompressedPixels    = mapUncompressedBuffers[miWriteToSource];
    mDXTCompressData.miTextureWidth          = liTextureWidth;
    mDXTCompressData.miTextureHeight         = liTextureHeight;
    mDXTCompressData.miSrcPitch              = liSrcPitch;
    mDXTCompressData.miCmpPitch              = liCmpPitch;
    mDXTCompressData.miQuality               = liQuality;
    mDXTCompressData.leSourceFormat          = leSourceFormat;
    mDXTCompressData.lbInputIsUncompressedYUYV = lbInputIsUncompressedYUYV;

    mCompressionCompleteCallback = lCompressionCompleteCallback;
    mpCompressionCompleteData    = lpCompressionCompleteData;
    mbNewImageToCompress         = true;
    return;
}

// ---------------------------------------------------------------------------
// SetNewTextureToDecompress @ 0x8287F0B8
//
// Validates the compressed pixel pointer, write-index bounds, callback vacancy,
// and buffer capacities.  The buffer-too-small check uses the familiar
// StrStreamBase-on-stack pattern (HIDWORD + BasePriorityQueue::Clear) to format
// the assert message; on PC this reduces to a plain FireAssert string.
// Copies the compressed data into the current compressed write-slot and records
// the job parameters.
// ---------------------------------------------------------------------------
void NetworkTextureDXTCompress::SetNewTextureToDecompress(
    char*            lpCompressedPixels,
    s32              liCompressedPixelSize,
    s32              liCompressedWidth,
    s32              liCompressedHeight,
    CompressCallback lDecodeCompleteCallback,
    void*            lpDecodeCompleteData)
{
    if (!lpCompressedPixels)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpCompressedPixels != NULL", KPC_SRC, 240);
        CgsDev::Assert::EndAssert();
    }

    if (miWriteToSource >= 2)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "miWriteToSource < KI_NUM_IMAGE_BUFFERS", KPC_SRC, 245);
        CgsDev::Assert::EndAssert();
    }

    if (miWriteToSource < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("miWriteToSource >= 0", KPC_SRC, 250);
        CgsDev::Assert::EndAssert();
    }

    if (mDecodeCompleteCallback)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "mDecodeCompleteCallback == NULL", KPC_SRC, 255);
        CgsDev::Assert::EndAssert();
    }

    if (liCompressedPixelSize > miCompressedBufferSize)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "liCompressedPixelSize <= miCompressedBufferSize", KPC_SRC, 260);
        CgsDev::Assert::EndAssert();
    }

    if (miUncompressedBufferSize < 8 * liCompressedPixelSize)
    {
        // StrStreamBase-on-stack overflow assert (X360 pattern: HIDWORD/Clear/vtable).
        CgsDev::Assert::BeginAssert();
        u64 lv21 = 0u;
        HIDWORD(lv21) = off_82000D00;
        reinterpret_cast<CgsContainers::BasePriorityQueue*>(&lv21)->
            CgsContainers::BasePriorityQueue::Clear();
        HIDWORD(lv21) = off_82000D08;
        (void)HIDWORD(lv21);  // X360: (*(HIDWORD(lv21)+4))(&lv21, "Uncompressed buffer...")
        CgsDev::Assert::FireAssert(
            "Uncompressed buffer is not large enough for decoded image", KPC_SRC, 268);
        CgsDev::Assert::EndAssert();
    }

    memcpy(mapCompressedBuffers[miWriteToSource],
           lpCompressedPixels,
           liCompressedPixelSize);

    mDXTDecodeData.lpCompressedPixels   = mapCompressedBuffers[miWriteToSource];
    mDXTDecodeData.miCompressedPixelSize = liCompressedPixelSize;
    mDXTDecodeData.miUncompressedSize    = 8 * liCompressedPixelSize;
    mDXTDecodeData.miCompressedWidth     = liCompressedWidth;
    mDXTDecodeData.miCompressedHeight    = liCompressedHeight;

    mDecodeCompleteCallback = lDecodeCompleteCallback;
    mpDecodeCompleteData    = lpDecodeCompleteData;
    mbNewImageToDecode      = true;
    return;
}

// ---------------------------------------------------------------------------
// Update @ 0x8287ECC0
//
// Per-tick state machine.  Compress lane: if idle and a new image is queued,
// launch a compress job; if a job is running and completes, fire the callback.
// Decode lane: same pattern.  The two lanes are independent; both are serviced
// every tick.
// ---------------------------------------------------------------------------
void NetworkTextureDXTCompress::Update()
{
    // ---- Compress lane --------------------------------------------------

    if (mbRunningCompressionJob)
        goto LABEL_COMPRESS_DONE;

    if (mbNewImageToCompress)
    {
        mDXTCompressData.lpCompressedPixels =
            mapCompressedBuffers[miJobWriteToTexture];

        mDXTCompressJob.EA::Jobs::Job::Clear();
        mDXTCompressJob.mEntryPoint.EA::Jobs::EntryPoint::SetCode(DXTCompressEntry);
        mDXTCompressJob.EA::Jobs::Job::SetData(&mDXTCompressData, 128);
        mDXTCompressJob.mEntryPoint.EA::Jobs::EntryPoint::SetName("DXTCompressJob");
        gJobManager.EA::Jobs::JobScheduler::AddJobs(&mDXTCompressJob, 1);

        mbRunningCompressionJob = true;
        mbNewImageToCompress    = false;
        goto LABEL_4;
    }

    if (mbRunningCompressionJob)
    {
LABEL_COMPRESS_DONE:
        if (mDXTCompressJob.EA::Jobs::Job::IsDone())
        {
            mbRunningCompressionJob = false;
            miReadFromTexture       = miJobWriteToTexture;
            miJobWriteToTexture     = miWriteToSource;
            miWriteToSource         = miJobReadFromSource;
            miJobReadFromSource     = miReadFromTexture;

            if (!mCompressionCompleteCallback)
                goto LABEL_4;

            mCompressionCompleteCallback(
                mapUncompressedBuffers[miReadFromTexture],
                mpCompressionCompleteData);
            mCompressionCompleteCallback = nullptr;
            mpCompressionCompleteData    = nullptr;
            goto LABEL_4;
        }
    }

    // ---- Decode lane ----------------------------------------------------

    if (!mbRunningDecodeJob)
    {
        if (mbNewImageToDecode)
        {
            mDXTDecodeData.lpUncompressedPixels =
                mapUncompressedBuffers[miJobWriteToTexture];

            mDXTDecodeJob.EA::Jobs::Job::Clear();
            mDXTDecodeJob.mEntryPoint.EA::Jobs::EntryPoint::SetCode(DXTDecodeEntry);
            mDXTDecodeJob.EA::Jobs::Job::SetData(&mDXTDecodeData, 128);
            mDXTDecodeJob.mEntryPoint.EA::Jobs::EntryPoint::SetName("DXTDecodeJob");
            gJobManager.EA::Jobs::JobScheduler::AddJobs(&mDXTDecodeJob, 1);

            mbRunningDecodeJob = true;
            mbNewImageToDecode = false;
            goto LABEL_4;
        }

        if (!mbRunningDecodeJob)
            goto LABEL_4;
    }

    if (mDXTDecodeJob.EA::Jobs::Job::IsDone())
    {
        mbRunningDecodeJob = false;

        if (mDecodeCompleteCallback)
        {
            mDecodeCompleteCallback(
                mapCompressedBuffers[miReadFromTexture],
                mpDecodeCompleteData);
            mDecodeCompleteCallback = nullptr;
            mpDecodeCompleteData    = nullptr;
        }
    }

LABEL_4: return;
}

} // namespace CgsNetwork

#undef HIDWORD
