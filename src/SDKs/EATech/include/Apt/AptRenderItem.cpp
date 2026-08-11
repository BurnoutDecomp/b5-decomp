// ===========================================================================
// EATech Apt -- AptRenderItem base.   DECOMPILED from the PS3 EXTERNAL ELF
// (cross-checked vs X360 ARTIST).
//   ctor 0x7E47A0 / dtor 0x80F860 / GetPosition/ColorMatrixConst 0x7DFB60/0x7DFB80
//   / ...Writable 0x7F1578/0x7FDF68 / depth 0x7DEE14/0x7DEEA0 / clipdepth
//   0x7DEDF8/0x7DEEA8 / flags 0x7DEE68/0x7DEE20/0x7DEE34 / mask 0x7DEE2C /
//   character 0x7DEE04/0x7DEE90/0x80F2DC / refcount 0x7DEE78/0x7DFA7C/0x7DFA98 /
//   manager links 0x7DEED0/0x7DEEC8/0x7DEED8.
//
// The position/colour transforms are allocated lazily (null reads return the
// shared identities). The reference count (mRefCount, byte 36) is mutated with
// the console lwarx/stwcx. atomic -> host interlocked. The base is abstract; each
// character subtype (Shape/Sprite/...) supplies Render()/Push/PopRenderData.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItem.h"
#include "SDKs/EATech/include/Apt/AptCharacter.h"        // Add/ReleaseCharacterReference
#include "SDKs/EATech/include/Apt/AptStd/AptMatrix.h"     // AptMatrix (position/mask)
#include "SDKs/EATech/include/Apt/AptStd/AptCXForm.h"     // AptCXForm (colour)
#include "SDKs/EATech/include/Apt/AptRenderingContext.h"  // the transform/colour stacks
#include "SDKs/EATech/include/Apt/AptDefine.h"            // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"                // DOGMA_PoolManager

#include <intrin.h>   // _InterlockedIncrement/Decrement/_InterlockedExchange
#include <new>        // placement new (lazy colour-matrix construct)

int AptRenderItem::sItemsAllocated = 0;

// ---------------------------------------------------------------------------
// Render-tree revision lock (X360 unk_8324E7CC) + the global render-item
// shutdown latch (X360 byte_8324E56C). The base render-item TU is their home;
// the subtype TUs (e.g. AptRenderItemCustomControl) declare them extern.
//
// The console brackets each revision-link swap with the lwarx/stwcx.
// interrupt-masked test-and-set on unk_8324E7CC; modelled here as an
// interlocked test-and-set (host-portable, uncontended on the single-threaded
// bring-up path -- see the PC-platform leaf markers below). byte_8324E56C gates
// the manager-link teardown in the dtor
// (true once the render system is shutting down -> skip the revision teardown).
// ---------------------------------------------------------------------------
volatile long gAptRenderTreeRevisionLock = 0;   // X360 unk_8324E7CC
bool          gbRenderItemShuttingDown   = false; // X360 byte_8324E56C

namespace
{
    // The console brackets each revision-link swap with an interrupt-masked lwarx/stwcx.
    // test-and-set on unk_8324E7CC.
    // FLAG PC-platform leaf: on single-threaded PC this is a plain _InterlockedExchange spin
    // (a threading primitive, not an engine Class::method).
    inline void AptRenderTreeLock_Acquire()
    {
        while (_InterlockedExchange(&gAptRenderTreeRevisionLock, 1) != 0)
        {
            // spin until the previous holder releases (sets it back to 0)
        }
    }
    // FLAG PC-platform leaf: paired release of the single-threaded render-tree spinlock.
    inline void AptRenderTreeLock_Release()
    {
        _InterlockedExchange(&gAptRenderTreeRevisionLock, 0);
    }
}

// Base render hooks -- empty (@0x7E49A8/0x7E499C/0x7E49A4/0x7E49A0). The base
// renderable draws nothing; the character subtypes override Render.
void AptRenderItem::Render(AptRenderingContext*, AptMaskRenderOperation, int) const {}
void AptRenderItem::PushRenderData(AptRenderingContext*, AptMaskRenderOperation, int) const {}
void AptRenderItem::PopRenderData(AptRenderingContext*, AptMaskRenderOperation, int) const {}
void AptRenderItem::PushRenderDataAbsolute(AptRenderingContext*) const {}

// ---------------------------------------------------------------------------
// Pool-backed sized delete: render items are allocated from gpNonGCPoolManager
// (AptRenderItem::Manager_CreateItem), so `delete this` at refcount zero must
// return the block to that pool. The sized form receives the dynamic subtype's
// size from the virtual deleting destructor.
// ---------------------------------------------------------------------------
void AptRenderItem::operator delete(void* p, size_t sz)
{
    if (p)
        gpNonGCPoolManager->Deallocate(p, sz);
}

// ctor @0x7E47A0
AptRenderItem::AptRenderItem(AptCharacter* pCharacter, int nCreatedOnTick)
{
    mpCharacter          = pCharacter;
    mpPositionMatrix     = nullptr;
    mpColorMatrix        = nullptr;
    mpMaskPositionMatrix = nullptr;
    mDepth               = -1;
    mClipDepth           = -1;
    mpMask               = nullptr;
    mCreatedOnTick       = nCreatedOnTick;
    mRefCount            = 0;
    mpManagerNextRevision = nullptr;
    mpManagerNextSibling  = nullptr;
    mpManagerFirstChild   = nullptr;

    // Flags: set isVisible (bit31), clear the rest. (Console: (flags|0x80000000)
    // rotated through the 19-bit field; the net effect for a fresh item.)
    mFlags = 0x1u;

    if (pCharacter)
        pCharacter->AddCharacterReference();
    ++sItemsAllocated;
}

// Chase a manager link to its latest revision (the console's
// `while (p->mpManagerNextRevision) p = p->mpManagerNextRevision`).
static AptRenderItem* LatestRevision(AptRenderItem* p)
{
    if (p)
        while (p->mpManagerNextRevision)
            p = p->mpManagerNextRevision;
    return p;
}

