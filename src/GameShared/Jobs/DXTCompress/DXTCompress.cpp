// The DXT compression job: the entry point and the job body.

#include "GameShared/Jobs/DXTCompress/DXTCompress.h"
#include "GameShared/Jobs/DXTCompress/DXTCompressAlgorithmFast.h"   // fdxt

#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT

DXTCompressJob gaDXTCompressJobs[KI_NUM_DXT_COMPRESS_JOBS];

// Spill the four params, resolve this job thread's context out of the six-entry array and run
// it over the job data block (the second param).
void DXTCompressEntry(EA::Jobs::Param laParam0,
                      EA::Jobs::Param laParam1,
                      EA::Jobs::Param laParam2,
                      EA::Jobs::Param laParam3)
{
    (void)laParam0;
    (void)laParam2;
    (void)laParam3;

    // ---- [PC platform layer] the job-thread index ----
    // The console derives it from the hardware thread id. This build runs the job body on the
    // thread that dispatches it (see NetworkTextureDXTCompress::Update), so the index is the
    // first context. Only that one computation is replaced; the bounds check, the six-entry
    // context array and the Execute call are the console's. Same precedent as RelocatorEntry
    // and TrafficJobEntry.
    const s32 liJobThreadIndex = 0;

    CGS_ASSERT(liJobThreadIndex < KI_NUM_DXT_COMPRESS_JOBS, "SPU Id out of range: ");

    gaDXTCompressJobs[liJobThreadIndex].Execute(static_cast<DXTCompressData*>(laParam1.mpValue));
}

// Compress the whole source image. A 32-bit source flagged as colour-only first has every
// pixel's alpha byte forced to 0xFF (so no block drops into three-colour mode). Then each band
// of four scan lines goes to fdxt, colour block only, at the requested quality.
void DXTCompressJob::Execute(DXTCompressData* lpData)
{
    mpData = lpData;

    char* lpSrc = lpData->mpSrcPixels;
    char* lpDst = lpData->mpDstPixels;

    const s32 liSrcSize = (lpData->miSrcHeight * lpData->miSrcWidth) << 2;
    if (lpData->mePixelFormat == renderengine::PIXELFORMAT_X8R8G8B8 && !lpData->mbInputIsUncompressedYUYV &&
        liSrcSize > 0)
    {
        char* lpAlpha = lpSrc;
        for (u32 luPixels = ((static_cast<u32>(liSrcSize) - 1) >> 2) + 1; luPixels != 0; --luPixels)
        {
            *lpAlpha = static_cast<char>(0xFF);
            lpAlpha += 4;
        }
    }

    for (s32 liLine = 0; liLine < mpData->miSrcHeight; liLine += 4)
    {
        fdxt(reinterpret_cast<u8*>(lpDst), reinterpret_cast<const u8*>(lpSrc), mpData->miSrcPitch,
             mpData->miSrcWidth / 4, 0, mpData->miQuality, static_cast<s32>(mpData->mePixelFormat),
             mpData->mbInputIsUncompressedYUYV);

        lpDst += mpData->miDstPitch;
        lpSrc += mpData->miSrcPitch * 4;
    }
}
