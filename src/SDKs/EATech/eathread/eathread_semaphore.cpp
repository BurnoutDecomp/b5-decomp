// X360-faithful EA::Thread::Semaphore wrapper (the Win32-semaphore variant linked
// into BURNOUT_X360_ARTIST.XEX). Source of truth: per-addr exports under
// .ida-exports/BURNOUT_X360_ARTIST.XEX/ (X360 asm overrides the divergent newer
// vendor snapshot). See eathread_semaphore.h for the committed-type / vendor-
// snapshot divergence FLAG and the WAIT-TIMEOUT FLAG.

// XDK size optimization matches ARTIST at 0x82B42970.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include <xtl.h>  // native semaphore APIs, atomics and _CountLeadingZeros
#include <new>        // placement new (SemaphoreFactory::ConstructSemaphore)
#include <cstring>

#include "SDKs/EATech/eathread/eathread_semaphore.h"

// ARTIST 0x82B42870. Return cancelled permits to waiters before banking the remainder.
void EASemaphoreData::UpdateCancelCount(int iAdjustment)
{
    int32_t oldCount, newCount, cmpCount;
    if (iAdjustment > 0)
    {
        oldCount = mnCount;
        while (oldCount < 0)
        {
            if ((newCount = oldCount + iAdjustment) > 0)
                newCount = 0;

            cmpCount = oldCount;
            oldCount = _InterlockedCompareExchange(reinterpret_cast<volatile long*>(&mnCount), newCount, cmpCount);
            if (oldCount == cmpCount)
            {
                iAdjustment -= newCount - oldCount;
                break;
            }
        }

        if (iAdjustment > 0)
            _InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&mnCancelCount), iAdjustment);
    }
}

