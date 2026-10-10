// On2 VP6 decoder: decoder creation. Kept in its own unit because it is the one entry point that
// places EA::Thread semaphores through EA::Thread::SemaphoreFactory.

#include "SDKs/EATech/include/Common/vp6/vp6_decoder.h"
#include "SDKs/EATech/eajobs/job.h"
#include "eathread/eathread_semaphore.h"

#include <new>

extern "C"
{

int VP6_StartDecoder(xPB_INST* pbi, u32 ImageWidth, u32 ImageHeight, bool UseJobs,
                     EA::Jobs::JobScheduler* Scheduler, int Affinity)
{
    *pbi = VP6_CreatePBInstance();
    (*pbi)->Configuration.VideoFrameWidth = ImageWidth;
    (*pbi)->Configuration.VideoFrameHeight = ImageHeight;

    (*pbi)->ReconFrameInfo = CreateFrameInfoInstance(&(*pbi)->Configuration);
    (*pbi)->quantizer = VP6_CreateQuantizer();

    if (!VP6_InitFrameDetails(*pbi))
    {
        VP6_DeletePBInstance(pbi);
        return 0;
    }

    (*pbi)->quantizer->LastFrameQuantizerValue = 0;

    if (UseJobs)
    {
        (*pbi)->UseJobs = 1;
        (*pbi)->CurrentJob = 0;
        (*pbi)->JobScheduler = Scheduler;
        (*pbi)->JobAffinity = Affinity;
        (*pbi)->CurrentJob = 0;

        // Two job pairs: a decode job and the completion job that waits on it.
        (*pbi)->Jobs = static_cast<EA::Jobs::Job*>(duck_mallocAlign(4 * sizeof(EA::Jobs::Job), 16, 0));
        for (int liPair = 0; liPair < 2; ++liPair)
        {
            new (&(*pbi)->Jobs[liPair * 2]) EA::Jobs::Job("VP6 Decode Job");
            new (&(*pbi)->Jobs[liPair * 2 + 1]) EA::Jobs::Job("VP6 Data Transfer Job");
        }

        (*pbi)->CallbackJobData = static_cast<VP6_CALLBACK_JOB_DATA*>(
            duck_mallocAlign(2 * sizeof(VP6_CALLBACK_JOB_DATA), 16, 0));
        (*pbi)->DecodeJobData = static_cast<VP6_DECODE_JOB_DATA*>(
            duck_mallocAlign(2 * sizeof(VP6_DECODE_JOB_DATA), 16, 0));

        (*pbi)->JobSlotSemaphoreMemory =
            duck_mallocAlign(static_cast<unsigned int>(EA::Thread::SemaphoreFactory::GetSemaphoreSize()), 16, 0);
        (*pbi)->JobSlotSemaphore = EA::Thread::SemaphoreFactory::ConstructSemaphore((*pbi)->JobSlotSemaphoreMemory);

        (*pbi)->FrameReleasedSemaphoreMemory =
            duck_mallocAlign(static_cast<unsigned int>(EA::Thread::SemaphoreFactory::GetSemaphoreSize()), 16, 0);
        (*pbi)->FrameReleasedSemaphore =
            EA::Thread::SemaphoreFactory::ConstructSemaphore((*pbi)->FrameReleasedSemaphoreMemory);
    }
    else
    {
        (*pbi)->UseJobs = 0;
    }

    return 1;
}

}
