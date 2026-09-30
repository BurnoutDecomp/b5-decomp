#ifndef EA_JOBS_PROFILING_H
#define EA_JOBS_PROFILING_H

#include "SDKs/EATech/eajobs/entry_point.h"

namespace EA { namespace Jobs {
    // DecFIGS profiling.h:21; ARTIST82BCB938..974 stores these four qwords,
    // followed by EntryPoint. Native pointers retain their full width.
    struct JobMetrics
    {
        u64 ticksAtSubmission;
        u64 ticksAtBegin;
        u64 ticksAtEnd;
        u64 threadId;
        EntryPoint entryPoint;
    };
    typedef void ProfilerCallback(const JobMetrics* pMetrics, int iCount, void* pContext);
} }

#endif
