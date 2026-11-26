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

#include <xtl.h>   // _InterlockedIncrement/Decrement/_InterlockedExchange
#include <new>        // placement new (lazy colour-matrix construct)

int AptRenderItem::sItemsAllocated = 0;

// ---------------------------------------------------------------------------
// Render-tree revision lock (X360 unk_8324E7CC) + the global render-item
// shutdown latch (X360 byte_8324E56C). The base render-item TU is their home;
// the subtype TUs (e.g. AptRenderItemCustomControl) declare them extern.
//
volatile long gAptRenderTreeRevisionLock = 0;   // X360 unk_8324E7CC
bool          gbRenderItemShuttingDown   = false; // X360 byte_8324E56C

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
    : mpCharacter(pCharacter)
    , mpMaskPositionMatrix(0)
    , mDepth(-1)
    , mClipDepth(-1)
    , mbIsVisible(1)
    , mbIsMask(0)
    , mbHasMask(0)
    , mbIsDeletionMark(0)
    , mbCreatedDynamic(0)
    , mbPendingHiddenByTree(0)
    , mbIsHiddenByTree(0)
    , mbRootNonVisibleItem(0)
    , meCharacterType(0)
    , mpMask(0)
    , mCreatedOnTick(nCreatedOnTick)
    , mRefCount(0)
    , mpManagerNextRevision(0)
    , mpManagerNextSibling(0)
    , mpManagerFirstChild(0)
{
    mpPositionMatrix = 0;
    mpColorMatrix = 0;
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
    mpPositionMatrix      = 0;
    mpColorMatrix         = 0;
    mpMaskPositionMatrix  = 0;
    mDepth                = -1;
    mClipDepth            = -1;
    mpMask                = 0;
    mCreatedOnTick        = nCreatedOnTick;
    mRefCount             = 0;
    mpManagerNextRevision = 0;
    mpManagerNextSibling  = 0;
    mpManagerFirstChild   = 0;

    // isVisible (bit31) set; render-type bits (18..23) cleared for the subtype to
    // stamp; bit27 carried over from the source.
    mFlags = 0x80000000u | (pSource->mFlags & 0x08000000u);

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
        mpColorMatrix = new AptCXForm(pSource->mpColorMatrix);   // pool class operator new
    }

    // Extended copy: mask, depth, manager links + the source's state-flag bits.
    if (bCopyExtended)
    {
        mDepth     = pSource->mDepth;
        mClipDepth = pSource->mClipDepth;
        mFlags = (mFlags & ~0x80000000u) | (pSource->mFlags & 0x80000000u);   // isVisible (bit31)
        mFlags = (mFlags & ~0x40000000u) | (pSource->mFlags & 0x40000000u);   // isMask   (bit30)
        mFlags = (mFlags & ~0x20000000u) | (pSource->mFlags & 0x20000000u);   // hasMask  (bit29)
        SetMaskMatrix(pSource->mpMaskPositionMatrix);
        mFlags = (mFlags & ~0x10000000u) | (pSource->mFlags & 0x10000000u);   // deletion mark (bit28)

        mpMask                = LatestRevision(pSource->mpMask);
        mpManagerNextRevision = 0;
        mpManagerNextSibling  = LatestRevision(pSource->mpManagerNextSibling);
        mpManagerFirstChild   = LatestRevision(pSource->mpManagerFirstChild);

        if (mpManagerNextSibling) mpManagerNextSibling->AddReference();
        if (mpManagerFirstChild)  mpManagerFirstChild->AddReference();
        if (mpMask)               mpMask->AddReference();

        if (pSource->mFlags & 0x04000000u)
            mFlags |= 0x02000000u;                                          // writable-revision (bit25) from source bit26
        mFlags = (mFlags & ~0x01000000u) | (pSource->mFlags & 0x01000000u);  // bit24
    }

    if (mpCharacter)
        mpCharacter->AddCharacterReference();
    ++sItemsAllocated;
}

