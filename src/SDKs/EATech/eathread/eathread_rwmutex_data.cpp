#include "SDKs/EATech/eathread/eathread_rwmutex_data.h"

// ARTIST82B43C18: these objects initialize even when the enclosing RWMutex
// defers its second initialization pass.
EARWMutexData::EARWMutexData()
    : mnReadWaiters(0), mnWriteWaiters(0), mnReaders(0), mThreadIdWriter(0)
    , mMutex(0, true), mReadCondition(), mWriteCondition()
{
}
