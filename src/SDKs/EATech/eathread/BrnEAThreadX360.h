#pragma once

// X360-faithful EA::Thread facade + parameter-struct layouts for the Burnout 5
// (BURNOUT_X360_ARTIST.XEX) build.
//
// =====================================================================
// FLAG -- COMMITTED-TYPE / VENDOR-SNAPSHOT DIVERGENCE (do NOT silently swap):
// b5-decomp/vendor/EAThread is a *newer* upstream EAThread snapshot whose
// layouts DIFFER from the older variant linked into the X360 binary. The X360
// asm is authoritative here, so this header reconstructs the X360 shapes
// instead of #including the divergent vendor headers. Concrete differences
// proven from the X360 disassembly (per-addr exports under
// .ida-exports/BURNOUT_X360_ARTIST.XEX/):
//
//   * SemaphoreParameters: X360 stores ONLY {mInitialCount@0, mbIntraProcess@4,
//     mName@5}. The X360 ctor never writes an mMaxCount field -- the modern
//     vendor SemaphoreParameters (mInitialCount/mMaxCount/mbIntraProcess/mName)
//     has an extra int the X360 build does not. (0x82B42910)
//   * MutexParameters: X360 *does* copy pName into mName (mbIntraProcess@0,
//     mName@1). The modern vendor MutexParameters ignores both ctor args and
//     only sets mbIntraProcess(true). (0x82B42710)
//   * GetThreadPriority/SetThreadPriority: X360 calls the Win32 primitive on
//     handle -2 with NO kThreadPriorityDefault remapping; the modern vendor
//     remaps. (0x82B425D0 / 0x82B425D8)
//   * ThreadSleep: X360 takes a pointer to a millisecond count and branches
//     SleepEx/SwitchToThread; modern takes a ThreadTime value. (0x82B42610)
//   * AllocateThreadDynamicData/FreeThreadDynamicData: X360 uses a FIXED static
//     pool of 24 EAThreadDynamicData slots (68 bytes each) plus a parallel
//     24-entry lwarx/stwcx. spinlock table; the modern vendor uses an entirely
//     different factory mechanism. (0x82B43CF8 / 0x82B43DD8)
//
// The conductor should decide whether to gate these X360 facts behind an X360
// platform branch in the vendor tree or keep this reconstruction as the X360
// home. This file does NOT modify the committed vendor copies.
// =====================================================================

#include "types.hpp"

// On the MSVC/Windows host the X360 OS primitives map to Win32. We declare the
// handful of Win32 entry points the X360 build uses rather than pulling all of
// <Windows.h> into this header.
typedef void* HANDLE;

namespace EA
{
namespace Thread
{
    // ThreadId under Win32/X360 is a thread HANDLE (void*). The X360 asm treats
    // GetThreadId's result as a raw pointer-sized id.
    typedef void* ThreadId;

    // ---- Parameter PODs (X360 layouts -- see FLAG above) -------------------

    // 0x82B42650: BarrierParameters(int height, bool bIntraProcess, const char* pName)
    // Layout: mnHeight@0 (4B), mbIntraProcess@4 (1B), mName@5 (16B, name[15]=0).
    struct BarrierParameters
    {
        int  mnHeight;        // offset 0
        bool mbIntraProcess;  // offset 4
        char mName[16];       // offset 5

        BarrierParameters(int height = 0, bool bIntraProcess = true, const char* pName = 0);
    };

    // 0x82B42910: SemaphoreParameters(int initialCount, bool bIntraProcess, const char* pName)
    // Layout: mInitialCount@0 (4B), mbIntraProcess@4 (1B), mName@5 (16B).
    // NOTE (FLAG): no mMaxCount on X360.
    struct SemaphoreParameters
    {
        int  mInitialCount;   // offset 0
        bool mbIntraProcess;  // offset 4
        char mName[16];       // offset 5

        SemaphoreParameters(int initialCount = 0, bool bIntraProcess = true, const char* pName = 0);
    };

