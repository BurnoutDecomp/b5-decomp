"""One-time native frame/barrier/atomic restoration; no production-ledger writes."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'

def write(name,text):
    (ROOT/name).write_bytes(text.replace('\r\n','\n').replace('\n','\r\n').encode('utf-8'))

write('src/SDKs/EATech/eathread/eathread_atomic.h','''#ifndef EA_THREAD_EAATOMIC_H
#define EA_THREAD_EAATOMIC_H

#include "types.hpp"
#include <xtl.h>

namespace EA
{
namespace Thread
{
    // ARTIST uses native masked-interrupt reservation operations for writes.
    // Aligned32/64-bit reads are plain lwz/ld, as the frame/data-stream sites show.
    template <class T>
    class AtomicInt
    {
    public:
        typedef T ValueType;
        CGS_STATIC_ASSERT((sizeof(ValueType) == 4 || sizeof(ValueType) == 8), "EA atomic word width");

        AtomicInt() {}
        AtomicInt(ValueType n) { SetValue(n); }
        ValueType GetValueRaw() const { return mValue; }
        ValueType GetValue() const { return mValue; }

        ValueType SetValue(ValueType n)
        {
            if (sizeof(ValueType) == 4)
                return static_cast<ValueType>(_InterlockedExchange(
                    reinterpret_cast<volatile LONG*>(&mValue), static_cast<LONG>(n)));
            return static_cast<ValueType>(_InterlockedExchange64(
                reinterpret_cast<volatile LONG64*>(&mValue), static_cast<LONG64>(n)));
        }

        bool SetValueConditional(ValueType n, ValueType condition)
        {
            if (sizeof(ValueType) == 4)
                return static_cast<u32>(_InterlockedCompareExchange(
                    reinterpret_cast<volatile LONG*>(&mValue), static_cast<LONG>(n),
                    static_cast<LONG>(condition))) == static_cast<u32>(condition);
            return static_cast<u64>(_InterlockedCompareExchange64(
                reinterpret_cast<volatile LONG64*>(&mValue), static_cast<LONG64>(n),
                static_cast<LONG64>(condition))) == static_cast<u64>(condition);
        }

        ValueType Increment()
        {
            if (sizeof(ValueType) == 4)
                return static_cast<ValueType>(_InterlockedIncrement(reinterpret_cast<volatile LONG*>(&mValue)));
            return static_cast<ValueType>(_InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(&mValue)));
        }

        ValueType Decrement()
        {
            if (sizeof(ValueType) == 4)
                return static_cast<ValueType>(_InterlockedDecrement(reinterpret_cast<volatile LONG*>(&mValue)));
            return static_cast<ValueType>(_InterlockedDecrement64(reinterpret_cast<volatile LONG64*>(&mValue)));
        }

        ValueType Add(ValueType n)
        {
            if (sizeof(ValueType) == 4)
            {
                const u32 nOld = static_cast<u32>(_InterlockedExchangeAdd(
                    reinterpret_cast<volatile LONG*>(&mValue), static_cast<LONG>(n)));
                return static_cast<ValueType>(nOld + static_cast<u32>(n));
            }
            const u64 nOld = static_cast<u64>(_InterlockedExchangeAdd64(
                reinterpret_cast<volatile LONG64*>(&mValue), static_cast<LONG64>(n)));
            return static_cast<ValueType>(nOld + static_cast<u64>(n));
        }

        operator const ValueType() const { return GetValue(); }
        ValueType operator=(ValueType n) { SetValue(n); return n; }
        ValueType operator+=(ValueType n) { return Add(n); }
        ValueType operator-=(ValueType n) { return Add(static_cast<ValueType>(u64(0) - static_cast<u64>(n))); }
        ValueType operator++() { return Increment(); }
        ValueType operator++(int) { return static_cast<ValueType>(static_cast<u64>(Increment()) - 1); }
        ValueType operator--() { return Decrement(); }
        ValueType operator--(int) { return static_cast<ValueType>(static_cast<u64>(Decrement()) + 1); }

    protected:
        volatile ValueType mValue;
    };

    typedef AtomicInt<int32_t> AtomicInt32;
    typedef AtomicInt<uint32_t> AtomicUint32;
    typedef AtomicInt<int64_t> AtomicInt64;
    typedef AtomicInt<uint64_t> AtomicUint64;
    typedef AtomicInt64 AtomicIWord;
    typedef AtomicUint64 AtomicUWord;
    typedef AtomicInt32 AtomicIntPtr;
    typedef AtomicUint32 AtomicUintPtr;

    class AtomicPointer : public AtomicInt<int32_t>
    {
    public:
        typedef void* PointerValueType;

        // The base zero-initialization and pointer store are both present in
        // ARTIST ThreadLayout::ThreadLayout828E15C8, including for a null pointer.
        AtomicPointer(void* p = 0) : AtomicInt<int32_t>(0) { SetValue(p); }
        AtomicPointer& operator=(void* p) { SetValue(p); return *this; }
        operator const void*() const { return GetValue(); }
        void* GetValue() const { return reinterpret_cast<void*>(static_cast<u32>(AtomicInt<int32_t>::GetValue())); }
        void* GetValueRaw() const { return reinterpret_cast<void*>(static_cast<u32>(AtomicInt<int32_t>::GetValueRaw())); }
        void* SetValue(void* p)
        {
            return reinterpret_cast<void*>(static_cast<u32>(AtomicInt<int32_t>::SetValue(
                static_cast<ValueType>(reinterpret_cast<uintptr_t>(p)))));
        }
        bool SetValueConditional(void* p, void* condition)
        {
            return AtomicInt<int32_t>::SetValueConditional(
                static_cast<ValueType>(reinterpret_cast<uintptr_t>(p)),
                static_cast<ValueType>(reinterpret_cast<uintptr_t>(condition)));
        }
    };

    CGS_STATIC_ASSERT((sizeof(AtomicInt32) == 4), "ARTIST atomic32");
    CGS_STATIC_ASSERT((sizeof(AtomicUint64) == 8), "ARTIST atomic64");
    CGS_STATIC_ASSERT((__alignof(AtomicUint64) == 8), "ARTIST atomic64 alignment");
    CGS_STATIC_ASSERT((sizeof(AtomicPointer) == 4), "ARTIST atomic pointer");
}
}

#endif
''')

write('src/SDKs/EATech/eathread/eathread_barrier_data.h','''#ifndef EA_THREAD_EABARRIERDATA_H
#define EA_THREAD_EABARRIERDATA_H

#include "SDKs/EATech/eathread/eathread_atomic.h"
#include "SDKs/EATech/eathread/eathread_semaphore.h"

// ARTIST EABarrierData::EABarrierData82B43A50.
struct EABarrierData
{
    EA::Thread::AtomicInt32 mnCurrent;
    s32 mnHeight;
    EA::Thread::AtomicInt32 mnIndex;
    EA::Thread::Semaphore mSemaphore0;
    EA::Thread::Semaphore mSemaphore1;

    EABarrierData();

private:
    EABarrierData(const EABarrierData&);
    EABarrierData& operator=(const EABarrierData&);
};

CGS_STATIC_ASSERT((offsetof(EABarrierData, mnIndex) == 8), "ARTIST barrier phase");
CGS_STATIC_ASSERT((offsetof(EABarrierData, mSemaphore0) == 0xC), "ARTIST barrier semaphore0");
CGS_STATIC_ASSERT((offsetof(EABarrierData, mSemaphore1) == 0x1C), "ARTIST barrier semaphore1");
CGS_STATIC_ASSERT((sizeof(EABarrierData) == 0x2C), "ARTIST barrier data");

#endif
''')

write('src/SDKs/EATech/eathread/eathread_barrier_data.cpp','''#include "SDKs/EATech/eathread/eathread_barrier_data.h"

// ARTIST82B43A50: atomic zeroes precede deferred semaphore construction.
EABarrierData::EABarrierData()
    : mnCurrent(0), mnHeight(0), mnIndex(0)
    , mSemaphore0(0, false), mSemaphore1(0, false)
{
}
''')

p=ROOT/'src/SDKs/EATech/eathread/eathread_barrier.h'
s=p.read_text()
s=s.replace('#include "types.hpp"','#include "types.hpp"\n#include "SDKs/EATech/eathread/eathread.h"',1)
s=s.replace('int Wait();','int Wait(const ThreadTime& timeoutAbsolute = kTimeoutNone);')
s=s.replace('    };\n}\n}', '    };\n\n    CGS_STATIC_ASSERT((sizeof(Barrier) == 0x2C), "ARTIST Barrier");\n}\n}')
write(str(p.relative_to(ROOT)),s)

p=ROOT/'src/SDKs/EATech/eathread/eathread_barrier.cpp'
s=p.read_text()
a=s.index('namespace\n{\n    // The X360 phase-semaphore')
b=s.index('// ---------------------------------------------------------------------------',a)
s=s[:a]+s[b:]
s=s.replace('mBarrierData.mnCurrent = pBarrierParameters->mnHeight;', 'mBarrierData.mnCurrent.SetValue(pBarrierParameters->mnHeight);')
s=s.replace('int Barrier::Wait()', 'int Barrier::Wait(const ThreadTime& timeoutAbsolute)')
s=s.replace('rActive.Wait(&KU_TIMEOUT_NONE)', 'rActive.Wait(&timeoutAbsolute)')
s=s.replace('//       r = (phase ? mSemaphore1 : mSemaphore0).Wait(kTimeoutNone);',
            '//       r = (phase ? mSemaphore1 : mSemaphore0).Wait(&timeoutAbsolute);')
write(str(p.relative_to(ROOT)),s)

write('src/GameShared/GameClasses/System/Threads/CgsThreadLayout.cpp','''// Native update/dispatch handshake from ARTIST828D7988/828E15C8/828E17D0.
#include "GameShared/GameClasses/System/Threads/CgsThreadLayout.h"
#include "GameShared/GameClasses/System/Timer/CgsTimeUtils.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>

namespace CgsSystem
{
    ThreadLayout::ThreadLayout()
        : mUpdateStartBarrier(0, false), mUpdateEndBarrier(0, false)
        , mAssertFromSecondaryThread(0)
    {
    }

    ThreadLayout::~ThreadLayout()
    {
    }

    // ARTIST828E1658: the worker blocks on the start barrier until the first frame.
    void ThreadLayout::InitThreads()
    {
        const EA::Thread::BarrierParameters lBarrierParameters(2, true);
        mUpdateStartBarrier.Init(&lBarrierParameters);
        mUpdateEndBarrier.Init(&lBarrierParameters);
        EA::Thread::Thread::SetGlobalRunnableFunctionUserWrapper(GlobalThreadBeginWrapper);
        mUpdateThreadId = EA::Thread::GetThreadId();
        EA::Thread::ThreadParameters lParameters;
        lParameters.mbSuspended = false;
        EA::Thread::SetThreadPriority(0);
        lParameters.mnProcessor = 2;
        lParameters.mnPriority = 0;
        lParameters.mpName = "DispatchThread";
        mDispatchThread.Begin(DispatchThread, this, &lParameters,
            EA::Thread::Thread::GetGlobalRunnableFunctionUserWrapper());
        if (CgsDev::Message::gxMessageFilterFlags & 1u)
            *CgsDev::Log::gpDebugPrint << "Main thread priority: " << EA::Thread::GetThreadPriority() << "\\n";
        if (CgsDev::Message::gxMessageFilterFlags & 1u)
            *CgsDev::Log::gpDebugPrint << "Dispatch priority: " << mDispatchThread.GetPriority() << "\\n";
    }

    intptr_t ThreadLayout::DispatchThread(void* lpContext)
    {
        ThreadLayout& lrLayout = *static_cast<ThreadLayout*>(lpContext);
        for (;;)
        {
            lrLayout.mUpdateStartBarrier.Wait();
            lrLayout.mpThreadClass->DispatchThread();
            lrLayout.mUpdateEndBarrier.Wait();
        }
    }

    bool ThreadLayout::Update()
    {
        if (const void* lpAssert = mAssertFromSecondaryThread.GetValue())
        {
            InteruptThreadForAssert(static_cast<const AssertData*>(lpAssert));
            return true;
        }
        mbUpdatingResourceSystem = false;
        mUpdateStartBarrier.Wait();
        mpThreadClass->OnStartOfUpdateFrame();
        const u32 luStartTime = GetSystemTimeMS();
        const bool lbResult = mpThreadClass->UpdateThread();
        const u32 luTimeTaken = GetSystemTimeMS() - luStartTime;
        static volatile bool _lbAllowStallTest = false;
        static volatile bool _lbContinue = false;
        if (_lbAllowStallTest && luTimeTaken > 250u)
        {
            std::printf("Crazy frame stall of %ums\\n", luTimeTaken);
            while (!_lbContinue) {}
            _lbContinue = false;
        }
        if (miPerfMonAllThreadSyncs >= 0)
            CgsDev::PerfMonCpu::StartMonitor(miPerfMonAllThreadSyncs);
        if (miPerfMonUpdateWaitForDispatch >= 0)
            CgsDev::PerfMonCpu::StartMonitor(miPerfMonUpdateWaitForDispatch);
        mUpdateEndBarrier.Wait();
        mbUpdatingResourceSystem = true;
        if (miPerfMonUpdateWaitForDispatch >= 0)
            CgsDev::PerfMonCpu::StopMonitor(miPerfMonUpdateWaitForDispatch);
        if (miPerfMonAllThreadSyncs >= 0)
            CgsDev::PerfMonCpu::StopMonitor(miPerfMonAllThreadSyncs);
        mpThreadClass->OnCompletionOfVsyncWait();
        if (miPerfMonResource >= 0)
            CgsDev::PerfMonCpu::StartMonitor(miPerfMonResource);
        mpThreadClass->ResourceUpdateThread(0);
        if (miPerfMonResource >= 0)
            CgsDev::PerfMonCpu::StopMonitor(miPerfMonResource);
        mpThreadClass->OnEndOfUpdateFrame();
        return lbResult;
    }

    void ThreadLayout::Begin(IThreadClass* lpThreadInterface, s32 liPerfMonResource,
        s32 liPerfMonAllThreadSyncs, s32 liPerfMonUpdateWaitForDispatch)
    {
        InitThreads();
        mpThreadClass = lpThreadInterface;
        miPerfMonResource = liPerfMonResource;
        miPerfMonUpdateWaitForDispatch = liPerfMonUpdateWaitForDispatch;
        miPerfMonAllThreadSyncs = liPerfMonAllThreadSyncs;
        meFrameRate = E_FRAMERATE_60HZ;
        miResourcePercentageOfFrame = 4;
        mbUpdatingResourceSystem = false;
    }

    intptr_t ThreadLayout::GlobalThreadBeginWrapper(EA::Thread::RunnableFunction pRunnable, void* pContext)
    {
        return pRunnable(pContext);
    }

    bool ThreadLayout::InteruptThreadForAssert(const AssertData* lpAssert)
    {
        const EA::Thread::ThreadId lDispatchThreadId = mDispatchThread.GetId();
        if (EA::Thread::GetThreadId() == lDispatchThreadId)
            return false;
        if (EA::Thread::GetThreadId() != mUpdateThreadId)
        {
            mAssertFromSecondaryThread.SetValue(const_cast<AssertData*>(lpAssert));
            for (;;)
            {
                const EA::Thread::ThreadTime lSleepMs = 1000;
                EA::Thread::ThreadSleep(&lSleepMs);
            }
        }
        if (!mbUpdatingResourceSystem)
            mUpdateEndBarrier.Wait();
        mpThreadClass->RenderAssert(lpAssert);
        return true;
    }
}
''')

p=ROOT/'src/GameShared/GameClasses/System/Threads/CgsThreadLayout.h'
s=p.read_text().replace('#include <exception>\n','')
a=s.index('    };',s.index('        EA::Thread::AtomicPointer     mAssertFromSecondaryThread;'))
s=s[:a]+'''
        void CheckLayout()
        {
            CGS_STATIC_ASSERT((offsetof(ThreadLayout, mUpdateThreadId) == 0x10), "ARTIST update id");
            CGS_STATIC_ASSERT((offsetof(ThreadLayout, mDispatchThread) == 0x14), "ARTIST dispatch thread");
            CGS_STATIC_ASSERT((offsetof(ThreadLayout, mUpdateStartBarrier) == 0x18), "ARTIST start barrier");
            CGS_STATIC_ASSERT((offsetof(ThreadLayout, mUpdateEndBarrier) == 0x44), "ARTIST end barrier");
            CGS_STATIC_ASSERT((offsetof(ThreadLayout, meFrameRate) == 0x70), "ARTIST frame rate");
            CGS_STATIC_ASSERT((offsetof(ThreadLayout, mbUpdatingResourceSystem) == 0x78), "ARTIST update phase");
            CGS_STATIC_ASSERT((offsetof(ThreadLayout, mAssertFromSecondaryThread) == 0x7C), "ARTIST secondary assert");
        }
'''+s[a:]
s+='\nCGS_STATIC_ASSERT((sizeof(CgsSystem::ThreadLayout) == 0x80), "ARTIST ThreadLayout size");\n'
write(str(p.relative_to(ROOT)),s)

p=ROOT/'src/GameShared/GameClasses/Memory/DataStream/CgsDataStreamResultReader.cpp'
s=p.read_text().replace('#include <atomic>   // std::atomic_thread_fence (End\'s closing lwsync)', '#include <xtl.h>  // native lwsync')
a=s.index('        // De-atomized:')
b=s.index('        mEncodedStatus.SetValue(0);',a)
s=s[:a]+s[b:]
s=s.replace('    void DataStreamResultReader::Begin()\n    {', '    void DataStreamResultReader::Begin()\n    {\n        __lwsync();')
a=s.index('        // De-atomized:')
b=s.index('    }',a)
s=s[:a]+'''        // Retry when a producer changes the status between the load and CAS.
        while (!mEncodedStatus.SetValueConditional(0, mEncodedStatus.GetValue())) {}
'''+s[b:]
s=s.replace('std::atomic_thread_fence(std::memory_order_acq_rel);  // lwsync', '__lwsync();')
write(str(p.relative_to(ROOT)),s)