// ---------------------------------------------------------------------------
// Clone copy-ctor @0x82AEB9A0 -- the shared base render-item copy helper every
// subtype Clone delegates to. Deep-copies pSource into this fresh pool block:
// the base visual state + lazily-allocated transforms always; with bCopyExtended
// the mask, depth, manager links and the copied state-flag bits too (each
// manager link chased to its latest revision and reference-counted).
// ---------------------------------------------------------------------------
AptRenderItem::AptRenderItem(const AptRenderItem* pSource, int nCreatedOnTick, bool bCopyExtended)
{
    mpCharacter           = pSource->mpCharacter;
    mpPositionMatrix      = nullptr;
    mpColorMatrix         = nullptr;
    mpMaskPositionMatrix  = nullptr;
    mDepth                = -1;
    mClipDepth            = -1;
    mpMask                = nullptr;
    mCreatedOnTick        = nCreatedOnTick;
    mRefCount             = 0;
    mpManagerNextRevision = nullptr;
    mpManagerNextSibling  = nullptr;
    mpManagerFirstChild   = nullptr;

    // isVisible (bit31) set; render-type bits (18..23) cleared for the subtype to
    // stamp; bit27 carried over from the source.
    mFlags = 0x1u | (pSource->mFlags & 0x10u);

    // Deep-copy the lazily-allocated transforms.
    if (pSource->mpPositionMatrix)
    {
        AptMatrix* p = static_cast<AptMatrix*>(gpNonGCPoolManager->Allocate(sizeof(AptMatrix)));
        if (p)
        {
            p->a = 0.0f; p->b = 0.0f; p->c = 0.0f; p->d = 0.0f; p->tx = 0.0f; p->ty = 0.0f;
            p->AptMatrixCopy(pSource->mpPositionMatrix);
        }
        mpPositionMatrix = p;
    }
    if (pSource->mpColorMatrix)
    {
        AptCXForm* p = static_cast<AptCXForm*>(gpNonGCPoolManager->Allocate(sizeof(AptCXForm)));
        mpColorMatrix = p ? new (p) AptCXForm(pSource->mpColorMatrix) : nullptr;
    }

    // Extended copy: mask, depth, manager links + the source's state-flag bits.
    if (bCopyExtended)
    {
        mDepth     = pSource->mDepth;
        mClipDepth = pSource->mClipDepth;
        mFlags = (mFlags & ~0x1u) | (pSource->mFlags & 0x1u);   // isVisible (bit31)
        mFlags = (mFlags & ~0x2u) | (pSource->mFlags & 0x2u);   // isMask   (bit30)
        mFlags = (mFlags & ~0x4u) | (pSource->mFlags & 0x4u);   // hasMask  (bit29)
        SetMaskMatrix(pSource->mpMaskPositionMatrix);
        mFlags = (mFlags & ~0x8u) | (pSource->mFlags & 0x8u);   // deletion mark (bit28)

        mpMask                = LatestRevision(pSource->mpMask);
        mpManagerNextRevision = nullptr;
        mpManagerNextSibling  = LatestRevision(pSource->mpManagerNextSibling);
        mpManagerFirstChild   = LatestRevision(pSource->mpManagerFirstChild);

        if (mpManagerNextSibling) mpManagerNextSibling->AddReference();
        if (mpManagerFirstChild)  mpManagerFirstChild->AddReference();
        if (mpMask)               mpMask->AddReference();

        if (pSource->mFlags & 0x20u)
            mFlags |= 0x40u;                                          // writable-revision (bit25) from source bit26
        mFlags = (mFlags & ~0x80u) | (pSource->mFlags & 0x80u);  // bit24
    }

    if (mpCharacter)
        mpCharacter->AddCharacterReference();
    ++sItemsAllocated;
}

// SetMaskMatrix @0x82AE52F8 -- lazily (de)allocate + copy the mask position matrix
// (null clears + frees it).
void AptRenderItem::SetMaskMatrix(const AptMatrix* pMatrix)
{
    if (mpMaskPositionMatrix)
    {
        if (pMatrix)
        {
            mpMaskPositionMatrix->AptMatrixCopy(pMatrix);
        }
        else
        {
            gpNonGCPoolManager->Deallocate(mpMaskPositionMatrix, sizeof(AptMatrix));
            mpMaskPositionMatrix = nullptr;
        }
    }
    else if (pMatrix)
    {
        AptMatrix* p = static_cast<AptMatrix*>(gpNonGCPoolManager->Allocate(sizeof(AptMatrix)));
        if (p)
        {
            p->a = 0.0f; p->b = 0.0f; p->c = 0.0f; p->d = 0.0f; p->tx = 0.0f; p->ty = 0.0f;
            p->AptMatrixCopy(pMatrix);
        }
        mpMaskPositionMatrix = p;
    }
}

// CopyRenderDataFrom @0x82AE5400 -- transfer pSource's visual state onto this item.
void AptRenderItem::CopyRenderDataFrom(const AptRenderItem* pSource)
{
    // Position matrix: copy the source's (lazy-alloc if we have none), or -- when the
    // source has none -- reset ours to identity.
    if (pSource->mpPositionMatrix)
    {
        if (!mpPositionMatrix)
        {
            AptMatrix* p = static_cast<AptMatrix*>(gpNonGCPoolManager->Allocate(sizeof(AptMatrix)));
            if (p) { p->a = 0.0f; p->b = 0.0f; p->c = 0.0f; p->d = 0.0f; p->tx = 0.0f; p->ty = 0.0f; }
            mpPositionMatrix = p;
        }
        if (mpPositionMatrix)
            mpPositionMatrix->AptMatrixCopy(pSource->mpPositionMatrix);
    }
    else if (mpPositionMatrix)
    {
        mpPositionMatrix->AptMatrixCopy(&gAptIdentityMatrix);   // X360 flt_8324E2B0
    }

    // Colour matrix: only the lazy-alloc path copies the source (the X360 leaves an
    // existing colour matrix untouched when the source also has one); when the source
    // has none, reset ours to the null colour transform.
    if (pSource->mpColorMatrix)
    {
        if (!mpColorMatrix)
        {
            AptCXForm* p = static_cast<AptCXForm*>(gpNonGCPoolManager->Allocate(sizeof(AptCXForm)));
            mpColorMatrix = p ? new (p) AptCXForm(pSource->mpColorMatrix) : nullptr;
        }
    }
    else if (mpColorMatrix)
    {
        mpColorMatrix->AptCXFormCopy(&gAptNullCXForm);   // X360 off_82F73388
    }

    // Carry the clip depth + the isVisible flag (bit 31) from the source.
    mClipDepth = pSource->mClipDepth;
    mFlags = (mFlags & ~0x1u) | (pSource->mFlags & 0x1u);
}