    // 0x82B42710: MutexParameters(bool bIntraProcess, const char* pName)
    // Layout: mbIntraProcess@0 (1B), mName@1 (16B). X360 copies pName.
    struct MutexParameters
    {
        bool mbIntraProcess;  // offset 0
        char mName[16];       // offset 1

        MutexParameters(bool bIntraProcess = true, const char* pName = 0);
    };

    // 0x82B426B0: ConditionParameters(bool bIntraProcess, const char* pName)
    // Layout: mbIntraProcess@0 (1B), mName@1 (16B). Same shape as MutexParameters.
    struct ConditionParameters
    {
        bool mbIntraProcess;  // offset 0
        char mName[16];       // offset 1

        ConditionParameters(bool bIntraProcess = true, const char* pName = 0);
    };

    // RWMutexParameters(bool bIntraProcess, const char* pName)
    // Layout: mbIntraProcess@0 (1B), mName@1 (16B). Same shape as MutexParameters
    // (EATech DWARF eathread_rwmutex.h:85). RWMutex::Init reads only mbIntraProcess
    // (it builds its two ConditionParameters from it); the X360 build does not call
    // an RWMutexParameters ctor in the recovered RWMutex TU, so this is declared as a
    // plain POD used by RWMutex::Init's signature.
    struct RWMutexParameters
    {
        bool mbIntraProcess;  // offset 0
        char mName[16];       // offset 1

        RWMutexParameters(bool bIntraProcess = true, const char* pName = 0);
    };

    // ---- Thread-dynamic-data pool (X360) -----------------------------------

    // EAThreadDynamicData is the per-thread bookkeeping record. The X360 binary
    // touches a 68-byte (0x44) record. Field meanings below are now grounded by
    // the EA::Thread::Thread object TU (Begin/GetStatus/SetName/~Thread @
    // 0x82B43F80/0x82B42EF8/0x82B43000/0x82B43E88) and the static thread-entry
    // trampoline sub_82B43EC8, cross-checked against the EATech DWARF
    // (eathread_thread.h). The X360 store offsets prove:
    //   +0x00 mhThread           : Win32 thread HANDLE (begin result; status
    //                              checks GetExitCodeThread/CloseHandle target).
    //   +0x08 mnStatus           : kStatusNone/Running/Ended (trampoline sets 1
    //                              then 2; GetStatus stores 2).
    //   +0x0C mnReturnValue      : trampoline stores the runnable's return; GetStatus
    //                              copies it out through its out-param.
    //   +0x10 mpStartContext[0]  : RunnableFunction / IRunnable* (Begin a2).
    //   +0x14 mpStartContext[1]  : user context pointer (Begin a3).
    //   +0x18 mhThreadHandleCopy : copy of mhThread handed to SetCurrentThreadHandle
    //                              inside the trampoline (a1[6]).
    //   +0x1C mpBeginThreadUserWrapper : optional begin-thread user wrapper (Begin a5).
    //   +0x20 mnRefCount         : AtomicInt32 refcount (lwarx/stwcx. inc in Begin,
    //                              dec in ~Thread / Begin-failure / trampoline; freed
    //                              when it reaches 0).
    //   +0x24 mName[32]          : thread name (SetName strncpy's 32 bytes, NUL@+0x43).
    //
    // FLAG: +0x04 is never touched by this TU's code and its meaning is not
    // recoverable from this dossier (the modern EATech DWARF places intptr_t
    // mnReturnValue earlier; the X360 layout above is asm-authoritative). It is
    // left as an HONEST placeholder. The record is exactly 68 bytes.
    struct EAThreadDynamicData
    {
        HANDLE mhThread;                  // +0x00
        u32    mnThreadId;                // +0x04  OS thread id (_beginthreadex writes it via the
                                          //        ThrdAddr=record+4 out-param @0x82B4409C; SetName reads it back @0x82B43068)
        s32    mnStatus;                  // +0x08
        s32    mnReturnValue;             // +0x0C
        void*  mpStartContext[2];         // +0x10, +0x14
        HANDLE mhThreadHandleCopy;        // +0x18
        void*  mpBeginThreadUserWrapper;  // +0x1C
        s32    mnRefCount;                // +0x20  (lwarx/stwcx. AtomicInt32)
        char   mName[32];                 // +0x24..0x43
    };

