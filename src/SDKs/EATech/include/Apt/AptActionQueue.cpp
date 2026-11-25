// XDK size optimization matches ARTIST at 0x82AD5E68 and 0x82ADC5E0.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "SDKs/EATech/include/Apt/AptActionQueue.h"

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValue::AddRef/Release + sReferenceRegistrationCb
#include "SDKs/EATech/include/Apt/AptCIH.h"               // AptCIH (the queued action target)
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"            // the CIH's bound instance
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h"  // mnLastActionFrame (the queue stamp)
#include "SDKs/EATech/Apt/DogmaAllocator.h"               // DOGMA_PoolManager (ring allocation)

#include <cstring>   // memmove

// ===========================================================================
//  AptActionQueueC member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
//  The X360 asm is authoritative (see AptActionQueue.h for the recovered layout
//  and the address of each function).
//
//  The queue is a circular deque of AptAnimationPoolData slots. mpFront is the
//  next slot to dequeue and mpBack is one-past the last live slot; mpFront ==
//  mpBack means empty. The ring wraps from the last physical slot back to
//  mpBegin. Throughout, the X360 advances a
//  slot pointer by the 20-byte slot stride and wraps it against
//  (mpBegin + mnCapacity*20); modelled here as ++/-- on the typed slot pointer
//  with an explicit wrap helper.
// ===========================================================================

// ---------------------------------------------------------------------------
// Shared Apt fixed-size pool (X360 off_8324D808). The ring buffer is allocated
// from / freed back to this pool -- the same DOGMA pool the other Apt
// display-list nodes use (declared under this name by AptPseudoCIH.h; the single
// underlying object is shared).
// ---------------------------------------------------------------------------
extern DOGMA_PoolManager* gpAptPseudoDataPool;   // off_8324D808

namespace
{
    // The slot stride the X360 uses everywhere (20 == sizeof one slot's five
    // dwords). On PC AptAnimationPoolData is wider (8-byte pointers), so element
    // arithmetic is done on the typed pointer; this literal is documented for the
    // X360 cross-reference only.
    const u32 KU_X360_SLOT_STRIDE = 20u;

    // The X360 "wrap-around" guard the deque arithmetic shares: when a slot
    // pointer reaches one past the last physical slot it folds back to mpBegin.
    // (X360: cmp against `mnCapacity*20 + *mpBegin`.)
    inline AptAnimationPoolData* WrapForward(AptAnimationPoolData* pSlot,
                                             AptAnimationPoolData* pBegin,
                                             u32                   nCapacity)
    {
        return (pSlot == pBegin + nCapacity) ? pBegin : pSlot;
    }
}

// ---- AptActionQueueC @ 0x82AE6780 ----------------------------------------
// Allocate the ring and its leading size word from the shared DOGMA pool.
// The 20-byte element product is bounded by the unsigned 32-bit size limit.
AptActionQueueC::AptActionQueueC(u32 nCapacity)
{
    mnCapacity = nCapacity;

    // ARTIST 0x82AE6798 compares capacity against the unsigned size limit.
    u32 luByteSize = sizeof(AptAnimationPoolData) * nCapacity;
    if (nCapacity > (0xFFFFFFFFu / sizeof(AptAnimationPoolData)))
        luByteSize = 0xFFFFFFFFu;

    // The ring begins after the allocation size stored in the leading word.
    u32* lpBlock = static_cast<u32*>(gpAptPseudoDataPool->Allocate(luByteSize + sizeof(u32)));
    *lpBlock = luByteSize;

    AptAnimationPoolData* lpRing =
        reinterpret_cast<AptAnimationPoolData*>(lpBlock + 1);
    mpBegin   = lpRing;
    mpBack    = lpRing;
    mpFront   = lpRing;

    ClearActions();
}

// ---- ClearActions @ 0x82ADF1D8 -------------------------------------------
// Drain the queue: walk every live slot front-to-back, Release the GC pointers
// it holds, mark it empty, then reset front == back == begin. An action slot
// holds one CIH ref; a function slot holds a context ref and a func-def ref.
void AptActionQueueC::ClearActions()
{
    AptAnimationPoolData* lpSlot = mpFront;
    if (lpSlot != mpBack)
    {
        do
        {
            switch (lpSlot->mnType)
            {
                case AptAnimationPoolData::E_ACTION_TYPE_ACTION:
                    if (lpSlot->action.mpCIH != 0)
                    {
                        lpSlot->action.mpCIH->Release();
                    }
                    break;

                case AptAnimationPoolData::E_ACTION_TYPE_FUNCTION:
                    if (lpSlot->function.mpContext != 0)
                    {
                        lpSlot->function.mpContext->Release();
                    }
                    if (lpSlot->function.mpFuncDef != 0)
                    {
                        lpSlot->function.mpFuncDef->Release();
                    }
                    break;

                default:
                    break;
            }

            lpSlot->mnType = AptAnimationPoolData::E_ACTION_TYPE_EMPTY;

            ++lpSlot;
            lpSlot = WrapForward(lpSlot, mpBegin, mnCapacity);
        }
        while (lpSlot != mpBack);
    }

    mpBack  = mpBegin;
    mpFront = mpBegin;
}