// SetMaskMatrix @0x82AE52F8 -- lazily (de)allocate + copy the mask position matrix
// (null clears + frees it).
// ARTIST tests with the record forms (0x82AE5310 `cmplwi r4,0`); the new matrix is
// value-initialised (memset 0x82AE5344) before the copy.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
void AptRenderItem::SetMaskMatrix(const AptMatrix* pMatrix)
{
    if (!mpMaskPositionMatrix && pMatrix)
    {
        mpMaskPositionMatrix = new AptMatrix();
        mpMaskPositionMatrix->AptMatrixCopy(pMatrix);
    }
    else if (mpMaskPositionMatrix && pMatrix)
    {
        mpMaskPositionMatrix->AptMatrixCopy(pMatrix);
    }
    else if (mpMaskPositionMatrix && !pMatrix)
    {
        delete mpMaskPositionMatrix;
        mpMaskPositionMatrix = 0;
    }
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

// CopyRenderDataFrom @0x82AE5400 -- transfer pSource's visual state onto this item.
// ARTIST tests with the record forms (0x82AE5468 `cmplwi r11,0`).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
void AptRenderItem::CopyRenderDataFrom(const AptRenderItem* pSource)
{
    if (pSource->mpPositionMatrix)
    {
        if (!mpPositionMatrix)
            mpPositionMatrix = new AptMatrix();
        mpPositionMatrix->AptMatrixCopy(pSource->mpPositionMatrix);
    }
    else if (mpPositionMatrix)
    {
        mpPositionMatrix->AptMatrixCopy(&gAptIdentityMatrix);   // X360 flt_8324E2B0
    }

    if (pSource->mpColorMatrix)
    {
        if (!mpColorMatrix)
            mpColorMatrix = new AptCXForm(pSource->mpColorMatrix);
    }
    else if (mpColorMatrix)
    {
        mpColorMatrix->AptCXFormCopy(&gAptNullCXForm);   // X360 off_82F73388
    }

    mClipDepth = pSource->mClipDepth;
    mbIsVisible = pSource->mbIsVisible;
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

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
            pRev->mpManagerNextRevision = 0;
            delete pRev;                                          // (*vtbl)(link, 1)
            mpManagerNextRevision = pNext;
        }
        if (mpManagerNextRevision)
        {
            AptRenderItem* pOld = mpManagerNextRevision;
            mpManagerNextRevision = 0;
            pOld->ReleaseReference();
        }

        while (AptRenderItem* pSib = mpManagerNextSibling)
        {
            if (pSib->mRefCount > 1)
                break;
            AptRenderItem* pNext = pSib->mpManagerNextSibling;    // the link's own [c:0x2C]
            pSib->mpManagerNextSibling = 0;
            delete pSib;
            mpManagerNextSibling = pNext;
        }
        if (mpManagerNextSibling)
        {
            AptRenderItem* pOld = mpManagerNextSibling;
            mpManagerNextSibling = 0;
            pOld->ReleaseReference();
        }

        while (AptRenderItem* pChild = mpManagerFirstChild)
        {
            if (pChild->mRefCount > 1)
                break;
            AptRenderItem* pNext = pChild->mpManagerFirstChild;   // the link's own [c:0x30]
            pChild->mpManagerFirstChild = 0;
            delete pChild;
            mpManagerFirstChild = pNext;
        }
        if (mpManagerFirstChild)
        {
            AptRenderItem* pOld = mpManagerFirstChild;
            mpManagerFirstChild = 0;
            pOld->ReleaseReference();
        }
    }

    if (mpCharacter)
    {
        mpCharacter->ReleaseCharacterReference();
        mpCharacter = 0;
    }
    if (mpMask)
    {
        AptRenderItem* pMask = mpMask;
        mpMask = 0;
        pMask->ReleaseReference();
    }
    if (mpMaskPositionMatrix)
    {
        gpNonGCPoolManager->Deallocate(mpMaskPositionMatrix, sizeof(AptMatrix));
        mpMaskPositionMatrix = 0;
    }
    if (mpPositionMatrix)
    {
        gpNonGCPoolManager->Deallocate(mpPositionMatrix, sizeof(AptMatrix));
        mpPositionMatrix = 0;
    }
    if (mpColorMatrix)
    {
        mpColorMatrix->~AptCXForm();   // resets the AptColorHelper vptrs (console inlined this)
        gpNonGCPoolManager->Deallocate(mpColorMatrix, sizeof(AptCXForm));
        mpColorMatrix = 0;
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

// ARTIST: value-initialised matrix (memset 0x82AE5228) seeded with identity; record-form test
// 0x82AE51FC.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
AptMatrix* AptRenderItem::GetPositionMatrixWritable()
{
    if (mpPositionMatrix)
        return mpPositionMatrix;
    mpPositionMatrix = new AptMatrix();
    mpPositionMatrix->AptMatrixCopy(&gAptIdentityMatrix);   // X360 flt_8324E2B0
    return mpPositionMatrix;
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

// ARTIST: pool new-expression with a null test before the copy constructor (0x82AE52BC..D8).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
AptCXForm* AptRenderItem::GetColorMatrixWritable()
{
    if (mpColorMatrix)
        return mpColorMatrix;
    mpColorMatrix = new AptCXForm(&gIdentityCXForm);   // X360 off_82F73388
    return mpColorMatrix;
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

// ---- depth / clip depth ---------------------------------------------------

// ---- flags ----------------------------------------------------------------
bool AptRenderItem::GetIsVisible() const { return (mFlags & 0x80000000u) != 0; }
bool AptRenderItem::GetIsMask() const    { return (mFlags & 0x40000000u) != 0; }
bool AptRenderItem::GetHasMask() const   { return static_cast<bool>(mbHasMask) && mpMask != 0; }   // ARTIST 0x82AD4F54 extracts the bit (rlwinm. 3,31,31): the bitfield read as bool
AptRenderItem* AptRenderItem::GetMask() const { return mpMask; }

// ---- character ------------------------------------------------------------
const AptCharacter* AptRenderItem::GetCharacterConst() const { return mpCharacter; }
AptCharacter*       AptRenderItem::GetCharacterWritable() const { return mpCharacter; }

// ARTIST uses the record-form zero-reference test at 0x82AE0854.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
void AptRenderItem::SetCharacter(AptCharacter* pCharacter)
{
    pCharacter->AddCharacterReference();
    mpCharacter->ReleaseCharacterReference();
    mpCharacter = pCharacter;
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

int AptRenderItem::GetCreatedOnTick() const { return mCreatedOnTick; }
int AptRenderItem::GetRefCount() const      { return mRefCount; }

// ---- reference count (atomic) --------------------------------------------


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

// IsRenderableForThisTick @0x82AD4FC8 -- drawable on nTick: aged in (created on or
// before nTick) AND the committed (non-writable, mFlags bit 25 clear) revision. The
// in-progress writable copy and future-tick items are never drawn.
// ARTIST returns through `li r3,1` / `beqlr` with record-form tests (0x82AD4FCC `subf.`,
// 0x82AD4FDC `rlwinm.`).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
bool AptRenderItem::IsRenderableForThisTick(int nTick) const
{
    if ((nTick - mCreatedOnTick) >= 0 && !mbIsHiddenByTree)
        return true;
    return false;
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

// Manager_IsDeletionMark @0x7DEEBC -- mFlags bit 28.
bool AptRenderItem::Manager_IsDeletionMark() const { return (mFlags & 0x10000000u) != 0; }

// Manager_SetNextRevision @0x7DEF00
void AptRenderItem::Manager_SetNextRevision(AptRenderItem* pNext) { mpManagerNextRevision = pNext; }

// Manager_SetDeletionMark @0x82ADB138 (PS3 @0x7E48DC) -- mark/unmark this revision
// for deletion (x64 mFlags bit 3; console bit 28). When MARKING, first release the
// whole revision link set in the console's store order -- first-child (+0x30),
// next-sibling (+0x2C), next-revision (+0x28), mask (+0x1C) -- nulling each slot
// before its ReleaseReference.
// ARTIST tests with the record forms (0x82ADB148 `clrlwi.`, 0x82ADB158 `cmplwi r3,0`).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
void AptRenderItem::Manager_SetDeletionMark(bool bMark)
{
    if (bMark)
    {
        if (mpManagerFirstChild)
        {
            AptRenderItem* pOld = mpManagerFirstChild;
            mpManagerFirstChild = 0;
            pOld->ReleaseReference();
        }
        if (mpManagerNextSibling)
        {
            AptRenderItem* pOld = mpManagerNextSibling;
            mpManagerNextSibling = 0;
            pOld->ReleaseReference();
        }
        if (mpManagerNextRevision)
        {
            AptRenderItem* pOld = mpManagerNextRevision;
            mpManagerNextRevision = 0;
            pOld->ReleaseReference();
        }
        if (mpMask)
        {
            AptRenderItem* pOld = mpMask;
            mpMask = 0;
            pOld->ReleaseReference();
        }
    }
    mbIsDeletionMark = bMark;   // rlwimi of the bool itself, 0x82ADB1A8
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

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
// SetIsHiddenByTree -- latch the pending hidden-by-tree flag; showing also clears the applied
// one. ARTIST has no out-of-line copy; PropagateTreeIsVisible inlines it for the mask
// (0x82ADA8EC..0x82ADA918) and for each masked child (0x82ADAA4C..0x82ADAA74).
void AptRenderItem::SetIsHiddenByTree(bool bNewValue)
{
    mbPendingHiddenByTree = bNewValue ? 1 : 0;
    if (!bNewValue)
        mbIsHiddenByTree = 0;
}

// ARTIST tests with the record forms (0x82ADA8F8 `rlwinm.`, 0x82ADA950, 0x82ADA9DC `clrlwi.`) and
// reads the first child before the mask's recursion (0x82ADA8CC).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
void AptRenderItem::PropagateTreeIsVisible(bool bVisible)
{
    AptRenderItem* pMask = mpMask;
    AptRenderItem* pChild = mpManagerFirstChild;

    if (pMask)
    {
        while (pMask->mpManagerNextRevision)
            pMask = pMask->mpManagerNextRevision;
        pMask->SetIsHiddenByTree(!bVisible);
        pMask->PropagateTreeIsVisible(bVisible);
    }

    while (pChild)
    {
        while (pChild->mpManagerNextRevision)
            pChild = pChild->mpManagerNextRevision;

        if (pChild->GetIsMask())
        {
            pChild = pChild->mpManagerNextSibling;
            continue;
        }

        if (bVisible == true)
        {
            if (pChild->GetIsVisible() == true)
            {
                if (pChild->GetIsHiddenByTree() == true)
                {
                    pChild->mbPendingHiddenByTree = 0;
                    pChild->mbIsHiddenByTree = 0;
                    pChild->PropagateTreeIsVisible(true);
                }
            }
            else
            {
                pChild->mbPendingHiddenByTree = 0;
                pChild->mbIsHiddenByTree = 0;
                pChild->mbRootNonVisibleItem = 1;
            }
        }
        else
        {
            if (pChild->GetIsVisible() == true)
            {
                if (!pChild->GetIsHiddenByTree())
                {
                    pChild->mbPendingHiddenByTree = 1;
                    pChild->PropagateTreeIsVisible(false);
                }
            }
            else
            {
                pChild->mbRootNonVisibleItem = 0;
                pChild->mbPendingHiddenByTree = 1;
            }
        }

        if (pChild->GetHasMask())
        {
            AptRenderItem* pChildMask = pChild->mpMask;
            while (pChildMask->mpManagerNextRevision)
                pChildMask = pChildMask->mpManagerNextRevision;
            pChildMask->SetIsHiddenByTree(!bVisible);
            pChildMask->PropagateTreeIsVisible(bVisible);
        }

        pChild = pChild->mpManagerNextSibling;
    }
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

namespace
{
    // Both ARTIST revision bodies inline the complete acquisition loop.
    __forceinline void LockAptRenderTreeRevision()
    {
        while (static_cast<unsigned long>(_InterlockedCompareExchange(&gAptRenderTreeRevisionLock, 1, 0)) != 0u)
        {
        }
    }

    inline void UnlockAptRenderTreeRevision()
    {
        _InterlockedExchange(&gAptRenderTreeRevisionLock, 0);
    }
}

// Manager_CloneNewItem @0x82ADAAE8 -- under the render-tree lock, virtual-Clone
// THIS item into a fresh non-extended revision, install it as our next revision
// ([c:0x28]), take its reference, and return it.
// Native size optimization preserves the lock register schedule at 0x82ADAB00
// and 0x82ADABB8 through the virtual Clone call and unlock.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
AptRenderItem* AptRenderItem::Manager_CloneNewItem(int nTick)
{
    LockAptRenderTreeRevision();
    AptRenderItem* pNew = Clone(nTick, false);
    mpManagerNextRevision = pNew;
    pNew->AddReference();
    UnlockAptRenderTreeRevision();
    return pNew;
}

// Manager_CreateNewRevision @0x82ADABA0 -- identical to Manager_CloneNewItem but
// the virtual Clone copies the EXTENDED state (mask / depth / manager links).
AptRenderItem* AptRenderItem::Manager_CreateNewRevision(int nTick)
{
    LockAptRenderTreeRevision();
    AptRenderItem* pNew = Clone(nTick, true);
    mpManagerNextRevision = pNew;
    pNew->AddReference();
    UnlockAptRenderTreeRevision();
    return pNew;
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

// Manager_GetRenderRevision @0x82ADAC58 -- walk the next-revision chain ([c:0x28])
// and return the newest revision that is renderable for nTick (aged in + committed,
// mFlags bit25 clear). Stops at the first non-renderable revision.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
AptRenderItem* AptRenderItem::Manager_GetRenderRevision(int nTick)
{
    AptRenderItem* pResult = this;
    if (pResult)
    {
        for (AptRenderItem* pRevision = mpManagerNextRevision; pRevision; pRevision = pRevision->mpManagerNextRevision)
        {
            if (!pRevision->IsRenderableForThisTick(nTick))
                break;
            pResult = pRevision;
        }
    }
    return pResult;
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

// Manager_UpdateNextSibling @0x82ADACA8 -- commit pRevision as THIS item's next
// sibling ([c:0x2C]) for the current render walk, under the lock. A deletion-marked
// revision (bit28) collapses to null; the old/new links are reference-counted.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
void AptRenderItem::Manager_UpdateNextSibling(AptRenderItem* pRevision)
{
    if (pRevision && pRevision->mbIsDeletionMark)   // mask test, 0x82ADACCC `rlwinm. 0,3,3`
        pRevision = 0;

    if (pRevision != mpManagerNextSibling)
    {
        LockAptRenderTreeRevision();
        if (pRevision)
            pRevision->AddReference();
        if (mpManagerNextSibling)
        {
            AptRenderItem* pOld = mpManagerNextSibling;
            mpManagerNextSibling = 0;
            pOld->ReleaseReference();
        }
        mpManagerNextSibling = pRevision;
        UnlockAptRenderTreeRevision();
    }
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

// Manager_UpdateMask @0x82ADB1B8 -- commit pRevision as THIS item's mask link
// ([c:0x1C]) for the current render walk (same deletion-mark collapse + ref-count
// + lock as Manager_UpdateNextSibling).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
void AptRenderItem::Manager_UpdateMask(AptRenderItem* pRevision)
{
    if (pRevision && pRevision->mbIsDeletionMark)   // mask test, 0x82ADACCC `rlwinm. 0,3,3`
        pRevision = 0;

    if (pRevision != mpMask)
    {
        LockAptRenderTreeRevision();
        if (pRevision)
            pRevision->AddReference();
        if (mpMask)
        {
            AptRenderItem* pOld = mpMask;
            mpMask = 0;
            pOld->ReleaseReference();
        }
        mpMask = pRevision;
        UnlockAptRenderTreeRevision();
    }
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

// Manager_UpdateFirstChild -- the first-child counterpart of the two above
// ([c:0x30]); the AptRenderTreeManager render walk and the custom-control Render
// (@0x82AEF8F8, its verified X360 caller) drive it. The dedicated X360 leaf has no
// per-address export; reconstructed identically to its committed
// Update_NextSibling/Mask instruction-twin siblings (deletion-mark collapse +
// ref-counted link swap under the lock).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
void AptRenderItem::Manager_UpdateFirstChild(AptRenderItem* pRevision)
{
    if (pRevision && pRevision->mbIsDeletionMark)   // mask test, 0x82ADACCC `rlwinm. 0,3,3`
        pRevision = 0;

    if (pRevision != mpManagerFirstChild)
    {
        LockAptRenderTreeRevision();
        if (pRevision)
            pRevision->AddReference();
        if (mpManagerFirstChild)
        {
            AptRenderItem* pOld = mpManagerFirstChild;
            mpManagerFirstChild = 0;
            pOld->ReleaseReference();
        }
        mpManagerFirstChild = pRevision;
        UnlockAptRenderTreeRevision();
    }
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

// Manager_SetFirstChild @0x82ADAF58 -- set THIS item's writable-revision first-child
// link ([c:0x30]): reference the new child, recompute its mask-driven visibility
// against this parent's flags (PropagateTreeIsVisible), then swap the link under the
// lock (releasing the old child). The visibility recompute is the console's exact
// bit arithmetic on the child's mFlags ([c:0x18]).
// ARTIST tests with the record forms (0x82ADAFA0 `rlwinm.`, 0x82ADAFD4 `clrlwi.`) and swaps the
// link under the render-tree revision lock (0x82ADB0C4..); void like Manager_SetNextSibling.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
void AptRenderItem::Manager_SetFirstChild(AptRenderItem* pChild)
{
    if (pChild)
    {
        pChild->AddReference();
        if (!pChild->GetIsVisible())
        {
            if (!GetIsHiddenByTree() && !mbRootNonVisibleItem)
            {
                pChild->mbPendingHiddenByTree = 0;
                pChild->mbIsHiddenByTree = 0;
                pChild->mbRootNonVisibleItem = 1;
            }
            else
            {
                pChild->mbRootNonVisibleItem = 0;
                pChild->mbPendingHiddenByTree = 1;
            }
        }
        else
        {
            pChild->mbRootNonVisibleItem = 0;
            if (mbIsVisible && GetIsHiddenByTree() != true)
            {
                if (pChild->GetIsHiddenByTree() == true)
                {
                    pChild->mbPendingHiddenByTree = 0;
                    pChild->mbIsHiddenByTree = 0;
                    pChild->PropagateTreeIsVisible(true);
                }
            }
            else if (!pChild->GetIsHiddenByTree())
            {
                pChild->mbPendingHiddenByTree = 1;
                pChild->PropagateTreeIsVisible(false);
            }
        }
    }

    LockAptRenderTreeRevision();
    if (mpManagerFirstChild)
    {
        AptRenderItem* pOldChild = mpManagerFirstChild;
        mpManagerFirstChild = 0;
        pOldChild->ReleaseReference();
    }
    mpManagerFirstChild = pChild;
    UnlockAptRenderTreeRevision();
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

// Manager_SetNextSibling @0x82ADAD90 -- the next-sibling counterpart of
// Manager_SetFirstChild ([c:0x2C]); the same reference + visibility recompute +
// locked link swap, with the visible/invisible branches mirrored as in the console.
// ARTIST tests with the record forms (0x82ADADDC `rlwinm.`, 0x82ADAE0C `clrlwi.`) and swaps the
// link under the render-tree revision lock (0x82ADAEE4..0x82ADAF4C); void: r3 is left as the last
// call returned it.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
void AptRenderItem::Manager_SetNextSibling(AptRenderItem* pSibling)
{
    if (pSibling)
    {
        pSibling->AddReference();
        if (!pSibling->GetIsVisible())
        {
            if (!GetIsHiddenByTree())
            {
                pSibling->mbPendingHiddenByTree = 0;
                pSibling->mbIsHiddenByTree = 0;
                pSibling->mbRootNonVisibleItem = 1;
            }
            else
            {
                pSibling->mbRootNonVisibleItem = 0;
                pSibling->mbPendingHiddenByTree = 1;
            }
        }
        else
        {
            pSibling->mbRootNonVisibleItem = 0;
            if (GetIsHiddenByTree() == true)
            {
                if (!pSibling->GetIsHiddenByTree())
                {
                    pSibling->mbPendingHiddenByTree = 1;
                    pSibling->PropagateTreeIsVisible(false);
                }
            }
            else if (pSibling->GetIsHiddenByTree() == true)
            {
                pSibling->mbPendingHiddenByTree = 0;
                pSibling->mbIsHiddenByTree = 0;
                pSibling->PropagateTreeIsVisible(true);
            }
        }
    }

    LockAptRenderTreeRevision();
    if (mpManagerNextSibling)
    {
        AptRenderItem* pOldSibling = mpManagerNextSibling;
        mpManagerNextSibling = 0;
        pOldSibling->ReleaseReference();
    }
    mpManagerNextSibling = pSibling;
    UnlockAptRenderTreeRevision();
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

// SetIsVisible @0x82AE0708 -- set the is-visible flag (bit31). When the bit changes
// and the node is not masked, recompute the subtree's mask-driven visibility.
// GetIsHiddenByTree -- either hidden-by-tree flag set. ARTIST has no out-of-line copy; its
// callers materialise the bool from both `(flags & bit) == bit` tests (SetIsVisible 0x82AE0754,
// SetHasMask 0x82ADB3FC).
bool AptRenderItem::GetIsHiddenByTree() const
{
    return mbPendingHiddenByTree == 1 || mbIsHiddenByTree == 1;
}

// ARTIST tests with the record forms (0x82AE0734 `rlwinm.`, 0x82AE077C `clrlwi.`).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
void AptRenderItem::SetIsVisible(bool bVisible)
{
    if (bVisible == mbIsVisible)
        return;

    if (bVisible)
    {
        if (mbRootNonVisibleItem)
        {
            mbRootNonVisibleItem = 0;
            if (!GetIsMask())
                PropagateTreeIsVisible(1);
        }
    }
    else if (!GetIsHiddenByTree())
    {
        mbPendingHiddenByTree = 0;
        mbIsHiddenByTree = 0;
        mbRootNonVisibleItem = 1;
        if (!GetIsMask())
            PropagateTreeIsVisible(0);
    }

    mbIsVisible = bVisible ? 1 : 0;
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

// SetHasMask @0x82ADB388 -- set the has-mask flag (bit29) from bHasMask, (re)bind
// pMask as the mask link ([c:0x1C]) under the lock with the visibility recompute,
// then clear bit29 when no mask remains.
// ARTIST tests the old mask with the record form (0x82ADB4AC `cmplwi r3,0`); void: r3 is left as
// the last call returned it. The mask swap runs under the render-tree revision lock
// (0x82ADB468..0x82ADB4D4).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
void AptRenderItem::SetHasMask(bool bHasMask, AptRenderItem* pMask)
{
    mbHasMask = bHasMask ? 1 : 0;

    if (mpMask != pMask)
    {
        if (pMask)
        {
            pMask->AddReference();
            if (GetIsVisible() && GetIsHiddenByTree() != true)
            {
                pMask->mbPendingHiddenByTree = 0;
                pMask->mbIsHiddenByTree = 0;
                pMask->mbRootNonVisibleItem = 0;
                pMask->PropagateTreeIsVisible(true);
            }
            else
            {
                pMask->mbRootNonVisibleItem = 0;
                pMask->mbPendingHiddenByTree = 1;
                pMask->PropagateTreeIsVisible(false);
            }
        }

        LockAptRenderTreeRevision();
        if (mpMask)
        {
            AptRenderItem* pOldMask = mpMask;
            mpMask = 0;
            pOldMask->ReleaseReference();
        }
        mpMask = pMask;
        UnlockAptRenderTreeRevision();
    }

    if (!mpMask)
        mbHasMask = 0;
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

// SetIsMask @0x82AEBF28 -- set the is-mask flag (bit30) + (re)bind the mask matrix
// ([c:0x10]): when becoming a mask, copy the matrix in (SetMaskMatrix); when
// clearing, free the existing mask matrix back to the pool.
// ARTIST tests the old matrix with the record form (0x82AEBF6C `cmplwi r4,0`); void: r3 is left as
// SetMaskMatrix returned it.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
void AptRenderItem::SetIsMask(bool bIsMask, const AptMatrix* pMaskMatrix)
{
    if (bIsMask == GetIsMask() && mpMaskPositionMatrix == pMaskMatrix)
        return;

    if (!bIsMask)
    {
        if (mpMaskPositionMatrix)
            gpNonGCPoolManager->Deallocate(mpMaskPositionMatrix, sizeof(AptMatrix));
        mpMaskPositionMatrix = 0;
    }
    else
    {
        SetMaskMatrix(pMaskMatrix);
    }

    mbIsMask = bIsMask ? 1 : 0;
}
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("", on)
#endif

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
