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

#include <intrin.h>   // _InterlockedIncrement/Decrement
#include <new>        // placement new (lazy colour-matrix construct)

int AptRenderItem::sItemsAllocated = 0;

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
    mFlags = 0x80000000u;

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
        AptCXForm* p = static_cast<AptCXForm*>(gpNonGCPoolManager->Allocate(sizeof(AptCXForm)));
        mpColorMatrix = p ? new (p) AptCXForm(pSource->mpColorMatrix) : nullptr;
    }

    // Extended copy: mask, depth, manager links + the source's state-flag bits.
    if (bCopyExtended)
    {
        mDepth     = pSource->mDepth;
        mClipDepth = pSource->mClipDepth;
        mFlags = (mFlags & 0x7FFFFFFFu) | (pSource->mFlags & 0x80000000u);   // isVisible (bit31)
        mFlags = (mFlags & 0xBFFFFFFFu) | (pSource->mFlags & 0x40000000u);   // isMask   (bit30)
        mFlags = (mFlags & 0xDFFFFFFFu) | (pSource->mFlags & 0x20000000u);   // hasMask  (bit29)
        SetMaskMatrix(pSource->mpMaskPositionMatrix);
        mFlags = (mFlags & 0xEFFFFFFFu) | (pSource->mFlags & 0x10000000u);   // deletion mark (bit28)

        mpMask                = LatestRevision(pSource->mpMask);
        mpManagerNextRevision = nullptr;
        mpManagerNextSibling  = LatestRevision(pSource->mpManagerNextSibling);
        mpManagerFirstChild   = LatestRevision(pSource->mpManagerFirstChild);

        if (mpManagerNextSibling) mpManagerNextSibling->AddReference();
        if (mpManagerFirstChild)  mpManagerFirstChild->AddReference();
        if (mpMask)               mpMask->AddReference();

        if (pSource->mFlags & 0x04000000u)
            mFlags |= 0x02000000u;                                          // writable-revision (bit25) from source bit26
        mFlags = (mFlags & 0xFEFFFFFFu) | (pSource->mFlags & 0x01000000u);  // bit24
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

// dtor @0x80F860
AptRenderItem::~AptRenderItem()
{
    // FLAG: the manager next-revision/sibling/child chain teardown ([10]/[11]/[12])
    // is the AptRenderTreeManager double-buffering cleanup, deferred with that
    // subsystem. Those links are null until the manager populates them, so there
    // is nothing to tear down in the current (pre-manager) phase.

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
    p->a = 0.0f; p->b = 0.0f; p->c = 0.0f; p->d = 0.0f; p->tx = 0.0f; p->ty = 0.0f;
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
AptRenderItem* AptRenderItem::SetDepth(int nDepth)         { mDepth = static_cast<int16_t>(nDepth); return this; }
AptRenderItem* AptRenderItem::SetClipDepth(int nClipDepth) { mClipDepth = static_cast<int16_t>(nClipDepth); return this; }

// ---- flags ----------------------------------------------------------------
bool AptRenderItem::GetIsVisible() const { return (mFlags >> 31) != 0; }
bool AptRenderItem::GetIsMask() const    { return ((mFlags >> 30) & 1) != 0; }
bool AptRenderItem::GetHasMask() const   { return (mFlags & 0x20000000u) != 0 && mpMask != nullptr; }
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
    return mCreatedOnTick == nTick || (mFlags & 0x02000000u) != 0;
}

// Manager_IsDeletionMark @0x7DEEBC -- mFlags bit 28.
bool AptRenderItem::Manager_IsDeletionMark() const { return ((mFlags >> 28) & 1u) != 0; }

// Manager_SetNextRevision @0x7DEF00
void AptRenderItem::Manager_SetNextRevision(AptRenderItem* pNext) { mpManagerNextRevision = pNext; }

// Manager_SetDeletionMark @0x7E48DC -- set the deletion-mark flag (bit 28).
// FLAG: the console also releases this revision's child/sibling references here
// (the full revision teardown); deferred with the concurrent double-buffering.
void AptRenderItem::Manager_SetDeletionMark(bool bMark)
{
    if (bMark) mFlags |= 0x10000000u;
    else       mFlags &= ~0x10000000u;
}

// ---------------------------------------------------------------------------
// Render-traversal helpers (PS3 External PushMatrices @0x7F21E4 / PopMatrices
// @0x7ECA68). Push: save + concat the item's colour transform and position
// matrix onto the render context; Pop: restore both. The subtypes bracket their
// geometry draw with these.
//
// FLAG: the console PushMatrices has an "optimised" fast-path (gAptOptFlags & 4 ->
// _drawCharacterInstOpti); the standard push/append path is reconstructed here.
// (Identity-singleton note: the item's null-matrix sentinels here are
// gIdentityMatrix/gIdentityCXForm, distinct from the context's gAptIdentityMatrix/
// gAptNullCXForm -- the concat still yields the correct result, just without the
// pointer-identity short-circuit; the two singleton sets should be unified.)
// ---------------------------------------------------------------------------
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

// AptCharacter_render -- the geometry-draw helper the render-item subtypes call
// (homed here, dispatching to AptCharacter::render @0x810E74). Now the shape
// render path is wired end-to-end: AptRenderItemShape::Render -> PushMatrices ->
// AptCharacter_render -> AptCharacter::render -> AptHook_DrawShape (the RW
// rasteriser boundary).
void AptCharacter_render(AptCharacter* pCharacter, AptRenderingContext* pCtx,
                         AptMaskRenderOperation eOp, int nTick)
{
    if (pCharacter)
        pCharacter->render(pCtx, eOp, nTick);
}