namespace
{
    // The action-slot stamp both enqueues snapshot: the CIH's bound sprite
    // instance's mnLastActionFrame (charInst word[8] +0x20; the console reads it
    // blind -- every queued-action CIH is sprite-family).
    inline s32 AptQueueStampOf(AptCIH* pCIH)
    {
        AptCharacterInst* pInst = pCIH->mpCharacterInst;
        return pInst ? static_cast<AptCharacterSpriteInstBase*>(pInst)->mnLastActionFrame : 0;
    }
}

// ---- AddActionBack @ 0x82AD94B0 ------------------------------------------
// Enqueue a clip-event action at the back. No-op if advancing the back cursor
// would collide with the front (ring full). Snapshots the instance depth, stores
// the CIH + event id + context, and AddRefs the CIH. Returns the CIH.
AptValue* AptActionQueueC::AddActionBack(const void* pEventStreamSlot, AptCIH* pCIH, s32 iContext)
{
    AptAnimationPoolData* lpBack = mpBack;

    AptAnimationPoolData* lpNextBack = lpBack + 1;
    lpNextBack = WrapForward(lpNextBack, mpBegin, mnCapacity);

    if (lpNextBack == mpFront)
    {
        return pCIH;   // ring full -- drop the action (X360 returns `this`)
    }

    lpBack->mnType         = AptAnimationPoolData::E_ACTION_TYPE_ACTION;
    // The queue stamp: the sprite's mnLastActionFrame (charInst word[8] +0x20 --
    // PS3 asm `slot[+8] = *(a3[8] + 0x20)`). The tick NEGATES it around
    // queueFrameActions, so a queued frame action carries -frame; RunActions'
    // gate then runs it only while the inst is still AT that frame.
    lpBack->action.miDepth   = AptQueueStampOf(pCIH);
    lpBack->action.mpEventStreamSlot = pEventStreamSlot;
    lpBack->action.mpCIH     = pCIH;
    pCIH->AddRef();
    lpBack->mnInput = iContext;

    mpBack = lpNextBack;
    return pCIH;
}

// ---- AddActionFront @ 0x82AD9548 -----------------------------------------
// Enqueue a clip-event action at the front (push onto the head). Mirrors
// AddActionBack but steps the front cursor back one slot first; no-op if that
// would collide with the back.
AptValue* AptActionQueueC::AddActionFront(const void* pEventStreamSlot, AptCIH* pCIH, s32 iContext)
{
    AptAnimationPoolData* lpNewFront = mpFront - 1;
    if (lpNewFront < mpBegin)
    {
        lpNewFront = mpBegin + mnCapacity - 1;
    }

    if (lpNewFront == mpBack)
    {
        return pCIH;   // ring full -- drop the action
    }

    mpFront = lpNewFront;
    // The same mnLastActionFrame stamp as AddActionBack (see there).
    lpNewFront->action.miDepth   = AptQueueStampOf(pCIH);
    lpNewFront->mnType           = AptAnimationPoolData::E_ACTION_TYPE_ACTION;
    lpNewFront->action.mpEventStreamSlot = pEventStreamSlot;
    lpNewFront->action.mpCIH     = pCIH;
    pCIH->AddRef();
    lpNewFront->mnInput = iContext;

    return pCIH;
}

// ---- AddFunctionBack @ 0x82AD95F8 ----------------------------------------
// Enqueue an ActionScript function call at the back. No-op if the ring is full.
// Stores the arg count, the context + func-def values (AddRefing both), and the
// return register. Returns the func-def.
AptValue* AptActionQueueC::AddFunctionBack(AptValue* pContext, AptValue* pFuncDef,
                                           s32 iReturnReg, s32 iArgCount)
{
    AptAnimationPoolData* lpBack = mpBack;

    AptAnimationPoolData* lpNextBack = lpBack + 1;
    lpNextBack = WrapForward(lpNextBack, mpBegin, mnCapacity);

    if (lpNextBack == mpFront)
    {
        return pFuncDef;   // ring full -- drop the call (X360 returns `this`)
    }

    lpBack->mnType             = AptAnimationPoolData::E_ACTION_TYPE_FUNCTION;
    lpBack->mnInput = iArgCount;
    lpBack->function.mpContext  = pContext;
    pContext->AddRef();
    lpBack->function.mpFuncDef  = pFuncDef;
    pFuncDef->AddRef();
    lpBack->function.miReturnReg = iReturnReg;

    mpBack = lpNextBack;
    return pFuncDef;
}

