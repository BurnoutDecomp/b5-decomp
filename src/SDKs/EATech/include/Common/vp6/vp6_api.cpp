// On2 VP6 decoder: library set-up, the playback instance, the synchronous frame decode and the
// job-system decode path of the Xbox 360 build.

#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"
#include "SDKs/EATech/eajobs/job.h"
#include "SDKs/EATech/eajobs/job_scheduler.h"
#include "eathread/eathread_semaphore.h"

#include <cstring>
#include <new>

namespace
{
    // The library's EA::Thread::kTimeoutNone: block without a timeout.
    const unsigned int KU_TIMEOUT_NONE = 0xFFFFFFFFu;
}

extern "C"
{

// ---- allocation ----------------------------------------------------------------------------

void* duck_malloc(unsigned int size, int /*type*/)
{
    return vp6::Alloc(size);
}

// Over-allocate, align, and keep the block the allocator returned just below the aligned
// pointer so duck_freeAlign can hand it back.
void* duck_mallocAlign(unsigned int size, unsigned int align, int /*type*/)
{
    unsigned char* lpRaw = static_cast<unsigned char*>(vp6::Alloc(size + align + sizeof(void*)));
    unsigned char* lpAligned = reinterpret_cast<unsigned char*>(
        (reinterpret_cast<uintptr_t>(lpRaw) + align + sizeof(void*)) & ~static_cast<uintptr_t>(align - 1));
    reinterpret_cast<void**>(lpAligned)[-1] = lpRaw;
    return lpAligned;
}

void duck_free(void* ptr)
{
    vp6::Free(ptr);
}

void duck_freeAlign(void* ptr)
{
    vp6::Free(static_cast<void**>(ptr)[-1]);
}

// ---- library and instance ------------------------------------------------------------------

void VP6_VPInitLibrary(void)
{
    VP6_DMachineSpecificConfig();
    InitVPUtil();
}

void VP6_DeleteTmpBuffers(PB_INSTANCE* pbi)
{
    if (pbi->ReconDataBuffer)
        duck_freeAlign(pbi->ReconDataBuffer);
    if (pbi->PredictionBuffer)
        duck_freeAlign(pbi->PredictionBuffer);
    if (pbi->TmpDataBuffer)
        duck_freeAlign(pbi->TmpDataBuffer);
    if (pbi->FilterTmpBuffer)
        duck_freeAlign(pbi->FilterTmpBuffer);

    pbi->ReconDataBuffer = 0;
    pbi->PredictionBuffer = 0;
    pbi->TmpDataBuffer = 0;
    pbi->FilterTmpBuffer = 0;
}

int VP6_AllocateTmpBuffers(PB_INSTANCE* pbi)
{
    VP6_DeleteTmpBuffers(pbi);

    pbi->ReconDataBuffer = static_cast<short*>(duck_mallocAlign(64 * sizeof(short), 128, 0));
    if (!pbi->ReconDataBuffer)
    {
        VP6_DeleteTmpBuffers(pbi);
        return 0;
    }

    pbi->PredictionBuffer = static_cast<short*>(duck_mallocAlign(64 * sizeof(short), 128, 0));
    if (!pbi->PredictionBuffer)
    {
        VP6_DeleteTmpBuffers(pbi);
        return 0;
    }

    pbi->TmpDataBuffer = duck_mallocAlign(64 * sizeof(short), 128, 0);
    if (!pbi->TmpDataBuffer)
    {
        VP6_DeleteTmpBuffers(pbi);
        return 0;
    }

    pbi->FilterTmpBuffer = duck_mallocAlign(512, 128, 0);
    if (!pbi->FilterTmpBuffer)
    {
        VP6_DeleteTmpBuffers(pbi);
        return 0;
    }

    return 1;
}

void VP6_DeletePBInstance(xPB_INST* pbi)
{
    if (*pbi)
    {
        VP6_DeleteTmpBuffers(*pbi);
        VP6_DeleteQuantizer(&(*pbi)->quantizer);
        DeleteFrameInfoInstance(&(*pbi)->ReconFrameInfo);
    }

    duck_freeAlign(*pbi);
    *pbi = 0;
}

PB_INSTANCE* VP6_CreatePBInstance(void)
{
    CONFIG_TYPE lConfiguration = {};

    PB_INSTANCE* pbi = static_cast<PB_INSTANCE*>(duck_mallocAlign(sizeof(PB_INSTANCE), 16, 0));
    if (!pbi)
        return 0;

    memset(pbi, 0, sizeof(PB_INSTANCE));
    memcpy(&pbi->Configuration, &lConfiguration, sizeof(CONFIG_TYPE));

    if (!VP6_AllocateTmpBuffers(pbi))
    {
        duck_freeAlign(pbi);
        return 0;
    }

    pbi->PostProcessingLevel = 0;
    memset(pbi->DcProbs, 0, sizeof(pbi->DcProbs));
    memset(pbi->AcProbs, 0, sizeof(pbi->AcProbs));
    pbi->CreateCodeArrays = 0;
    return pbi;
}

void VP6_SetPbParam(xPB_INST pbi, PB_COMMAND_TYPE Command, uintptr_t Parameter)
{
    if (Command != PBC_SET_POSTPROC)
        return;
    if (Parameter == 9)
        return;
    pbi->PostProcessingLevel = static_cast<unsigned int>(Parameter);
}

// Describe the last reconstructed frame; the visible picture sits inside a 48-pixel luma
// (24-pixel chroma) border.
void VP6_GetYUVConfig(xPB_INST pbi, YUV_BUFFER_CONFIG* YuvConfig)
{
    YuvConfig->YWidth = pbi->Configuration.VideoFrameWidth;
    YuvConfig->YHeight = pbi->Configuration.VideoFrameHeight;
    YuvConfig->YStride = pbi->Configuration.YStride;
    YuvConfig->UVWidth = pbi->Configuration.VideoFrameWidth >> 1;
    YuvConfig->UVHeight = pbi->Configuration.VideoFrameHeight >> 1;
    YuvConfig->UVStride = pbi->Configuration.UVStride;

    YuvConfig->YBuffer = reinterpret_cast<char*>(pbi->LastFrameRecon + (pbi->Configuration.YStride + 1) * 48 +
                                                 pbi->ReconYDataOffset);
    YuvConfig->UBuffer = reinterpret_cast<char*>(pbi->LastFrameRecon + (pbi->Configuration.UVStride + 1) * 24 +
                                                 pbi->ReconUDataOffset);
    YuvConfig->VBuffer = reinterpret_cast<char*>(pbi->LastFrameRecon + (pbi->Configuration.UVStride + 1) * 24 +
                                                 pbi->ReconVDataOffset);
    YuvConfig->YBufferStart = reinterpret_cast<char*>(pbi->LastFrameRecon + pbi->ReconYDataOffset);

    ClearSysState();
}

// Decode one compressed frame into the reconstruction buffers. On the job path this runs as the
// decode job; the frame-buffer rotation waits until the previous picture's completion callback
// has released it.
int VP6_DecodeFrameToYUV(xPB_INST pbi, char* VideoBufferPtr, unsigned int /*ByteCount*/,
                         u32 /*ImageWidth*/, u32 /*ImageHeight*/)
{
    const unsigned char* lpBuffer = reinterpret_cast<const unsigned char*>(VideoBufferPtr);

    InitHeaderBuffer(&pbi->HeaderBits, lpBuffer);
    if (!VP6_LoadFrame(pbi))
        return -1;

    if (pbi->UseHuffman)
    {
        pbi->br3.BitsLeft = 0;
        pbi->br3.Value = 0;
        pbi->br3.Position = lpBuffer + pbi->Buff2Offset;
    }
    else
    {
        VP6_StartDecode(&pbi->br2, lpBuffer + pbi->Buff2Offset);
    }

    VP6_DecodeFrameMbs(pbi);

    pbi->FrameReleasedSemaphore->Wait(&KU_TIMEOUT_NONE);

    // The new picture becomes the last frame; the next one is built in the spare buffer when the
    // golden-frame swap left one, otherwise in the old last frame.
    unsigned char* lpSpareFrame = pbi->SpareFrame;
    unsigned char* lpOldLastFrame = pbi->LastFrameRecon;
    pbi->LastFrameRecon = pbi->ThisFrameRecon;
    pbi->ThisFrameRecon = lpSpareFrame ? lpSpareFrame : lpOldLastFrame;
    pbi->SpareFrame = 0;

    UpdateUMVBorder(pbi->ReconFrameInfo, pbi->LastFrameRecon);

    if (pbi->FrameType == 0 || pbi->RefreshGoldenFrame)
    {
        pbi->SpareFrame = pbi->GoldenFrame;
        pbi->GoldenFrame = pbi->LastFrameRecon;
    }

    ClearSysState();
    return 0;
}

// Completion job: hand the finished picture to the caller's callback, then release the frame.
void DecodeCallBackJob(EA::Jobs::Param, EA::Jobs::Param lData, EA::Jobs::Param, EA::Jobs::Param)
{
    VP6_CALLBACK_JOB_DATA* lpData = static_cast<VP6_CALLBACK_JOB_DATA*>(lData.mpValue);
    YUV_BUFFER_CONFIG lYuvConfig;

    VP6_GetYUVConfig(lpData->pbi, &lYuvConfig);
    lpData->pbi->JobSlotSemaphore->Post(1);

    lpData->Callback(lpData->FrameNumber, lYuvConfig.YBufferStart, lYuvConfig.YWidth, lYuvConfig.YHeight,
                     lpData->Context, lpData->Param0, lpData->Param1);

    lpData->pbi->FrameReleasedSemaphore->Post(1);
}

// Decode job.
void XenonDecodeJob(EA::Jobs::Param, EA::Jobs::Param lData, EA::Jobs::Param, EA::Jobs::Param)
{
    VP6_DECODE_JOB_DATA* lpData = static_cast<VP6_DECODE_JOB_DATA*>(lData.mpValue);
    VP6_DecodeFrameToYUV(lpData->pbi, lpData->VideoBuffer, lpData->ByteCount, lpData->ImageWidth,
                         lpData->ImageHeight);
}

// Queue one frame on the job system: its decode job, then a completion job that reports the
// picture through Callback. The two pairs alternate, so frame N+2 first waits for frame N's
// completion job.
int VP6_DecodeFrameToYUV_JOB(xPB_INST pbi, char* VideoBufferPtr, unsigned int ByteCount,
                             u32 /*ImageWidth*/, u32 /*ImageHeight*/, int FrameNumber,
                             VP6_DECODE_CALLBACK Callback, void* Context, int Param0, int Param1)
{
    const int liPair = pbi->CurrentJob;
    EA::Jobs::Job* lpDecodeJob = &pbi->Jobs[liPair * 2];
    EA::Jobs::Job* lpCallbackJob = &pbi->Jobs[liPair * 2 + 1];

    if (FrameNumber >= 2)
        lpCallbackJob->SleepOn();

    lpDecodeJob->Clear();
    lpCallbackJob->Clear();

    VP6_DECODE_JOB_DATA* lpDecodeData = &pbi->DecodeJobData[liPair];
    lpDecodeData->pbi = pbi;
    lpDecodeData->VideoBuffer = VideoBufferPtr;
    lpDecodeData->ByteCount = ByteCount;
    lpDecodeData->FrameNumber = FrameNumber;

    VP6_CALLBACK_JOB_DATA* lpCallbackData = &pbi->CallbackJobData[liPair];
    lpCallbackData->pbi = pbi;
    lpCallbackData->FrameNumber = FrameNumber;
    lpCallbackData->Callback = Callback;
    lpCallbackData->Context = Context;
    lpCallbackData->Param0 = Param0;
    lpCallbackData->Param1 = Param1;

    lpDecodeJob->SetData(lpDecodeData, sizeof(VP6_DECODE_JOB_DATA));
    lpDecodeJob->SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL, reinterpret_cast<const void*>(&XenonDecodeJob), 0);
    lpDecodeJob->GetEntryPoint().SetAffinity(static_cast<EA::Jobs::JobAffinity>(pbi->JobAffinity));

    lpCallbackJob->SetData(lpCallbackData, sizeof(VP6_CALLBACK_JOB_DATA));
    lpCallbackJob->SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL, reinterpret_cast<const void*>(&DecodeCallBackJob), 0);
    lpCallbackJob->SetAllowSleepOn(true);
    lpCallbackJob->DependsOn(*lpDecodeJob, EA::Jobs::Event::EVENT_WHEN_JOB_END);

    if (FrameNumber == 0)
    {
        pbi->JobSlotSemaphore->Post(1);
        pbi->FrameReleasedSemaphore->Post(1);
    }

    pbi->JobSlotSemaphore->Wait(&KU_TIMEOUT_NONE);
    pbi->JobScheduler->AddJobs(lpDecodeJob, 2);

    pbi->CurrentJob = (pbi->CurrentJob + 1) % 2;
    return 0;
}

