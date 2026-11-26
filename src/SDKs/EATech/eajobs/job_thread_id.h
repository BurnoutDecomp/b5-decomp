#pragma once

#include "types.hpp"
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"

namespace EA { namespace Jobs {
typedef u64 JobThreadId;

// Header-inline: ARTIST holds these only as COMDAT copies inside the unoptimised
// PolygonSoupTester job TU, between its own functions (GetJobThreadId 0x829158F8 calls
// ToJobThreadId 0x82915920, whose body round-trips r3 through the stack), and the job manager's
// own object (0x82BC9830..0x82BCC6B0) has no copy: every optimised user inlines them.
inline JobThreadId ToJobThreadId(EA::Thread::ThreadId lThreadId)
{
    return static_cast<JobThreadId>(reinterpret_cast<uintptr_t>(lThreadId));
}

inline JobThreadId GetJobThreadId()
{
    return ToJobThreadId(EA::Thread::GetThreadId());
}
}}