    // X360: 24 statically-allocated EAThreadDynamicData slots guarded by a
    // parallel 24-entry busy-flag table (one int per slot, claimed via
    // lwarx/stwcx.). On the MSVC host the PowerPC reservation sequence is
    // modeled with an interlocked compare-and-swap; this is a host mapping of an
    // un-modelable-by-name SIMD/atomic primitive and is FLAGGED as such.
    const s32 KI_THREAD_DYNAMIC_DATA_COUNT = 24;

    // 0x82B43CF8
    EAThreadDynamicData* AllocateThreadDynamicData();
    // 0x82B43DD8
    void                 FreeThreadDynamicData(EAThreadDynamicData* pData);

    // ---- Facade ------------------------------------------------------------

    // 0x82B424C8: caches the current thread's HANDLE in module TLS and registers
    // it in the dynamic-thread-handle table when it was duplicated for us.
    void     SetCurrentThreadHandle(HANDLE hThread, bool bDynamic);
    // 0x82B42560
    ThreadId GetThreadId();
    // 0x82B425D0
    int      GetThreadPriority();
    // 0x82B425D8
    bool     SetThreadPriority(int nPriority);
    // 0x82B42610: a1 points at a millisecond count; 0 -> yield, else SleepEx.
    u32      ThreadSleep(const u32* lpuMilliseconds);

    // ---- IRunnable + RunnableFunction (EATech DWARF eathread_thread.h) ------

    // A thread body can be either a free function (RunnableFunction) or an
    // IRunnable instance. The X360 trampoline (sub_82B43EC8) calls the stored
    // wrapper if present, else dispatches the raw function pointer.
    struct IRunnable
    {
        // Polymorphic base destructor. Defined OUT-OF-LINE in BrnEAThreadX360.cpp so
        // the interface owns a real .cpp definition home -- the X360 base
        // scalar-deleting destructor @ 0x82BC9930 (stores the IRunnable vtable
        // off_821823FC into *this, then for the deleting variant conditionally
        // operator-delete's). The body is empty (no members, no base); the vtable
        // store + delete-tail is what the compiler emits.
        virtual ~IRunnable();
        virtual intptr_t Run(void* pContext = 0) = 0;
    };

    // intptr_t RunnableFunction(void* pContext);
    typedef intptr_t (*RunnableFunction)(void* pContext);

    // The two optional global user-wrappers the trampoline can route through.
    typedef intptr_t (*RunnableFunctionUserWrapper)(RunnableFunction pFunction, void* pContext);
    typedef intptr_t (*RunnableClassUserWrapper)(IRunnable* pRunnable, void* pContext);

    // 0x82B42D58: ThreadParameters POD (EATech DWARF eathread_thread.h:404).
    // Only mnStackSize/mnPriority/mnProcessor/mpName are read by Begin (a4[1],
    // a4[2], a4[3], a4[5]); the X360 build threads them through unchanged.
    //
    // X360 default-ctor store order (@ 0x82B42D58):
    //   +0x0C mnProcessor = -1                (li r9,-1;            stw 0xC)
    //   +0x14 mpName      = &unk_821473E3      (lis/addi r10;       stw 0x14)
    //   +0x00 mpStack     = 0                  (stw r11(=0),0)
    //   +0x04 mnStackSize = 0                  (stw r11,4)
    //   +0x08 mnPriority  = 0                  (stw r11,8)
    //   +0x10 mbSuspended = 0                  (stb r11,0x10)
    // The mpName store targets a fixed pooled string constant at the unaligned
    // .rodata address 0x821473E3 -- the EA SDK empty-name literal "" (an odd,
    // char-pool-only address; the trailing-NUL slot of the string pool). The
    // X360 ctor therefore initializes mpName to a non-null pointer-to-"" rather
    // than to NULL.
    struct ThreadParameters
    {
        void*       mpStack;        // a4[0]
        size_t      mnStackSize;    // a4[1]  (-> _beginthreadex StackSize)
        int         mnPriority;     // a4[2]  (-> SetThreadPriority if nonzero)
        int         mnProcessor;    // a4[3]  (-> XSetThreadProcessor)
        bool        mbSuspended;    // a4[4]
        const char* mpName;         // a4[5]  (-> Thread::SetName)