// ---- AddFunctionFront @ 0x82AD96A8 ---------------------------------------
// Enqueue an ActionScript function call at the front. Mirrors AddFunctionBack
// but steps the front cursor back one slot first.
AptValue* AptActionQueueC::AddFunctionFront(AptValue* pContext, AptValue* pFuncDef,
                                            s32 iReturnReg, s32 iArgCount)
{
    AptAnimationPoolData* lpNewFront = mpFront - 1;
    if (lpNewFront < mpBegin)
    {
        lpNewFront = mpBegin + mnCapacity - 1;
    }

    if (lpNewFront == mpBack)
    {
        return pFuncDef;   // ring full -- drop the call
    }

    mpFront = lpNewFront;
    lpNewFront->mnType             = AptAnimationPoolData::E_ACTION_TYPE_FUNCTION;
    lpNewFront->mnInput = iArgCount;
    lpNewFront->function.mpContext  = pContext;
    pContext->AddRef();
    lpNewFront->function.mpFuncDef  = pFuncDef;
    pFuncDef->AddRef();
    lpNewFront->function.miReturnReg = iReturnReg;

    return pFuncDef;
}

// ---- RemoveActionFor @ 0x82ADF2A0 ----------------------------------------
// Remove the first live ACTION slot queued for pContext (an action whose
// mpCIH == pContext, that is not the current item), Release its CIH, and close
// the gap. The X360 picks whichever end is nearer to minimise the memmove:
//   * a slot below the back run   -> shift the slots after it down, mpBack--
//   * a slot above the front      -> shift the slots before it up,  mpFront++
//   * the front slot itself       -> just pop the front (no shift)
// Returns pContext (X360 fastcall return value).
AptValue* AptActionQueueC::RemoveActionFor(AptValue* pContext)
{
    AptAnimationPoolData* lpFront = mpFront;
    AptAnimationPoolData* lpBack  = mpBack;
    AptAnimationPoolData* lpSlot  = lpFront;

    if (lpFront == lpBack)
    {
        return pContext;   // empty
    }

    for (;;)
    {
        const bool lbMatches =
            lpSlot->mnType == AptAnimationPoolData::E_ACTION_TYPE_ACTION
            && lpSlot->action.mpCIH == pContext
            && lpSlot != mpCurItem;

        if (lbMatches)
        {
            if (lpSlot < lpBack)
            {
                // Interior of the back run: drop pContext's ref and shift every
                // slot after lpSlot down by one, then retreat the back cursor.
                pContext->Release();
                const u32 lnTrailing =
                    static_cast<u32>(lpBack - lpSlot) - 1u;   // slots after lpSlot
                memmove(lpSlot, lpSlot + 1, lnTrailing * sizeof(AptAnimationPoolData));

                AptAnimationPoolData* lpNewBack = mpBack - 1;
                if (lpNewBack < mpBegin)
                {
                    lpNewBack = mpBegin + mnCapacity - 1;
                }
                mpBack = lpNewBack;
                return pContext;
            }

            if (lpSlot > lpFront)
            {
                // In the wrapped front run: drop the ref and shift every slot
                // before lpSlot up by one, then advance the front cursor.
                pContext->Release();
                const u32 lnLeading = static_cast<u32>(lpSlot - lpFront);
                memmove(lpFront + 1, lpFront, lnLeading * sizeof(AptAnimationPoolData));

                AptAnimationPoolData* lpNewFront = mpFront + 1;
                lpNewFront = WrapForward(lpNewFront, mpBegin, mnCapacity);
                mpFront = lpNewFront;
                return pContext;
            }

            // lpSlot == lpFront: pop the front (the X360 advances mpFront here
            // without an extra shift or Release -- the front slot is the next to
            // be dequeued, so its ref is settled on dequeue).
            AptAnimationPoolData* lpNewFront = mpFront + 1;
            lpNewFront = WrapForward(lpNewFront, mpBegin, mnCapacity);
            mpFront = lpNewFront;
            return pContext;
        }

        // Advance to the next slot (wrapping); stop once we return to the back.
        ++lpSlot;
        lpSlot = WrapForward(lpSlot, mpBegin, mnCapacity);
        if (lpSlot == mpBack)
        {
            return pContext;
        }
    }
}

