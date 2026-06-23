// X360-faithful EA::Thread::Semaphore wrapper (the Win32-semaphore variant linked
// into BURNOUT_X360_ARTIST.XEX). Source of truth: per-addr exports under
// .ida-exports/BURNOUT_X360_ARTIST.XEX/ (X360 asm overrides the divergent newer
// vendor snapshot). See eathread_semaphore.h for the committed-type / vendor-
// snapshot divergence FLAG and the WAIT-TIMEOUT FLAG.

#include <Windows.h>  // CreateSemaphoreA / WaitForSingleObject / CloseHandle / GetTickCount
#include <intrin.h>   // _InterlockedDecrement -- portable stand-in for the X360
                      // interrupt-masked lwarx/stwcx. reservation idiom.

#include "SDKs/EATech/eathread/eathread_semaphore.h"

namespace EA
{
namespace Thread
{

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

    SemaphoreParameters lDefault;  // {mInitialCount=0, mbIntraProcess=true, mName=""}
    if (pParameters == 0 && bInitialize)
    {
        lDefault.mInitialCount  = 0;     // stw 0
        lDefault.mbIntraProcess = true;  // stb 1
        lDefault.mName[0]       = '\0';  // stb 0
        pParameters = &lDefault;
    }

    Init(pParameters);
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
    if (pParameters == 0)
        return false;
    if (mSemaphoreData.mhSemaphore != 0)
        return false;

    s32 iCount = pParameters->mInitialCount;     // lwz 0(a2)
    mSemaphoreData.mnCount = iCount;             // stw r11, 4(this)
    if (iCount < 0)
        mSemaphoreData.mnCount = 0;              // clamp negative initial count

    mSemaphoreData.mbIntraProcess = (pParameters->mbIntraProcess != false); // lbz 4(a2) -> stb 0xC
    LONG        lInitialCount = 0;
    const char* pName         = 0;

    if (mSemaphoreData.mbIntraProcess)
    {
        // intra-process: OS object starts at 0 permits, no name.
        lInitialCount = 0;
        pName         = 0;
    }
    else
    {
        // inter-process: a non-empty name is passed through; the OS object is
        // seeded with the live (clamped) permit count.
        pName         = (pParameters->mName[0] != '\0') ? pParameters->mName : 0;
        lInitialCount = mSemaphoreData.mnCount;
    }

    HANDLE hSemaphore = ::CreateSemaphoreA(0, lInitialCount, 0x3FFFFFFF, pName);
    mSemaphoreData.mhSemaphore = hSemaphore;
    return hSemaphore != 0;
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
    // ---- absolute deadline -> relative Win32 timeout ------------------------
    DWORD dwTimeout;
    const u32 uDeadline = *pTimeoutAbsolute;
    if (uDeadline == 0xFFFFFFFFu || uDeadline == 0u)
    {
        // INFINITE (-1) and poll (0) pass through unchanged.
        dwTimeout = uDeadline;
    }
    else
    {
        const DWORD dwRef = ::GetTickCount() + 0x2710u; // + 10000 ms
        if (uDeadline <= dwRef)
            dwTimeout = 0;
        else
            dwTimeout = uDeadline - dwRef;
    }

    volatile long* const lpCount =
        reinterpret_cast<volatile long*>(&mSemaphoreData.mnCount); // r30+4

    if (mSemaphoreData.mbIntraProcess)  // lbz 0xC(r30) != 0
    {
        // atomically mnCount -= 1; r11 = post-decrement value.
        const long lPost = _InterlockedDecrement(lpCount);

        if (lPost < 0)  // count went negative -> there were no free permits
        {
            if (::WaitForSingleObject(mSemaphoreData.mhSemaphore, dwTimeout) != 0)
            {
                // OS wait failed/timed out: hand the borrowed permit back.
                mSemaphoreData.UpdateCancelCount(1);
                return kResultError;  // -2
            }
        }

        // Acquired (either immediately or after the OS wait): publish and report
        // the remaining count, clamped at 0.
        // lwsync -- acquire barrier (modelled as the interlocked op's ordering).
        const long lRemaining = mSemaphoreData.mnCount; // lwz 0(r31)
        if (lRemaining <= 0)
            return 0;
        return static_cast<int>(lRemaining);
    }
    else
    {
        const DWORD dwResult = ::WaitForSingleObject(mSemaphoreData.mhSemaphore, dwTimeout);
        if (dwResult == 0)  // WAIT_OBJECT_0
        {
            // success: atomically mnCount -= 1, return the post-decrement value.
            return static_cast<int>(_InterlockedDecrement(lpCount));
        }

        // failure: reproduce the X360 bit-test verbatim --
        //   ((cntlzw(dwResult - 258) & 0x20) == 0) - 2
        // dwResult == WAIT_TIMEOUT (258) -> (258-258)==0 -> cntlzw==32 -> &0x20!=0
        //   -> (==0) is false (0) -> 0 - 2 = -2.
        // dwResult != 258 -> non-zero -> cntlzw < 32 -> &0x20==0 -> true (1)
        //   -> 1 - 2 = -1.
        const unsigned int uDelta = dwResult - 258u;        // WAIT_TIMEOUT
        unsigned long      uLeadingZeros = 0;
        // cntlzw: count of leading zero bits of the 32-bit value (32 when zero).
        if (uDelta == 0u)
            uLeadingZeros = 32u;
        else
        {
            unsigned long uIndex;
            _BitScanReverse(&uIndex, uDelta);  // index of highest set bit
            uLeadingZeros = 31u - uIndex;
        }
        return ((uLeadingZeros & 0x20u) == 0u ? 1 : 0) - 2;
    }
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
    const s32 iCount = mSemaphoreData.mnCount;
    if (iCount > 0)
        return iCount;
    return 0;
}

} // namespace Thread
} // namespace EA