// dtor @0x80F860 (X360 twin @0x82AEBCE0 -- manager-chain teardown decompiled from
// the ARTIST asm).
AptRenderItem::~AptRenderItem()
{
    // The manager next-revision / next-sibling / first-child chain teardown
    // (console a1[10]/[11]/[12]), skipped once the render system is shutting down
    // (byte_8324E56C). Each chain collapses through its own same-kind link: pop
    // links this dtor holds the LAST reference to (mRefCount <= 1) and delete them
    // directly (the console's virtual deleting-dtor call, flags=1), re-linking to
    // the popped link's own next; stop at the first link someone else still
    // references and Release it instead.
    if (!gbRenderItemShuttingDown)
    {
        while (AptRenderItem* pRev = mpManagerNextRevision)
        {
            if (pRev->mRefCount > 1)
                break;
            AptRenderItem* pNext = pRev->mpManagerNextRevision;   // the link's own [c:0x28]
            pRev->mpManagerNextRevision = nullptr;
            delete pRev;                                          // (*vtbl)(link, 1)
            mpManagerNextRevision = pNext;
        }
        if (mpManagerNextRevision)
        {
            AptRenderItem* pOld = mpManagerNextRevision;
            mpManagerNextRevision = nullptr;
            pOld->ReleaseReference();
        }

        while (AptRenderItem* pSib = mpManagerNextSibling)
        {
            if (pSib->mRefCount > 1)
                break;
            AptRenderItem* pNext = pSib->mpManagerNextSibling;    // the link's own [c:0x2C]
            pSib->mpManagerNextSibling = nullptr;
            delete pSib;
            mpManagerNextSibling = pNext;
        }
        if (mpManagerNextSibling)
        {
            AptRenderItem* pOld = mpManagerNextSibling;
            mpManagerNextSibling = nullptr;
            pOld->ReleaseReference();
        }

        while (AptRenderItem* pChild = mpManagerFirstChild)
        {
            if (pChild->mRefCount > 1)
                break;
            AptRenderItem* pNext = pChild->mpManagerFirstChild;   // the link's own [c:0x30]
            pChild->mpManagerFirstChild = nullptr;
            delete pChild;
            mpManagerFirstChild = pNext;
        }
        if (mpManagerFirstChild)
        {
            AptRenderItem* pOld = mpManagerFirstChild;
            mpManagerFirstChild = nullptr;
            pOld->ReleaseReference();
        }
    }

    if (mpCharacter)
    {
        mpCharacter->ReleaseCharacterReference();
        mpCharacter = nullptr;
    }
    if (mpMask)
    {
        AptRenderItem* pMask = mpMask;
        mpMask = nullptr;
        pMask->ReleaseReference();
    }
    if (mpMaskPositionMatrix)
    {
        gpNonGCPoolManager->Deallocate(mpMaskPositionMatrix, sizeof(AptMatrix));
        mpMaskPositionMatrix = nullptr;
    }
    if (mpPositionMatrix)
    {
        gpNonGCPoolManager->Deallocate(mpPositionMatrix, sizeof(AptMatrix));
        mpPositionMatrix = nullptr;
    }
    if (mpColorMatrix)
    {
        mpColorMatrix->~AptCXForm();   // resets the AptColorHelper vptrs (console inlined this)
        gpNonGCPoolManager->Deallocate(mpColorMatrix, sizeof(AptCXForm));
        mpColorMatrix = nullptr;
    }
    --sItemsAllocated;
}

// `vector deleting destructor' @0x82AEF2B0 -- DROPPED (compiler-synthesized thunk,
// like every sibling AptRenderItem* class). The X360 body is exactly
//   ~AptRenderItem(); if (flag & 1) operator delete(this); return this;
// which MSVC re-derives from the committed virtual ~AptRenderItem() above plus the
// sized AptRenderItem::operator delete. No separate hand-written logical source.

// ---- transforms -----------------------------------------------------------
const AptMatrix* AptRenderItem::GetPositionMatrixConst() const
{
    return mpPositionMatrix ? mpPositionMatrix : &gIdentityMatrix;
}

const AptCXForm* AptRenderItem::GetColorMatrixConst() const
{
    return mpColorMatrix ? mpColorMatrix : &gIdentityCXForm;
}

const AptMatrix* AptRenderItem::GetMaskPositionMatrixConst() const { return mpMaskPositionMatrix; }

AptMatrix* AptRenderItem::GetPositionMatrixWritable()
{
    if (mpPositionMatrix)
        return mpPositionMatrix;
    AptMatrix* p = static_cast<AptMatrix*>(gpNonGCPoolManager->Allocate(sizeof(AptMatrix)));
    // console 0x82AE51E0: memset(0) then copies the IDENTITY AptMatrix (flt_8324E2B0 ==
    // gAptIdentityMatrix): a=d=1, the rest 0. A zero 2x3 affine would collapse every
    // vertex to the origin -- unlike the const getters + the clone/SetMask lazy-allocs
    // (which copy a source over the zeros), this getter has no follow-up copy, so it must
    // seed identity itself (the sibling GetColorMatrixWritable likewise copies identity).
    p->a = 1.0f; p->b = 0.0f; p->c = 0.0f; p->d = 1.0f; p->tx = 0.0f; p->ty = 0.0f;
    mpPositionMatrix = p;
    return p;
}

