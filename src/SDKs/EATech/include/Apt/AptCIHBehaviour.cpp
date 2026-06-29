// ===========================================================================
// EATech Apt -- AptCIH behavioural follow-ons (batch 1).   DECOMPILED from the
// X360 ARTIST.XEX. The node core (ctor/dtor/GC virtuals/state-flag-link/name
// accessors + the type predicates + SetDirtyState) lives in AptCIH.cpp; this file
// carries the standalone behavioural methods as they are reconstructed + verified.
//   GetFirstChild 0x82ADC938 / ContainsNativeHashVirtual 0x82AD74B8 /
//   HasEventMember 0x82AD74E0 / ForceCleanNativeHash 0x82AF2338 /
//   GetAnimationInst 0x82B7B358 / GetCosAngle 0x82AD7448.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCIH.h"
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"            // GetTypeTag / mpProperties
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h"  // mDisplayList (sprite-base child list)
#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h"   // GetAnimationInst downcast
#include "SDKs/EATech/include/Apt/AptDisplayList.h"              // mpHead / AptDisplayListNode::mpFirst / AsState
#include "SDKs/EATech/include/Apt/AptDisplayListState.h"         // insert / AddToDelayReleaseList
#include "SDKs/EATech/include/Apt/AptRenderItem.h"               // GetRenderItem / CopyRenderDataFrom
#include "SDKs/EATech/include/Apt/AptNativeHash.h"               // mnEventHandlerMask / mp__Proto__ / DestroyGCPointers
#include "SDKs/EATech/include/Apt/AptStd/AptMatrix.h"            // AptMatrix a/b/c
#include "SDKs/EATech/include/Apt/AptStd/AptCXForm.h"            // AptCXForm (writable colour return)
#include "SDKs/EATech/include/Apt/AptRenderTreeManager.h"        // AptCurrentRenderTreeManager + Update_Item*
#include "SDKs/EATech/include/Apt/AptRenderingContext.h"         // multMatrix + gAptIdentityMatrix
#include "SDKs/EATech/include/Apt/AptDefine.h"                   // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"                      // DOGMA_PoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"               // gpGCPoolManager Allocate/Deallocate
#include "SDKs/EATech/Apt/AptValueGCAllocator.h"                 // AptValueGC_MemItem::SetIsAllocated

#include <cmath>   // sqrtf

// FLAG (Apt context singleton -- console off_8324E574; owned by the AptTarget /
// linker boot TU): cancel this node's in-flight asset load + drop the ActionScript
// actions queued against it. Declared here as the x64-native accessors; their
// bodies (the linker CancelLoad + the action-queue removal) land with that TU.
void AptApt_CancelLoad(AptCIH* pNode);
void AptApt_RemoveActionFor(AptCIH* pNode);

// ---------------------------------------------------------------------------
// GetFirstChild @0x82ADC938 -- the first placed child of this node. Only the
// sprite-base character instances (movie-clip type 5 / animation type 9) carry a
// child display list (AptCharacterSpriteInstBase::mDisplayList, a subtype member
// outside the AptCharacterInst base); every other character type has no children,
// so the result is null. For a sprite-base node the child list's head node is read
// (lazily pool-allocated, so it can be null), and its first listed AptCIH returned.
// The X360 reads mpCharacterInst directly (no null guard -- only called on placed
// nodes that own an instance).
// ---------------------------------------------------------------------------
AptCIH* AptCIH::GetFirstChild() const
{
    const uint32_t nType = mpCharacterInst->GetTypeTag();
    const bool bSpriteBase = (nType == 5 || nType == 9);   // IsSpriteInstBase
    if (!bSpriteBase)
        return nullptr;

    const AptCharacterSpriteInstBase* pSpriteInst =
        static_cast<const AptCharacterSpriteInstBase*>(mpCharacterInst);

    const AptDisplayListNode* pHead = pSpriteInst->mDisplayList.mpHead;
    if (!pHead)
        return nullptr;   // head node not yet pool-allocated (empty list)

    return pHead->mpFirst;
}

