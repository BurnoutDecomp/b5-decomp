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
#include "SDKs/EATech/include/Apt/AptTarget.h"                   // gpAptTarget (the Apt context singleton)
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"          // AptAnimationTarget (animation director)
#include "SDKs/EATech/include/Apt/AptLinker.h"                   // AptLinker::CancelLoad (mpLinker @+0x20)
#include "SDKs/EATech/include/Apt/AptActionQueue.h"              // AptActionQueueC::RemoveActionFor (mpActionQueue)
#include "SDKs/EATech/include/Apt/AptRenderingContext.h"         // multMatrix + gAptIdentityMatrix
#include "SDKs/EATech/include/Apt/AptCharacterShape.h"           // shape/static-text leaf bounds (mBounds@+0x10)
#include "SDKs/EATech/include/Apt/AptRenderItemDynamicText.h"     // dyn-text leaf bounds (GetBoundsConst)
#include "SDKs/EATech/include/Apt/AptStd/AptRect.h"               // AptRect (bounds accumulator)
#include "SDKs/EATech/include/Apt/AptDefine.h"                   // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"                      // DOGMA_PoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"               // gpGCPoolManager Allocate/Deallocate
#include "SDKs/EATech/Apt/AptValueGCAllocator.h"                 // AptValueGC_MemItem::SetIsAllocated
#include "SDKs/EATech/include/Apt/AptActionQueue.h"              // AptActionQueueC (queueClipEvents enqueue)
#include "SDKs/EATech/include/Apt/AptPseudoDisplayList.h"        // mergeState shim cast target
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"         // gAptActionInterpreter (the clip-event immediate run)
#include "SDKs/EATech/include/Apt/AptScriptFunctionByteCodeBlock.h" // the onLoad/onUnload block wrapper
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"            // findChild / getIsDefined
#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h"      // the operand-stack pop (the unload-call tail)
#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h"    // mAnimationFilePtr (the zombie file-state swap)
#include "SDKs/EATech/include/Apt/AptFile.h"                      // mnState / mnField12 / mFileName


#include <cmath>   // sqrtf


// FLAG (Apt context singleton -- console off_8324E574; owned by the AptTarget /
// linker boot TU): cancel this node's in-flight asset load + drop the ActionScript
// actions queued against it. Declared here as the x64-native accessors; their
// bodies (the linker CancelLoad + the action-queue removal) land with that TU.
void AptApt_CancelLoad(AptCIH* pNode);
void AptApt_RemoveActionFor(AptCIH* pNode);

// The "null input" context id queueClipEvents stamps on an enterFrame enqueue
// (console gNullInput; defined in AptGlobals.cpp).
extern int gAptNullInputId;

// The undefined singleton (off_8324D814) the cleared event slots reset to.
extern AptValue* gpUndefinedValue;

// The shutdown flag (byte_8324E7C9) gating the unload-event tail.
extern unsigned char gbAptInShutdown;

// The process-wide AS action interpreter (off_8324E760; defined in AptGlobals.cpp).
extern AptActionInterpreter gAptActionInterpreter;

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
// FindAndSetEvents @0x82B02740 -- bind the built-in AS clip/button event handlers.
//
// The X360 walks a static descriptor table (dword_82F72DB4 .. dword_82F72DE4, six
// 8-byte {eventMask, nameIndex} pairs, EXTRACTED from the decrypted XEX below). For
// each descriptor whose event bit is not already set in this node's native-hash event
// mask AND that names a real handler (mask & 0x201C7), it resolves the named child
// against the runtime AS-name table (AptValue::findChild(&gAptEventNameTable[index]));
// when the child exists the event bit is OR'd into the hash mask. The return is 1 iff
// any of the "needs a per-frame update tick" events (mask 0x200C0) was newly bound.
// ---------------------------------------------------------------------------

// FLAG (homed elsewhere -- the AS-name registration boot TU; console dword_8324E580):
// the runtime table of registered ActionScript handler-name strings (EAStringC). The
// descriptors index into it by name id. x64-native: an array of EAStringC (8 bytes
// each), indexed by element -- NOT the console 4-byte stride.
extern EAStringC* gAptEventNameTable;

namespace
{
    struct AptEventDescriptor { uint32_t nEventMask; uint32_t nNameIndex; };

    // dword_82F72DB4 .. dword_82F72DE4 (EXTRACTED, decrypted XEX rodata).
    const AptEventDescriptor KaAptEventDescriptors[] = {
        { 0x00000002u, 56u },
        { 0x00000080u, 58u },
        { 0x00000040u, 57u },
        { 0x00000001u, 59u },
        { 0x00000004u, 69u },
        { 0x00000100u, 53u },
    };

    const uint32_t KU_AptEventHandlerNames    = 0x000201C7u;  // descriptors that name a handler
    const uint32_t KU_AptEventNeedsUpdateTick = 0x000200C0u;  // events that force a per-frame tick
}