namespace EA
{
namespace Thread
{

// ARTIST 0x82B42910. The EAThread semaphore implementation also owns its parameters.
SemaphoreParameters::SemaphoreParameters(int initialCount, bool bIntraProcess, const char* pName)
{
    mInitialCount  = initialCount;
    mbIntraProcess = bIntraProcess;
    if (pName)
    {
        std::strncpy(mName, pName, 15);
        mName[15] = 0;
    }
    else
    {
        mName[0] = 0;
    }
}

// ---------------------------------------------------------------------------
// Semaphore::Semaphore @ 0x82B43960
//
//   stw 0, 0(r3) ; stw 0, 4(r3) ; stw 0, 8(r3)   -> HANDLE / mnCount / mnCancelCount = 0
//   stb 1, 0xC(r3)                               -> mbIntraProcess = 1
//   if (a2 == 0 && (a3 & 0xFF) != 0)             -> no params but bInitialize:
//     build {mInitialCount=0, mbIntraProcess=1, mName[0]=0} on the stack and use it
//   bl EA::Thread::Semaphore::Init(this, a2)
//
// The three zero stores + the mbIntraProcess=1 store are the EASemaphoreData
// member's in-place initialisation; Init then (re)derives the live fields from
// the parameters. The X360 stack params struct matches SemaphoreParameters
// {mInitialCount@0, mbIntraProcess@4 (set 1), mName@5 (NUL@+5)}.
// ---------------------------------------------------------------------------
Semaphore::Semaphore(const SemaphoreParameters* pParameters, bool bInitialize)
{
    mSemaphoreData.mhSemaphore   = 0;
    mSemaphoreData.mnCount       = 0;
    mSemaphoreData.mnCancelCount = 0;
    mSemaphoreData.mbIntraProcess = true;  // stb 1, 0xC(r3)

    if (!pParameters && bInitialize)
    {
        SemaphoreParameters lDefault;
        Init(&lDefault);
    }
    else
    {
        Init(pParameters);
    }
}

// The integer overload initializes the same native handle, using the caller's permit count.
Semaphore::Semaphore(int iInitialCount)
{
    mSemaphoreData.mhSemaphore = 0;
    mSemaphoreData.mnCount = 0;
    mSemaphoreData.mnCancelCount = 0;
    mSemaphoreData.mbIntraProcess = true;

    SemaphoreParameters lParameters(iInitialCount);
    Init(&lParameters);
}

// ---------------------------------------------------------------------------
// Semaphore::~Semaphore @ 0x82B42970
//
//   lwz r3, 0(r3) ; cmplwi r3, 0 ; beqlr ; b CloseHandle
//
// If the OS HANDLE (at +0) is non-null, CloseHandle it (tail-call); otherwise
// nothing to do.
// ---------------------------------------------------------------------------
Semaphore::~Semaphore()
{
    if (mSemaphoreData.mhSemaphore)
    {
        ::CloseHandle(mSemaphoreData.mhSemaphore);
    }
}

// ---------------------------------------------------------------------------
// Semaphore::Init @ 0x82B42988
//
//   if (a2 == 0)               return 0;            -> no params
//   if (*(this+0) != 0)        return 0;            -> already have a handle
//   v = a2->mInitialCount (lwz 0(a2)); store to mnCount(+4); if (v < 0) mnCount = 0
//   ip = a2->mbIntraProcess (lbz 4(a2)); store to mbIntraProcess(+0xC)
//   if (ip != 0)  { initCount = 0; name = 0; }
//   else          { name = (a2->mName[0] != 0) ? &a2->mName : 0;
//                   initCount = mnCount; }
//   handle = CreateSemaphoreA(0, initCount, 0x3FFFFFFF, name)
//   *(this+0) = handle
//   return (cntlzw(handle) >> 5 & 1) == 0   -> true iff handle != 0
//
// CreateSemaphoreA's lInitialCount is the (clamped >= 0) mnCount for a *named*
// (inter-process) semaphore, but 0 for an intra-process one (the intra-process
// variant tracks permits purely in mnCount and only uses the OS object as a
// blocking wait). The return is "handle != null" (cntlzw of a non-zero 32-bit
// value is < 32, so bit 5 is clear -> the (==0) test yields true).
// ---------------------------------------------------------------------------
bool Semaphore::Init(const SemaphoreParameters* pParameters)
{
    // EAThread eathread_semaphore_pc.cpp:123 shape: one guarded block, the member flag copied as is
    // (ARTIST lbz/stb 0x82B429C8..0x82B429D0, no normalisation), the intra-process semaphore created
    // with no permits and no name, the result read back from the member (0x82B42A0C..0x82B42A1C).
    if (pParameters && !mSemaphoreData.mhSemaphore)
    {
        mSemaphoreData.mnCount = pParameters->mInitialCount;

        if (mSemaphoreData.mnCount < 0)
            mSemaphoreData.mnCount = 0;

        mSemaphoreData.mbIntraProcess = pParameters->mbIntraProcess;

        if (mSemaphoreData.mbIntraProcess)
            mSemaphoreData.mhSemaphore = ::CreateSemaphoreA(0, 0, 0x3FFFFFFF, 0);
        else
            mSemaphoreData.mhSemaphore = ::CreateSemaphoreA(0, (LONG)mSemaphoreData.mnCount, 0x3FFFFFFF,
                                                            pParameters->mName[0] ? pParameters->mName : 0);

        return mSemaphoreData.mhSemaphore != 0;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Semaphore::Wait @ 0x82B42A40
//
// See eathread_semaphore.h's WAIT-TIMEOUT FLAG for the (asm-authoritative) 10s
// absolute-deadline -> relative-timeout conversion reproduced below. The body
// has two arms keyed on mbIntraProcess (+0xC):
//
//   intra-process: atomically mnCount-=1; if it was still >= 0 we got a permit
//     immediately (lwsync, return the clamped count). If it went negative, block
//     on the OS object; a failed/timed-out OS wait rolls the permit back via
//     UpdateCancelCount(this, 1) and returns -2.
//
//   inter-process: block on the OS object first; on success atomically mnCount-=1
//     and return the post-decrement count; on failure map WAIT_TIMEOUT(258) -> -2,
//     any other failure -> -1 (the cntlzw bit-test below, reproduced verbatim).
// ---------------------------------------------------------------------------
int Semaphore::Wait(const u32* pTimeoutAbsolute)
{
    DWORD dwTimeout;
    if (*pTimeoutAbsolute == INFINITE || *pTimeoutAbsolute == 0)
    {
        dwTimeout = *pTimeoutAbsolute;
    }
    else
    {
        const DWORD dwReferenceTime = ::GetTickCount() + 10000;
        dwTimeout = *pTimeoutAbsolute > dwReferenceTime ? *pTimeoutAbsolute - dwReferenceTime : 0;
    }

    if (mSemaphoreData.mbIntraProcess)
    {
        const long lRemaining = _InterlockedDecrement(
            reinterpret_cast<volatile long*>(&mSemaphoreData.mnCount));
        if (lRemaining < 0 && ::WaitForSingleObject(mSemaphoreData.mhSemaphore, dwTimeout) != WAIT_OBJECT_0)
        {
            mSemaphoreData.UpdateCancelCount(1);
            return kResultError;
        }

        MemoryBarrier();
        return GetCount();
    }

    const DWORD dwResult = ::WaitForSingleObject(mSemaphoreData.mhSemaphore, dwTimeout);
    if (dwResult == WAIT_OBJECT_0)
    {
        return _InterlockedDecrement(reinterpret_cast<volatile long*>(&mSemaphoreData.mnCount));
    }

    // The inter-process path returns -2 for a timeout and -1 for other failures.
    return (dwResult != WAIT_TIMEOUT) - 2;
}

// ARTIST 0x82B42B70. Publish permits and, where needed, wake kernel waiters.
int Semaphore::Post(int iCount)
{
    if (iCount > 0)
    {
        if (mSemaphoreData.mbIntraProcess)
        {
            MemoryBarrier();
            if (mSemaphoreData.mnCancelCount > 0 && mSemaphoreData.mnCount < 0)
            {
                const long lCancelled = _InterlockedExchange(
                    reinterpret_cast<volatile long*>(&mSemaphoreData.mnCancelCount), 0);
                mSemaphoreData.UpdateCancelCount(lCancelled);
            }

            const long lPrevious = _InterlockedExchangeAdd(
                reinterpret_cast<volatile long*>(&mSemaphoreData.mnCount), iCount);
            if (lPrevious < 0)
            {
                const long lWaiting = -lPrevious;
                const long lWake = iCount < lWaiting ? iCount : lWaiting;
                ::ReleaseSemaphore(mSemaphoreData.mhSemaphore, lWake, 0);
            }
            return GetCount();
        }

        volatile long* const lpCount = reinterpret_cast<volatile long*>(&mSemaphoreData.mnCount);
        _InterlockedExchangeAdd(lpCount, iCount);
        if (!::ReleaseSemaphore(mSemaphoreData.mhSemaphore, iCount, 0))
        {
            _InterlockedExchangeAdd(lpCount, -iCount);
            return -1;
        }
    }
    return mSemaphoreData.mnCount;
}

// ---------------------------------------------------------------------------
// Semaphore::GetCount @ 0x82B42CB0
//
//   lwz r11, 4(r3) ; cmpwi r11, 0 ; bgt + ; li r11, 0 ; mr r3, r11
//
// Return mnCount, but report a negative count (waiters present, no free permits)
// as 0.
// ---------------------------------------------------------------------------
int Semaphore::GetCount() const
{
    const int iCount = mSemaphoreData.mnCount;
    return iCount > 0 ? iCount : 0;
}

// ---------------------------------------------------------------------------
// SemaphoreFactory::ConstructSemaphore @ 0x82B43A30
//
//   cmplwi cr6, r3, 0 ; beq cr6, +     -> if (pMemory == 0) return 0;
//   li r5, 1 ; li r4, 0 ; b Semaphore::Semaphore   (tail-call the ctor with
//                                                    pParameters = 0, bInitialize = 1)
//
// Placement-construct a Semaphore into the caller's memory; the ctor runs with
// its defaults (no parameters, initialize = true). A null memory pointer
// short-circuits to null without constructing.
// ---------------------------------------------------------------------------
Semaphore* SemaphoreFactory::ConstructSemaphore(void* pMemory)
{
    return new (pMemory) Semaphore;
}

} // namespace Thread
} // namespace EA
