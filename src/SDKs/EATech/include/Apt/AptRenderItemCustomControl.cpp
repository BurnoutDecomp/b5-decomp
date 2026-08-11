// ===========================================================================
// EATech Apt -- AptRenderItemCustomControl.   DECOMPILED from the X360 ARTIST.XEX.
//   copy-ctor      0x82AEF460   (from another custom control)
//   from-sprite-ctor 0x82AEF550 (promote a plain sprite)
//   dtor           0x82AEF5B8   / Clone 0x82AEFDE8 / CopyFromSprite 0x82AEF9F8
//   PopRenderData  0x82ADB538   / PushRenderDataAbsolute 0x82ADB4F8
//   Render         0x82AEF8F8   (deferred -- see the FLAG below)
//   (the str + Z-id accessors are inline in the .h).
//
// A custom control is a game-supplied widget the Apt player does not draw itself:
// it forwards the control's descriptor (type / target / properties strings, or the
// host-resolved Z-id handle) to the host custom-control render hooks. The non-GC
// pool is gpNonGCPoolManager (X360 off_8324D808); the render-tree revision swap is
// bracketed by the Apt render-tree lock (X360 unk_8324E7CC), modelled here as an
// interlocked test-and-set.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptRenderItemCustomControl.h"
#include "SDKs/EATech/include/Apt/AptRenderHooks.h"        // custom-control host hooks
#include "SDKs/EATech/include/Apt/AptDefine.h"             // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"                // DOGMA_PoolManager

#include <intrin.h>   // _InterlockedExchange (the render-tree spin lock)
#include <new>        // placement new

// ---------------------------------------------------------------------------
// The Apt render-tree revision lock (X360 unk_8324E7CC) -- a single-word test-and-
// set spin lock bracketing a render-item revision swap. Defined by the Apt runtime;
// declared here. The console uses the lwarx/stwcx. interrupt-masked idiom;
// modelled as an interlocked test-and-set (host-portable; the PC-platform leaf
// markers sit on the shims below). Uncontended on the single-threaded bring-up path.
// ---------------------------------------------------------------------------
extern volatile long gAptRenderTreeRevisionLock;   // X360 unk_8324E7CC

namespace
{
    // The console's interrupt-masked lwarx/stwcx. test-and-set on the render-tree revision
    // lock (unk_8324E7CC).
    // FLAG PC-platform leaf: modelled as a single-threaded _InterlockedExchange spin
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

// ---------------------------------------------------------------------------
// Copy-ctor @0x82AEF460 -- copy another custom control: delegate the base/sprite
// copy up the chain, copy the three descriptor strings from the source, stamp the
// custom-control render-type flag, and start with no resolved render-data handle.
// ---------------------------------------------------------------------------
AptRenderItemCustomControl::AptRenderItemCustomControl(const AptRenderItemCustomControl* pSource,
                                                       int nCreatedOnTick, bool bCopyExtended)
    : AptRenderItemSprite(pSource, nCreatedOnTick, bCopyExtended)
{
    // Copy the descriptor strings (the EAStringC assignment takes a reference on
    // the source's shared buffer -- the console did the raw pointer copy + atomic
    // +0x10000 refcount bump inline, skipping the bump for the empty sentinel).
    mTypeStr             = pSource->mTypeStr;
    mTargetStr           = pSource->mTargetStr;
    mCustomPropertiesStr = pSource->mCustomPropertiesStr;

    // Custom-control render-type flag: bit 22 (console __ROR4__(1,10) & 0xFC0000).
    mFlags = (mFlags & ~0x3F00u) | 0x1000u;   // custom-control=16; x64 type field (XB1 ctor 0x140826B00 `or 1000h`)
    mZId = 0;
}

// ---------------------------------------------------------------------------
// "From sprite" ctor @0x82AEF550 -- promote a plain sprite render item to a custom
// control: same base/sprite copy chain (so it inherits the sprite's character /
// matrices / instance name), but the descriptor strings start EMPTY and there is
// no render-data handle.
// ---------------------------------------------------------------------------
AptRenderItemCustomControl::AptRenderItemCustomControl(const AptRenderItemSprite* pSourceSprite,
                                                       int nCreatedOnTick, bool bCopyExtended)
    : AptRenderItemSprite(pSourceSprite, nCreatedOnTick, bCopyExtended)
    // mTypeStr / mTargetStr / mCustomPropertiesStr default-construct to the shared
    // empty string -- left empty (a plain sprite has no descriptor).
{
    mFlags = (mFlags & ~0x3F00u) | 0x1000u;   // custom-control=16; x64 type field (XB1 ctor 0x140826B00 `or 1000h`)
    mZId = 0;
}

// ---------------------------------------------------------------------------
// dtor @0x82AEF5B8 -- when custom-control rendering is enabled, ask the host to
// tear down the render data the Z-id handle owns, then clear it. The three
// descriptor strings, the inherited instance name, and the base render item
// destruct automatically (the console inlined each EAStringC refcount drop + the
// base dtor).
// ---------------------------------------------------------------------------
AptRenderItemCustomControl::~AptRenderItemCustomControl()
{
    if (gbAptCustomControlRenderEnabled)
    {
        if (gpfnAptDestroyCustomControl && mZId)
            gpfnAptDestroyCustomControl(mZId);
        mZId = 0;
    }
}

// ---------------------------------------------------------------------------
// Clone @0x82AEFDE8 -- pool-allocate a new custom control (72 bytes) and copy-
// construct it from *this; null on allocation failure.
// ---------------------------------------------------------------------------
AptRenderItem* AptRenderItemCustomControl::Clone(int nCreatedOnTick, bool bCopyExtended)
{
    void* pMem = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemCustomControl));
    if (!pMem)
        return nullptr;
    return new (pMem) AptRenderItemCustomControl(this, nCreatedOnTick, bCopyExtended);
}

