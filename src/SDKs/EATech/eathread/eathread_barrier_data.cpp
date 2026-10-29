#include "SDKs/EATech/eathread/eathread_barrier_data.h"

// ARTIST82B43A50: atomic zeroes precede deferred semaphore construction.
EABarrierData::EABarrierData()
    : mnCurrent(0), mnHeight(0), mnIndex(0)
    , mSemaphore0(0, false), mSemaphore1(0, false)
{
}
