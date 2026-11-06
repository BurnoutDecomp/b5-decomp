"""One-time C++03/native storage recovery for the local scheduler."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'

def write(name,s):
    (ROOT/name).write_bytes(s.replace('\n','\r\n').encode('utf-8'))

p=ROOT/'src/SDKs/EATech/eajobs/local_backend.h'
s=p.read_text()
s=s.replace('            Param mArguments[4];','            u32 mArgumentWords[4];')
s=s.replace('            ExecutionData() : mArguments{} {}\n','')
s=s.replace('            u64                          mSemaphoreAlign;', '            u32                          mSemaphoreAlign;')
s=s.replace('        CGS_STATIC_ASSERT((sizeof(SlotWord)==sizeof(u64)), "packed job slot width");\n','')
s=s.replace('SlotWord*', 'u64*')
s=s.replace('virtual int ExecuteReadyJobs(int iAffinity);','virtual bool ExecuteReadyJobs(int iAffinity);')
needle='    // @ 0x82BCBE70 -- MSVC'
a=s.index(needle)
s=s[:a]+'''    CGS_STATIC_ASSERT((sizeof(Param) == 4), "ARTIST job argument word");
    CGS_STATIC_ASSERT((sizeof(JobInstance::ExecutionData) == 0x18), "ARTIST argument/profile overlay");
    CGS_STATIC_ASSERT((offsetof(JobInstance, mExecution) == 0x38), "ARTIST saved arguments");
    CGS_STATIC_ASSERT((offsetof(JobInstance, mEventLists) == 0x50), "ARTIST job event lists");
    CGS_STATIC_ASSERT((offsetof(JobInstance, mGarbageCollectorLock) == 0x270), "ARTIST job reference count");
    CGS_STATIC_ASSERT((offsetof(JobInstance, mSemaphoreStorage) == 0x274), "ARTIST completion semaphore");
    CGS_STATIC_ASSERT((offsetof(JobInstance, mEventListLock) == 0x284), "ARTIST event lock");
    CGS_STATIC_ASSERT((sizeof(JobInstance) == 0x290), "ARTIST job instance stride");

'''+s[a:]
write(str(p.relative_to(ROOT)),s)

p=ROOT/'src/SDKs/EATech/eajobs/local_backend.cpp'
s=p.read_text().replace('#include <xtl.h> // QueryPerformanceCounter, LARGE_INTEGER\n','')
s=s.replace('        new (&mExecution) ExecutionData;\n        for (int i=0;i<4;++i) mExecution.mArguments[i]=pArguments[i];',
            '        std::memcpy(mExecution.mArgumentWords, pArguments, sizeof(mExecution.mArgumentWords));')
s=s.replace('mExecution.mArguments[0],mExecution.mArguments[1],\n                                    mExecution.mArguments[2],mExecution.mArguments[3]',
            'Param(mExecution.mArgumentWords[0]), Param(mExecution.mArgumentWords[1]),\n                                    Param(mExecution.mArgumentWords[2]), Param(mExecution.mArgumentWords[3])')
s=s.replace('const u64 threadId=static_cast<u64>(EA::Thread::GetSysThreadId());\n        QueryPerformanceCounter(&end);\n        mExecution.mProfile={threadId,static_cast<u64>(begin.QuadPart),static_cast<u64>(end.QuadPart)};',
            'const u64 threadId = static_cast<u64>(reinterpret_cast<uintptr_t>(EA::Thread::GetThreadId()));\n        mExecution.mProfile.mBegin = static_cast<u64>(begin.QuadPart);\n        mExecution.mProfile.mThreadId = threadId;\n        QueryPerformanceCounter(&end);\n        mExecution.mProfile.mEnd = static_cast<u64>(end.QuadPart);')
s=s.replace('lpSemaphore->Wait(EA::Thread::kTimeoutNone);','lpSemaphore->Wait(&EA::Thread::kTimeoutNone);')
s=s.replace('SlotWord', 'u64').replace('        pLocation=&slot.mHigh;',
            '        // Native big-endian queue entries expose their first (high) word.\n        pLocation = reinterpret_cast<u32*>(&slot);')
s=s.replace('    int LocalBackend::ExecuteReadyJobs(int iAffinity)', '    bool LocalBackend::ExecuteReadyJobs(int iAffinity)')
a=s.index('    // Pin the LocalBackend layout.')
b=s.index('    // @ 0x82BCBCB8',a)
s=s[:a]+'''    // ARTIST82BCBCB8: exact native fields, including natural alignment before
    // the submission counter after the32worker array ends at594hex.
    static void LocalBackend_AssertLayout()
    {
        CGS_STATIC_ASSERT((offsetof(LocalBackend, mpJobInstances) == 4), "ARTIST job instances");
        CGS_STATIC_ASSERT((offsetof(LocalBackend, mpPriorityQueue) == 8), "ARTIST priority queue");
        CGS_STATIC_ASSERT((offsetof(LocalBackend, mNextSlot) == 0xC), "ARTIST allocation cursor");
        CGS_STATIC_ASSERT((offsetof(LocalBackend, mNumSlots) == 0x10), "ARTIST job capacity");
        CGS_STATIC_ASSERT((offsetof(LocalBackend, mThreads) == 0x14), "ARTIST worker array");
        CGS_STATIC_ASSERT((offsetof(LocalBackend, mThreads) + sizeof(((LocalBackend*)0)->mThreads) == 0x594), "ARTIST worker array end");
        CGS_STATIC_ASSERT((offsetof(LocalBackend, mSubmissionCounter) == 0x598), "ARTIST submission counter");
        CGS_STATIC_ASSERT((offsetof(LocalBackend, mpProfilingCallback) == 0x5A0), "ARTIST profiling callback");
        CGS_STATIC_ASSERT((offsetof(LocalBackend, mpProfilingContext) == 0x5A4), "ARTIST profiling context");
        CGS_STATIC_ASSERT((offsetof(LocalBackend, mGarbageCollectorCursor) == 0x5A8), "ARTIST garbage collector cursor");
        CGS_STATIC_ASSERT((offsetof(LocalBackend, mpEnableProfiling) == 0x5AC), "ARTIST profiling flag");
        CGS_STATIC_ASSERT((offsetof(LocalBackend, mJobThreadSleepTimeoutMS) == 0x5B0), "ARTIST worker sleep timeout");
        CGS_STATIC_ASSERT((sizeof(LocalBackend) == 0x5B8), "ARTIST local scheduler size");
    }

'''+s[b:]
write(str(p.relative_to(ROOT)),s)
