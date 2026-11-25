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
    // ARTIST checks for an empty queue at 0x82ADF1FC before the wrap loop.
    if (!IsLastItem(lpSlot))
    {
        do
        {
            if (lpSlot->mnType == AptAnimationPoolData::E_ACTION_TYPE_ACTION)
            {
                lpSlot->action.mpCIH->Release();
            }
            else if (lpSlot->mnType == AptAnimationPoolData::E_ACTION_TYPE_FUNCTION)
            {
                lpSlot->function.mpContext->Release();
                lpSlot->function.mpFuncDef->Release();
            }
            lpSlot->mnType = AptAnimationPoolData::E_ACTION_TYPE_EMPTY;
            lpSlot = GetNextItem(lpSlot);
        }
        while (!IsLastItem(lpSlot));
    }

    mpFront = mpBack = mpBegin;
}

// The queued action captures the frame on its bound sprite instance.
void AptActionQueueC::AddActionBack(AptActionBlock* pActionBlock, AptCIH* pCIH, s32 iContext)
{
    AptAnimationPoolData* lpNewBack = GetNextItem(mpBack);
    if (lpNewBack != mpFront)
    {
        mpBack->mnType = AptAnimationPoolData::E_ACTION_TYPE_ACTION;
        mpBack->action.miDepth = static_cast<AptCharacterSpriteInstBase*>(pCIH->mpCharacterInst)->mnLastActionFrame;
        mpBack->action.mpActionBlock = pActionBlock;
        mpBack->action.mpCIH = pCIH;
        pCIH->AddRef();
        mpBack->mnInput = iContext;
        mpBack = lpNewBack;
    }
}

void AptActionQueueC::AddActionFront(AptActionBlock* pActionBlock, AptCIH* pCIH, s32 iContext)
{
    AptAnimationPoolData* lpNewFront = DecrementDequeLocation(mpFront);
    if (lpNewFront != mpBack)
    {
        mpFront = lpNewFront;
        mpFront->action.miDepth = static_cast<AptCharacterSpriteInstBase*>(pCIH->mpCharacterInst)->mnLastActionFrame;
        mpFront->mnType = AptAnimationPoolData::E_ACTION_TYPE_ACTION;
        mpFront->action.mpActionBlock = pActionBlock;
        mpFront->action.mpCIH = pCIH;
        pCIH->AddRef();
        mpFront->mnInput = iContext;
    }
}

void AptActionQueueC::AddFunctionBack(AptCIH* pContext, AptValue* pFuncDef,
                                     s32 nParams, s32 iInput)
{
    AptAnimationPoolData* lpNewBack = GetNextItem(mpBack);
    if (lpNewBack != mpFront)
    {
        mpBack->mnType = AptAnimationPoolData::E_ACTION_TYPE_FUNCTION;
        mpBack->mnInput = iInput;
        mpBack->function.mpContext = pContext;
        mpBack->function.mpContext->AddRef();
        mpBack->function.mpFuncDef = pFuncDef;
        mpBack->function.mpFuncDef->AddRef();
        mpBack->function.miReturnReg = nParams;
        mpBack = lpNewBack;
    }
}

void AptActionQueueC::AddFunctionFront(AptCIH* pContext, AptValue* pFuncDef,
                                      s32 nParams, s32 iInput)
{
    AptAnimationPoolData* lpNewFront = DecrementDequeLocation(mpFront);
    if (lpNewFront != mpBack)
    {
        mpFront = lpNewFront;
        mpFront->mnType = AptAnimationPoolData::E_ACTION_TYPE_FUNCTION;
        mpFront->mnInput = iInput;
        mpFront->function.mpContext = pContext;
        mpFront->function.mpContext->AddRef();
        mpFront->function.mpFuncDef = pFuncDef;
        mpFront->function.mpFuncDef->AddRef();
        mpFront->function.miReturnReg = nParams;
    }
}

// RemoveActionFor @0x82ADF2A0. Leave the executing slot in place and move
// the appropriate contiguous run of the ring when removing a queued action.
void AptActionQueueC::RemoveActionFor(AptCIH* pCIH)
{
    AptAnimationPoolData* lpSlot = mpFront;
    while (!IsLastItem(lpSlot))
    {
        if (lpSlot->mnType == AptAnimationPoolData::E_ACTION_TYPE_ACTION
            && lpSlot->action.mpCIH == pCIH)
        {
            if (lpSlot == mpCurItem)
            {
                lpSlot = GetNextItem(lpSlot);
                continue;
            }
            if (lpSlot < mpBack)
            {
                pCIH->Release();
                memmove(lpSlot, lpSlot + 1, (mpBack - lpSlot - 1) * sizeof(AptAnimationPoolData));
                mpBack = DecrementDequeLocation(mpBack);
                break;
            }
            else if (lpSlot > mpFront)
            {
                pCIH->Release();
                memmove(mpFront + 1, mpFront, (lpSlot - mpFront) * sizeof(AptAnimationPoolData));
                mpFront = GetNextItem(mpFront);
                break;
            }
            else if (lpSlot == mpFront)
            {
                mpFront = GetNextItem(mpFront);
                break;
            }
        }
        lpSlot = GetNextItem(lpSlot);
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

// ---- IsLastItemOrBeyond @ 0x82AD5E20 -------------------------------------
// True iff pSlot is at or past the back, accounting for ring wrap (front > back).
bool AptActionQueueC::IsLastItemOrBeyond(AptAnimationPoolData* pSlot) const
{
    AptAnimationPoolData* lpFront = mpFront;
    AptAnimationPoolData* lpBack = mpBack;
    if (lpFront <= lpBack)
        return pSlot >= lpBack;
    return pSlot >= lpBack && pSlot < lpFront;
}

// ---- SetCurItem @ 0x82AD5E60 ---------------------------------------------
// Set the "current item" cursor. VOID per the x64 mangling (?SetCurItem@
// AptActionQueueC@@QEAAXPEAUAptActionPool@1@@Z @0x140841240: `mov [rcx+18h],rdx / retn`).
void AptActionQueueC::SetCurItem(AptAnimationPoolData* pSlot)
{
    mpCurItem = pSlot;
}
