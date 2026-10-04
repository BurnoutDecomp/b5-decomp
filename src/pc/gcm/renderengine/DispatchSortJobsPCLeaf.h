#pragma once

#include "GameShared/Jobs/RadixSort/RadixSort.h"
#include "SDKs/EATech/eajobs/job.h"
#include "SDKs/EATech/eajobs/job_scheduler.h"
#include "eathread/eathread.h"

namespace renderengine
{
    // FLAG PC-platform leaf: each immutable mesh-frame bank owns its submitted
    // sort descriptors until every list is consumed or the bank is rebuilt.
    // The console renderer had one bank; the PC producer overlaps two frames.
    class DispatchSortJobsPC
    {
    public:
        static constexpr u32 KU_COUNT = 16;
        inline static constexpr u32 KAU_LISTS[KU_COUNT] =
            {0,2,1,3,4,5,6,7,8,9,10,21,11,19,15,20};
        DispatchSortJobsPC();
        ~DispatchSortJobsPC();
        DispatchSortJobsPC(const DispatchSortJobsPC&) = delete;
        DispatchSortJobsPC& operator=(const DispatchSortJobsPC&) = delete;
        void Begin(CgsGraphics::DispatchFrame* lpFrame,
                   EA::Jobs::JobScheduler* lpScheduler, bool lbWide);
        void WaitList(u32 luList);
        void WaitAll();
        bool Pending() const { return muPending != 0; }
    private:
        void WaitIndex(u32 luIndex);
        EA::Jobs::Job maJobs[KU_COUNT];
        SortInfo maData[KU_COUNT];
        u32 muPending = 0;
        EA::Thread::ThreadId mOwnerThread;
    };
}
