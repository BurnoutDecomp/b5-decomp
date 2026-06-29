// X360-faithful EABarrierData ctor (the data half of EA::Thread::Barrier in
// BURNOUT_X360_ARTIST.XEX). Source of truth: per-addr exports under
// .ida-exports/BURNOUT_X360_ARTIST.XEX/ (X360 asm overrides the divergent PS3/Posix
// DWARF). See eathread_barrier_data.h for the committed-type / vendor-snapshot
// divergence FLAG.
//
// Vendor EA code reconstructed in its canonical home.

#include "SDKs/EATech/eathread/eathread_barrier_data.h"

// ---------------------------------------------------------------------------
// EABarrierData::EABarrierData @ 0x82B43A50
//
//   mr   r31, r3                 ; this
//   li   r11, 0
//   loc_82B43A68 (masked-interrupt lwarx/stwcx. on r31+0):
//     mfmsr r9 ; mtmsree r13 ; lwarx r10,0,r31 ; stwcx. r11,0,r31 ; mtmsree r9
//     bne  loc_82B43A68          ; -> *(this+0x00) = 0   (mnCurrent, AtomicInt32)
//   addi r9, r31, 8
//   stw  r11, 4(r31)             ; -> *(this+0x04) = 0   (mnHeight, plain int)
//   loc_82B43A88 (masked-interrupt lwarx/stwcx. on r9 == r31+8):
//     mfmsr r8 ; mtmsree r13 ; lwarx r10,0,r9 ; stwcx. r11,0,r9 ; mtmsree r8
//     bne  loc_82B43A88          ; -> *(this+0x08) = 0   (mnIndex, AtomicInt32)
//   li   r5, 0 ; li r4, 0 ; addi r3, r31, 0xC
//   bl   EA::Thread::Semaphore::Semaphore   ; mSemaphore0 = Semaphore(0, false)
//   li   r5, 0 ; li r4, 0 ; addi r3, r31, 0x1C
//   bl   EA::Thread::Semaphore::Semaphore   ; mSemaphore1 = Semaphore(0, false)
//   mr   r3, r31 ; blr           ; return this
//
// The +0x00 and +0x08 stores use the X360 masked-interrupt (mfmsr/mtmsree)
// lwarx/stwcx. reservation -- the AtomicInt32 zero-init primitive. On a freshly
// constructed object this is just a zeroing store; following the committed sibling
// precedent (AllocateThreadDynamicData's "X360 zeroes it via lwarx/stwcx.; plain
// store here") it is modelled as a plain assignment. The +0x04 store is a plain
// non-atomic int. The two embedded Semaphores are built with Semaphore(0, false)
// (pParameters = 0, bInitialize = false) -- the asm sets r4 = r5 = 0 -- so no OS
// object is created at construction; Barrier::Init creates them on demand.
// ---------------------------------------------------------------------------
EABarrierData::EABarrierData()
    : mSemaphore0(0, false)  // +0x0C  bl Semaphore::Semaphore(this+0xC, 0, 0)
    , mSemaphore1(0, false)  // +0x1C  bl Semaphore::Semaphore(this+0x1C, 0, 0)
{
    // +0x00 mnCurrent: AtomicInt32 zero-init (X360 masked-interrupt lwarx/stwcx.).
    mnCurrent = 0;
    // +0x04 mnHeight: plain `stw r11,4(r31)`.
    mnHeight = 0;
    // +0x08 mnIndex: AtomicInt32 zero-init (X360 masked-interrupt lwarx/stwcx.).
    mnIndex = 0;
}
