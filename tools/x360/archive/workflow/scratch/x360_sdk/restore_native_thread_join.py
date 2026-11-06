"""One-time recovery of native join and IRunnable thread startup."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'

def write(name,text):
    (ROOT/name).write_bytes(text.replace('\r\n','\n').replace('\n','\r\n').encode('utf-8'))

p=ROOT/'src/SDKs/EATech/eathread/BrnEAThreadX360.h'
s=p.read_text()
a=s.index('        // Block until the started thread ends')
b=s.index('        // Start a thread running',a)
s=s[:a]+'''        // ARTIST82B42DD0: absolute32-bit deadline first, optional return output second.
        Status WaitForEnd(const u32& timeoutAbsolute, intptr_t* pThreadReturnValue = 0);

'''+s[b:]
needle='        // Poll thread status; optionally writes the return value out.'
a=s.index(needle)
s=s[:a]+'''        // ARTIST82B442F8: the distinct IRunnable path uses its class entry trampoline.
        ThreadId Begin(IRunnable* pRunnable, void* pContext = 0,
                       const ThreadParameters* pParameters = 0,
                       RunnableClassUserWrapper pUserWrapper = 0);

'''+s[a:]
needle='        static RunnableFunctionUserWrapper GetGlobalRunnableFunctionUserWrapper();'
s=s.replace(needle,needle+'\n\n        // ARTIST82B42DB0: separate class-wrapper global at8324FF04.\n        static RunnableClassUserWrapper GetGlobalRunnableClassUserWrapper();')
write(str(p.relative_to(ROOT)),s)

p=ROOT/'src/SDKs/EATech/eathread/BrnEAThreadX360.cpp'
s=p.read_text()
needle='    volatile LONG                            glProcessorRoundRobin = 0;'
a=s.index(needle)
s=s[:a]+'''    EA::Thread::RunnableClassUserWrapper gpGlobalRunnableClassUserWrapper = 0; //8324FF04
'''+s[a:]

needle='    // 0x82B42DC0 -- single'
a=s.index(needle)
s=s[:a]+'''    // ARTIST82B44248: wrapper or IRunnable::Run at virtual slot4.
    static unsigned __stdcall ThreadClassEntryTrampoline(void* pArg)
    {
        EAThreadDynamicData* const pData = static_cast<EAThreadDynamicData*>(pArg);
        MemoryBarrier();
        IRunnable* const pRunnable = static_cast<IRunnable*>(pData->mpStartContext[0]);
        void* const pContext = pData->mpStartContext[1];
        SetCurrentThreadHandle(pData->mhThreadHandleCopy, false);
        pData->mnStatus = Thread::kStatusRunning;
        RunnableClassUserWrapper const pWrapper =
            reinterpret_cast<RunnableClassUserWrapper>(pData->mpBeginThreadUserWrapper);
        const intptr_t nReturn = pWrapper ? pWrapper(pRunnable, pContext) : pRunnable->Run(pContext);
        pData->mnReturnValue = static_cast<s32>(nReturn);
        SetCurrentThreadHandle(0, false);
        pData->mnStatus = Thread::kStatusEnded;
        if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(&pData->mnRefCount)) == 0)
            FreeThreadDynamicData(pData);
        return static_cast<unsigned>(nReturn);
    }

'''+s[a:]

needle='    // 0x82B42EF8\n'
a=s.index(needle)
s=s[:a]+'''    // ARTIST82B42DD0. An initially absent record waits for publication; an
    // already-present record with a null handle takes the ended path directly.
    Thread::Status Thread::WaitForEnd(const u32& timeoutAbsolute, intptr_t* pThreadReturnValue)
    {
        MemoryBarrier();
        if (!mThreadData.mpData)
        {
            while (!mThreadData.mpData || !mThreadData.mpData->mhThread)
            {
                const u32 nNow = ::GetTickCount() + 0x2710u;
                if (nNow >= timeoutAbsolute)
                    break;
                ::SleepEx(1, TRUE);
                MemoryBarrier();
            }
            if (!mThreadData.mpData)
                return kStatusNone;
            pThreadReturnValue = 0;
            MemoryBarrier();
        }

        if (mThreadData.mpData->mhThread)
        {
            u32 nWait = timeoutAbsolute;
            if (nWait != 0 && nWait != 0xFFFFFFFFu)
            {
                const u32 nNow = ::GetTickCount() + 0x2710u;
                const u32 nDeadline = timeoutAbsolute;
                nWait = nDeadline > nNow ? nDeadline - nNow : 0;
            }
            if (::WaitForSingleObject(mThreadData.mpData->mhThread, nWait) == WAIT_TIMEOUT)
                return kStatusRunning;
            ::CloseHandle(mThreadData.mpData->mhThread);
            mThreadData.mpData->mhThread = 0;
            mThreadData.mpData->mnStatus = kStatusEnded;
        }
        if (pThreadReturnValue)
        {
            MemoryBarrier();
            *pThreadReturnValue = mThreadData.mpData->mnReturnValue;
        }
        return kStatusEnded;
    }

'''+s[a:]

needle='    // 0x82B43F80\n'
a=s.index(needle)
s=s[:a]+'''    // ARTIST82B42DB0.
    RunnableClassUserWrapper Thread::GetGlobalRunnableClassUserWrapper()
    {
        return gpGlobalRunnableClassUserWrapper;
    }

'''+s[a:]

a=s.index('    // 0x82B43F80\n')
s=s[:a]
for class_path in [False,True]:
    signature=('IRunnable* pRunnable' if class_path else 'RunnableFunction pFunction')
    wrapper=('RunnableClassUserWrapper' if class_path else 'RunnableFunctionUserWrapper')
    entry=('ThreadClassEntryTrampoline' if class_path else 'ThreadEntryTrampoline')
    start=('pRunnable' if class_path else 'reinterpret_cast<void*>(pFunction)')
    original=('82B442F8' if class_path else '82B43F80')
    s+=f'''    // ARTIST{original}.
    ThreadId Thread::Begin({signature}, void* pContext,
        const ThreadParameters* pParameters, {wrapper} pUserWrapper)
    {{
        EAThreadDynamicData* const pOld = mThreadData.mpData;
        if (pOld && InterlockedDecrement(reinterpret_cast<volatile LONG*>(&pOld->mnRefCount)) == 0)
            FreeThreadDynamicData(pOld);

        EAThreadDynamicData* const pData = AllocateThreadDynamicData();
        if (pData)
        {{
            pData->mhThread = 0;
            pData->mnThreadId = 0;
            pData->mnStatus = 0;
            pData->mnReturnValue = 0;
            pData->mpBeginThreadUserWrapper = 0;
            InterlockedExchange(reinterpret_cast<volatile LONG*>(&pData->mnRefCount), 0);
        }}
        mThreadData.mpData = pData;
        // The native body does not suppress the reference operations on allocation failure.
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(&pData->mnRefCount));
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(&mThreadData.mpData->mnRefCount));
        mThreadData.mpData->mhThread = 0;
        mThreadData.mpData->mpStartContext[0] = {start};
        mThreadData.mpData->mpStartContext[1] = pContext;
        mThreadData.mpData->mpBeginThreadUserWrapper = reinterpret_cast<void*>(pUserWrapper);

        const unsigned nStackSize = pParameters ? static_cast<unsigned>(pParameters->mnStackSize) : 0;
        const uintptr_t nHandle = _beginthreadex(0, nStackSize, {entry},
            mThreadData.mpData, CREATE_SUSPENDED,
            reinterpret_cast<unsigned*>(&mThreadData.mpData->mnThreadId));
        mThreadData.mpData->mhThread = reinterpret_cast<HANDLE>(nHandle);
        if (mThreadData.mpData->mhThread)
        {{
            if (pParameters)
                SetName(pParameters->mpName);
            mThreadData.mpData->mhThreadHandleCopy = mThreadData.mpData->mhThread;
            if (pParameters && pParameters->mnPriority != 0 && mThreadData.mpData)
                ::SetThreadPriority(mThreadData.mpData->mhThread, pParameters->mnPriority);

            int nProcessor;
            if (pParameters && pParameters->mnProcessor >= 0)
            {{
                nProcessor = pParameters->mnProcessor;
                if (nProcessor == glProcessorRoundRobin)
                    InterlockedIncrement(&glProcessorRoundRobin);
            }}
            else
            {{
                nProcessor = giDefaultProcessor;
                if (nProcessor < 0)
                    nProcessor = InterlockedIncrement(&glProcessorRoundRobin);
            }}
            if (mThreadData.mpData)
            {{
                if (nProcessor < 0)
                    nProcessor = 0;
                XSetThreadProcessor(mThreadData.mpData->mhThread, static_cast<DWORD>(nProcessor % 6));
            }}
            ResumeThread(mThreadData.mpData->mhThread);
            return mThreadData.mpData->mhThread;
        }}
        if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(&mThreadData.mpData->mnRefCount)) == 0)
            FreeThreadDynamicData(mThreadData.mpData);
        if (InterlockedDecrement(reinterpret_cast<volatile LONG*>(&mThreadData.mpData->mnRefCount)) == 0)
            FreeThreadDynamicData(mThreadData.mpData);
        mThreadData.mpData = 0;
        return 0;
    }}

'''
s+='}\n}\n'
write(str(p.relative_to(ROOT)),s)

p=ROOT/'src/SDKs/EATech/eajobs/job_thread.h'
s=p.read_text().replace('#include <atomic>\n','').replace('// Native EAThread implementation linked by the PC build.','// Native EAThread implementation.')
s=s.replace('       // +0x09 on console','        bool                      mbQuit;       // +0x09')
s=s.replace('native Win32 wake handle','native XDK wake handle')
needle='    };\n}\n}\n}'
gates='''    };

    CGS_STATIC_ASSERT((offsetof(JobThread, mbStarted) == 8), "ARTIST worker start flag");
    CGS_STATIC_ASSERT((offsetof(JobThread, mbQuit) == 9), "ARTIST worker quit flag");
    CGS_STATIC_ASSERT((offsetof(JobThread, mThread) == 0xC), "ARTIST worker thread");
    CGS_STATIC_ASSERT((offsetof(JobThread, mParameters) == 0x10), "ARTIST worker parameters");
    CGS_STATIC_ASSERT((offsetof(JobThread, mhEvent) == 0x28), "ARTIST worker event");
    CGS_STATIC_ASSERT((sizeof(JobThread) == 0x2C), "ARTIST worker stride");
}
}
}'''
assert needle in s;s=s.replace(needle,gates)
write(str(p.relative_to(ROOT)),s)

p=ROOT/'src/SDKs/EATech/eajobs/local_backend.h'
s=p.read_text().replace('static constexpr u64 KU_SLOT_STATE_', 'static const u64 KU_SLOT_STATE_')
write(str(p.relative_to(ROOT)),s)