// ---- RegisterReferences @ 0x82AD97B0 -------------------------------------
// GC mark walk: register every held AptValue pointer with the collector so the
// queued actions/functions survive a collection. The X360 walks logical slots
// 0..count-1 via GetDequeLocation and registers each slot's GC pointers under
// the embedded debug names. (count = (mpBack - mpFront)/20, wrapped into
// [0, mnCapacity).)
void AptActionQueueC::RegisterReferences()
{
    // Live slot count, wrapped into [0, mnCapacity) (the X360 adds mnCapacity to
    // a negative front-to-back distance).
    s32 liCount = static_cast<s32>(mpBack - mpFront);
    if (liCount < 0)
    {
        liCount += static_cast<s32>(mnCapacity);
    }

    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        AptAnimationPoolData* lpSlot = GetDequeLocation(liIndex);

        if (lpSlot->mnType == AptAnimationPoolData::E_ACTION_TYPE_ACTION)
        {
            AptValue::sReferenceRegistrationCb(
                0, &lpSlot->action.mpCIH,
                "AptAnimationPoolData::action.pCIH", 1);
        }
        else if (lpSlot->mnType == AptAnimationPoolData::E_ACTION_TYPE_FUNCTION)
        {
            AptValue::sReferenceRegistrationCb(
                0, &lpSlot->function.mpContext,
                "AptAnimationPoolData::function.pContext", 1);
            AptValue::sReferenceRegistrationCb(
                0, &lpSlot->function.mpFuncDef,
                "AptAnimationPoolData::function.pFuncDef", 0);
        }
    }
}

// ---- GetDequeLocation @ 0x82AD9758 ---------------------------------------
// Address of the logical slot nIndex positions ahead of the front, wrapping
// around the ring. (X360: idx = (frontIndex + nIndex) % mnCapacity, normalised
// to [0, mnCapacity); the divide-guard trap instructions are no-ops here.)
AptAnimationPoolData* AptActionQueueC::GetDequeLocation(s32 nIndex)
{
    const s32 liFrontIndex = static_cast<s32>(mpFront - mpBegin);
    s32 liIndex = (liFrontIndex + nIndex) % static_cast<s32>(mnCapacity);
    if (liIndex < 0)
    {
        liIndex += static_cast<s32>(mnCapacity);
    }
    return mpBegin + liIndex;
}

// ---- DecrementDequeLocation @ 0x82AD5E68 ---------------------------------
// The slot one position before pSlot, wrapping to the last slot when stepping
// below mpBegin.
AptAnimationPoolData* AptActionQueueC::DecrementDequeLocation(AptAnimationPoolData* pSlot)
{
    AptAnimationPoolData* lpPrev = pSlot - 1;
    if (lpPrev < mpBegin)
    {
        lpPrev = mpBegin + mnCapacity - 1;
    }
    return lpPrev;
}

// ---- GetNextItem @ 0x82ADC5E0 --------------------------------------------
// The slot one position after pSlot, wrapping to mpBegin when stepping past the
// last slot.
AptAnimationPoolData* AptActionQueueC::GetNextItem(AptAnimationPoolData* pSlot)
{
    AptAnimationPoolData* lpNext = pSlot + 1;
    if (lpNext == mpBegin + mnCapacity)
    {
        return mpBegin;
    }
    return lpNext;
}

// ---- IsLastItem @ 0x82AD5E08 ---------------------------------------------
// True iff pSlot is exactly the back of the queue.
bool AptActionQueueC::IsLastItem(AptAnimationPoolData* pSlot) const
{
    return pSlot == mpBack;
}

// ---- IsLastItemOrBeyond @ 0x82AD5E20 -------------------------------------
// True iff pSlot is at or past the back, accounting for ring wrap (front > back).
bool AptActionQueueC::IsLastItemOrBeyond(AptAnimationPoolData* pSlot) const
{
    AptAnimationPoolData* lpFront = mpFront;
    AptAnimationPoolData* lpBack  = mpBack;

    if (lpFront <= lpBack)
    {
        // Non-wrapped: anything at or past the back.
        return pSlot >= lpBack;
    }

    // Wrapped (front > back): the live region is [front, end) U [begin, back).
    // pSlot is "at or beyond the back" iff it is outside that live region.
    if (pSlot < lpBack)
    {
        return false;
    }
    if (pSlot >= lpFront)
    {
        return false;
    }
    return true;
}

// ---- SetCurItem @ 0x82AD5E60 ---------------------------------------------
// Set the "current item" cursor. VOID per the x64 mangling (?SetCurItem@
// AptActionQueueC@@QEAAXPEAUAptActionPool@1@@Z @0x140841240: `mov [rcx+18h],rdx / retn`).
void AptActionQueueC::SetCurItem(AptAnimationPoolData* pSlot)
{
    mpCurItem = pSlot;
}