AptCXForm* AptRenderItem::GetColorMatrixWritable()
{
    if (mpColorMatrix)
        return mpColorMatrix;
    AptCXForm* p = static_cast<AptCXForm*>(gpNonGCPoolManager->Allocate(sizeof(AptCXForm)));
    new (p) AptCXForm(&gIdentityCXForm);   // copy-from-pointer ctor (console: AptCXForm(v3,&gIdentityCXForm))
    mpColorMatrix = p;
    return p;
}

// ---- depth / clip depth ---------------------------------------------------
int16_t AptRenderItem::GetDepth() const     { return mDepth; }
int16_t AptRenderItem::GetClipDepth() const { return mClipDepth; }
void AptRenderItem::SetDepth(int nDepth)         { mDepth = static_cast<int16_t>(nDepth); }      // x64 QEAAXH@Z: void
void AptRenderItem::SetClipDepth(int nClipDepth) { mClipDepth = static_cast<int16_t>(nClipDepth); }   // x64 QEAAXH@Z: void

// ---- flags ----------------------------------------------------------------
bool AptRenderItem::GetIsVisible() const { return (mFlags & 0x1u) != 0; }
bool AptRenderItem::GetIsMask() const    { return (mFlags & 0x2u) != 0; }
bool AptRenderItem::GetHasMask() const   { return (mFlags & 0x4u) != 0 && mpMask != nullptr; }
AptRenderItem* AptRenderItem::GetMask() const { return mpMask; }

// ---- character ------------------------------------------------------------
const AptCharacter* AptRenderItem::GetCharacterConst() const { return mpCharacter; }
AptCharacter*       AptRenderItem::GetCharacterWritable() const { return mpCharacter; }

AptCharacter* AptRenderItem::SetCharacter(AptCharacter* pCharacter)
{
    if (pCharacter)
        pCharacter->AddCharacterReference();
    AptCharacter* pOld = mpCharacter;
    if (pOld)
        pOld->ReleaseCharacterReference();
    mpCharacter = pCharacter;
    return pOld;
}

int AptRenderItem::GetCreatedOnTick() const { return mCreatedOnTick; }
int AptRenderItem::GetRefCount() const      { return mRefCount; }

// ---- reference count (atomic) --------------------------------------------
void AptRenderItem::AddReference() const
{
    _InterlockedIncrement(reinterpret_cast<volatile long*>(&const_cast<AptRenderItem*>(this)->mRefCount));
}

void AptRenderItem::ReleaseReference() const
{
    const long now = _InterlockedDecrement(
        reinterpret_cast<volatile long*>(&const_cast<AptRenderItem*>(this)->mRefCount));
    if (now == 0)
        delete const_cast<AptRenderItem*>(this);   // virtual deleting dtor -> sized operator delete (pool)
}

// ---- render-tree-manager links -------------------------------------------
AptRenderItem* AptRenderItem::Manager_GetFirstChild() const   { return mpManagerFirstChild; }
AptRenderItem* AptRenderItem::Manager_GetNextSibling() const  { return mpManagerNextSibling; }
AptRenderItem* AptRenderItem::Manager_GetNextRevision() const { return mpManagerNextRevision; }

// IsWritableForThisTick @0x7DEF54 -- already the writable revision for nTick?
bool AptRenderItem::IsWritableForThisTick(int nTick) const
{
    return mCreatedOnTick == nTick || (mFlags & 0x40u) != 0;
}

// IsRenderableForThisTick @0x82AD4FC8 -- drawable on nTick: aged in (created on or
// before nTick) AND the committed (non-writable, mFlags bit 25 clear) revision. The
// in-progress writable copy and future-tick items are never drawn.
bool AptRenderItem::IsRenderableForThisTick(int nTick) const
{
    if ((nTick - mCreatedOnTick) < 0)
        return false;
    return (mFlags & 0x40u) == 0;
}

// Manager_IsDeletionMark @0x7DEEBC -- mFlags bit 28.
bool AptRenderItem::Manager_IsDeletionMark() const { return (mFlags & 0x8u) != 0; }

// Manager_SetNextRevision @0x7DEF00
void AptRenderItem::Manager_SetNextRevision(AptRenderItem* pNext) { mpManagerNextRevision = pNext; }

// Manager_SetDeletionMark @0x82ADB138 (PS3 @0x7E48DC) -- mark/unmark this revision
// for deletion (x64 mFlags bit 3; console bit 28). When MARKING, first release the
// whole revision link set in the console's store order -- first-child (+0x30),
// next-sibling (+0x2C), next-revision (+0x28), mask (+0x1C) -- nulling each slot
// before its ReleaseReference.
void AptRenderItem::Manager_SetDeletionMark(bool bMark)
{
    if (bMark)
    {
        if (mpManagerFirstChild)
        {
            AptRenderItem* pOld = mpManagerFirstChild;
            mpManagerFirstChild = nullptr;
            pOld->ReleaseReference();
        }
        if (mpManagerNextSibling)
        {
            AptRenderItem* pOld = mpManagerNextSibling;
            mpManagerNextSibling = nullptr;
            pOld->ReleaseReference();
        }
        if (mpManagerNextRevision)
        {
            AptRenderItem* pOld = mpManagerNextRevision;
            mpManagerNextRevision = nullptr;
            pOld->ReleaseReference();
        }
        if (mpMask)
        {
            AptRenderItem* pOld = mpMask;
            mpMask = nullptr;
            pOld->ReleaseReference();
        }
    }
    // console: v2[6] = (bMark << 28) & 0x10000000 | (v2[6] & 0xEFFFFFFF) -> x64 bit 3.
    mFlags = (bMark ? 0x8u : 0u) | (mFlags & ~0x8u);
}

// ===========================================================================
// Render-tree double-buffering (DECOMPILED from the X360 ARTIST.XEX -- the full
// revision-tree mutators the AptRenderTreeManager facade drives), decompiled
// faithfully after the earlier bring-up deferral.
//
// x64 fork (PC-platform threading): the console mutates mRefCount with the lwarx/stwcx. atomic and
// brackets the revision-link swaps with the unk_8324E7CC interrupt-masked spin
// lock; both are modelled host-portably (AddReference/ReleaseReference =
// _Interlocked*; the lock = _InterlockedExchange). Behaviour is identical on the
// single-threaded path.
// ===========================================================================

