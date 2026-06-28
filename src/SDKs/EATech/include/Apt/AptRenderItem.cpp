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