// ---------------------------------------------------------------------------
// ContainsNativeHashVirtual @0x82AD74B8 -- AptValue vtbl[3] override. The X360
// body inlines GetNativeHash (read mpCharacterInst, null-guard, read its
// mpProperties) and coerces the result to a boolean. Reuse GetNativeHash so the
// null-guard stays in one place.
// ---------------------------------------------------------------------------
bool AptCIH::ContainsNativeHashVirtual() const
{
    return GetNativeHash() != nullptr;
}

// ---------------------------------------------------------------------------
// HasEventMember @0x82AD74E0 -- does this value (or anything up its __proto__
// chain) carry an AS event handler in nEventMask? A tight prototype-chain walk:
// each link's native hash is reached through GetNativeHashVirtual() (AptValue
// slot 2); a plain value returns null (chain end), an AptValueWithHash/AptObject/
// AptCIH returns its property table. Returns the masked int (not a bool) -- the
// callers (HasMouseEvent / objectMemberSet) use it as a truthiness test.
// ---------------------------------------------------------------------------
int AptCIH::HasEventMember(int nEventMask)
{
    for (AptValue* pValue = this; pValue != nullptr; )
    {
        AptNativeHash* pHash = pValue->GetNativeHashVirtual();
        if (!pHash)
            return 0;

        const int nMatched = pHash->mnEventHandlerMask & nEventMask;
        if (nMatched != 0)
            return nMatched;

        pValue = pHash->mp__Proto__;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// ForceCleanNativeHash @0x82AF2338 -- tear down + free the character instance's
// per-instance property hash, then clear the slot. The X360 reads the char inst
// (mpCharacterInst), grabs its property hash (mpProperties), and -- if present --
// runs DestroyGCPointers() (release every held value/key + free the bucket table)
// followed by the scalar-deleting destructor (~AptNativeHash + free the storage),
// then nulls mpProperties. The pseudocode's `int`/`return result` is a Hex-Rays
// artifact (r3/this flows through the deleting destructor's return). AptNativeHash
// is pool-allocated (gpNonGCPoolManager), so the scalar-deleting destructor is the
// explicit dtor + Deallocate; ~AptNativeHash is a no-op once DestroyGCPointers has
// run, preserving the X360's two-step order.
// ---------------------------------------------------------------------------
void AptCIH::ForceCleanNativeHash()
{
    AptCharacterInst* pCharacterInst = GetCharacterInst();
    AptNativeHash* pHash = pCharacterInst->mpProperties;
    if (pHash)
    {
        pHash->DestroyGCPointers();
        pHash->~AptNativeHash();
        gpNonGCPoolManager->Deallocate(pHash, sizeof(AptNativeHash));
        pCharacterInst->mpProperties = nullptr;
    }
}

// ---------------------------------------------------------------------------
// GetAnimationInst @0x82B7B358 -- mpCharacterInst narrowed to the concrete
// animation subtype. The X360 is a bare `lwz r3, 0x20(r3); blr` (mpCharacterInst
// at dword [8]); the caller has already confirmed IsAnimationInst (type tag 9),
// so no guard is emitted. The downcast is well-formed (AptCharacterAnimationInst
// derives, through AptCharacterSpriteInstBase, from AptCharacterInst).
// ---------------------------------------------------------------------------
AptCharacterAnimationInst* AptCIH::GetAnimationInst() const
{
    return static_cast<AptCharacterAnimationInst*>(mpCharacterInst);
}

// ---------------------------------------------------------------------------
// GetCosAngle @0x82AD7448 -- extract cos(theta) from an affine transform's first
// column (a, b). Static helper (the X360 takes the matrix pointer in r3, not a CIH
// `this`). When the rotation terms b and c are both exactly zero there is no
// rotation, so cos(theta) = 1; otherwise normalise the first column:
// a / sqrt(a*a + b*b). All ops are single-precision in the asm.
// ---------------------------------------------------------------------------
float AptCIH::GetCosAngle(const AptMatrix* pMatrix)
{
    if (pMatrix->b == 0.0f && pMatrix->c == 0.0f)
        return 1.0f;

    return pMatrix->a / sqrtf(pMatrix->a * pMatrix->a + pMatrix->b * pMatrix->b);
}

// ---------------------------------------------------------------------------
// Remove @0x82AFC0B0 -- tear this node out of the live scene.
//
// X360 signature: __fastcall(r3=this, r4=bClearGCRoots), forwarded verbatim to
// ClearCIH. The pseudocode's `int` return is a codegen artifact -- the body ends
// by tail-calling AptValue::Release() (vtable slot +4, void here) and the sole
// caller (AptDisplayListState::AddToDelayReleaseList) discards it -> void.
//   refcount>1 test: lwz r11,4(this); rlwinm 0,6,17; cmplwi 0x4000; ble -- the
//       0x03FFC000 field is a 12-bit count (LSB bit14), so 0x4000 == 1: getRefCount()>1.
//   CIH-state test:  lwz r11,0xC(this); rlwinm. 0,1,2 (mask 0x60000000) -- the
//       setIsDefined(false) is gated on GetCIHState() == 0.
// ---------------------------------------------------------------------------
void AptCIH::Remove(bool bClearGCRoots)
{
    // Cancel any in-flight asset load + drop the queued ActionScript actions
    // (through the Apt context singleton -- see the FLAG'd accessors above).
    AptApt_CancelLoad(this);
    AptApt_RemoveActionFor(this);

    // Tear down the placed/character state.
    ClearCIH(bClearGCRoots);

    // If anything outside the display list still references this node, it survives
    // as an AS-visible "zombie" but must leave the scene: unhook it from the render
    // tree and -- unless it is mid-transition (a non-zero CIH state) -- mark it undefined.
    if (getRefCount() > 1)
    {
        if (AptRenderTreeManager* pManager = AptCurrentRenderTreeManager())
        {
            pManager->Update_ItemNextSiblingChanged(this);
            pManager->Update_ItemFirstChildChanged(this);
        }

        if (GetCIHState() == 0)
            setIsDefined(false);
    }

    // Drop the display list's own reference on this node (X360 vtable slot +4).
    Release();
}

// ---------------------------------------------------------------------------
// ReplaceZombieChild @0x82AFE098 -- replace a "zombie" child node in this
// container's child display list with a freshly constructed replacement.
//
// Only the sprite-base character instances (movie-clip type 5 / animation type 9)
// carry a child display list (AptCharacterSpriteInstBase::mDisplayList); every
// other character type has none, so the call is a no-op returning `this`. When the
// list exists: the replacement inherits the zombie's instance name + its render
// item's visual state (transforms/visibility, via AptRenderItem::CopyRenderDataFrom);
// the zombie is unlinked + queued on the list's delayed-release list (so a node
// swapped mid-tick is released safely after the walk); and the replacement is
// inserted immediately after the zombie's former previous sibling, taking its depth
// slot. The sole caller (AptLinker::ConvertToZombie) discards the return value.
// ---------------------------------------------------------------------------
AptCIH* AptCIH::ReplaceZombieChild(AptCIH* pNewChild, AptCIH* pZombie)
{
    AptDisplayListState* pChildList = nullptr;
    AptCharacterInst* pCharInst = mpCharacterInst;
    const uint32_t nType = pCharInst->GetTypeTag();
    if (nType == 5 || nType == 9)   // movie-clip / animation == IsSpriteInstBase
    {
        AptCharacterSpriteInstBase* pSpriteInst =
            static_cast<AptCharacterSpriteInstBase*>(pCharInst);
        pChildList = pSpriteInst->mDisplayList.AsState();
    }

    if (!pChildList)
        return this;

    // Remember where the zombie sat (its previous sibling) before unlinking it.
    AptCIH* pInsertAfter = pZombie->GetDisplayListPrevious();

    // The replacement inherits the zombie's instance name + render visual state.
    pNewChild->SetInstanceName(pZombie->GetInstanceName());

    AptRenderItem* pZombieItem = pZombie->mpCharacterInst->GetRenderItem();
    AptRenderItem* pNewItem = pNewChild->mpCharacterInst->GetRenderItemWritable();
    pNewItem->CopyRenderDataFrom(pZombieItem);

    // Unlink the zombie from the live list (queued for delayed release), then link
    // the replacement into the zombie's former position.
    pChildList->AddToDelayReleaseList(pZombie, false);
    return pChildList->insert(pInsertAfter, pNewChild);
}

// ===========================================================================
// Behavioural batch 2 -- delegated transform / flag / native-hash accessors.
// Each forwards through the char inst's (writable) render item or its property
// hash; reconstructed + adversarially verified against the X360 asm.
// ===========================================================================

// GetPositionMatrixConst @0x82ADC2F0 -- the node's position transform (identity
// when unset). Inlined chain mpCharacterInst->mpRenderItem->mpPositionMatrix with
// the null->gIdentityMatrix fallback == AptRenderItem::GetPositionMatrixConst().
const AptMatrix* AptCIH::GetPositionMatrixConst() const
{
    return GetCharacterInst()->GetRenderItem()->GetPositionMatrixConst();
}

// GetColorMatrixConst @0x82ADC318 -- the node's colour transform (identity CXForm
// when unset), through the char inst's render item.
const AptCXForm* AptCIH::GetColorMatrixConst() const
{
    return GetCharacterInst()->GetColorMatrixConst();
}

// GetPositionMatrixWritable @0x82AE6730 -- the tick-writable position matrix (lazily
// allocates an identity AptMatrix on first write), via the writable render item.
AptMatrix* AptCIH::GetPositionMatrixWritable()
{
    return GetCharacterInst()->GetRenderItemWritable()->GetPositionMatrixWritable();
}

// GetColorMatrixWritable @0x82AE6758 -- the tick-writable colour transform (lazy-
// allocated), via the writable render item.
AptCXForm* AptCIH::GetColorMatrixWritable()
{
    return GetCharacterInst()->GetRenderItemWritable()->GetColorMatrixWritable();
}

// SetDepth @0x82AE2300 -- stamp the render depth into the char inst's WRITABLE
// render item (a halfword store of the 16-bit depth); returns that render item.
AptRenderItem* AptCIH::SetDepth(int16_t nDepth)
{
    return GetCharacterInst()->GetRenderItemWritable()->SetDepth(nDepth);
}

// GetIsPlaying @0x82AD5C00 -- the movie-clip play-head state (bit 6 of the sprite-
// base inst's mnClipActionFlags low byte). Only valid on a sprite-base node; the
// X360 reads mpCharacterInst unconditionally (no null/type guard).
bool AptCIH::GetIsPlaying() const
{
    const AptCharacterSpriteInstBase* pSpriteInst =
        static_cast<const AptCharacterSpriteInstBase*>(mpCharacterInst);
    return ((pSpriteInst->mnClipActionFlags >> 6) & 1u) != 0;
}

// SetEventHandler @0x82AD5B48 -- OR nEventMask into the per-instance property hash's
// packed event-handler mask (inlined GetNativeHash + AptNativeHash::SetEventHandler;
// the mask write is not null-guarded -- callers only invoke once a hash exists).
void AptCIH::SetEventHandler(int nEventMask)
{
    GetNativeHash()->SetEventHandler(nEventMask);
}

// RemoveEventHandler @0x82AD5B70 -- clear the given AS event-handler bits in the
// per-instance property-hash mask (inlined GetNativeHash + AptNativeHash::
// RemoveEventHandler; likewise not null-guarded).
void AptCIH::RemoveEventHandler(int32_t nMask)
{
    GetNativeHash()->RemoveEventHandler(nMask);
}

// SetHasMask @0x82AE22B8 -- set the has-mask flag + (un)bind the mask render item,
// delegating to the char inst's writable render item.
void AptCIH::SetHasMask(bool bHasMask, AptRenderItem* pMask)
{
    AptRenderItem* pRenderItem = GetCharacterInst()->GetRenderItemWritable();
    pRenderItem->SetHasMask(bHasMask, pMask);
}

// SetIsInserted @0x82AECE40 -- when this node owns a character instance, notify the
// render tree that the item was (re)inserted (AptCharacterInst::ItemInserted re-reads
// mpCharacterInst from the node, so `this` is passed); a null inst short-circuits.
AptCIH* AptCIH::SetIsInserted()
{
    if (GetCharacterInst() == nullptr)
        return this;
    return AptCharacterInst::ItemInserted(this);
}

// ===========================================================================
// Behavioural batch 3 -- GC-value pool allocation + the Release override.
// ===========================================================================

// operator new @0x82AE5B90 -- AptCIH is an AptValueGC value: allocate from the GC
// value pool (gpGCPoolManager) and mark the AptValueGC_MemItem header allocated.
// The gpGCPoolManager null-guard matches every committed GC-value sibling operator
// new (AptPrototype/AptArray/...) for the pre-AptInit startup window.
void* AptCIH::operator new(size_t size)
{
    if (gpGCPoolManager == nullptr)
        return nullptr;
    void* pMem = gpGCPoolManager->Allocate(size);
    reinterpret_cast<AptValueGC_MemItem*>(pMem)->SetIsAllocated(gAptValueGCSizeOffset, true);
    return pMem;
}

// operator delete @0x82AE72E8 -- free the GC block; on a successful free clear the
// AptValueGC_MemItem allocated flag (the inlined DeallocateAptValueGC form).
void AptCIH::operator delete(void* p, size_t size)
{
    if (gpGCPoolManager->Deallocate(p, size))
        reinterpret_cast<AptValueGC_MemItem*>(p)->SetIsAllocated(gAptValueGCSizeOffset, false);
}

// Release @0x82AE7390 -- AptValue vtbl[1] override. A CIH that is in state 1 and
// singly-referenced (only the display list holds it) pins itself: Release is a
// deliberate no-op (the X360 leaves `this` in r3 without decrementing). Any other
// state, or any refcount != 1, runs the normal AptValue::Release.
void AptCIH::Release()
{
    if (GetCIHState() != 1u || getRefCount() != 1u)
    {
        AptValue::Release();
        return;
    }
    // CIHState == 1 && refcount == 1: singly-owned, pinned -> no-op.
}

// ---------------------------------------------------------------------------
// SetMask @0x82AF6650 -- wire (or tear down) the mask MASTER/SLAVE relationship.
// `this` is the MASTER (the node setMask is invoked on); pMaskSlave is the SLAVE
// (the node made into a mask). "#!MASKSLAVE!#" lives on the slave's hash -> the
// master; "#!MASKMASTER!#" lives on the master's hash -> the slave. The matrix
// handed to the slave is its world transform concatenated up the master's parent
// chain. Per-node native-hash null-guards (the X360 passes a null hash to
// Unset/Set when mpProperties is absent) become GetNativeHash() guards.
// ---------------------------------------------------------------------------
void AptCIH::SetMask(AptCIH* pMaskSlave)
{
    // Only a defined character-instance-handle (or the CIHNone placeholder) is a
    // legal mask slave; anything else -> no-op.
    const bool bSlaveIsCIH =
        (pMaskSlave->getVtblIndex() == AptVFT_CharacterInstHandle && pMaskSlave->getIsDefined()) ||
        pMaskSlave->getVtblIndex() == AptVFT_CIHNone;
    if (!bSlaveIsCIH)
        return;

    AptRenderTreeManager* pManager = AptCurrentRenderTreeManager();
    const int nTick = gnCurrUpdateTick;
    const EAStringC strMaskSlave("#!MASKSLAVE!#");
    const EAStringC strMaskMaster("#!MASKMASTER!#");

    // (1) If THIS master already carries a mask, tear that prior wiring down.
    AptRenderItem* pMasterRenderItem = mpCharacterInst->mpRenderItem;
    if (pMasterRenderItem->GetHasMask() && pMasterRenderItem->GetMask())
    {
        if (AptCIH* pCurrentSlave = GetMask())
        {
            pCurrentSlave->mpCharacterInst->GetRenderItemWritable()->SetIsMask(false, nullptr);
            pCurrentSlave->SetGeneralizedProcessDirtyState(false);
            if (pManager)
                pManager->Update_ItemSetMaskState(pMaskSlave, nTick, false);
            if (AptNativeHash* pSlaveHash = pCurrentSlave->GetNativeHash())
                pSlaveHash->Unset(strMaskSlave);
        }
        if (ContainsNativeHashVirtual())
        {
            if (AptNativeHash* pMasterHash = GetNativeHash())
                pMasterHash->Unset(strMaskMaster);
        }
    }

    // (2) If the requested slave is ALREADY a mask (for another master), unhook it.
    AptCharacterInst* pSlaveCharInst = pMaskSlave->mpCharacterInst;
    if (pSlaveCharInst->mpRenderItem->GetIsMask())
    {
        pSlaveCharInst->GetRenderItemWritable()->SetIsMask(false, nullptr);
        pMaskSlave->SetGeneralizedProcessDirtyState(false);
        if (pManager)
            pManager->Update_ItemSetMaskState(pMaskSlave, nTick, false);

        AptNativeHash* pSlaveHash = pMaskSlave->GetNativeHash();
        AptCIH* pOldMaster = pSlaveHash
            ? static_cast<AptCIH*>(pSlaveHash->Lookup(strMaskSlave))
            : nullptr;

        pOldMaster->mpCharacterInst->GetRenderItemWritable()->SetHasMask(false, nullptr);
        if (AptNativeHash* pOldMasterHash = pOldMaster->GetNativeHash())
            pOldMasterHash->Unset(strMaskMaster);
        if (pSlaveHash)
            pSlaveHash->Unset(strMaskSlave);
    }

    // (3) Build the slave's world mask matrix from the master's parent chain.
    AptMatrix maskMatrix = gAptIdentityMatrix;
    for (AptCIH* pAncestor = mpDisplayListParent; pAncestor;
         pAncestor = pAncestor->mpDisplayListParent)
    {
        const AptMatrix* pPos = pAncestor->mpCharacterInst->mpRenderItem->mpPositionMatrix;
        if (!pPos)
            pPos = &gAptIdentityMatrix;
        AptRenderingContext::multMatrix(&maskMatrix, pPos, &maskMatrix);
    }

    // (4) Install the slave as a mask + cross-link master<->slave.
    pSlaveCharInst->GetRenderItemWritable()->SetIsMask(true, &maskMatrix);
    pMaskSlave->SetGeneralizedProcessDirtyState(true);
    if (pManager)
        pManager->Update_ItemSetMaskState(pMaskSlave, nTick, true);

    if (AptNativeHash* pSlaveHash = pMaskSlave->GetNativeHash())
        pSlaveHash->Set(strMaskSlave, this);
    if (ContainsNativeHashVirtual())
    {
        if (AptNativeHash* pMasterHash = GetNativeHash())
            pMasterHash->Set(strMaskMaster, pMaskSlave);
    }

    AptRenderItem* pSlaveRenderItem = pSlaveCharInst->GetRenderItemWritable();
    mpCharacterInst->GetRenderItemWritable()->SetHasMask(true, pSlaveRenderItem);
}

// ===========================================================================
// Behavioural batch 4 -- procedural-property reader + visibility.
// ===========================================================================

// FLAG (deferred): the X360 static-local bounding-rect helper (sub_82AE2C58) seeds
// the rect, expands it via AptCIH::GetBoundingRect (@0x82AE2B30 -- not yet homed; it
// needs the AptCharacterShape glyph/geometry bounds layout), then zeroes the rect if
// nothing expanded. Until GetBoundingRect lands this returns an empty rect, so the
// _width/_height procedural properties read 0; every other property is fully faithful.
static void GetBoundingRectClamped(const AptCIH* /*pThis*/, float afRect[4])
{
    afRect[0] = 0.0f; afRect[1] = 0.0f; afRect[2] = 0.0f; afRect[3] = 0.0f;
}

// GetProceduralProperty @0x82AE2D10 -- the AS getProperty reader. The X360 indexes a
// 12-entry remap table (byte_82145248, unexported) to select one of twelve arms; the
// case-label integers below are inferred to match SetProceduralProperty's arm order
// (only index 11 == _visible is confirmed, by IsVisible). Each arm's behaviour IS
// proven from the asm.
float AptCIH::GetProceduralProperty(uint32_t nPropertyIndex) const
{
    const AptMatrix* pPos = GetPositionMatrixConst();
    const AptCXForm* pCx  = GetColorMatrixConst();
    const float kSkewEpsilon = 1.1754944e-38f;   // FLT_MIN; FLAG: exact bits unread

    switch (nPropertyIndex)
    {
    case 4:   // _width -- bounding-rect extent on X
    {
        float afRect[4];
        GetBoundingRectClamped(this, afRect);
        const float fWidth = afRect[2] - afRect[0];
        return (fWidth >= 0.0f) ? fWidth : 0.0f;
    }
    case 5:   // _height -- bounding-rect extent on Y
    {
        float afRect[4];
        GetBoundingRectClamped(this, afRect);
        const float fHeight = afRect[3] - afRect[1];
        return (fHeight >= 0.0f) ? fHeight : 0.0f;
    }
    case 2:   // _rotation (degrees) -- cached authored angle (mpAssetString dual-use) or derived
        if (mpAssetString != nullptr)
            return *reinterpret_cast<const float*>(mpAssetString);
        if (!(fabsf(pPos->b) >= kSkewEpsilon) && !(fabsf(pPos->c) >= kSkewEpsilon))
            return 0.0f;
        {
            const float fAngleDeg = acosf(GetCosAngle(pPos)) * 57.29578f;
            return (pPos->b >= 0.0f) ? fAngleDeg : -fAngleDeg;
        }
    case 0:   // _xscale (percent)
        if (!(fabsf(pPos->b) >= kSkewEpsilon) && !(fabsf(pPos->c) >= kSkewEpsilon))
            return pPos->a * 100.0f;
        return sqrtf(pPos->a * pPos->a + pPos->b * pPos->b) * 100.0f;
    case 1:   // _yscale (percent)
        if (!(fabsf(pPos->b) >= kSkewEpsilon) && !(fabsf(pPos->c) >= kSkewEpsilon))
            return pPos->d * 100.0f;
        return sqrtf(pPos->c * pPos->c + pPos->d * pPos->d) * 100.0f;
    case 6:   // _x -- translation X
        return pPos->tx;
    case 7:   // _y -- translation Y
        return pPos->ty;
    case 3:   // _alpha -- colour scale alpha as percent
        return pCx->scale.GetValuef(AptColorHelper::Alpha) * 100.0f;
    case 8:   // colour translate Red (additive)
        return pCx->translate.GetValuef(AptColorHelper::Red);
    case 9:   // colour translate Green
        return pCx->translate.GetValuef(AptColorHelper::Green);
    case 10:  // colour translate Blue
        return pCx->translate.GetValuef(AptColorHelper::Blue);
    case 11:  // _visible (confirmed by IsVisible) -- render item isVisible -> 0.0/1.0
        return GetCharacterInst()->GetRenderItem()->GetIsVisible() ? 1.0f : 0.0f;
    default:
        return -1.0f;
    }
}

// IsVisible @0x82AE2F30 -- this node + every display-list ancestor must be visible.
// Walks up mpDisplayListParent; if any node's _visible procedural property is 0, the
// node is hidden -> not visible. Reaching the root (null parent) means all visible.
bool AptCIH::IsVisible() const
{
    for (const AptCIH* pNode = this; pNode != nullptr; pNode = pNode->mpDisplayListParent)
    {
        if (pNode->GetProceduralProperty(11) == 0.0f)
            return false;
    }
    return true;
}