// PropagateTreeIsVisible @0x82ADA8B8 -- recompute the mask-driven visibility state
// (mFlags bits 24..26) down this item's mask -> first-child -> sibling subtree.
//   nVisibleMode 1 = becoming hidden by an enclosing mask, 0 = becoming shown.
// Recursive (the console recurses through the mask, every first-child's latest
// revision, and the masks of hasMask children).
AptRenderItem* AptRenderItem::PropagateTreeIsVisible(int nVisibleMode)
{
    AptRenderItem* result = this;

    // ---- the mask sub-item (mpMask, [c:0x1C]) -----------------------------
    AptRenderItem* pMask = mpMask;                 // [c:0x1C]
    AptRenderItem* pChild = mpManagerFirstChild;   // [c:0x30]

    if (pMask)
    {
        pMask = LatestRevision(pMask);             // chase [c:0x28] next-revision
        // bit26 (0x04000000) := (nVisibleMode == 0); clears bit25-adjacent slot.
        uint32_t v5 = ((nVisibleMode == 0) ? 0x20u : 0u) | (pMask->mFlags & ~0x20u);
        pMask->mFlags = v5;
        if (nVisibleMode)
            pMask->mFlags = v5 & ~0x40u;       // also clear bit25 (0x02000000)
        result = pMask->PropagateTreeIsVisible(nVisibleMode);
    }

    // ---- the first-child + its next-sibling chain (each latest revision) ---
    if (pChild)
    {
        for (;;)
        {
            pChild = LatestRevision(pChild);        // chase [c:0x28]
            uint32_t v6 = pChild->mFlags;           // [c:0x18]

            if ((v6 & 0x2u) == 0)                   // not an isMask node (x64 bit 1; sub_14083DD80 `v8 & 2`)
            {
                uint32_t v7 = v6 & 0x1u;            // isVisible bit (x64 bit 0; sub_14083DD80 `v8 & 1`)
                if (nVisibleMode == 1)
                {
                    if (v7 == 1)
                    {
                        bool bTreeVis = ((v6 & 0x20u) == 0x20u) ||
                                        ((v6 & 0x40u) == 0x40u);
                        if (bTreeVis)
                        {
                            uint32_t v9 = v6 & ~0x60u;  // clear bits 25+26
                            pChild->mFlags = v9;
                            result = pChild->PropagateTreeIsVisible(1);
                        }
                    }
                    else
                    {
                        pChild->mFlags = (v6 & ~0xE0u) | 0x80u; // set bit24
                    }
                }
                else
                {
                    if (v7 == 1)
                    {
                        bool bTreeVis = ((v6 & 0x20u) == 0x20u) ||
                                        ((v6 & 0x40u) == 0x40u);
                        if (!bTreeVis)
                        {
                            uint32_t v9 = v6 | 0x20u;  // set bit26
                            pChild->mFlags = v9;
                            result = pChild->PropagateTreeIsVisible(0);
                        }
                    }
                    else
                    {
                        pChild->mFlags = (v6 & ~0xA0u) | 0x20u; // set bit26, clear bit25
                    }
                }

                // If this child has its own mask (bit29) bound, recurse into it.
                bool bHasOwnMask = ((pChild->mFlags & 0x4u) != 0) && (pChild->mpMask != nullptr);
                if (bHasOwnMask)
                {
                    AptRenderItem* pi = LatestRevision(pChild->mpMask);  // [c:0x1C] -> [c:0x28]
                    uint32_t v15 = ((nVisibleMode == 0) ? 0x20u : 0u) | (pi->mFlags & ~0x20u);
                    pi->mFlags = v15;
                    if (nVisibleMode)
                        pi->mFlags = v15 & ~0x40u;
                    result = pi->PropagateTreeIsVisible(nVisibleMode);
                }
            }

            // advance to the next sibling's latest revision (loop's [c:0x2C]).
            pChild = pChild->mpManagerNextSibling;   // [c:0x2C]
            if (!pChild)
                break;
        }
    }

    return result;
}

// Manager_CloneNewItem @0x82ADAAE8 -- under the render-tree lock, virtual-Clone
// THIS item into a fresh non-extended revision, install it as our next revision
// ([c:0x28]), take its reference, and return it.
AptRenderItem* AptRenderItem::Manager_CloneNewItem(int nTick)
{
    AptRenderTreeLock_Acquire();
    AptRenderItem* pNew = Clone(nTick, false);   // (**a1)(a1, a2, 0)
    mpManagerNextRevision = pNew;                // [c:0x28]
    if (pNew)
        pNew->AddReference();                    // atomic ++mRefCount [c:0x24]
    AptRenderTreeLock_Release();
    return pNew;
}

// Manager_CreateNewRevision @0x82ADABA0 -- identical to Manager_CloneNewItem but
// the virtual Clone copies the EXTENDED state (mask / depth / manager links).
AptRenderItem* AptRenderItem::Manager_CreateNewRevision(int nTick)
{
    AptRenderTreeLock_Acquire();
    AptRenderItem* pNew = Clone(nTick, true);    // (**a1)(a1, a2, 1)
    mpManagerNextRevision = pNew;                // [c:0x28]
    if (pNew)
        pNew->AddReference();                    // atomic ++mRefCount [c:0x24]
    AptRenderTreeLock_Release();
    return pNew;
}

// Manager_GetRenderRevision @0x82ADAC58 -- walk the next-revision chain ([c:0x28])
// and return the newest revision that is renderable for nTick (aged in + committed,
// mFlags bit25 clear). Stops at the first non-renderable revision.
AptRenderItem* AptRenderItem::Manager_GetRenderRevision(int nTick)
{
    AptRenderItem* result = this;
    if (!result)
        return result;
    for (AptRenderItem* i = result->mpManagerNextRevision; i; i = i->mpManagerNextRevision) // [c:0x28]
    {
        char bRenderable;
        if ((nTick - i->mCreatedOnTick) < 0 || (bRenderable = 1, (i->mFlags & 0x40u) != 0)) // [c:0x20]/[c:0x18]
            bRenderable = 0;
        if (!bRenderable)
            break;
        result = i;
    }
    return result;
}