int AptCIH::FindAndSetEvents()
{
    int nNeedsTick = 0;
    AptNativeHash* pHash = GetNativeHashVirtual();   // vtable slot 2

    for (const AptEventDescriptor& desc : KaAptEventDescriptors)
    {
        if ((desc.nEventMask & pHash->mnEventHandlerMask) == 0 &&
            (desc.nEventMask & KU_AptEventHandlerNames) != 0)
        {
            if (findChild(&gAptEventNameTable[desc.nNameIndex], nullptr) != nullptr)
            {
                pHash->mnEventHandlerMask |= desc.nEventMask;
                if ((desc.nEventMask & KU_AptEventNeedsUpdateTick) != 0)
                    nNeedsTick = 1;
            }
        }
    }
    return nNeedsTick;
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
// SetCharacterInst @0x82B00548 -- install (or replace) this node's character inst.
//
// A null pCharacterInst is replaced by a freshly built "none" instance (the X360
// inlines CreateCharacterInst(nullptr): a DOGMA-pool alloc + the base ctor + the base
// vtable). When the (resolved) instance differs from the current one, it is stored and
// the OLD instance is retired: its render data is optionally moved into the new one
// (bMoveRenderData -- AS reparent), its per-frame animation timer functions are removed
// when it was an animation node (the new instance's type tag == 9, console mask
// 0x24000000), it is GC-torn-down + freed, and the render tree is told the item moved.
// Both the replace path and the no-old path end in Update_ItemMoved.
//   field map: mpCharacterInst @[c:0x20]; the new inst's mTypeFlags @[c:+0x08]
//   (type tag in bits 26..31); the animation tag is 9 (0x24000000 >> 26).
// ---------------------------------------------------------------------------

// FLAG (homed with AptAnimationTarget; console @0x82B0...): remove every per-frame
// timer/event function this node registered against the animation director. Declared
// here (free-function form, like AptAnimationTarget_RunActions) until that TU bodies it.
void AptAnimationTarget_RemoveTimerFunctions(AptAnimationTarget* pAnim, AptCIH* pNode);

void AptCIH::SetCharacterInst(AptCharacterInst* pCharacterInst, bool bMoveRenderData)
{
    AptCharacterInst* pNew = pCharacterInst;
    if (pNew == nullptr)
        pNew = AptCharacterInst::CreateCharacterInst(nullptr);   // base "none" instance

    AptCharacterInst* pOld = mpCharacterInst;
    if (pNew == pOld)
        return;

    mpCharacterInst = pNew;

    if (pOld != nullptr)
    {
        if (bMoveRenderData)
            pNew->MoveRenderDataFrom(pOld);

        // The freshly installed instance is an animation node (type tag 9) -> drop its
        // registered per-frame timer functions from the animation director.
        if ((mpCharacterInst->GetTypeTag() == 9u) && gpAptTarget != nullptr)
            AptAnimationTarget_RemoveTimerFunctions(gpAptTarget->mpAnimationTarget, this);

        // GC-tear-down + free the old instance (the X360's DestroyGCPointers + the
        // vtable-0 scalar-deleting destructor; AptCharacterInst is DOGMA-pool-backed).
        pOld->DestroyGCPointers();
        pOld->~AptCharacterInst();
        gpNonGCPoolManager->Deallocate(pOld, sizeof(AptCharacterInst));
    }

    // Notify the render tree the node's render item moved (FLAG: the X360 reads the
    // manager + tick from gpAptTarget; the established convention resolves the same
    // manager through AptCurrentRenderTreeManager() + gnCurrUpdateTick).
    if (AptRenderTreeManager* pManager = AptCurrentRenderTreeManager())
        pManager->Update_ItemMoved(this, gnCurrUpdateTick);
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
// FLAG: the bounding-rect mode threaded through GetBoundingRect (X360 dword_8324E2AC);
// it is plumbed through the recursion but not read for the bounds math. Defined by
// the Apt runtime; declared here so the clamp helper passes it faithfully.
extern int gAptBoundingRectMode;

// GetBoundingRectClamped -- sub_82AE2C58: seed the rect to the empty (+/-FLT_MAX)
// sentinel, accumulate this node's world bounds (identity transform = local space),
// then zero the rect if nothing expanded it. (afRect[0..3] == AptRect left/top/right/
// bottom -- the X360 GetBoundingRect path fills exactly that 16-byte rect.)
// non-static: the X360 sub_82AE2C58 is one shared helper called by GetProceduralProperty
// (_width/_height) AND the getBounds/hitTest native methods (AptCIHNativeFunctionHelper.cpp).
void GetBoundingRectClamped(const AptCIH* pThis, float* afRect)   // float* (not float[4]): MSVC mangles the array param as float*const, breaking the cross-TU extern
{
    AptRect* pRect = reinterpret_cast<AptRect*>(afRect);
    pRect->fLeft = 3.4028235e38f;  pRect->fTop    = 3.4028235e38f;
    pRect->fRight = -3.4028235e38f; pRect->fBottom = -3.4028235e38f;

    const_cast<AptCIH*>(pThis)->GetBoundingRect(gAptBoundingRectMode, &gAptIdentityMatrix, pRect);

    if (pRect->fLeft == 3.4028235e38f && pRect->fTop == 3.4028235e38f &&
        pRect->fRight == -3.4028235e38f && pRect->fBottom == -3.4028235e38f)
    {
        pRect->fLeft = 0.0f; pRect->fTop = 0.0f; pRect->fRight = 0.0f; pRect->fBottom = 0.0f;
    }
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
    case 6:   // _rotation (degrees) -- cached authored angle (mpAssetString dual-use) or derived
        if (mpAssetString != nullptr)
            return *reinterpret_cast<const float*>(mpAssetString);
        if (!(fabsf(pPos->b) >= kSkewEpsilon) && !(fabsf(pPos->c) >= kSkewEpsilon))
            return 0.0f;
        {
            const float fAngleDeg = acosf(GetCosAngle(pPos)) * 57.29578f;
            return (pPos->b >= 0.0f) ? fAngleDeg : -fAngleDeg;
        }
    case 2:   // _xscale (percent)
        if (!(fabsf(pPos->b) >= kSkewEpsilon) && !(fabsf(pPos->c) >= kSkewEpsilon))
            return pPos->a * 100.0f;
        return sqrtf(pPos->a * pPos->a + pPos->b * pPos->b) * 100.0f;
    case 3:   // _yscale (percent)
        if (!(fabsf(pPos->b) >= kSkewEpsilon) && !(fabsf(pPos->c) >= kSkewEpsilon))
            return pPos->d * 100.0f;
        return sqrtf(pPos->c * pPos->c + pPos->d * pPos->d) * 100.0f;
    case 0:   // _x -- translation X
        return pPos->tx;
    case 1:   // _y -- translation Y
        return pPos->ty;
    case 7:   // _alpha -- colour scale alpha (stored 0..255) as percent
        // flt_82145600 = 0.3921569 == 100/255 (the scale-alpha field is 0..255, not 0..1;
        // cross-checked vs SetProceduralProperty's inverse percent->0..255 store).
        return pCx->scale.GetValuef(AptColorHelper::Alpha) * 0.3921569f;
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

// ---------------------------------------------------------------------------
// SetGeneralizedProcessDirtyState @0x82ADCA50 -- mark/clear this node's
// generalized-process-dirty state (mFlagsA bit24 = self, bit23 = subtree).
//
// SET (bDirty): set bit24 on this node, then walk up mpDisplayListParent setting
//   each ancestor's bit23 until one already has it. CLEAR: clear bit24; if the
//   subtree bit (bit23) is not set, scan this node's children for one still flagged
//   dirty and return it (the X360's outer parent-walk re-scans the same child list,
//   so it is a single scan gated on having a parent). The return is `this` on every
//   path except a found dirty child; the sole callers (SetMask/InsertChild) discard
//   it, so the scan is observable only through that discarded value -- no side effect.
//   (The X360 leaves r3 == `this` when no parent exists; reconstructed faithfully as
//   `return this`, where an earlier ungated reconstruction wrongly returned null.)
// ---------------------------------------------------------------------------
AptCIH* AptCIH::SetGeneralizedProcessDirtyState(bool bDirty)
{
    if (bDirty)
    {
        mFlagsA |= 0x01000000u;   // self generalized-process-dirty (bit24)
        for (AptCIH* pAncestor = mpDisplayListParent; pAncestor; )
        {
            if (pAncestor->mFlagsA & 0x00800000u)   // subtree bit already set -> stop
                break;
            AptCIH* pNext = pAncestor->mpDisplayListParent;
            pAncestor->mFlagsA |= 0x00800000u;       // propagate subtree dirty (bit23)
            pAncestor = pNext;
        }
        return this;
    }

    mFlagsA &= 0xFEFFFFFFu;   // clear the self bit (bit24)
    if (!(mFlagsA & 0x00800000u))
    {
        for (AptCIH* pAncestor = mpDisplayListParent; pAncestor; pAncestor = pAncestor->mpDisplayListParent)
        {
            for (AptCIH* pChild = GetFirstChild(); pChild; pChild = pChild->mpDisplayListNext)
            {
                if (pChild->mFlagsA & 0x01800000u)   // child still dirty (bit23 | bit24)
                    return pChild;
            }
        }
    }
    return this;
}

// SetIsMask @0x82AECE58 -- toggle this node's mask state: forward (bIsMask,
// pMaskMatrix) to the writable render item's SetIsMask (lazily (de)allocates + copies
// the mask matrix + flips the isMask bit), then (un)mark the generalized-process
// dirty state across the subtree (the X360 propagates that call's return).
AptCIH* AptCIH::SetIsMask(bool bIsMask, const AptMatrix* pMaskMatrix)
{
    AptRenderItem* pRenderItem = GetCharacterInst()->GetRenderItemWritable();
    pRenderItem->SetIsMask(bIsMask, pMaskMatrix);
    return SetGeneralizedProcessDirtyState(bIsMask);
}

// ---------------------------------------------------------------------------
// GetBoundingRect @0x82AE2B30 -- accumulate this node's world-space bounds into
// pAccumulator. Concat this node's position matrix onto pParentTransform, then by
// character type: a sprite-base (movie-clip/animation) recurses into its child
// display list; a leaf shape/static-text expands by the character's authored
// AptRect (AptCharacterShape::mBounds @+0x10); a dynamic-text expands by its render
// item's baked bounds; a morph (and the null-character "level") contributes nothing.
// nMode is plumbed through the recursion (unread by the bounds math). Returns pAccumulator.
// ---------------------------------------------------------------------------
AptRect* AptCIH::GetBoundingRect(int nMode, const AptMatrix* pParentTransform, AptRect* pAccumulator)
{
    AptCharacterInst* pCharInst = mpCharacterInst;
    if (pCharInst != nullptr && pCharInst->GetTypeTag() != 15)   // 15 == the null-char "level" node
    {
        // scratch = pParentTransform concat this node's position transform (identity if none).
        const AptMatrix* pPos = pCharInst->GetRenderItem()->GetPositionMatrixConst();
        AptMatrix scratch;
        AptRenderingContext::multMatrix(pParentTransform, pPos, &scratch);

        const uint32_t nType = pCharInst->GetTypeTag();
        if (nType == 5 || nType == 9)   // movie-clip / animation -> recurse into the child display list
        {
            AptCharacterSpriteInstBase* pSprite = static_cast<AptCharacterSpriteInstBase*>(pCharInst);
            pSprite->mDisplayList.GetBoundingRect(nMode, &scratch, pAccumulator);
        }
        else
        {
            const AptRect* pLeafBounds = nullptr;
            if (nType == 2)   // dynamic text -> the render item's baked bounds
            {
                pLeafBounds = static_cast<AptRenderItemDynamicText*>(pCharInst->GetRenderItem())->GetBoundsConst();
            }
            else if (nType != 8)   // not morph (a morph contributes no bounds here)
            {
                AptCharacter* pChar = pCharInst->GetRenderItem()->GetCharacterWritable();
                if (pChar->mnType == 1 || pChar->mnType == 10)   // shape / static-text
                    pLeafBounds = &static_cast<AptCharacterShape*>(pChar)->mBounds;
            }
            if (pLeafBounds != nullptr)
                AptRenderingContext::expandBoundingRect(pLeafBounds, &scratch, pAccumulator);
        }
    }
    return pAccumulator;
}

// ---------------------------------------------------------------------------
// HasMouseEvent @0x82AE2268 -- does this node respond to any mouse event? OR of two
// sources: (1) the sprite/movie-clip's packed clip-event flags carry a mouse-event
// bit (mnClipActionFlags & the shifted mouse-event mask), and (2) an ActionScript
// mouse handler is registered in this value's __proto__ chain (HasEventMember with
// the same mouse-event set un-shifted). mnClipActionFlags stores the clip-event mask
// in its high 24 bits, so the two literals are the one mouse-event set in its two
// storage positions.
// ---------------------------------------------------------------------------
namespace
{
    const uint32_t KU_AptMouseEventClipMask    = 0x09FC3800u;  // mouse bits, shifted clip-event position
    const uint32_t KU_AptMouseEventHandlerMask = 0x0009FC38u;  // same bits, native-hash handler position
}

bool AptCIH::HasMouseEvent()
{
    const AptCharacterSpriteInstBase* pSpriteInst =
        static_cast<const AptCharacterSpriteInstBase*>(mpCharacterInst);
    if ((pSpriteInst->mnClipActionFlags & KU_AptMouseEventClipMask) != 0)
        return true;

    return HasEventMember(static_cast<int>(KU_AptMouseEventHandlerMask)) != 0;
}

// ===========================================================================
// Behavioural batch 5 -- mask world-matrix rebuild + native-function teardown.
// ===========================================================================

// FLAG (Apt runtime scratch -- console dword_8324E53C; a process-wide fixed buffer
// the runtime uses to stage this node's display-list ancestor pointers while it
// folds their transforms). x64-native: a buffer of AptCIH* (8-byte pointers), NOT
// the console 4. Owned + sized by the Apt runtime boot TU; declared here so the
// mask-matrix fold can stage into it faithfully (one mask is processed at a time,
// so the buffer is never re-entered).
extern AptCIH** gAptMaskAncestorScratch;

// The minimum colour-scale alpha a mask render item is allowed to carry (console
// flt_82145924). A near-transparent mask would clip nothing visibly, so the mask's
// scale-alpha is clamped up to this floor before the matrix fold.
namespace { const float KF_AptMaskMinScaleAlpha = 51.0f; }

// ---------------------------------------------------------------------------
// ProcessMaskMatricies @0x82AEDAE0 -- rebuild a mask node's world position matrix.
//
// Only acts when this node's render item is flagged as a mask (mFlags bit30 ==
// GetIsMask). It first clamps the mask's colour-scale alpha up to the minimum mask
// alpha (the const read uses the identity-CXForm fallback when the node has no own
// colour matrix). It then folds every display-list ancestor's position transform
// into an identity-seeded mask matrix -- ancestors are staged into the runtime
// scratch buffer parent-first, then applied root-first (the X360 walks the staged
// array back-to-front) so the result is the node's full world transform. That matrix
// is installed via the writable render item (SetIsMask(true, .)) and the generalized-
// process dirty state is (re)marked. Returns true when a mask was processed.
//   field map: mpCharacterInst @[c:0x20]; its mpRenderItem @[c:+0x04]; the render
//   item's mFlags @[c:+0x18] (bit30 isMask), mpColorMatrix @[c:+0x0C], mpPositionMatrix
//   @[c:+0x08]; mpDisplayListParent @[c:0x1C].
// ---------------------------------------------------------------------------
bool AptCIH::ProcessMaskMatricies()
{
    AptCharacterInst* pCharInst = mpCharacterInst;
    AptRenderItem* pRenderItem = pCharInst->GetRenderItem();
    if (!pRenderItem->GetIsMask())
        return false;

    // Clamp the mask's colour-scale alpha up to the floor (const read falls back to
    // the identity CXForm when the node carries no own colour matrix).
    const AptCXForm* pColorConst = pRenderItem->GetColorMatrixConst();
    if (pColorConst->scale.GetValuef(AptColorHelper::Alpha) < KF_AptMaskMinScaleAlpha)
    {
        AptCXForm* pColorWritable = pCharInst->GetRenderItemWritable()->GetColorMatrixWritable();
        pColorWritable->scale.SetValuef(AptColorHelper::Alpha, KF_AptMaskMinScaleAlpha);
    }

    // Seed the mask matrix to identity, then stage the display-list ancestors into the
    // runtime scratch (immediate parent first, root last).
    AptMatrix maskMatrix = gAptIdentityMatrix;

    int nAncestorCount = 0;
    AptCIH** ppStage = gAptMaskAncestorScratch;
    for (AptCIH* pAncestor = mpDisplayListParent; pAncestor; pAncestor = pAncestor->mpDisplayListParent)
    {
        *ppStage = pAncestor;
        ++ppStage;
        ++nAncestorCount;
    }

    // Apply them root-first (back-to-front over the staged array), clearing each slot
    // as it is consumed. multMatrix(maskMatrix, ancestorPos, maskMatrix) == post-concat.
    AptCIH** ppCursor = gAptMaskAncestorScratch + nAncestorCount;
    while (nAncestorCount != 0)
    {
        --ppCursor;
        --nAncestorCount;
        AptCIH* pAncestor = *ppCursor;
        *ppCursor = nullptr;

        const AptMatrix* pPos = pAncestor->mpCharacterInst->GetRenderItem()->mpPositionMatrix;
        if (!pPos)
            pPos = &gAptIdentityMatrix;
        AptRenderingContext::multMatrix(&maskMatrix, pPos, &maskMatrix);
    }

    // Install the rebuilt world mask matrix + re-mark the generalized-process dirty state.
    mpCharacterInst->GetRenderItemWritable()->SetIsMask(true, &maskMatrix);
    SetGeneralizedProcessDirtyState(true);
    return true;
}

// ===========================================================================
// Behavioural batch 6 -- the AS setProperty writer.
// ===========================================================================

// ---------------------------------------------------------------------------
// SetProceduralProperty @0x82AE73C0 -- write a built-in AS "procedural" property by
// selector (the mirror of GetProceduralProperty). The X360 dispatches through a
// 12-entry jump table (word_82145258); the selectors decoded from that table are:
//   0 _x   1 _y   2 _xscale   3 _yscale   4 _width   5 _height   6 _rotation
//   7 _alpha   8 colour-translate Red   9 Green   10 Blue   11 _visible
// Every arm first stamps the AS-changed flag (mFlagsA bit31) from bASChanged, then
// writes through the char inst's WRITABLE position/colour matrix (or SetIsVisible).
//
// The console float constants (read from the decrypted XEX rodata):
//   flt_82001CC0 = 0.0   flt_82002540 = 9.99999975e-05 (the near-zero scale floor)
//   flt_820049E0 = 100.0   flt_82145830 = 1.13225734 (the _width/_height scale floor)
//   flt_82002138 = 0.01   flt_82001C98 = 1.0   flt_820025FC = 180.0  flt_820048B4 = -180.0
//   flt_82004928 = 360.0  flt_82145824 = 0.0174532905 (deg->rad)  dbl_82145828 = 360.0
//   flt_821455F4 = -255.0  flt_82010C20 = 255.0  flt_82001DA0 = 0.5
//
// Field map (flat-float access matches the asm): position matrix a@0 b@4 c@8 d@0xC
// tx@0x10 ty@0x14; colour matrix scale.Alpha@0x04, translate.Red@0x1C / Green@0x20 /
// Blue@0x24. The matrices are reached via GetRenderItemWritable()->GetPositionMatrix-
// Writable()/GetColorMatrixWritable() exactly as the X360.
// ---------------------------------------------------------------------------
namespace
{
    const float KF_AptProcZero        = 0.0f;            // flt_82001CC0
    const float KF_AptProcScaleFloor  = 9.99999975e-05f; // flt_82002540 (near-zero scale)
    const float KF_AptProcPercent     = 100.0f;          // flt_820049E0
    const float KF_AptProcWidthFloor  = 1.13225734f;     // flt_82145830
    const float KF_AptProcHundredth   = 0.01f;           // flt_82002138
    const float KF_AptProcOne         = 1.0f;            // flt_82001C98
    const float KF_AptProc180         = 180.0f;          // flt_820025FC
    const float KF_AptProcNeg180      = -180.0f;         // flt_820048B4
    const float KF_AptProc360         = 360.0f;          // flt_82004928
    const float KF_AptProcDegToRad    = 0.0174532905f;   // flt_82145824
    const float KF_AptProcColorMin    = -255.0f;         // flt_821455F4
    const float KF_AptProcColorMax    = 255.0f;          // flt_82010C20
    const float KF_AptProcHalf        = 0.5f;            // flt_82001DA0
}

void AptCIH::SetProceduralProperty(uint32_t nSelector, float fValue, bool bASChanged)
{
    if (nSelector > 11u)
        return;

    // Stamp the AS-changed flag (mFlagsA bit31) from bASChanged on every arm.
    auto markASChanged = [this, bASChanged]()
    {
        mFlagsA = (mFlagsA & 0x7FFFFFFFu) | (bASChanged ? 0x80000000u : 0u);
    };

    float f31 = fValue;

    switch (nSelector)
    {
    case 0:   // _x -- store translation X (position matrix tx @0x10)
    {
        markASChanged();
        AptMatrix* pPos = GetCharacterInst()->GetRenderItemWritable()->GetPositionMatrixWritable();
        pPos->tx = f31;
        return;
    }
    case 1:   // _y -- store translation Y (position matrix ty @0x14)
    {
        markASChanged();
        AptMatrix* pPos = GetCharacterInst()->GetRenderItemWritable()->GetPositionMatrixWritable();
        pPos->ty = f31;
        return;
    }
    case 2:   // _xscale (percent) -- scale the first column (a,b)
    {
        f31 *= KF_AptProcHundredth;          // percent -> ratio
        markASChanged();
        if (f31 == KF_AptProcZero)
            f31 = KF_AptProcScaleFloor;       // never let the column collapse
        AptMatrix* pPos = GetCharacterInst()->GetRenderItemWritable()->GetPositionMatrixWritable();
        if (!(fabsf(pPos->b) >= KF_AptProcScaleFloor) && !(fabsf(pPos->c) >= KF_AptProcScaleFloor))
        {
            pPos->a = f31;                    // no rotation -> a is the bare x scale
        }
        else
        {
            const float fLen = sqrtf(pPos->a * pPos->a + pPos->b * pPos->b);
            const float fInv = KF_AptProcOne / fLen;
            pPos->a = (pPos->a * fInv) * f31;
            pPos->b = (pPos->b * fInv) * f31;
        }
        return;
    }
    case 3:   // _yscale (percent) -- scale the second column (c,d)
    {
        f31 *= KF_AptProcHundredth;
        markASChanged();
        if (f31 == KF_AptProcZero)
            f31 = KF_AptProcScaleFloor;
        AptMatrix* pPos = GetCharacterInst()->GetRenderItemWritable()->GetPositionMatrixWritable();
        if (!(fabsf(pPos->b) >= KF_AptProcScaleFloor) && !(fabsf(pPos->c) >= KF_AptProcScaleFloor))
        {
            pPos->d = f31;
        }
        else
        {
            const float fLen = sqrtf(pPos->c * pPos->c + pPos->d * pPos->d);
            const float fInv = KF_AptProcOne / fLen;
            pPos->d = (pPos->d * fInv) * f31;
            pPos->c = (pPos->c * fInv) * f31;
        }
        return;
    }
    case 4:   // _width -- drive _xscale so the world width matches f31
    {
        markASChanged();
        if (f31 < KF_AptProcZero)
            return;
        if (f31 == KF_AptProcZero)
            f31 = KF_AptProcScaleFloor;

        float afRect[4];
        GetBoundingRectClamped(this, afRect);
        const float fWidth = afRect[2] - afRect[0];
        if (fWidth == KF_AptProcZero)
            return;

        AptMatrix* pPos = GetCharacterInst()->GetRenderItemWritable()->GetPositionMatrixWritable();
        if (!(fabsf(pPos->b) >= KF_AptProcScaleFloor) && !(fabsf(pPos->c) >= KF_AptProcScaleFloor))
        {
            // No rotation: derive the target xscale percent and re-enter case 2.
            float fScale = f31 / fWidth;
            if (!(fScale >= KF_AptProcScaleFloor))
                fScale = KF_AptProcScaleFloor;
            float fXScale = (pPos->a * fScale) * KF_AptProcPercent;
            if (!(fXScale >= KF_AptProcWidthFloor))
                fXScale = KF_AptProcWidthFloor;
            SetProceduralProperty(2u, fXScale, true);
        }
        else
        {
            // Rotated: scale the first column by f31 / current width.
            const float fCurWidth = GetProceduralProperty(4u);
            if (fCurWidth == KF_AptProcZero)
                return;
            float fScale = f31 / fCurWidth;
            if (!(fScale >= KF_AptProcScaleFloor))
                fScale = KF_AptProcScaleFloor;
            pPos->a *= fScale;
            pPos->b *= fScale;
        }
        return;
    }
    case 5:   // _height -- drive _yscale so the world height matches f31
    {
        markASChanged();
        if (f31 < KF_AptProcZero)
            return;
        if (f31 == KF_AptProcZero)
            f31 = KF_AptProcScaleFloor;

        float afRect[4];
        GetBoundingRectClamped(this, afRect);
        const float fHeight = afRect[3] - afRect[1];
        if (fHeight == KF_AptProcZero)
            return;

        AptMatrix* pPos = GetCharacterInst()->GetRenderItemWritable()->GetPositionMatrixWritable();
        if (!(fabsf(pPos->b) >= KF_AptProcScaleFloor) && !(fabsf(pPos->c) >= KF_AptProcScaleFloor))
        {
            float fScale = f31 / fHeight;
            if (!(fScale >= KF_AptProcScaleFloor))
                fScale = KF_AptProcScaleFloor;
            float fYScale = (pPos->d * fScale) * KF_AptProcPercent;
            if (!(fYScale >= KF_AptProcWidthFloor))
                fYScale = KF_AptProcWidthFloor;
            SetProceduralProperty(3u, fYScale, true);
        }
        else
        {
            const float fCurHeight = GetProceduralProperty(5u);
            if (fCurHeight == KF_AptProcZero)
                return;
            float fScale = f31 / fCurHeight;
            if (!(fScale >= KF_AptProcScaleFloor))
                fScale = KF_AptProcScaleFloor;
            pPos->c *= fScale;
            pPos->d *= fScale;
        }
        return;
    }
    case 6:   // _rotation (degrees) -- wrap to (-180,180], cache, rebuild the rotation
    {
        markASChanged();
        // Wrap into [-180, 180] via fmod 360 then a 360 subtract for the high half.
        // fmod only when outside [-180, 180] (f31 > 180 || f31 < -180).
        if (f31 > KF_AptProc180 || f31 < KF_AptProcNeg180)
            f31 = static_cast<float>(fmod(static_cast<double>(f31), 360.0));   // dbl_82145828
        if (f31 > KF_AptProc180)
            f31 -= KF_AptProc360;

        // Cache the authored angle in the lazily-allocated mpAssetString slot (the
        // X360 reuses it as a single-float store; allocated from the DOGMA pool).
        if (mpAssetString == nullptr)
            mpAssetString = gpNonGCPoolManager->Allocate(4);   // FLAG: console off_8324D808 (DOGMA pool)
        *reinterpret_cast<float*>(mpAssetString) = f31;

        // Rebuild the first two columns from the cached scales + the new angle.
        AptMatrix* pPos = GetCharacterInst()->GetRenderItemWritable()->GetPositionMatrixWritable();
        const float fScaleX = sqrtf(pPos->a * pPos->a + pPos->b * pPos->b);
        const float fScaleY = sqrtf(pPos->c * pPos->c + pPos->d * pPos->d);
        const float fRad = f31 * KF_AptProcDegToRad;
        const float fCos = static_cast<float>(cos(static_cast<double>(fRad)));
        const float fSin = static_cast<float>(sin(static_cast<double>(fRad)));
        pPos->a = fCos * fScaleX;
        pPos->d = fCos * fScaleY;
        pPos->b = fSin * fScaleX;
        pPos->c = -(fSin * fScaleY);
        return;
    }
    case 7:   // _alpha (percent) -- colour-scale alpha, rounded to 0..255 then clamped
    {
        markASChanged();
        AptCXForm* pCx = GetCharacterInst()->GetRenderItemWritable()->GetColorMatrixWritable();
        if (f31 < KF_AptProcZero)
        {
            // FLAG: the X360 folds the negative-alpha path into a shared store epilogue
            // (loc_82AE7808); reconstructed as the plain clamp-and-store of the raw value.
            float fClamped = f31;
            if (!(fClamped >= KF_AptProcColorMin)) fClamped = KF_AptProcColorMin;
            if (fClamped > KF_AptProcColorMax)     fClamped = KF_AptProcColorMax;
            pCx->scale.SetValuef(AptColorHelper::Alpha, fClamped);
            return;
        }
        // percent -> 0..255, round to nearest integer (fctiwz), clamp to [-255, 255].
        const float fScaled = (f31 * KF_AptProcHundredth) * KF_AptProcColorMax;
        float fVal = static_cast<float>(static_cast<int32_t>(fScaled));
        if (!(fVal >= KF_AptProcColorMin)) fVal = KF_AptProcColorMin;
        if (fVal > KF_AptProcColorMax)     fVal = KF_AptProcColorMax;
        pCx->scale.SetValuef(AptColorHelper::Alpha, fVal);
        return;
    }
    case 8:   // colour translate Red -- clamp [-255, 255], store at translate.Red (@0x1C)
    {
        markASChanged();
        AptCXForm* pCx = GetCharacterInst()->GetRenderItemWritable()->GetColorMatrixWritable();
        float fVal = f31;
        if (!(fVal >= KF_AptProcColorMin)) fVal = KF_AptProcColorMin;
        if (fVal > KF_AptProcColorMax)     fVal = KF_AptProcColorMax;
        pCx->translate.SetValuef(AptColorHelper::Red, fVal);
        return;
    }
    case 9:   // colour translate Green (@0x20)
    {
        markASChanged();
        AptCXForm* pCx = GetCharacterInst()->GetRenderItemWritable()->GetColorMatrixWritable();
        float fVal = f31;
        if (!(fVal >= KF_AptProcColorMin)) fVal = KF_AptProcColorMin;
        if (fVal > KF_AptProcColorMax)     fVal = KF_AptProcColorMax;
        pCx->translate.SetValuef(AptColorHelper::Green, fVal);
        return;
    }
    case 10:  // colour translate Blue (@0x24)
    {
        markASChanged();
        AptCXForm* pCx = GetCharacterInst()->GetRenderItemWritable()->GetColorMatrixWritable();
        float fVal = f31;
        if (!(fVal >= KF_AptProcColorMin)) fVal = KF_AptProcColorMin;
        if (fVal > KF_AptProcColorMax)     fVal = KF_AptProcColorMax;
        pCx->translate.SetValuef(AptColorHelper::Blue, fVal);
        return;
    }
    case 11:  // _visible -- bool from (f31 > 0.5), set render-item visibility + dirty
    {
        const bool bVisible = (f31 > KF_AptProcHalf);
        GetCharacterInst()->GetRenderItemWritable()->SetIsVisible(bVisible);
        SetGeneralizedProcessDirtyState(bVisible);
        return;
    }
    default:
        return;
    }
}

// ---------------------------------------------------------------------------
// CleanNativeFunctions @0x82AD6FB8 -- shutdown teardown of the process-wide
// ActionScript native-function singletons. Each non-null slot has its value Released
// (AptValue vtable slot 1, the deleting destructor path) and is nulled. The X360
// unrolls the 27 slots in a fixed (non-address) order; reproduced verbatim so the
// teardown sequence matches. The slots are owned by the AS-builtin registration boot
// TU, so they are FLAG'd extern here.
// ---------------------------------------------------------------------------
// FLAG (homed elsewhere -- the AS-builtin registration boot TU; console
// off_8324E42C..0x8324E494): the per-built-in native-function singletons. Each is an
// AptValueGC (AptNativeFunction) torn down through the AptValue Release virtual.
extern AptValue* gpAptNativeFn_8324E440;
extern AptValue* gpAptNativeFn_8324E43C;
extern AptValue* gpAptNativeFn_8324E48C;
extern AptValue* gpAptNativeFn_8324E488;
extern AptValue* gpAptNativeFn_8324E480;
extern AptValue* gpAptNativeFn_8324E484;
extern AptValue* gpAptNativeFn_8324E47C;
extern AptValue* gpAptNativeFn_8324E444;
extern AptValue* gpAptNativeFn_8324E448;
extern AptValue* gpAptNativeFn_8324E44C;
extern AptValue* gpAptNativeFn_8324E450;
extern AptValue* gpAptNativeFn_8324E454;
extern AptValue* gpAptNativeFn_8324E45C;
extern AptValue* gpAptNativeFn_8324E458;
extern AptValue* gpAptNativeFn_8324E460;
extern AptValue* gpAptNativeFn_8324E46C;
extern AptValue* gpAptNativeFn_8324E474;
extern AptValue* gpAptNativeFn_8324E470;
extern AptValue* gpAptNativeFn_8324E478;
extern AptValue* gpAptNativeFn_8324E430;
extern AptValue* gpAptNativeFn_8324E434;
extern AptValue* gpAptNativeFn_8324E42C;
extern AptValue* gpAptNativeFn_8324E490;
extern AptValue* gpAptNativeFn_8324E494;
extern AptValue* gpAptNativeFn_8324E464;
extern AptValue* gpAptNativeFn_8324E468;
extern AptValue* gpAptNativeFn_8324E438;

void AptCIH::CleanNativeFunctions()
{
    // Each: if the singleton exists, Release it (vtable slot 1) and clear the slot.
    // The X360 order is preserved exactly (it is not by address).
    AptValue** const apSlots[] = {
        &gpAptNativeFn_8324E440, &gpAptNativeFn_8324E43C, &gpAptNativeFn_8324E48C,
        &gpAptNativeFn_8324E488, &gpAptNativeFn_8324E480, &gpAptNativeFn_8324E484,
        &gpAptNativeFn_8324E47C, &gpAptNativeFn_8324E444, &gpAptNativeFn_8324E448,
        &gpAptNativeFn_8324E44C, &gpAptNativeFn_8324E450, &gpAptNativeFn_8324E454,
        &gpAptNativeFn_8324E45C, &gpAptNativeFn_8324E458, &gpAptNativeFn_8324E460,
        &gpAptNativeFn_8324E46C, &gpAptNativeFn_8324E474, &gpAptNativeFn_8324E470,
        &gpAptNativeFn_8324E478, &gpAptNativeFn_8324E430, &gpAptNativeFn_8324E434,
        &gpAptNativeFn_8324E42C, &gpAptNativeFn_8324E490, &gpAptNativeFn_8324E494,
        &gpAptNativeFn_8324E464, &gpAptNativeFn_8324E468, &gpAptNativeFn_8324E438,
    };

    for (AptValue** ppSlot : apSlots)
    {
        if (*ppSlot)
        {
            (*ppSlot)->Release();
            *ppSlot = nullptr;
        }
    }
}

// ===========================================================================
// Behavioural batch 7 -- per-frame dynamic-text refresh.
// ===========================================================================

// FLAG (homed with the dynamic-text render-item TU; console &unk_82F72DB0): the
// "unresolved/empty text render-data handle" sentinel. mZID equal to it (or 0) means
// the field has no resolved render data yet. Mirrors AptRenderItemDynamicText.cpp.
extern intptr_t gAptEmptyTextRenderDataZID;

// ---------------------------------------------------------------------------
// ProcessTextInst @0x82B076F0 -- refresh a dynamic-text node's baked render data.
//
// Acts only on a dynamic-text character instance (type tag 2; the console tests
// (mTypeFlags & 0xFC000000) == 0x08000000). When the node is visible and its render
// item's text render-data handle is still unresolved (mZID == 0 or the empty-text
// sentinel) OR its text is flagged dirty (mStateFlags bit0 clear), the text is
// (re)built via EnsureStringAllocated, fed the display-list parent so an inherited
// TextFormat resolves. Returns true when this is a dynamic-text node (whether or not
// a rebuild was needed), false otherwise.
// ---------------------------------------------------------------------------
bool AptCIH::ProcessTextInst()
{
    if (mpCharacterInst->GetTypeTag() != 2u)   // (mTypeFlags & 0xFC000000) != 0x08000000
        return false;

    if (IsVisible())
    {
        AptRenderItemDynamicText* pTextItem =
            static_cast<AptRenderItemDynamicText*>(mpCharacterInst->GetRenderItem());
        const intptr_t nZID = pTextItem->mZID;
        if (nZID == 0 || nZID == gAptEmptyTextRenderDataZID ||
            (pTextItem->mStateFlags & 1u) == 0)
        {
            EnsureStringAllocated(mpDisplayListParent);   // (homed in AptCIHText.cpp)
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// The generalised-process callback slots (AptRenderLinkStubs.cpp) + the per-node
// generalised process. The console AptUpdate driver (sub_82B0D608) installs
// AptCIH::ProcessTextInst / ProcessCustomControls / ProcessMaskMatricies into
// dword_8324E41C/420/424 around AptDisplayList::GeneralisedProcess each frame; the
// registered callbacks are invoked per node by AptCIH::GeneralisedProcess (above).
//
// AptCIH_ProcessTextInstCb is the free-function adapter matching the callback slot
// signature (AptCIH*, AptCIH*, void*) -> unsigned int; it forwards to the node's
// ProcessTextInst (which reads only its `this`, like the console's r3-in call).
// AptCIH_RunGeneralisedTextProcess installs it, walks pRoot's subtree via
// GeneralisedProcess, and restores the slot -- the faithful text-refresh pass a host
// per-frame driver runs (the console does it inside AptUpdate).
// ---------------------------------------------------------------------------
extern unsigned int (*AptCIH_sCIHProcessCb)(AptCIH*, AptCIH*, void*);   // dword_8324E41C

unsigned int AptCIH_ProcessTextInstCb(AptCIH* pNode, AptCIH* /*pRoot*/, void* /*pCtx*/)
{
    return pNode->ProcessTextInst() ? 1u : 0u;
}

void AptCIH_RunGeneralisedTextProcess(AptCIH* pRoot)
{
    if (pRoot == nullptr)
        return;
    unsigned int (*pPrev)(AptCIH*, AptCIH*, void*) = AptCIH_sCIHProcessCb;
    AptCIH_sCIHProcessCb = &AptCIH_ProcessTextInstCb;
    pRoot->GeneralisedProcess(pRoot, nullptr);
    AptCIH_sCIHProcessCb = pPrev;
}

// ===========================================================================
// Behavioural batch 8 -- the un-homed AptCIH behavioural cluster ("AptCIH" link
// cluster): the mask resolver, the clip-event predicates + queue, the generalised-
// process per-node pass, ClearCIH teardown, and the free-function shims the sibling
// TUs (AptDisplayList / AptLinker / AptAnimationTarget / AptCIHNativeFunctionHelper)
// reference by name. DECOMPILED from the PS3 EXTERNAL ELF (DWARF-named) + the X360
// ARTIST.XEX, cross-checked. EnsureStringAllocated (the deep dynamic-text/glyph layout
// engine) stays a FLAG stub -- its body belongs with the Apt text/font subsystem TU.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"   // (cluster shim types)

// ---- FLAG (un-homed runtime singletons referenced by the deep cluster bodies; each
// declared verbatim by its console-name equivalent, bodies/storage owned by the
// AS-interpreter, GC, and Apt-runtime boot TUs) ------------------------------------
// AddToRemList (console AptAnimationTarget::AddToRemList @0x82B...): queue a node that
// is still externally referenced onto the animation director's removal list.
void AptAnimationTarget_AddToRemList(AptAnimationTarget* pAnim, AptCIH* pItem);

// PreDestroy hook (console dword_8324E8A0): the optional process-wide pre-destroy notify
// callback; null until a host installs it. Owned by the Apt-runtime boot TU.
extern void (*gpAptCIHPreDestroyHook)(AptCIH* pCIH);   // dword_8324E8A0

// ---------------------------------------------------------------------------
// GetMask @0x82AE7B48 -- the current mask SLAVE of this MASTER node. The X360 first
// gates on the master render item carrying a mask: hasMask flag (mFlags bit29 /
// 0x20000000 at render-item +0x18 == GetHasMask) AND a non-null mask pointer
// (render-item +0x1C == GetMask) AND this value containing a native hash (vtbl slot 3
// == ContainsNativeHashVirtual). When all hold, it looks the slave up under the
// "#!MASKMASTER!#" key in this master's per-instance property hash.
// ---------------------------------------------------------------------------
AptCIH* AptCIH::GetMask() const
{
    AptRenderItem* pRenderItem = mpCharacterInst->GetRenderItem();
    if (!pRenderItem->GetHasMask() ||
        !pRenderItem->GetMask() ||
        !const_cast<AptCIH*>(this)->ContainsNativeHashVirtual())
    {
        return nullptr;
    }

    const EAStringC strMaskMaster("#!MASKMASTER!#");
    AptNativeHash* pHash = mpCharacterInst ? mpCharacterInst->mpProperties : nullptr;
    return pHash ? static_cast<AptCIH*>(pHash->Lookup(strMaskMaster)) : nullptr;
}

// ---------------------------------------------------------------------------
// HasClipEvent @0x82B027C0 -- the sprite-base clip-event flag set (the high 24 bits of
// mnClipActionFlags, read here `>> 8`) intersected with nEventMask. The X360 reads the
// char inst's +0x14 word unconditionally (only valid on a sprite-base node).
// ---------------------------------------------------------------------------
bool AptCIH::HasClipEvent(int nEventMask)
{
    const AptCharacterSpriteInstBase* pSpriteInst =
        static_cast<const AptCharacterSpriteInstBase*>(mpCharacterInst);
    return ((pSpriteInst->mnClipActionFlags >> 8) & static_cast<uint32_t>(nEventMask)) != 0;
}

// HasEvent @0x82B02838 -- a packed clip-event flag OR an AS __proto__ handler matches.
bool AptCIH::HasEvent(int nEventMask)
{
    if (HasClipEvent(nEventMask))
        return true;
    return HasEventMember(nEventMask) != 0;
}

// ---------------------------------------------------------------------------
// queueClipEvents @0x82B0... -- scan + dispatch this node's handlers for nEventMask.
//
// FAITHFUL CORE (from the PS3 DWARF body): no-op on a dynamic-text node (char type tag
// 2) or when the node carries no matching handler at all (HasEvent). When the sprite's
// per-instance clip-event record table carries an entry whose mask matches, the entry's
// queued action is pushed onto the animation director's deferred-action queue (front for
// enterFrame mask 2 / the keyframe-id-gated 0x20000 path, back otherwise), stamped with
// nFrameId. The byte-code-block construction + AS-interpreter execution of a matched
// record entry (masks 512 / 4 -> a fresh AptScriptFunctionByteCodeBlock run through
// gAptActionInterpreter) and the bDeferred __proto__-member dispatch are the deferred
// AS-execution sub-paths -- FLAG'd to the cluster callee below until that engine is homed.
// ---------------------------------------------------------------------------

// The clip-event record scan + enqueue (HOMED 2026-07-01 from the PS3 body @0x815BD0;
// was the AptRenderLinkStubs return-0 stub). The sprite's registered handler list
// (mpClipEventHandlers == the placement's clipActions block) is scanned for records
// whose mask matches; each match enqueues its action-stream SLOT ADDRESS onto the
// director's deferred queue:
//   mask 2 (enterFrame)          -> AddActionFront(..., gAptNullInputId)
//   mask 0x20000 (keyframe)      -> AddActionFront(..., nFrameId) when the record's
//                                   keyframe id == nFrameId >> 17
//   masks 512 / 4 / 0x40000      -> the immediate BYTE-CODE-BLOCK run (FLAG below)
//   anything else (incl. 1 load) -> AddActionBack(..., nFrameId)
//
// UN-STAGED (2026-07-01, with the recovered StringPool table + the extracted
// aClipEvents pairs): the masks-512/4/0x40000 immediate byte-code-block run and
// the bDeferred __proto__ event-member dispatch are BOTH live below. The only
// remaining FLAG is the cross-CIH handler rebind sub-branch (see inline).
int AptCIH_queueClipEvents_RunMatched(AptCIH* pNode, int nEventMask, unsigned int nFrameId,
                                      int bDeferred)
{
    AptCharacterSpriteInstBase* const pSprite =
        static_cast<AptCharacterSpriteInstBase*>(pNode->GetCharacterInst());
    AptClipEventHandlerList* pList = pSprite->mpClipEventHandlers;   // charInst word[6]

    int nResult = 0;
    // The same converter-data plausibility guard as _addToSetCaches (see there): a
    // 4-byte-straddled record-array slot marks an unusable (never-relocated) block.
    if (pList != nullptr)
    {
        const uintptr_t luRecs = reinterpret_cast<uintptr_t>(pList->mpHandlers);
        if (luRecs < 0x10000u || (luRecs >> 47) != 0u ||
            (luRecs & 0xFFFFFFFFull) == 0u)   // the straddle leaves the low dword zero
            pList = nullptr;
    }
    if (pList != nullptr && pList->mnCount > 0)
    {
        AptActionQueueC* const pQueue =
            (gpAptTarget != nullptr && gpAptTarget->mpAnimationTarget != nullptr)
                ? gpAptTarget->mpAnimationTarget->mpActionQueue : nullptr;

        for (int32_t i = 0; i < pList->mnCount; ++i)
        {
            AptClipEventHandler& rRec = pList->mpHandlers[i];
            if ((rRec.mnEventFlags & static_cast<uint32_t>(nEventMask)) == 0u)
                continue;

            if (nEventMask == 512 || nEventMask == 4 || nEventMask == 0x40000)
            {
                // The IMMEDIATE byte-code-block run (PS3 LABEL_24/27): wrap the
                // matched record's parsed action stream in a fresh
                // AptScriptFunctionByteCodeBlock named onLoad [59] (mask 512) /
                // onUnload [69] (masks 4 / 0x40000), then call it NOW through the
                // interpreter -- not queued. The console runs the FIRST matched
                // record and returns 1.
                const int nNameCode = (nEventMask == 512) ? 59 : 69;
                const EAStringC* pName = &StringPool::saConstant[nNameCode];   // GetString(code)

                // qword_1059C588: the static (zero-init) constant-pool descriptor
                // the console passes by value -- a clip-event block has no pool of
                // its own (its streams use the movie dictionary via op 0x88).
                AptConstantPool lEmptyPool = { nullptr, 0 };

                AptScriptFunctionByteCodeBlock* pBlock = new AptScriptFunctionByteCodeBlock(
                    const_cast<unsigned char*>(rRec.mpActionStream), -1, lEmptyPool,
                    pName->GetBuffer(), static_cast<AptValue*>(pNode), nullptr);
                if (pBlock != nullptr)
                {
                    // PrepareForExecution == PushStaticData (the register window).
                    AptValue** lpSavedHeap = AptScriptFunctionBase::PushStaticData();

                    // Push the node onto the interpreter's CIH/target stack, pin both.
                    gAptActionInterpreter.mpCIHStack[gAptActionInterpreter.mnCIHStackTop] = pNode;
                    ++gAptActionInterpreter.mnCIHStackTop;
                    pNode->AddRef();
                    pBlock->AddRef();

                    gAptActionInterpreter.callFunction(
                        static_cast<AptValue*>(pNode), pBlock, 0, nullptr, nullptr);

                    pBlock->Release();
                    // Pop the CIH stack (Release the pinned node).
                    gAptActionInterpreter
                        .mpCIHStack[gAptActionInterpreter.mnCIHStackTop - 1]->Release();
                    --gAptActionInterpreter.mnCIHStackTop;

                    gAptActionInterpreter.CleanupAfterExecution(lpSavedHeap);
                }
                return 1;
            }

            if (pQueue == nullptr)
                continue;

            if (nEventMask == 2)
            {
                // enterFrame: queued at the FRONT with the null-input context id.
                nResult = 1;
                pQueue->AddActionFront(&rRec.mpActionStream, pNode, gAptNullInputId);
            }
            else if (nEventMask == 0x20000)
            {
                // keyframe event: only the record whose keyframe id matches the
                // packed frame id's high bits (console `rec[1] == a3 >> 17`).
                if (rRec.mnKeyFrameId == (nFrameId >> 17))
                {
                    nResult = 1;
                    pQueue->AddActionFront(&rRec.mpActionStream, pNode,
                                           static_cast<s32>(nFrameId));
                }
            }
            else
            {
                // Everything else (incl. mask 1 construct/load): queued at the BACK.
                nResult = 1;
                pQueue->AddActionBack(&rRec.mpActionStream, pNode,
                                      static_cast<s32>(nFrameId));
            }
        }
    }

    // The bDeferred __proto__ event-member dispatch tail (PS3 LABEL_17+): when an
    // AS-defined event member exists anywhere up the __proto__ chain, find the
    // aClipEvents pair whose mask matches, look the handler up (own hash first,
    // else findChild), and queue it as a FUNCTION slot.
    if (bDeferred != 0 && pNode->HasEventMember(nEventMask) != 0)
    {
        // aClipEvents @0x82F72DB4 (GROUND TRUTH, extracted from the ARTIST XEX
        // rodata): 6 {clip-event mask, StringCode} pairs.
        static const struct { uint32_t mnMask; int32_t mnNameCode; } skAptClipEvents[6] = {
            { 0x002, 56 },   // onEnterFrame
            { 0x080, 58 },   // onKeyUp
            { 0x040, 57 },   // onKeyDown
            { 0x001, 59 },   // onLoad
            { 0x004, 69 },   // onUnload
            { 0x100, 53 },   // onData
        };

        int nPair = -1;
        for (int k = 0; k < 6; ++k)
        {
            if ((skAptClipEvents[k].mnMask & static_cast<uint32_t>(nEventMask)) != 0u)
            {
                nPair = k;
                break;
            }
        }
        if (nPair == -1)
            return nResult;

        AptActionQueueC* const pQueue =
            (gpAptTarget != nullptr && gpAptTarget->mpAnimationTarget != nullptr)
                ? gpAptTarget->mpAnimationTarget->mpActionQueue : nullptr;
        if (pQueue == nullptr)
            return nResult;

        const EAStringC* pName = &StringPool::saConstant[skAptClipEvents[nPair].mnNameCode];   // GetString(code)
        const uint32_t nPairMask = skAptClipEvents[nPair].mnMask;

        // Own-hash lookup first (the objectMemberLookup vtbl route's fast path).
        AptNativeHash* pHash = pNode->GetNativeHashVirtual();
        AptValue* pHandler = (pHash != nullptr) ? pHash->Lookup(*pName) : nullptr;
        if (pHandler != nullptr)
        {
            // Found on this node: masks 0x4000/0x2000 queue at the BACK, the rest
            // jump the queue at the FRONT (the console's two exits).
            if (nPairMask == 0x4000u || nPairMask == 0x2000u)
                pQueue->AddFunctionBack(static_cast<AptValue*>(pNode), pHandler,
                                        0, static_cast<s32>(nFrameId));
            else
                pQueue->AddFunctionFront(static_cast<AptValue*>(pNode), pHandler,
                                         0, static_cast<s32>(nFrameId));
            return 1;
        }

        // Not on the own hash: resolve up the chain (findChild). Only a DEFINED
        // value dispatches (the console tests the value's defined bit).
        AptValue* pChild = pNode->findChild(pName, nullptr);
        if (pChild == nullptr || !pChild->getIsDefined())
            return nResult;

        // FLAG (deferred sub-branch, console-faithful gate): when the resolved
        // handler is bound to a DIFFERENT CIH the console re-binds it (Release the
        // old root-animation ref, DecZombieCount, GetRootAnimation + the packed
        // zombie-count increment, AddRef, setGCRoot) before queueing. That
        // cross-CIH rebind (the AptScriptFunctionBase +36 root-animation slot) is
        // its own follow-on; the SAME-CIH handler (the common case) queues below.

        if (nPairMask == 0x4000u || nPairMask == 0x2000u || nPairMask == 0x001u)
            pQueue->AddFunctionBack(static_cast<AptValue*>(pNode), pChild,
                                    0, static_cast<s32>(nFrameId));
        else
            pQueue->AddFunctionFront(static_cast<AptValue*>(pNode), pChild,
                                     0, static_cast<s32>(nFrameId));
        return 1;
    }

    return nResult;
}

AptValue* AptCIH::queueClipEvents(int nEventMask, unsigned int nFrameId, int bDeferred)
{
    AptCharacterInst* const pInst = mpCharacterInst;

    // Dynamic-text nodes (type tag 2) never carry clip events; and a node with no
    // matching handler at all short-circuits to 0.
    if (pInst->GetTypeTag() == 2u || !HasEvent(nEventMask))
        return reinterpret_cast<AptValue*>(0);

    // The faithful record-table scan + the AS byte-code execution / __proto__ dispatch
    // live in the deferred AS-execution sub-path (FLAG). The integer result the X360
    // returns (0 / 1) is what every caller treats truthily; carried back as the shared
    // value pointer's int role.
    const int nRan = AptCIH_queueClipEvents_RunMatched(this, nEventMask, nFrameId, bDeferred);
    return reinterpret_cast<AptValue*>(static_cast<intptr_t>(nRan));
}

// ---------------------------------------------------------------------------
// GeneralisedProcess @ per-node -- the deferred generalised-process pass over one node.
//
// When the early-return gate is armed (AptCIH::sbGeneralisedProcessEarlyReturn) the node
// is skipped unless it is a defined, non-dead (CIHState != 3), subtree-or-self-dirty
// (mFlagsA bits 23/24) node whose render item is flagged dirty (mFlags bit31 set, or the
// 0x40000000 process bit). The registered process callbacks (sCIHProcessCb[0..2]) run and
// their results are OR'd; a sprite(5)/animation(9) recurses its child display list. The
// fold is written back into the subtree-dirty bit (mFlagsA bit23) and returned (0/1).
// ---------------------------------------------------------------------------

// FLAG (un-homed Apt-runtime generalised-process state; console AptCIH::bEarlyReturn /
// AptCIH::sCIHProcessCb[0..2] / AptCIH::nTreeDepth): the process gate flag, the up-to-three
// registered per-node process callbacks, and the recursion-depth counter. Owned + installed
// by the Apt generalised-process boot TU; declared here so the homed pass names them.
extern bool AptCIH_sbGeneralisedProcessEarlyReturn;                                   // bEarlyReturn
extern unsigned int (*AptCIH_sCIHProcessCb)(AptCIH*, AptCIH*, void*);                 // sCIHProcessCb
extern unsigned int (*AptCIH_sCIHProcessCb1)(AptCIH*, AptCIH*, void*);                // sCIHProcessCb1
extern unsigned int (*AptCIH_sCIHProcessCb2)(AptCIH*, AptCIH*, void*);               // sCIHProcessCb2
extern int AptCIH_snGeneralisedProcessTreeDepth;                                      // nTreeDepth

unsigned int AptCIH::GeneralisedProcess(AptCIH* pRoot, void* pContext)
{
    if (AptCIH_sbGeneralisedProcessEarlyReturn)
    {
        if (!getIsDefined())
            return 0;
        if (((mFlagsA >> 29) & 3u) == 3u)         // CIHState 3 == dead
            return 0;
        if ((mFlagsA & 0x01800000u) == 0u)         // neither subtree (bit23) nor self (bit24) dirty
            return 0;
        const uint32_t nItemFlags = mpCharacterInst->GetRenderItem()->mFlags;
        // X360: v5 >= 0 && (v5 & 0x40000000) == 0  ->  skip  (a non-dirty render item).
        if ((nItemFlags & 0x80000000u) == 0u && (nItemFlags & 0x40000000u) == 0u)
            return 0;
    }

    unsigned int nResult = 0;
    if (AptCIH_sCIHProcessCb)
        nResult = AptCIH_sCIHProcessCb(this, pRoot, pContext);
    if (AptCIH_sCIHProcessCb1)
        nResult = AptCIH_sCIHProcessCb1(this, pRoot, pContext) | nResult;
    if (AptCIH_sCIHProcessCb2)
        nResult = AptCIH_sCIHProcessCb2(this, pRoot, pContext) | nResult;

    const uint32_t nType = mpCharacterInst->GetTypeTag();
    if (nType == 5u || nType == 9u)
    {
        ++AptCIH_snGeneralisedProcessTreeDepth;
        AptCharacterSpriteInstBase* pSprite =
            static_cast<AptCharacterSpriteInstBase*>(mpCharacterInst);
        // FLAG (x64): AptDisplayList::GeneralisedProcess's first arg is the root context
        // (a pointer in the X360) modelled as int in its existing signature; the pointer
        // is carried through the int parameter (the DL walk only re-passes it to each
        // node's GeneralisedProcess, never dereferencing it here).
        const int nRootArg = static_cast<int>(reinterpret_cast<intptr_t>(pRoot));
        nResult = static_cast<unsigned int>(
                      pSprite->mDisplayList.GeneralisedProcess(nRootArg, -1, 0))
                  | nResult;
        --AptCIH_snGeneralisedProcessTreeDepth;
    }

    // Fold the OR'd result into the subtree-dirty bit (mFlagsA bit23), leaving every
    // other bit untouched (the X360 rotate-mask idiom).
    mFlagsA = (mFlagsA & 0xFF7FFFFFu) | ((nResult << 23) & 0x00800000u);
    return (mFlagsA >> 23) & 1u;
}

// ===========================================================================
// ClearCIH @0x82AF6020 -- tear down this node's placed character state.
//
// FAITHFUL (from the X360 ARTIST.XEX + PS3 DWARF): early-out on a transitioning
// (CIHState == 1) or never-constructed (mFlagsA bit27 clear) node. Clear the dirty
// bit, drop the node from the animation director's pending action/new-instance tables
// and the input-target's drag/focus slots, drop its per-frame timer functions when it
// is an animation (type tag 9), then -- when it is a mask master or slave -- unwire the
// "#!MASKMASTER!#"/"#!MASKSLAVE!#" cross-links + render-item mask flags (reusing GetMask
// / SetGeneralizedProcessDirtyState / the render-tree mask-state notify). Finally swap in
// a fresh "none" character instance (bClearGCRoots) or null the slot, GC-tear-down + free
// the old instance, and run the value teardown.
//
// FLAG: the deferred AS-execution tail (the type-5/16 unload-event dispatch +
// callFunction, the zombie-vector push + partial GC, and the host render hook
// dword_8324E8C8) is the deep AS-interpreter / GC sub-path -- routed through the cluster
// callee below until that engine is homed. The observable state teardown (the mask
// unwiring + the char-inst swap/free) is reconstructed faithfully here.
// ===========================================================================

// FLAG (un-homed action-queue / new-inst / input-target / GC sub-paths of ClearCIH;
// console sub_82ADBD50 + __::remove + the off_8324E528 zombie vector + dword_8324E8C8
// render hook + AptPartialGarbageCollection). Drops this node from every deferred queue
// the director / input target hold it in. The unload-event tail + the zombie
// decision live in ClearCIH itself (shipped order); the helper's int return is
// vestigial (always 0) and ignored.
int AptCIH_ClearCIH_DrainQueuesAndZombie(AptCIH* pNode, bool bClearGCRoots);

// ---- the zombie-vector surface (AptGC.cpp) --------------------------------
// The vector (X360 off_8324E528), the zombies-dirty flag (byte_8324E38F), the
// host notify hook (dword_8324E8C8), and the deferred-release vector the FULL-
// zombie arm flushes (off_8324E51C).
extern AptValueVector* gpAptZombieVector;
extern bool            gbAptZombiesDirty;
extern void          (*gpAptZombieNotifyHook)(int bImmediate, int nReserved,
                                              const char* pInstanceName,
                                              const char* pFileName);
extern AptValueVector* gpAptDeferredReleaseVector;   // off_8324E51C
extern AptCIH*         AptGetAnimationAtLevel(int nLevel);

// AptCIH_ClearCIH_DrainQueuesAndZombie -- ClearCIH's director-table DRAIN
// (HOMED 2026-07-02 from the X360 body @0x82AF6020; was the return-0 link-stub,
// which left cleared nodes dangling in the director's sets/tables -- the bulk
// removeObject path under mergeState walked them freed): remove the node from
// the director's input set (sub_82ADBD50) + listener set (__::remove), clear
// the press/rollover event slots to the undefined singleton when they point at
// it, and scrub it out of the shared new-instances table (Release + null the
// slot). The UNLOAD-EVENT tail + the zombie decision live in ClearCIH itself,
// in the shipped order (both consoles run the unload tail AFTER the mask
// master/slave teardown -- X360 bl chain @0x82AF6374, XB1 sub_1408333D0).
// Returns nonzero when the node became a zombie -- the zombie arm is still a
// staged FLAG (see the note at the ClearCIH call site), so 0 for now.
int AptCIH_ClearCIH_DrainQueuesAndZombie(AptCIH* pNode, bool /*bClearGCRoots*/)
{
    // ---- the director-set / event-slot / new-inst DRAIN --------------------
    AptAnimationTarget* const pDir =
        (gpAptTarget != nullptr) ? gpAptTarget->mpAnimationTarget : nullptr;
    if (pDir != nullptr)
    {
        // sub_82ADBD50(&director->mInputSet, node): find + clear + Release.
        for (uint32_t i = 0; i < pDir->mInputSet.mnCapacity; ++i)
        {
            if (pDir->mInputSet.mppSlots != nullptr &&
                reinterpret_cast<AptCIH*>(pDir->mInputSet.mppSlots[i]) == pNode)
            {
                pDir->mInputSet.mppSlots[i] = nullptr;
                pNode->Release();
                break;
            }
        }
        // The press / roll-over event slots reset to the undefined singleton.
        if (reinterpret_cast<AptCIH*>(pDir->mpOnPressObject) == pNode)
            pDir->mpOnPressObject = gpUndefinedValue;
        if (reinterpret_cast<AptCIH*>(pDir->mpOnRollOverObject) == pNode)
            pDir->mpOnRollOverObject = gpUndefinedValue;
        // __::remove(&director->mListenerSet, node): same slot scrub (the
        // listener adds do not AddRef -- the console remove releases nothing).
        for (uint32_t i = 0; i < pDir->mListenerSet.mnCapacity; ++i)
        {
            if (pDir->mListenerSet.mppSlots != nullptr &&
                reinterpret_cast<AptCIH*>(pDir->mListenerSet.mppSlots[i]) == pNode)
            {
                pDir->mListenerSet.mppSlots[i] = nullptr;
                break;
            }
        }
    }

    // The shared new-instances table: Release + null every slot holding the node.
    {
        AptValue** const ppInsts =
            static_cast<AptValue**>(AptAnimationTarget::GetNewInsts());
        const int nCount = AptAnimationTarget::GetNewInstSize();
        for (int i = 0; ppInsts != nullptr && i < nCount; ++i)
        {
            if (reinterpret_cast<AptCIH*>(ppInsts[i]) == pNode)
            {
                pNode->Release();     // (*(*v11 + 4))()
                ppInsts[i] = nullptr;
            }
        }
    }

    // ---- the zombie decision: FLAG (staged) -- see the ClearCIH call site ---
    return 0;
}

void AptCIH::ClearCIH(bool bClearGCRoots)
{
    // Transitioning (CIHState == 1) or not defined -> no-op. NB the second test reads the
    // AptValue base bitfield at +4 (mnValueData bit27 == mbIsDefined), NOT mFlagsA at +0xC.
    // console @0x82AF6020: `lwz r11,0xC` CIHState==1, then `lwz r11,4; extrwi. r11,r11,1,4`.
    if (((mFlagsA & 0x60000000u) == 0x20000000u) || !getIsDefined())
        return;

    SetDirtyState(false, false);

    // Drop the node from the director's deferred-action / new-instance tables, the input
    // target's drag/focus slots, the new-insts table, and (when it became externally
    // referenced) the zombie vector + partial GC; run the unload-event tail. Returns
    // nonzero when the node was queued as a zombie (the instance is NOT freed below).
    AptCIH_ClearCIH_DrainQueuesAndZombie(this, bClearGCRoots);

    // Animation node (type tag 9) -> remove its per-frame timer functions.
    if (mpCharacterInst->GetTypeTag() == 9u && gpAptTarget != nullptr)
        gpAptTarget->mpAnimationTarget->RemoveTimerFunctions(this);

    // ---- mask MASTER teardown: this node owns a mask (render item hasMask + a slave) --
    AptRenderItem* pRenderItem = mpCharacterInst->GetRenderItem();
    if (pRenderItem->GetHasMask() && pRenderItem->GetMask())
    {
        const EAStringC strMaskMaster("#!MASKMASTER!#");
        const EAStringC strMaskSlave("#!MASKSLAVE!#");
        if (AptCIH* pSlave = GetMask())
        {
            if (AptRenderTreeManager* pManager = AptCurrentRenderTreeManager())
                pManager->Update_ItemSetMaskState(pSlave, gnCurrUpdateTick, false);
            pSlave->mpCharacterInst->GetRenderItemWritable()->SetIsMask(false, nullptr);
            pSlave->SetGeneralizedProcessDirtyState(false);
            if (AptNativeHash* pSlaveHash =
                    pSlave->mpCharacterInst ? pSlave->mpCharacterInst->mpProperties : nullptr)
                pSlaveHash->Unset(strMaskSlave);
        }
        if (AptNativeHash* pMasterHash =
                mpCharacterInst ? mpCharacterInst->mpProperties : nullptr)
            pMasterHash->Unset(strMaskMaster);
        mpCharacterInst->GetRenderItemWritable()->SetHasMask(false, nullptr);
    }

    // ---- mask SLAVE teardown: this node IS a mask (render item isMask) -----------------
    if (mpCharacterInst->GetRenderItem()->GetIsMask())
    {
        const EAStringC strMaskMaster("#!MASKMASTER!#");
        const EAStringC strMaskSlave("#!MASKSLAVE!#");
        AptNativeHash* pSlaveHash =
            mpCharacterInst ? mpCharacterInst->mpProperties : nullptr;
        AptCIH* pMaster = pSlaveHash
            ? static_cast<AptCIH*>(pSlaveHash->Lookup(strMaskSlave))
            : nullptr;
        if (pMaster)
        {
            pMaster->mpCharacterInst->GetRenderItemWritable()->SetHasMask(false, nullptr);
            mpCharacterInst->GetRenderItemWritable()->SetIsMask(false, nullptr);
            SetGeneralizedProcessDirtyState(false);
            if (pSlaveHash)
                pSlaveHash->Unset(strMaskSlave);
            if (AptNativeHash* pMasterHash =
                    pMaster->mpCharacterInst ? pMaster->mpCharacterInst->mpProperties : nullptr)
                pMasterHash->Unset(strMaskMaster);
        }
    }

    // ---- the unload-event tail (skipped during shutdown) --------------------
    // Shipped order: AFTER the mask master/slave teardown, BEFORE the zombie
    // decision + instance teardown (X360 @0x82AF6374 bl chain; XB1 the same).
    // A sprite(5)/custom-control(16) node with an unload handler (record bit
    // 0x400 or a __proto__ member for mask 4) queues its unload clip events
    // and -- when an AS-defined onUnload handler resolves (findChild
    // &saConstant[69], a defined function value, vtbl 34..36) -- calls it
    // immediately through the interpreter.
    if (!gbAptInShutdown)
    {
        AptCharacterInst* const pInstU = mpCharacterInst;
        const uint32_t nTagU = (pInstU != nullptr) ? pInstU->GetTypeTag() : 0u;
        if (nTagU == 5u || nTagU == 16u)
        {
            AptCharacterSpriteInstBase* const pSprite =
                static_cast<AptCharacterSpriteInstBase*>(pInstU);
            if ((pSprite->mnClipActionFlags & 0x400u) != 0u ||
                HasEventMember(4) != 0)
            {
                queueClipEvents(4, 0, 0);
                // findChild(&saConstant[69] == the interned "onUnload" key).
                AptValue* const pChild =
                    findChild(&StringPool::saConstant[69], nullptr);
                if (pChild != nullptr && pChild->getIsDefined())
                {
                    // A callable function value (vtbl indices 34..36).
                    const int nVtbl = static_cast<int>(pChild->getVtblIndex());
                    if (nVtbl >= 34 && nVtbl <= 36)
                    {
                        gAptActionInterpreter.callFunction(
                            static_cast<AptValue*>(this), pChild, 0, nullptr, nullptr);
                        // Pop the call result off the operand stack.
                        reinterpret_cast<AptValueVector*>(
                            &gAptActionInterpreter.mnStackTop)->pop();
                    }
                }
            }
        }
    }

    // ---- the zombie decision (X360 @0x82AF6374..; XB1 sub_1408333D0 tail) ----
    // An externally referenced type-9 (animation/level) node survives its
    // removal as an AS-visible ZOMBIE instead of being torn down: the AS side
    // still holds live references (IncZombieCount) against the loaded movie.
    // NOT-zombie preconditions (any -> the immediate clear below): in shutdown;
    // zombie count <= 0 (the X360 tests the SIGNED int16 `(flagsA<<9)&0xFFFF0000
    // <= 0` -- sign bit == our count field's top bit); not an animation node
    // (charInst mTypeFlags tag 9, mask 0xFC000000 == 0x24000000); or the root
    // level-0 animation itself.
    bool bBecameZombie = false;
    if (!gbAptInShutdown
        && GetZombieCount() > 0
        && (mpCharacterInst->mTypeFlags & 0xFC000000u) == 0x24000000u
        && this != AptGetAnimationAtLevel(0))
    {
        if (gpAptZombieVector == nullptr ||
            gpAptZombieVector->mnCapacity >= gpAptZombieVector->mnTop)
        {
            // No vector (config word 14 == 0) or full: the node cannot be kept.
            // Mark it CIHState 2 (dead-transition), fire the notify hook with
            // bImmediate=1, and fall through to the immediate clear.
            SetCIHState(2);   // X360 `flagsA = 0x40000000 | (flagsA & 0x9FFFFFFF)`
            if (gpAptZombieNotifyHook != nullptr)
            {
                const AptFile* pFile =
                    static_cast<AptCharacterAnimationInst*>(mpCharacterInst)
                        ->mAnimationFilePtr.pData;
                gpAptZombieNotifyHook(1, 0, GetInstanceName().GetBuffer(),
                                      pFile ? pFile->mFileName.GetBuffer() : "");
            }
        }
        else
        {
            // FULL ZOMBIE: scrub the AS-visible surface (the child display list +
            // the property hash), flush any pending deferred releases, and --
            // when the scrub itself did not drain the zombie count -- park the
            // node: AS-changed/HasClass cleared, SetReleaseAtEnd (plain Release
            // pins it from here), pushed onto the vector, CIHState 1, the source
            // AptFile's state swapped to 5 (old saved in mnField12), the notify
            // hook fired with bImmediate=0, and the zombies-dirty flag raised.
            static_cast<AptCharacterSpriteInstBase*>(mpCharacterInst)
                ->mDisplayList.clear(true);                     // charInst+28, arg 1
            if (AptNativeHash* pHash = GetNativeHashVirtual())  // vtbl[2]
                pHash->ClearData();
            if (gpAptDeferredReleaseVector != nullptr &&
                gpAptDeferredReleaseVector->mnCapacity != 0)    // family-(B) live count
                gpAptDeferredReleaseVector->ReleaseValues();

            if (GetZombieCount() > 0)   // re-check: the scrub may have drained it
            {
                mFlagsA &= ~0x80000000u;   // AS-changed clear (the X360 rlwinm 1,31)
                SetHasClass(0);            // vtable slot 5
                SetReleaseAtEnd();
                // Push onto the zombie vector (family-(B): count == mnCapacity,
                // capacity == mnTop); a lost race to full rolls the pin back.
                if (gpAptZombieVector->mnCapacity < gpAptZombieVector->mnTop)
                    gpAptZombieVector->mppItems[gpAptZombieVector->mnCapacity++] =
                        static_cast<AptValue*>(this);
                else
                    ClearReleaseAtEnd();
                SetCIHState(1);            // X360 `flagsA & 0x9FFFFFFF | 0x20000000`

                AptFile* pFile =
                    static_cast<AptCharacterAnimationInst*>(mpCharacterInst)
                        ->mAnimationFilePtr.pData;
                if (pFile != nullptr)
                {
                    const int32_t nOldState = pFile->mnState;
                    pFile->mnState   = 5;
                    pFile->mnField12 = nOldState;
                }
                if (gpAptZombieNotifyHook != nullptr)
                    gpAptZombieNotifyHook(0, 0, GetInstanceName().GetBuffer(),
                                          pFile ? pFile->mFileName.GetBuffer() : "");
                gbAptZombiesDirty = true;
                bBecameZombie = true;
            }
        }
    }
    if (bBecameZombie)
        return;   // the X360 returns straight after byte_8324E38F = 1

    // ---- char-instance teardown -------------------------------------------------------
    AptCharacterInst* pOld = mpCharacterInst;
    if (pOld == nullptr)
    {
        // X360/XB1 tail: clear the AS-changed flag inline (mFlagsA bit 31, the
        // X360 `rlwinm 1,31`), then tail-call vtable slot 5 == SetHasClass(0)
        // (X360 0x82AD7418 `insrwi ..,1,3` = our bit 28; XB1 sub_140841530, the
        // same slot on its LE flag layout). The previous transcription mislabeled
        // the slot-5 call as Release(), which fabricated an extra reference drop
        // on every cleared node.
        mFlagsA &= ~0x80000000u;
        SetHasClass(0);
        return;
    }

    // The immediate-free path (every non-zombie removal reaches here).
    {
        setGCRoot(0);
        if (bClearGCRoots)
        {
            // Replace the old instance with a fresh "none" + carry the render data across.
            AptCharacterInst* pNone = AptCharacterInst::CreateCharacterInst(nullptr);
            mpCharacterInst = pNone;
            pNone->MoveRenderDataFrom(pOld);
        }
        else
        {
            mpCharacterInst = nullptr;
        }

        // Notify the render tree the old item was removed (when it still had one), tear
        // down its property hash, then GC-destroy + free it.
        if (pOld->mpRenderItem != nullptr)
        {
            if (AptRenderTreeManager* pManager = AptCurrentRenderTreeManager())
                pManager->Update_ItemRemoved(pOld->GetRenderItemWritable(), gnCurrUpdateTick);
        }
        if (pOld->mpProperties != nullptr)
        {
            pOld->mpProperties->DestroyGCPointers();
            pOld->mpProperties->~AptNativeHash();
            gpNonGCPoolManager->Deallocate(pOld->mpProperties, sizeof(AptNativeHash));
            pOld->mpProperties = nullptr;
        }
        pOld->DestroyGCPointers();
        pOld->~AptCharacterInst();
        gpNonGCPoolManager->Deallocate(pOld, sizeof(AptCharacterInst));

        // AS-changed clear + vtable slot 5 == SetHasClass(0) (see the null-inst
        // arm note above).
        mFlagsA &= ~0x80000000u;
        SetHasClass(0);
    }
}

// ===========================================================================
// Free-function shims -- the AptCIH_* / AptDisplayList_* entry points the sibling Apt
// TUs reference by name (the X360 calls each with the receiver in r3). Each forwards to
// the now-homed method so there is a single definition.
// ===========================================================================

// AptCIH_tick -- AptCIH::tick @0x82B0BED8 (homed in AptCIH.cpp). Returns int (tick's
// enterFrame-stage result the AptDisplayList walk OR-s into nResult).
int AptCIH_tick(AptCIH* pCIH) { return pCIH->tick(); }

// AptCIH_GeneralisedProcess -- AptCIH::GeneralisedProcess per-node. The AptDisplayList
// walk passes (node, nFlags) where nFlags is the walk's `a2` (the root context pointer
// in the X360); the third (void* context) arg is unused by that call site (r5 carries
// the leftover), so it is forwarded as null.
int AptCIH_GeneralisedProcess(AptCIH* pCIH, int a2)
{
    // FLAG (x64): the X360 a2 is the root-context pointer; the existing free-shim/DL
    // signatures carry it through `int`, so widen back via intptr_t (the value is only
    // re-passed, never dereferenced through this width).
    AptCIH* const pRoot = reinterpret_cast<AptCIH*>(static_cast<intptr_t>(a2));
    return static_cast<int>(pCIH->GeneralisedProcess(pRoot, nullptr));
}

// AptCIH_queueClipEvents -- AptCIH::queueClipEvents (canonical (int, unsigned int, int)).
// The console passes the CIH as an AptValue*; narrow to the node.
AptValue* AptCIH_queueClipEvents(AptValue* pCIH, int nEventMask, unsigned int nFrameId, int bDeferred)
{
    return static_cast<AptCIH*>(pCIH)->queueClipEvents(nEventMask, nFrameId, bDeferred);
}

// AptCIH_SetCharacterInst -- AptCIH::SetCharacterInst @0x82B00548 (homed above).
void AptCIH_SetCharacterInst(AptCIH* pCIH, AptCharacterInst* pInst, int bFlag)
{
    pCIH->SetCharacterInst(pInst, bFlag != 0);
}

// AptCIH_InsertChild -- AptCIH::InsertChild @0x82B09CA0: place pCharacter into pNode's
// child display list (mpCharacterInst->mDisplayList) at nDepth under pName. The X360
// seeds the placement field (a6/nPlacementField18) from pSource's char-inst render-item
// +0x18 when pSource is given, then forwards to AptDisplayList::placeObject.
AptCIH* AptCIH_InsertChild(AptCIH* pNode, AptCIH* pSource, AptCharacter* pCharacter,
                           int nDepth, EAStringC* pName, AptValue* pInitObject)
{
    AptCharacterSpriteInstBase* pSpriteInst =
        static_cast<AptCharacterSpriteInstBase*>(pNode->GetCharacterInst());
    AptDisplayList* pChildList = &pSpriteInst->mDisplayList;

    // console @0x82B09CD0: a SINGLE deref reads the source char-inst's subclass placement
    // slot at +0x18 (`lwz r29,0x18(*(a2+0x20))`) -- the same field AptCharInst_SetPlacement-
    // Field18 writes; encapsulated (subclass-specific role), NOT the render-item mFlags.
    extern const void* AptCharInst_GetPlacementField18(AptCharacterInst* pInst);
    const void* pPlacementClipActions = nullptr;
    if (pSource != nullptr)
        pPlacementClipActions = AptCharInst_GetPlacementField18(pSource->GetCharacterInst());

    return pChildList->placeObject(
        /*pExistingNode*/ nullptr, nDepth, pCharacter, pName, pNode,
        /*bForceRemove*/ 1, /*nClipDepth*/ -1, /*fFrameValue*/ 0.0,
        /*pColorXForm*/ nullptr, /*pPositionMatrix*/ nullptr,
        pPlacementClipActions, /*pClassObject*/ pInitObject);
}

// AptDisplayList_mergeState -- AptDisplayList::mergeState @0x82B0B438 (homed in
// AptDisplayList.cpp). The X360 passes the source AptPseudoDisplayList directly as the
// merge-info pointer (its [0]=inner list / [1]=owning parent are read inside), the
// AS property hash as pParentHash, and bForward as the keep-removed flag.
void* AptDisplayList_mergeState(AptDisplayList* pList, AptPseudoDisplayList* pScratch,
                                void* pProperties, char bForward)
{
    return pList->mergeState(reinterpret_cast<void**>(pScratch),
                             static_cast<AptNativeHash*>(pProperties), bForward);
}

// AptCIH_PreDestroyHook -- AptCIH::PreDestroy's optional notify hook (console
// dword_8324E8A0). When a host has installed the callback, dispatch to it; else no-op.
void AptCIH_PreDestroyHook(AptCIH* pCIH)
{
    if (gpAptCIHPreDestroyHook)
        gpAptCIHPreDestroyHook(pCIH);
}

// ===========================================================================
// AptDisplayListState::AddToDelayReleaseList @0x82AFD028 -- detach pItem from the live
// list and (when still externally referenced) queue it on the director's removal list,
// then release it. The X360: call the item's vtable[0] -- which is AptValue::AddRef
// (X360 CIH vtbl[+0] = 0x82ADCF20 = AddRef; XB1 slot 0 = 0x14082D030, the saturating
// count+1 -- both read from the shipped vtables, NOT PreDestroy as previously
// mislabeled) -- then removeItem it from this list, AptCIH::Remove(item, bDelay), and
// -- when its refcount field (mValueBitfield count, the 0x03FFC000 packed field >
// 0x4000 i.e. > 1) shows an outside reference -- AddToRemList it; finally tail-call
// its Release (vtable[1]). The leading AddRef is the PIN that keeps the node alive
// across Remove's internal releases (the ClearCIH new-inst scrub + Remove's own
// display-list-reference drop): without it a node that enters at refcount 1..2 hits
// zero INSIDE Remove and the tail reads/vcalls tear into freed pool memory (the
// DOGMA free-list link overwrites the vptr at word 0). The node can only ever be
// freed by the LAST line of this function, after which nothing touches it.
// ===========================================================================
// EnsureStringAllocated @0x82B06F08 is now HOMED in AptCIHText.cpp (the deep dynamic-text
// build/layout path -- AptCharacterTextInst::UpdateText + the AptAllocateStringParameters
// build + the host pfnAllocateString call + the SetZID/box-align/measure fold). The prior
// FLAG no-op stub here is retired.

AptCIH* AptDisplayListState::AddToDelayReleaseList(AptCIH* pItem, bool bDelay)
{
    // X360: (**pItem)(pItem) -- the value's vtable[0] == AddRef (the lifetime pin;
    // see the header note).
    pItem->AddRef();

    removeItem(pItem);
    pItem->Remove(bDelay);

    // refcount > 1 (an outside reference survives) -> queue on the director's rem list.
    if (pItem->getRefCount() > 1u && gpAptTarget != nullptr)
        AptAnimationTarget_AddToRemList(gpAptTarget->mpAnimationTarget, pItem);

    pItem->Release();   // X360 vtable[1]
    return pItem;
}

// ===========================================================================
// Apt-context teardown shims -- the two free helpers AptCIH::Remove @0x82AFC0B0
// calls before clearing the node. Both reach into the active Apt context
// (gpAptTarget, console off_8324E574) and forward to the owning sub-object; the
// X360 inlines these reads at Remove's call sites (no separate function), so the
// free-function form is the decompile-time split. Homed here next to Remove.
// ===========================================================================

// AptApt_CancelLoad -- cancel this node's in-flight asset load through the context's
// file linker. X360 @0x82AFC0C8: r3 = gpAptTarget->mpLinker (+0x20); r4 = pNode;
// AptLinker::CancelLoad(linker, pNode). pNode (an AptCIH) is an AptValue, the
// CancelLoad arg type.
void AptApt_CancelLoad(AptCIH* pNode)
{
    gpAptTarget->mpLinker->CancelLoad(pNode);
}

// AptApt_RemoveActionFor -- drop every ActionScript action queued against this node.
// X360 @0x82AFC0D8: r11 = gpAptTarget->mpAnimationTarget (+0x18); r3 =
// animTarget->mpActionQueue (+0x0C); r4 = pNode; AptActionQueueC::RemoveActionFor(
// queue, pNode). The console discards the AptValue* return; void here to match the
// call-site declaration.
void AptApt_RemoveActionFor(AptCIH* pNode)
{
    gpAptTarget->mpAnimationTarget->mpActionQueue->RemoveActionFor(pNode);
}
