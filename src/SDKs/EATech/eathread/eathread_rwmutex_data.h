#ifndef EA_THREAD_EARWMUTEXDATA_H
#define EA_THREAD_EARWMUTEXDATA_H

#include "SDKs/EATech/eathread/eathread_condition.h"

// ARTIST EARWMutexData::EARWMutexData82B43C18.
struct EARWMutexData
{
    s32 mnReadWaiters;
    s32 mnWriteWaiters;
    s32 mnReaders;
    EA::Thread::ThreadId mThreadIdWriter;
    EA::Thread::Mutex mMutex;
    EA::Thread::Condition mReadCondition;
    EA::Thread::Condition mWriteCondition;

    EARWMutexData();

private:
    EARWMutexData(const EARWMutexData&);
    EARWMutexData& operator=(const EARWMutexData&);
};

CGS_STATIC_ASSERT((offsetof(EARWMutexData, mMutex) == 0x10), "ARTIST RW mutex");
CGS_STATIC_ASSERT((offsetof(EARWMutexData, mReadCondition) == 0x40), "ARTIST reader condition");
CGS_STATIC_ASSERT((offsetof(EARWMutexData, mWriteCondition) == 0xA0), "ARTIST writer condition");
CGS_STATIC_ASSERT((sizeof(EARWMutexData) == 0x100), "ARTIST RW data size");

#endif
