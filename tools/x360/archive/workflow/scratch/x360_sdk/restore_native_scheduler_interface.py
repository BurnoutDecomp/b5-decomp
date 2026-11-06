"""Recover the attested22-slot scheduler interface and native structure returns."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
def write(name,s):(ROOT/name).write_bytes(s.replace('\n','\r\n').encode('utf-8'))

p=ROOT/'src/SDKs/EATech/eajobs/job_types.h';s=p.read_text()
needle='    struct EntryPoint;'
a=s.index(needle)
s=s[:a]+'''    struct JobThreadHandle;
    struct JobThreadParameters;
    struct JobMetrics;
    typedef void ProfilerCallback(const JobMetrics* pMetrics, int iCount, void* pContext);
'''+s[a:]
a=s.index('        class SchedulerBackend\n')
b=s.index('\n    };',a) if False else s.index('\n    }\n\n    // job_instance_handle.h',a)
s=s[:a]+'''        // ARTIST base82182408 andLocalBackend82182500 have these22slots.
        class SchedulerBackend
        {
        public:
            virtual ~SchedulerBackend();                                      //00
            virtual int GetNumJobsInFlight() const = 0;                         //04
            virtual int GetNumThreads() const = 0;                              //08
            virtual JobThreadHandle GetThreadHandle(u32 uIndex) const = 0;      //0C
            virtual JobThreadHandle AddThread(const JobThreadParameters&) = 0;  //10
            virtual void RequestEnd(u64 uHandle) = 0;                           //14
            virtual void WaitForEnd(u64 uHandle) = 0;                           //18
            virtual void WakeThreads();                                        //1C
            virtual u64 GetThreadId(u64 uHandle) const = 0;                      //20
            virtual JobThreadParameters GetThreadParameters(u64 uHandle) const = 0; //24
            virtual JobInstanceHandle CreateNotReadyInstance(const EntryPoint*, const Param*) = 0; //28
            virtual void GetEnabler(u64,u64,u32*&,u32&) = 0;                     //2C
            virtual int IsJobComplete(u64,u64) = 0;                             //30
            virtual int SubmitEvent(u64,u64,const void*,int) = 0;               //34
            virtual Event AddBarrier(u64,u64) = 0;                              //38
            virtual int SleepOn(u64,u64) = 0;                                   //3C
            virtual int GetAllowSleepOn(u64,u64) = 0;                           //40
            virtual void SetProfilingCallback(ProfilerCallback*);              //44
            virtual void SetProfilingContext(void*);                            //48
            virtual void FlushProfile();                                       //4C
            virtual bool ExecuteReadyJobs(int);                                //50
            virtual u32 GetSleepTimeoutMs() = 0;                                //54
        };
'''+s[b:]
write(str(p.relative_to(ROOT)),s)

p=ROOT/'src/SDKs/EATech/eajobs/profiling.h';s=p.read_text();s=s.replace('    typedef void ProfilerCallback(const JobMetrics* pMetrics, int iCount, void* pContext);\n','');write(str(p.relative_to(ROOT)),s)

p=ROOT/'src/SDKs/EATech/eajobs/local_backend.h';s=p.read_text()
s=s.replace('        virtual void CreateNotReadyInstance(JobInstanceHandle* pOutHandle,\n                                            const EntryPoint*  pEntryPoint,\n                                            const Param*       pParams);',
            '        virtual JobInstanceHandle CreateNotReadyInstance(const EntryPoint* pEntryPoint, const Param* pParams);')
s=s.replace('        void AddThread(JobThreadHandle* pOutHandle, const JobThreadParameters& rParameters);',
            '        JobThreadHandle AddThread(const JobThreadParameters& rParameters);')
s=s.replace('        void WaitForEnd(int iIndex);', '        void RequestEnd(u64 uHandle);\n        void WaitForEnd(u64 uHandle);')
s=s.replace('        void GetThreadHandle(JobThreadHandle* pOutHandle, Detail::SchedulerBackend* pBackend,\n                             u32 uHandle) const;',
            '        JobThreadHandle GetThreadHandle(u32 uHandle) const;')
s=s.replace('        u64 GetThreadId(int iIndex) const;', '        u64 GetThreadId(u64 uHandle) const;')
s=s.replace('        void GetThreadParameters(JobThreadParameters* pOut, int iIndex) const;',
            '        JobThreadParameters GetThreadParameters(u64 uHandle) const;')
write(str(p.relative_to(ROOT)),s)

p=ROOT/'src/SDKs/EATech/eajobs/local_backend.cpp';s=p.read_text()
s=s.replace('    void LocalBackend::CreateNotReadyInstance(JobInstanceHandle* pOutHandle,\n                                              const EntryPoint*  pEntryPoint,\n                                              const Param*       pParams)',
            '    JobInstanceHandle LocalBackend::CreateNotReadyInstance(const EntryPoint* pEntryPoint, const Param* pParams)')
s=s.replace('                    pOutHandle->mSubmissionId     = 0;\n                    pOutHandle->mSchedulerBackend = 0;\n                    pOutHandle->mIndex            = 0;\n                    return;', '                    return JobInstanceHandle();')
s=s.replace('                    pOutHandle->mSubmissionId     = luSubmissionId;\n                    pOutHandle->mSchedulerBackend = this;\n                    pOutHandle->mIndex            = static_cast<u16>(liSlot);\n                    return;', '                    return lNewHandle;')
# Replace only the thread facade bodies; job scheduling algorithms are retained.
a=s.index('    void LocalBackend::AddThread(');b=s.index('    // @ 0x82BC9CB8 -- GetEnabler.',a)
s=s[:a]+'''    JobThreadHandle LocalBackend::AddThread(const JobThreadParameters& rParameters)
    {
        int index = 0;
        while (mThreads[index].mbStarted)
        {
            if (++index >= KI_NUM_THREADS)
                return JobThreadHandle();
        }
        mThreads[index].Start(&rParameters, this);
        return JobThreadHandle(this, static_cast<u32>(index));
    }

    int LocalBackend::GetNumThreads() const
    {
        int count = 0;
        for (int index = 0; index < KI_NUM_THREADS; ++index)
            if (mThreads[index].mbStarted) ++count;
        return count;
    }

    // ARTIST82BCBA40: the low word selects a worker; wake it after publishing quit.
    void LocalBackend::RequestEnd(u64 uHandle)
    {
        JobThread& worker = mThreads[static_cast<u32>(uHandle)];
        worker.mbQuit = true;
        ::SetEvent(worker.mhEvent);
    }

    // ARTIST82BCA738.
    void LocalBackend::WaitForEnd(u64 uHandle)
    {
        mThreads[static_cast<u32>(uHandle)].WaitForEnd();
    }

    // ARTIST82BC9E20: hidden return buffer precedes this and the handle word.
    JobThreadHandle LocalBackend::GetThreadHandle(u32 uHandle) const
    {
        return JobThreadHandle(const_cast<LocalBackend*>(this), uHandle);
    }

    // ARTIST82BC9E30: zero-extend the worker's native thread HANDLE.
    u64 LocalBackend::GetThreadId(u64 uHandle) const
    {
        return static_cast<u64>(reinterpret_cast<uintptr_t>(
            mThreads[static_cast<u32>(uHandle)].mThread.GetId()));
    }

    // ARTIST82BCA770: the24-byte structure is returned in the hidden buffer.
    JobThreadParameters LocalBackend::GetThreadParameters(u64 uHandle) const
    {
        return mThreads[static_cast<u32>(uHandle)].mParameters;
    }

'''+s[b:]
write(str(p.relative_to(ROOT)),s)

p=ROOT/'src/SDKs/EATech/eajobs/job_instance_handle.cpp';s=p.read_text().replace('#include <stdint.h>   // uintptr_t\n','')
needle='        SchedulerBackend::~SchedulerBackend()\n        {\n        }'
assert needle in s
s=s.replace(needle,needle+'''

        // ARTIST base slots1C/44/48/4C resolve to the genuine bare blr82AD5078.
        void SchedulerBackend::WakeThreads() {}
        void SchedulerBackend::SetProfilingCallback(ProfilerCallback*) {}
        void SchedulerBackend::SetProfilingContext(void*) {}
        void SchedulerBackend::FlushProfile() {}

        // ARTIST base slot50 resolves to li r3,0;blr82B0F1B8.
        bool SchedulerBackend::ExecuteReadyJobs(int) { return false; }
''')
write(str(p.relative_to(ROOT)),s)

p=ROOT/'src/SDKs/EATech/eajobs/job.cpp';s=p.read_text()
s=s.replace('        JobInstanceHandle lNewHandle;\n        lpBackend->CreateNotReadyInstance(&lNewHandle, &mEntryPoint, mParams);',
            '        JobInstanceHandle lNewHandle = lpBackend->CreateNotReadyInstance(&mEntryPoint, mParams);')
write(str(p.relative_to(ROOT)),s)

p=ROOT/'src/SDKs/EATech/eajobs/job_scheduler.cpp';s=p.read_text()
s=s.replace('        LocalBackend::LocalBackend* lpBackend =\n            static_cast<LocalBackend::LocalBackend*>(mSchedulers[lrParameters.environment]);\n        lpBackend->AddThread(&lHandle, lrParameters);',
            '        Detail::SchedulerBackend* lpBackend = mSchedulers[lrParameters.environment];\n        lHandle = lpBackend->AddThread(lrParameters);')
write(str(p.relative_to(ROOT)),s)
