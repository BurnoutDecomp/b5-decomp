// X360-faithful EARWMutexData ctor (the data half of EA::Thread::RWMutex in
// BURNOUT_X360_ARTIST.XEX). Source of truth: per-addr exports under
// .ida-exports/BURNOUT_X360_ARTIST.XEX/ (X360 asm overrides the divergent newer
// vendor snapshot). See eathread_rwmutex_data.h for the committed-type /
// vendor-snapshot divergence FLAG.
//
// Vendor EA code reconstructed in its canonical home.

#include "SDKs/EATech/eathread/eathread_rwmutex_data.h"

#include <windows.h>  // CRITICAL_SECTION, InitializeCriticalSection

// ---------------------------------------------------------------------------
// EARWMutexData::EARWMutexData @ 0x82B43C18
//
//   mr   r31, r3                 ; this
//   li   r30, 0
//   li   r5, 1 ; li r4, 0 ; addi r3, r31, 0x10
//   stw  r30, 0(r31)             ; -> mnReadWaiters   = 0  (+0x00)
//   stw  r30, 4(r31)             ; -> mnWriteWaiters  = 0  (+0x04)
//   stw  r30, 8(r31)             ; -> mnReaders       = 0  (+0x08)
//   stw  r30, 0xC(r31)           ; -> mThreadIdWriter = 0  (+0x0C)
//   bl   EA::Thread::Mutex::Mutex ; mMutex = Mutex(0, 1) -> InitializeCriticalSection(this+0x10)
//   addi r29, r31, 0x40
//   mr   r3, r29
//   bl   EAConditionData::EAConditionData          ; mReadCondition data half (this+0x40)
//   li   r28, 1
//   stb  r30, var_5F(r1)         ; ConditionParameters.mName[0] = 0
//   stb  r28, var_60(r1)         ; ConditionParameters.mbIntraProcess = 1 (true)
//   addi r4, r1, var_60 ; mr r3, r29
//   bl   EA::Thread::Condition::Init               ; mReadCondition.Init(&cp)
//   addi r29, r31, 0xA0
//   mr   r3, r29
//   bl   EAConditionData::EAConditionData          ; mWriteCondition data half (this+0xA0)
//   stb  r28, var_40(r1)         ; ConditionParameters.mbIntraProcess = 1 (true)
//   stb  r30, var_3F(r1)         ; ConditionParameters.mName[0] = 0
//   addi r4, r1, var_40 ; mr r3, r29
//   bl   EA::Thread::Condition::Init               ; mWriteCondition.Init(&cp)
//   mr   r3, r31 ; b __restgprlr_28 ; return this
//
// The X360 ctor first builds each Condition's data half (EAConditionData ctor =
// member-wise construction with no OS objects -- the committed Condition(0, false)
// form) and THEN drives Condition::Init from a freshly built ConditionParameters
// {mbIntraProcess = true, mName = ""}. Each condition gets its own stack-built
// ConditionParameters; both carry intra-process = 1 and an empty name (the asm
// stores 1 into mbIntraProcess and 0 into the first name byte for both). The
// embedded Mutex is constructed via EA::Thread::Mutex::Mutex(this+0x10, 0, 1)
// (pParameters = 0, bInitialize = 1), modelled on the host as InitializeCriticalSection
// of mMutex -- the committed device.cpp / eathread_condition.h Mutex->CRITICAL_SECTION
// mapping.
// ---------------------------------------------------------------------------
EARWMutexData::EARWMutexData()
    : mReadCondition(0, false)   // +0x40  EAConditionData ctor (data half, no OS objects)
    , mWriteCondition(0, false)  // +0xA0  EAConditionData ctor (data half, no OS objects)
{
    // +0x00..+0x0C: zero the four counters (stw r30 stores).
    mnReadWaiters   = 0;
    mnWriteWaiters  = 0;
    mnReaders       = 0;
    mThreadIdWriter = 0;

    // +0x10: EA::Thread::Mutex::Mutex(this+0x10, 0, 1) -> build the critical section.
    ::InitializeCriticalSection(&mMutex);

    // +0x40 mReadCondition: Init from ConditionParameters{intra = true, name = ""}.
    EA::Thread::ConditionParameters lReadConditionParams(true, 0);
    mReadCondition.Init(&lReadConditionParams);

    // +0xA0 mWriteCondition: Init from ConditionParameters{intra = true, name = ""}.
    EA::Thread::ConditionParameters lWriteConditionParams(true, 0);
    mWriteCondition.Init(&lWriteConditionParams);
}