// ---------------------------------------------------------------------------
// CopyFromSprite @0x82AEF9F8 -- the render-tree revision swap that converts a
// sprite render item into a custom control. Under the Apt render-tree lock: pool-
// allocate + from-sprite-construct a new custom control, install it as the source
// sprite's next render revision, take a reference, and return it.
// ---------------------------------------------------------------------------
AptRenderItem* AptRenderItemCustomControl::CopyFromSprite(AptRenderItem* pSourceSprite,
                                                          int nCreatedOnTick, bool bCopyExtended)
{
    AptRenderTreeLock_Acquire();

    void* pMem = gpNonGCPoolManager->Allocate(sizeof(AptRenderItemCustomControl));
    AptRenderItemCustomControl* pNew = pMem
        ? new (pMem) AptRenderItemCustomControl(
              static_cast<const AptRenderItemSprite*>(pSourceSprite), nCreatedOnTick, bCopyExtended)
        : nullptr;

    // Install the new control as the source sprite's next render revision.
    pSourceSprite->Manager_SetNextRevision(pNew);

    // Take the revision's reference (the console bumps mRefCount atomically).
    if (pNew)
        pNew->AddReference();

    AptRenderTreeLock_Release();
    return pNew;
}

// ---------------------------------------------------------------------------
// Render @0x82AEF8F8 -- draw the control through the host hooks (by descriptor
// strings, or by the resolved Z-id when custom rendering is enabled and the control
// has no type string), bracketed by this item's + its content node's transforms.
// ---------------------------------------------------------------------------
void AptRenderItemCustomControl::Render(AptRenderingContext* pCtx, AptMaskRenderOperation eOp, int nTick) const
{
    // FLAG (parked -- payload slot only): the console (@0x82AEF8F8) refreshes the
    // first-child link (Manager_GetRenderRevision @0x82ADAC58 + Manager_UpdateFirstChild,
    // both now HOMED in AptRenderItem.cpp), double-pushes the matrices (this, then
    // the content node), and hands the content character's host payload -- the
    // console word at character+0x20, PAST the 0x10-byte base -- to
    // gpfnAptDrawCustomControl (type/target/properties buffers) or ...ById (mZId)
    // under the byte_82F733F6 + empty-type gate, then double-pops. Still deferred:
    // the payload's x64 offset needs the custom-control character record's
    // serialized 1:7:8 layout (.apt parse follow-on); everything else is ready.
    (void)pCtx; (void)eOp; (void)nTick;
}

// ---------------------------------------------------------------------------
// PopRenderData @0x82ADB538 -- notify the host render-data pop hook for the
// (inherited) instance name. No matrix pop here (the host positions the control).
// ---------------------------------------------------------------------------
void AptRenderItemCustomControl::PopRenderData(AptRenderingContext*, AptMaskRenderOperation, int) const
{
    if (!mInstanceName.IsEmpty() && gpfnAptCustomControlPopRenderData)
        gpfnAptCustomControlPopRenderData(mInstanceName.GetBuffer());
}

// ---------------------------------------------------------------------------
// PushRenderDataAbsolute @0x82ADB4F8 -- notify the host render-data push hook for
// the (inherited) instance name. No matrix push here (the host positions the
// control).
// ---------------------------------------------------------------------------
void AptRenderItemCustomControl::PushRenderDataAbsolute(AptRenderingContext*) const
{
    if (!mInstanceName.IsEmpty() && gpfnAptCustomControlPushRenderData)
        gpfnAptCustomControlPushRenderData(mInstanceName.GetBuffer());
}