        // Defined out-of-line in BrnEAThreadX360.cpp (the X360 ctor @ 0x82B42D58
        // is a real, called out-of-line function -- 12 call sites).
        ThreadParameters();
    };

    // ---- The Thread object (this TU) ---------------------------------------

    // EAThreadData is the single-pointer wrapper that is the Thread object's
    // only data member: mpData -> EAThreadDynamicData (or null when unstarted).
    struct EAThreadData
    {
        EAThreadDynamicData* mpData;
    };

    // EA::Thread::Thread -- the thread handle object. Its sole instance member
    // is mThreadData.mpData (== *this as an EAThreadDynamicData** in the asm,
    // which dereferences r3 at offset 0 to reach mpData).
    //
    // Recovered functions (X360 addrs):
    //   ~Thread                            0x82B43E88
    //   GetStatus                          0x82B42EF8
    //   SetGlobalRunnableFunctionUserWrapper 0x82B42D98
    //   GetId                              0x82B42FB8
    //   GetPriority                        0x82B42FD8
    //   SetName                            0x82B43000
    //   Begin                              0x82B43F80
    class Thread
    {
    public:
        // Status (EATech DWARF eathread_thread.h:364).
        enum Status
        {
            kStatusNone    = 0,
            kStatusRunning = 1,
            kStatusEnded   = 2
        };

        // @ 0x82B42DC0 -- default ctor: start with no attached dynamic-data record
        // (mThreadData.mpData = 0). The X360 body is a single `stw 0,0(this)`; it is
        // the construction JobThread runs on its embedded Thread subobject at +0xC.
        Thread();

        ~Thread();

        // Block until the started thread ends (then optionally returns its value via
        // pThreadReturnValue) but give up after pTimeoutAbsolute. Reconstructed
        // OUT-OF-GROUP: the body lives in the (separate, not-yet-homed) EA::Thread
        // object TU; this is a forward declaration so callers (e.g.
        // EA::Jobs::LocalBackend::JobThread::WaitForEnd) compile and link against the
        // canonical symbol. FLAGGED -- declared here, defined in its own TU.
        Status WaitForEnd(intptr_t* pThreadReturnValue = 0,
                          const int* pTimeoutAbsolute  = 0);

        // Start a thread running pFunction(pContext), per pParameters. Returns
        // the new thread's HANDLE (ThreadId) or 0 on failure. (0x82B43F80)
        ThreadId Begin(RunnableFunction       pFunction,
                       void*                   pContext     = 0,
                       const ThreadParameters* pParameters  = 0,
                       RunnableFunctionUserWrapper pUserWrapper = 0);

        // Poll thread status; optionally writes the return value out. (0x82B42EF8)
        Status GetStatus(intptr_t* pThreadReturnValue = 0);

        // (0x82B42FB8) thread id or 0 if unstarted.
        ThreadId GetId() const;

        // (0x82B42FD8) Win32 priority, or 0x80000000 if unstarted.
        int GetPriority() const;

        // (0x82B43000) set the OS-visible thread name (and raise the VS
        // SetThreadName debugger exception on the X360 build).
        void SetName(const char* pName);

        // (0x82B42D98) latch the process-wide function user-wrapper once.
        static void SetGlobalRunnableFunctionUserWrapper(RunnableFunctionUserWrapper pUserWrapper);

        // (0x82B42D88) read back the process-wide function user-wrapper (the global
        // sGlobalRunnableFunctionUserWrapper). Begin's default 5th argument; rw::core::
        // filesys::Device::Start passes its result explicitly. Declared here (the Thread
        // home); the out-of-line body lives with the other Thread statics.
        static RunnableFunctionUserWrapper GetGlobalRunnableFunctionUserWrapper();

    protected:
        EAThreadData mThreadData;
    };
}
}
