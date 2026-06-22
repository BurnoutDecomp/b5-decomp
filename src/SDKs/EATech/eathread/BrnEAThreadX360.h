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

    // ---- Thread-dynamic-data pool (X360) -----------------------------------

    // EAThreadDynamicData is the per-thread bookkeeping record. The X360 binary
    // touches a 68-byte (0x44) record and zeroes offsets 0,4,8,0xC,0x1C plus a
    // spinlock word at +0x20 on allocation. Only the touched offsets are named;
    // the remainder is opaque payload preserved to keep the 68-byte size.
    //
    // FLAG: the exact semantics of most of these fields are not recoverable
    // from this leaf TU alone (they are written/read by EAThreadDynamicData
    // users outside this dossier). Names below are HONEST placeholders sized to
    // the proven 68-byte record; do not treat field meanings beyond the offsets
    // as X360 fact.
    struct EAThreadDynamicData
    {
        HANDLE mhThread;        // +0x00  (zeroed on alloc; CloseHandle target on free)
        u32    muField04;       // +0x04
        u32    muField08;       // +0x08
        u32    muField0C;       // +0x0C
        u32    muOpaque10[3];   // +0x10..0x1B (untouched on alloc)
        u32    muField1C;       // +0x1C
        s32    miSpinLock;      // +0x20  (lwarx/stwcx. word; zeroed on alloc)
        u32    muOpaque24[8];   // +0x24..0x43  (fills out to 68 bytes)
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
}
}
