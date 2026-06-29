#ifndef EA_JOBS_JOBS_H
#define EA_JOBS_JOBS_H

#include "types.hpp"

// SDKs/EATech/eajobs/jobs.h
//
// EA::Jobs namespace-level facade helpers for the EA Tech "job_manager" SDK,
// reconstructed from the X360 .XEX (BURNOUT_X360_ARTIST.XEX). Sibling to the
// committed EA::Jobs::Event (event.h) / EA::Jobs::EntryPoint (entrypoint.cpp)
// surfaces -- this header homes the loose namespace-scope functions:
//
//   AtomicStore(u32* puLocation, u32 uValue)  @ 0x82BCC6E8
//   SetAllocator(void* pAllocator)            @ 0x82BC9830
//   TicksToSeconds(u64 uTicks)                @ 0x82BC9988
//   ToJobThreadId(u32 uThreadId)              @ 0x82915920
//
// Vendor EA code reconstructed in its canonical home; members/names follow the EA
// SDK convention, not the Brn/Cgs project convention.

namespace EA
{
namespace Jobs
{
    // The process-wide Jobs allocator (the off_8327F280 object SetAllocator
    // installs). Only the Free virtual the SDK's scalar-deleting destructors
    // dispatch into is committed (the X360 asm proves the call site: allocator
    // vtable slot +0x0C, called as Free(this, ptr, 0)). The concrete allocator is
    // installed by the host and lives outside this group; this is the minimal
    // abstract surface the SDK calls BY NAME.
    class Allocator
    {
    public:
        virtual ~Allocator() {}
        // vtable slot +0x04: allocate a block. The X360 calls it as
        // Allocate(size, pName, flags, align, alignOffset) (`lwz r11,0(r3) / lwz r11,4(r11)`
        // then bctrl with r4=size, r5=name, r6=flags, r7=align, r8=alignOffset). Returns
        // the block (or 0). Named per the EA ICoreAllocator-style debug-alloc surface.
        virtual void* Allocate(u32 uSize, const char* pName, int iFlags,
                               u32 uAlign, u32 uAlignOffset) = 0;
        // vtable slot +0x08: release-with-flags overload placeholder (the X360 backend
        // never calls it; reserved so Free lands at the proven +0x0C slot).
        virtual void  AllocateReserved() = 0;
        // vtable slot +0x0C: release a block this allocator handed out.
        virtual void Free(void* pBlock, int iSize) = 0;
    };

    // @ 0x82BC9830 sibling -- typed view of the installed allocator (off_8327F280),
    // for the SDK's scalar-deleting destructors. Null until SetAllocator runs.
    Allocator* GetAllocator();

    // @ 0x82BCC6E8 -- atomically store uValue into *puLocation. On X360 this masks
    // interrupts (mfmsr/mtmsree) around a lwarx/stwcx. reservation pair, retrying
    // until the store-conditional succeeds. Returns puLocation (the X360 leaves the
    // target address in r3). The portable reconstruction expresses the same atomic
    // overwrite.
    u32* AtomicStore(u32* puLocation, u32 uValue);

    // @ 0x82BC9830 -- install the process-wide Jobs allocator (stored in the
    // off_8327F280 file static read by the rest of the job_manager SDK).
    void SetAllocator(void* pAllocator);

    // @ 0x82BC9988 -- convert a hardware tick count to seconds. The first call
    // lazily computes and caches the per-tick second value (1.0 / 49875000.0, the
    // X360 timebase frequency) in dbl_8327F288; subsequent calls reuse it. Returns a
    // single-precision-rounded result (the X360 frsp's f1 before returning).
    f32 TicksToSeconds(u64 uTicks);

    // @ 0x82915920 -- reinterpret a thread id as a job-thread id (identity pass
    // through; the X360 round-trips r3 through the stack and returns it unchanged).
    u32 ToJobThreadId(u32 uThreadId);
}
}

#endif // EA_JOBS_JOBS_H