int VP6_StopDecoder(xPB_INST* pbi)
{
    if (*pbi)
    {
        if ((*pbi)->UseJobs)
        {
            for (int liPair = 0; liPair < 2; ++liPair)
            {
                EA::Jobs::Job* lpCallbackJob = &(*pbi)->Jobs[liPair * 2 + 1];
                if (lpCallbackJob->GetAllowSleepOn())
                    lpCallbackJob->SleepOn();
            }

            for (int liPair = 0; liPair < 2; ++liPair)
            {
                (*pbi)->Jobs[liPair * 2].~Job();
                (*pbi)->Jobs[liPair * 2 + 1].~Job();
            }

            duck_freeAlign((*pbi)->Jobs);
            (*pbi)->Jobs = 0;
            duck_freeAlign((*pbi)->CallbackJobData);
            (*pbi)->CallbackJobData = 0;
            duck_freeAlign((*pbi)->DecodeJobData);
            (*pbi)->DecodeJobData = 0;

            (*pbi)->JobSlotSemaphore->~Semaphore();
            duck_freeAlign((*pbi)->JobSlotSemaphoreMemory);
            (*pbi)->JobSlotSemaphoreMemory = 0;

            (*pbi)->FrameReleasedSemaphore->~Semaphore();
            duck_freeAlign((*pbi)->FrameReleasedSemaphoreMemory);
            (*pbi)->FrameReleasedSemaphoreMemory = 0;
        }

        (*pbi)->JobScheduler = 0;
        VP6_DeleteQuantizer(&(*pbi)->quantizer);
        DeleteFrameInfoInstance(&(*pbi)->ReconFrameInfo);
        VP6_DeleteFragmentInfo(*pbi);
        VP6_DeleteFrameInfo(*pbi);
        VP6_DeletePBInstance(pbi);
    }

    return 1;
}

}