// Manager_UpdateNextSibling @0x82ADACA8 -- commit pRevision as THIS item's next
// sibling ([c:0x2C]) for the current render walk, under the lock. A deletion-marked
// revision (bit28) collapses to null; the old/new links are reference-counted.
void AptRenderItem::Manager_UpdateNextSibling(AptRenderItem* pRevision)
{
    AptRenderItem* v2 = pRevision;
    if (pRevision && (pRevision->mFlags & 0x8u) != 0)  // [c:0x18] deletion mark
        v2 = nullptr;
    if (v2 != mpManagerNextSibling)                           // [c:0x2C]
    {
        AptRenderTreeLock_Acquire();
        if (v2)
            v2->AddReference();                               // atomic ++mRefCount
        if (mpManagerNextSibling)
        {
            AptRenderItem* pOld = mpManagerNextSibling;
            mpManagerNextSibling = nullptr;
            pOld->ReleaseReference();
        }
        mpManagerNextSibling = v2;
        AptRenderTreeLock_Release();
    }
}

// Manager_UpdateMask @0x82ADB1B8 -- commit pRevision as THIS item's mask link
// ([c:0x1C]) for the current render walk (same deletion-mark collapse + ref-count
// + lock as Manager_UpdateNextSibling).
void AptRenderItem::Manager_UpdateMask(AptRenderItem* pRevision)
{
    AptRenderItem* v2 = pRevision;
    if (pRevision && (pRevision->mFlags & 0x8u) != 0)  // [c:0x18] deletion mark
        v2 = nullptr;
    if (v2 != mpMask)                                         // [c:0x1C]
    {
        AptRenderTreeLock_Acquire();
        if (v2)
            v2->AddReference();
        if (mpMask)
        {
            AptRenderItem* pOld = mpMask;
            mpMask = nullptr;
            pOld->ReleaseReference();
        }
        mpMask = v2;
        AptRenderTreeLock_Release();
    }
}

// Manager_UpdateFirstChild -- the first-child counterpart of the two above
// ([c:0x30]); the AptRenderTreeManager render walk and the custom-control Render
// (@0x82AEF8F8, its verified X360 caller) drive it. The dedicated X360 leaf has no
// per-address export; reconstructed identically to its committed
// Update_NextSibling/Mask instruction-twin siblings (deletion-mark collapse +
// ref-counted link swap under the lock).
void AptRenderItem::Manager_UpdateFirstChild(AptRenderItem* pRevision)
{
    AptRenderItem* v2 = pRevision;
    if (pRevision && (pRevision->mFlags & 0x8u) != 0)  // [c:0x18] deletion mark
        v2 = nullptr;
    if (v2 != mpManagerFirstChild)                           // [c:0x30]
    {
        AptRenderTreeLock_Acquire();
        if (v2)
            v2->AddReference();
        if (mpManagerFirstChild)
        {
            AptRenderItem* pOld = mpManagerFirstChild;
            mpManagerFirstChild = nullptr;
            pOld->ReleaseReference();
        }
        mpManagerFirstChild = v2;
        AptRenderTreeLock_Release();
    }
}

// Manager_SetFirstChild @0x82ADAF58 -- set THIS item's writable-revision first-child
// link ([c:0x30]): reference the new child, recompute its mask-driven visibility
// against this parent's flags (PropagateTreeIsVisible), then swap the link under the
// lock (releasing the old child). The visibility recompute is the console's exact
// bit arithmetic on the child's mFlags ([c:0x18]).
AptRenderItem* AptRenderItem::Manager_SetFirstChild(AptRenderItem* pChild)
{
    AptRenderItem* result = nullptr;

    if (pChild)
    {
        pChild->AddReference();                          // atomic ++mRefCount [c:0x24]
        uint32_t v11 = pChild->mFlags;                   // [c:0x18]
        if ((v11 & 0x1u) != 0)                           // child isVisible (x64 bit 0: sub_14083C4A0 `flags & 1`)
        {
            uint32_t v15 = v11 & ~0x80u;            // clear bit24
            pChild->mFlags = v15;
            uint32_t v16 = mFlags;                       // parent [c:0x18]
            // x64 sub_14083C4A0 / X360 0x82ADAF58: the set-0x20 + Propagate(0) arm runs when
            // the parent is INVISIBLE (bit 0 clear) OR its tree-state bits (0x60) are set.
            bool bParentVisible = ((v16 & 0x1u) == 0) ||
                                  ((v16 & 0x20u) == 0x20u) ||
                                  ((v16 & 0x40u) == 0x40u);

            if (bParentVisible)
            {
                bool bChildTreeVis = ((v15 & 0x20u) == 0x20u) ||
                                     ((v15 & 0x40u) == 0x40u);
                if (!bChildTreeVis)
                {
                    pChild->mFlags = v15 | 0x20u;  // set bit26
                    pChild->PropagateTreeIsVisible(0);
                }
            }
            else
            {
                bool bChildTreeVis = ((v15 & 0x20u) == 0x20u) ||
                                     ((v15 & 0x40u) == 0x40u);
                if (bChildTreeVis)
                {
                    pChild->mFlags = v15 & ~0x60u;  // clear bits 25+26
                    pChild->PropagateTreeIsVisible(1);
                }
            }
        }
        else
        {
            // child invisible: stamp its tree-invisible state from the parent's flags.
            uint32_t v12 = mFlags;                       // parent [c:0x18]
            bool bParentTreeVis = ((v12 & 0x20u) == 0x20u) ||
                                  ((v12 & 0x40u) == 0x40u);
            uint32_t v14;
            if (bParentTreeVis || (v12 & 0x80u) != 0)
                v14 = (v11 & ~0xA0u) | 0x20u; // set bit26, clear bit25
            else
                v14 = (v11 & ~0xE0u) | 0x80u; // set bit24, clear bits 25+26
            pChild->mFlags = v14;
        }
    }

    AptRenderTreeLock_Acquire();
    result = mpManagerFirstChild;                        // [c:0x30]
    if (result)
    {
        mpManagerFirstChild = nullptr;
        result->ReleaseReference();
    }
    mpManagerFirstChild = pChild;
    AptRenderTreeLock_Release();
    return result;
}

