#ifndef EA_THREAD_EABARRIERDATA_H
#define EA_THREAD_EABARRIERDATA_H

#include "SDKs/EATech/eathread/eathread_atomic.h"
#include "SDKs/EATech/eathread/eathread_semaphore.h"

// ARTIST EABarrierData::EABarrierData82B43A50.
struct EABarrierData
{
    EA::Thread::AtomicInt32 mnCurrent;
    s32 mnHeight;
    EA::Thread::AtomicInt32 mnIndex;
    EA::Thread::Semaphore mSemaphore0;
    EA::Thread::Semaphore mSemaphore1;

    EABarrierData();

private:
    EABarrierData(const EABarrierData&);
    EABarrierData& operator=(const EABarrierData&);
};

CGS_STATIC_ASSERT((offsetof(EABarrierData, mnIndex) == 8), "ARTIST barrier phase");
CGS_STATIC_ASSERT((offsetof(EABarrierData, mSemaphore0) == 0xC), "ARTIST barrier semaphore0");
CGS_STATIC_ASSERT((offsetof(EABarrierData, mSemaphore1) == 0x1C), "ARTIST barrier semaphore1");
CGS_STATIC_ASSERT((sizeof(EABarrierData) == 0x2C), "ARTIST barrier data");

#endif
