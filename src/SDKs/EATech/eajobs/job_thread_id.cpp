#include "SDKs/EATech/eajobs/job_thread_id.h"

namespace EA { namespace Jobs {
JobThreadId ToJobThreadId(EA::Thread::ThreadId lThreadId)
{
    return static_cast<JobThreadId>(reinterpret_cast<uintptr_t>(lThreadId));
}

JobThreadId GetJobThreadId()
{
    return ToJobThreadId(EA::Thread::GetThreadId());
}
}}