// Manager_SetNextSibling @0x82ADAD90 -- the next-sibling counterpart of
// Manager_SetFirstChild ([c:0x2C]); the same reference + visibility recompute +
// locked link swap, with the visible/invisible branches mirrored as in the console.
AptRenderItem* AptRenderItem::Manager_SetNextSibling(AptRenderItem* pSibling)
{
    AptRenderItem* result = nullptr;

    if (pSibling)
    {
        pSibling->AddReference();                        // atomic ++mRefCount [c:0x24]
        uint32_t v11 = pSibling->mFlags;                 // [c:0x18]
        if ((v11 & 0x1u) != 0)                           // sibling isVisible (x64 bit 0: sub_14083C590 `flags & 1`)
        {
            uint32_t v16 = v11 & ~0x80u;            // clear bit24
            pSibling->mFlags = v16;
            uint32_t v17 = mFlags;                       // this [c:0x18]
            bool bSelfVisible = ((v17 & 0x20u) == 0x20u) ||
                                ((v17 & 0x40u) == 0x40u);
            uint32_t v20 = v16 & 0x20u;
            if (bSelfVisible)   // self 0x60 bits set (x64 sub_14083C590: `(flags & 0x60) != 0` arm)
            {
                bool bSibTreeVis = (v20 == 0x20u) ||
                                   ((v16 & 0x40u) == 0x40u);
                if (!bSibTreeVis)
                {
                    pSibling->mFlags = v16 | 0x20u; // set bit26
                    pSibling->PropagateTreeIsVisible(0);
                }
            }
            else                // self 0x60 bits clear
            {
                bool bSibTreeVis = (v20 == 0x20u) ||
                                   ((v16 & 0x40u) == 0x40u);
                if (bSibTreeVis)
                {
                    pSibling->mFlags = v16 & ~0x60u; // clear bits 25+26
                    pSibling->PropagateTreeIsVisible(1);
                }
            }
        }
        else
        {
            uint32_t v12 = mFlags;                       // this [c:0x18]
            bool bSelfTreeVis = ((v12 & 0x20u) == 0x20u) ||
                                ((v12 & 0x40u) == 0x40u);
            uint32_t v15;
            if (bSelfTreeVis)
                v15 = (v11 & ~0xA0u) | 0x20u; // set bit26, clear bit25
            else
                v15 = (v11 & ~0xE0u) | 0x80u; // set bit24, clear bits 25+26
            pSibling->mFlags = v15;
        }
    }

    AptRenderTreeLock_Acquire();
    result = mpManagerNextSibling;                       // [c:0x2C]
    if (result)
    {
        mpManagerNextSibling = nullptr;
        result->ReleaseReference();
    }
    mpManagerNextSibling = pSibling;
    AptRenderTreeLock_Release();
    return result;
}

// SetIsVisible @0x82AE0708 -- set the is-visible flag (bit31). When the bit changes
// and the node is not masked, recompute the subtree's mask-driven visibility.
AptRenderItem* AptRenderItem::SetIsVisible(bool bVisible)
{
    AptRenderItem* result = this;
    uint32_t v4 = mFlags;                                // [c:0x18]
    if ((unsigned)bVisible != (v4 & 0x1u))               // bit changed (x64 bit 0: sub_140841B00 `a2 != (flags & 1)`)
    {
        bool bSkipPropagate = false;
        int v6 = 0;
        if (bVisible)
        {
            if ((v4 & 0x80u) == 0)                 // bit24 (tree-invisible-by-parent) clear
            {
                bSkipPropagate = true;
            }
            else
            {
                uint32_t v5 = v4 & ~0x80u;          // clear bit24
                mFlags = v5;
                if ((v5 & 0x2u) != 0)                    // isMask (x64 bit 1) -> no propagate
                    bSkipPropagate = true;
                else
                    v6 = 1;
            }
        }
        else
        {
            bool bTreeVis = ((v4 & 0x20u) == 0x20u) ||
                            ((v4 & 0x40u) == 0x40u);
            if (bTreeVis)
            {
                bSkipPropagate = true;
            }
            else
            {
                uint32_t v8 = (v4 & ~0xE0u) | 0x80u;  // set bit24, clear 25+26
                mFlags = v8;
                if ((v8 & 0x2u) != 0)                    // isMask (x64 bit 1)
                    bSkipPropagate = true;
                else
                    v6 = 0;
            }
        }
        if (!bSkipPropagate)
            result = PropagateTreeIsVisible(v6);

        // bit31 := bVisible (console cntlzw idiom).
        mFlags = (bVisible ? 0x1u : 0u) | (mFlags & ~0x1u);
    }
    return result;
}

// SetHasMask @0x82ADB388 -- set the has-mask flag (bit29) from bHasMask, (re)bind
// pMask as the mask link ([c:0x1C]) under the lock with the visibility recompute,
// then clear bit29 when no mask remains.
AptRenderItem* AptRenderItem::SetHasMask(bool bHasMask, AptRenderItem* pMask)
{
    AptRenderItem* result = this;
    AptRenderItem* v3 = this;
    AptRenderItem* v5 = mpMask;                          // [c:0x1C]

    // bit29 := bHasMask.
    mFlags = ((bHasMask ? 0x4u : 0u)) | (mFlags & ~0x4u);

    if (v5 != pMask)
    {
        if (pMask)
        {
            pMask->AddReference();                       // atomic ++mRefCount
            uint32_t v11 = mFlags;                       // [c:0x18]
            int v14;
            uint32_t v15;
            bool bTreeVis = ((v11 & 0x20u) == 0x20u) ||
                            ((v11 & 0x40u) == 0x40u);
            if ((v11 & 0x1u) == 0 || bTreeVis)           // self invisible (x64 bit 0), or tree-hidden
            {
                v14 = 0;
                v15 = (pMask->mFlags & ~0xA0u) | 0x20u; // set bit26, clear bit25
            }
            else
            {
                v14 = 1;
                v15 = pMask->mFlags & ~0xE0u;       // clear bits 24..26
            }
            pMask->mFlags = v15;
            pMask->PropagateTreeIsVisible(v14);
        }

        AptRenderTreeLock_Acquire();
        result = v3->mpMask;                             // [c:0x1C]
        if (result)
        {
            v3->mpMask = nullptr;
            result->ReleaseReference();
        }
        v3->mpMask = pMask;
        AptRenderTreeLock_Release();
    }

    if (!v3->mpMask)
        v3->mFlags &= ~0x4u;                      // no mask -> clear bit29
    return result;
}

// SetIsMask @0x82AEBF28 -- set the is-mask flag (bit30) + (re)bind the mask matrix
// ([c:0x10]): when becoming a mask, copy the matrix in (SetMaskMatrix); when
// clearing, free the existing mask matrix back to the pool.
AptRenderItem* AptRenderItem::SetIsMask(bool bIsMask, const AptMatrix* pMaskMatrix)
{
    AptRenderItem* result = this;
    AptRenderItem* v3 = this;
    unsigned v4 = bIsMask ? 1u : 0u;

    // change iff the bit30 state differs, or the mask matrix pointer differs.
    if (((unsigned)bIsMask != ((mFlags & 0x2u) ? 1u : 0u)) ||
        (mpMaskPositionMatrix != pMaskMatrix))           // [c:0x10]
    {
        if (bIsMask)
        {
            SetMaskMatrix(pMaskMatrix);
        }
        else
        {
            if (mpMaskPositionMatrix)                     // [c:0x10]
                gpNonGCPoolManager->Deallocate(mpMaskPositionMatrix, sizeof(AptMatrix));
            v3->mpMaskPositionMatrix = nullptr;
        }
        // bit30 := bIsMask.
        v3->mFlags = ((v4 != 0) ? 0x2u : 0u) | (v3->mFlags & ~0x2u);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Render-traversal helpers (PS3 External PushMatrices @0x7F21E4 / PopMatrices
// @0x7ECA68). Push: save + concat the item's colour transform and position
// matrix onto the render context; Pop: restore both. The subtypes bracket their
// geometry draw with these.
//
// RESOLVED (2026-08-07, the _data_opti_flags_word dump): the console PushMatrices
// @0x82AEF150 gates on dword_82F73008 & 4 -> drawCharacterInstOpti @0x82ADFA90 (a
// clip-stack fast path; the PopMatrices twin just decrements the clip index). The
// gate word's SHIPPED .data value is 0x2 (the neighbouring dumped words are the
// "FSCommand:" string pointer + its 0x17/0x1A literal lengths, anchoring the read)
// and no writer exists in the export set, so bit2 is clear for the whole run: the
// opti fast path is DEAD on the shipped ARTIST build, and the standard push/append
// path below IS the shipped behaviour (not a de-opt). Gate constant pinned below.
// (Identity-singleton note: the item's null-matrix sentinels here are
// gIdentityMatrix/gIdentityCXForm, distinct from the context's gAptIdentityMatrix/
// gAptNullCXForm -- the concat still yields the correct result, just without the
// pointer-identity short-circuit; the two singleton sets should be unified.)
// ---------------------------------------------------------------------------

// dword_82F73008 -- the Apt render opti-flags config word, pinned at its shipped
// .data value (the _data_opti_flags_word dump; bit2 == the dead
// drawCharacterInstOpti gate -- see the note above).
static const unsigned int KU_AptOptiFlagsWord = 0x2u;   // dword_82F73008 (shipped)

void PushMatrices(AptRenderingContext* pCtx, const AptRenderItem* pItem)
{
    pCtx->pushColourTransform();
    pCtx->appendColourTransform(const_cast<AptCXForm*>(pItem->GetColorMatrixConst()));
    pCtx->pushVertexMatrix();
    pCtx->appendVertexMatrix(const_cast<AptMatrix*>(pItem->GetPositionMatrixConst()));
}

void PopMatrices(AptRenderingContext* pCtx, const AptRenderItem* /*pItem*/)
{
    pCtx->popColourTransform();
    pCtx->popVertexMatrix();
}

// PushMatricesAbsolute (PS3 External @0x7F28B8) -- the absolute (world-space) push
// variant. Like PushMatrices it pushes + appends the item's colour transform and
// saves the current vertex matrix, but then RESETS the current vertex matrix to the
// world-space identity (setVertexMatrix(&gIdentityMatrix)) and appends the item's
// MASK position matrix (GetMaskPositionMatrixConst, console *(a2+4)) -- so the item
// draws at an absolute screen transform independent of its parents' matrices. Used
// by PushRenderDataAbsolute (sprite/button/etc.).
//
// RESOLVED (2026-08-07): the console fast-paths dword_82F73008&4 -> _drawCharacterInstAbsoluteOpti
// @0x82AE00A8 -- dead on the shipped build (the gate word ships as 0x2, bit2 clear;
// see KU_AptOptiFlagsWord above), so the standard push/append path IS the shipped
// behaviour (matching PushMatrices, whose opti fast-path is likewise dead config).
void PushMatricesAbsolute(AptRenderingContext* pCtx, const AptRenderItem* pItem)
{
    pCtx->pushColourTransform();
    pCtx->appendColourTransform(const_cast<AptCXForm*>(pItem->GetColorMatrixConst()));
    pCtx->pushVertexMatrix();
    pCtx->setVertexMatrix(&gIdentityMatrix);
    pCtx->appendVertexMatrix(const_cast<AptMatrix*>(pItem->GetMaskPositionMatrixConst()));
}

// ---------------------------------------------------------------------------
// RETIRED (2026-07-06): the AptRenderItem_GetDepth/SetDepth/CopyVisualFrom and
// AptCharacter_render free-function shims. The X360 inlines these as raw field
// reads/writes + a vtbl-slot dispatch, so the callers now invoke the named members
// directly (AptLinker.cpp: GetDepth()/SetDepth()/CopyRenderDataFrom(); the render-
// item subtypes AptRenderItemShape/Morph::Render: pCharacter->render()) -- no
// free-function stand-in remains.
// ---------------------------------------------------------------------------
