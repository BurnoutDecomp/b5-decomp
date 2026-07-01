// ===========================================================================
// EATech Apt -- AptCharacterAnimationInst bodies.   Reconstructed from the X360
// ARTIST.XEX pseudocode/asm (the authoritative spine; no Feb-2007 / DecFIGS source
// exists for this class):
//     PreDestroy                 @ 0x82AFE148
//     ~AptCharacterAnimationInst @ 0x82AFFEC0
//     `scalar deleting destructor' @ <thunk>  (compiler-generated; dropped --
//                                              ~AptCharacterAnimationInst below is
//                                              the real destructor body)
//
// The animation sibling of AptCharacterSpriteInst: it derives from the shared
// movie-clip base AptCharacterSpriteInstBase and owns a reference to the imported
// source .apt file plus the teardown of that imported movie's character table.
// ===========================================================================

#include <new>   // placement new for MakeCharacterAnimationInst

#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h"  // base dtor (chained)

// The instantiation step-probe sink (host-implemented; weak no-op default in AptCharacterHelper.cpp).
extern "C" void CgsApt_GalProbe(const char* pcStep, const void* p);

#include "SDKs/EATech/include/Apt/AptCharacterInst.h"            // GetRenderItemWritable / mpRenderItem
#include "SDKs/EATech/include/Apt/AptRenderItem.h"               // mpCharacter
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"       // ClearCharacterList / ResetInitIndicators / IncCharacterList
#include "SDKs/EATech/include/Apt/AptDisplayList.h"              // mDisplayList.clear
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"                // AptSharedPtrIncRef / AptSharedPtrDecRef / AptSharedPtrDelete
#include "SDKs/EATech/include/Apt/AptFile.h"                     // AptFile (the shared pointee)
#include "SDKs/EATech/include/Apt/AptCharacter.h"                // AptCharacter base (the embedded movie root follows it)
#include "SDKs/EATech/include/Apt/AptCIH.h"                      // KU_AptEmbeddedMovieOff (native-8 header size 0x20)

// ---------------------------------------------------------------------------
// AptMovieCharacter_GetAnimation -- a movie/animation AptCharacter embeds its
// AptCharacterAnimation movie root by value immediately after the AptCharacter base
// (X360: `addi r3, mpCharacter, 0x10; blr` -- the embedded timeline, the same embedded
// movie AptMovie.h documents). The owning character-subtype that would carry it as a
// named `AptCharacterAnimation mAnimation;` member has no home header yet, so -- per the
// header FLAG -- the embedded root is reached through this single accessor instead of a
// raw cast at the call sites.
//
// FLAG (x64 fork): the console embeds it at the literal +0x10 (== console
// sizeof(AptCharacter)); on the x64 gate the SERIALIZED header widens to 0x20 under the
// 8-byte pointer rule (the .apt converter's GUIAPT64 "1:7:8" layout). The embedded-root
// offset is therefore KU_AptEmbeddedMovieOff (0x20 -- the SAME def-base offset
// AptCIH_GetClipMovie uses; VERIFIED vs TITLE_SCREEN02.bundle), so the returned
// AptCharacterAnimation* lands on the real def-base region (where the frame count / the
// fixed-up character table live). Null-safe.
// ---------------------------------------------------------------------------
AptCharacterAnimation* AptMovieCharacter_GetAnimation(AptCharacter* pCharacter)
{
    if (pCharacter == nullptr)
        return nullptr;
    return reinterpret_cast<AptCharacterAnimation*>(
        reinterpret_cast<char*>(pCharacter) + KU_AptEmbeddedMovieOff);   // [c:0x10] addi r3, mpCharacter, <hdrSize>
}

// ---------------------------------------------------------------------------
// PreDestroy @0x82AFE148
//   li   r4, 0
//   addi r3, r3, 0x1C
//   b    AptDisplayList::clear        (tail call)
//   -> AptDisplayList::clear(this + 0x1C, 0)
//
// this + 0x1C is the inherited child display list (AptCharacterSpriteInstBase's
// mDisplayList @+0x1C). Unlike the base PreDestroy (which calls mDisplayList.
// PreDestroy() -- clear + free + null the head node), the animation instance only
// CLEARS the listed entries (releases the placed children) and keeps the head node.
// ---------------------------------------------------------------------------
void AptCharacterAnimationInst::PreDestroy()
{
    mDisplayList.clear(false);
}

// ---------------------------------------------------------------------------
// ~AptCharacterAnimationInst @0x82AFFEC0
//   *this = off_82145FE8;                                  // vtable stamp (codegen;
//                                                          //   the family models its
//                                                          //   vtable as the manual
//                                                          //   mpVTable_unused member,
//                                                          //   so nothing is written)
//   AptCharacterAnimation::ClearCharacterList(this->mpRenderItem->mpCharacter + 0x10);
//   if ( this->GetRenderItemWritable()->mpCharacter )
//       AptCharacterAnimation::ResetInitIndicators(GetRenderItemWritable()->mpCharacter + 0x10);
//   p = this->mAnimationFilePtr;  this->mAnimationFilePtr = 0;
//   if ( p && --p->refcount == 0 ) AptSharedPtrDelete(p);   // inline lwarx/stwcx. decref
//   return ~AptCharacterSpriteInstBase(this);               // chained base dtor
//
// The movie root the two AptCharacterAnimation calls operate on is the
// AptCharacterAnimation embedded at char+0x10 (reached by name through
// AptMovieCharacter_GetAnimation -- see the header FLAG). The first call reads the
// CURRENT render item's character directly (this->mpRenderItem); the init-indicator
// reset uses the tick-WRITABLE render item (GetRenderItemWritable, evaluated twice
// in the asm: once for the null guard, once for the character pointer).
//
// The source-file release is the AptSharedPtr<AptFile> teardown the asm inlines
// (interrupt-masked lwarx/addi -1/stwcx. decrement, then AptSharedPtrDelete when the
// new count is zero); de-optimised here to the named shared-ptr primitives, the same
// idiom AptCharacterAnimation::IncCharacterList uses.
//
// The vtable store and the embedded mDisplayList + AptCharacterInst base teardown
// are codegen the C++ compiler re-emits (entering the destructor + the implicitly
// chained ~AptCharacterSpriteInstBase), so they are not hand-written here.
// ---------------------------------------------------------------------------
AptCharacterAnimationInst::~AptCharacterAnimationInst()
{
    // Tear down the imported movie's character list (drop a character reference per
    // table entry). The movie root is embedded in the current render item's
    // character at char+0x10.
    AptMovieCharacter_GetAnimation(mpRenderItem->mpCharacter)->ClearCharacterList();

    // If the writable render item still owns a character, restore that movie's init
    // indicators (the instance is going away before all its init actions ran).
    if (GetRenderItemWritable()->mpCharacter)
    {
        AptMovieCharacter_GetAnimation(GetRenderItemWritable()->mpCharacter)->ResetInitIndicators();
    }

    // Drop one reference on the imported source .apt file; delete the shared AptFile
    // when its reference count reaches zero.
    AptFile* pFile = mAnimationFilePtr.pData;
    mAnimationFilePtr.pData = nullptr;
    if (pFile && AptSharedPtrDecRef(pFile) == 0)
        AptSharedPtrDelete(pFile);

    // ~AptCharacterSpriteInstBase() (the embedded display list + the AptCharacterInst
    // base teardown) is chained automatically by the compiler.
}

// ---------------------------------------------------------------------------
// MakeCharacterAnimationInst -- the AptCharacterAnimationInst::AptCharacterAnimationInst
// ctor @0x82AFFDE8 wrapped as the AptLinker::Update factory. The X360 ctor is folded
// (it follows AptCompleteAnimationAsyncLoad's range with no own export symbol);
// DISASSEMBLED from the decrypted ARTIST.XEX @0x82AFFDE8 and cross-checked against the
// PS3 EXTERNAL lift (._ZN25AptCharacterAnimationInstC2EP12AptCharacter12AptSharedPtrI7AptFileE
// @0xF5B4B0). The ctor takes (this, AptCharacter* a2, AptFilePtr& a3):
//   AptCharacterSpriteInstBase::AptCharacterSpriteInstBase(this, a2);  // base ctor
//   *this = off_82145FE8;                       // AnimInst vtable (automatic codegen)
//   *(this+0x28) = 0;                           // mAnimationFilePtr.pData = 0
//   *(this+0x24) = 0;                           // mAnimationState_unknown = 0
//   if(a2+0xC != a3) AptSharedPtr<AptFile>::operator=(a2+0xC, a3);   // (a2's embedded AptFilePtr) = a3
//   if(this+0x28 != a3) AptSharedPtr<AptFile>::operator=(this+0x28, a3);// mAnimationFilePtr = a3
//   held = a3; if(held) incref(held);               // pinned copy of the file
//   AptCharacterAnimation::IncCharacterList(this->mpRenderItem->mpCharacter + <embed>, &held);
//     (<embed> = char+0x10 console / char+0x20 native-8 == KU_AptEmbeddedMovieOff)
//   if(held && --held.count==0) AptSharedPtrDelete(held);   // drop the pin
//   return this;
//
// The Update caller (AptLinker::Update @0x82B0D028) derives the two ctor args from
// the pending AptFile: a2 = pFile->mpData (the loaded movie root, used as the
// AptCharacter the base ctor builds over) and a3 = an incref'd AptFilePtr(pFile).
// The Make* convention owns the 44-byte pool allocation (the caller's separate
// Allocate(0x2C) is the presence test the X360 folds into the ctor prologue).
// ---------------------------------------------------------------------------
AptCharacterAnimationInst* MakeCharacterAnimationInst(AptFile* pFile)
{
    CgsApt_GalProbe("MakeCAI: enter pFile", pFile);
    if (pFile == nullptr)
        return nullptr;
    void* lpMem = gpAptSharedPtrPool->Allocate(sizeof(AptCharacterAnimationInst));    // Allocate(off_8324D808, 0x2C)
    CgsApt_GalProbe("MakeCAI: alloc inst", lpMem);
    if (lpMem == nullptr)
        return nullptr;

    // Derive the ctor args from the pending file (the Update caller's r4/r5 setup):
    // the movie root (pFile->mpData) is the AptCharacter the base ctor builds over,
    // and the held file reference is an incref'd AptFilePtr(pFile).
    AptCharacter* lpCharacter = reinterpret_cast<AptCharacter*>(pFile->mpData);   // r4 = *(pFile+0x14)
    CgsApt_GalProbe("MakeCAI: mpData(character)", lpCharacter);
    AptFilePtr    laHeldFile;
    laHeldFile.pData = pFile;
    if (pFile != nullptr)
        AptSharedPtrIncRef(pFile);                       // incref into the stack AptFilePtr (r5)

    // ---- base ctor + member init (the folded ctor body) -----------------------
    AptCharacterAnimationInst* pInst =
        static_cast<AptCharacterAnimationInst*>(lpMem);

    // AptCharacterSpriteInstBase::AptCharacterSpriteInstBase(this, character).
    // (The AnimInst vtable store `*this = off_82145FE8` is the manual-vtable family's
    // automatic codegen -- not hand-written here, matching the dtor + the base ctor.)
    CgsApt_GalProbe("MakeCAI: SpriteInstBase ctor ...", nullptr);
    ::new (static_cast<void*>(static_cast<AptCharacterSpriteInstBase*>(pInst)))
        AptCharacterSpriteInstBase(lpCharacter);
    CgsApt_GalProbe("MakeCAI: SpriteInstBase done; renderItem", pInst->mpRenderItem);

    pInst->mAnimationFilePtr.pData = nullptr;    // *(this+0x28) = 0 (pre-op= clear)
    pInst->mAnimationState_unknown = 0;          // *(this+0x24) = 0

    // (1) @0x82AFFDF8..: the movie root's own embedded AptFilePtr (a2+0xC) = a3.
    // AptSharedPtr<AptFile>::operator=(character+0xC, &held). The PS3 lift gates this
    // ref-counted store on the source not already being the destination slot
    // (`if (v5 != a3)`, v5 = a2+12) -- the standard AptSharedPtr self-assign guard.
    // FLAG (external serialised .apt data): +0x0C is a fixed slot in the loaded movie
    // root blob (not a modelled C++ member), addressed by its console byte offset like
    // the other raw .apt-record accesses. lpCharacter is pFile->mpData (a resolved load,
    // non-null here). The char table this slot lives in is relocated by Fixup.
    {
        AptFilePtr* lpRootFilePtr =
            reinterpret_cast<AptFilePtr*>(reinterpret_cast<char*>(lpCharacter) + 0x0C);   // a2+0xC (v5)
        if (lpRootFilePtr != &laHeldFile)   // if (v5 != a3)
        {
            AptFile* lpOld = lpRootFilePtr->pData;
            lpRootFilePtr->pData = laHeldFile.pData;
            if (laHeldFile.pData != nullptr)
                AptSharedPtrIncRef(laHeldFile.pData);
            if (lpOld != nullptr && AptSharedPtrDecRef(lpOld) == 0)
                AptSharedPtrDelete(lpOld);
        }
    }

    // (2) @0x82AFFE20..: mAnimationFilePtr (this+0x28) = a3 (same operator=, same
    // self-assign guard `if (v6 != a3)`, v6 = a1+40).
    {
        if (&pInst->mAnimationFilePtr != &laHeldFile)   // if (v6 != a3)
        {
            AptFile* lpOld = pInst->mAnimationFilePtr.pData;
            pInst->mAnimationFilePtr.pData = laHeldFile.pData;
            if (laHeldFile.pData != nullptr)
                AptSharedPtrIncRef(laHeldFile.pData);
            if (lpOld != nullptr && AptSharedPtrDecRef(lpOld) == 0)
                AptSharedPtrDelete(lpOld);
        }
    }

    // (3) @0x82AFFE38..74: form the by-value IncCharacterList argument (copy *a3 into a
    // stack AptFilePtr + incref it -- the pin sp+0x50), then IncCharacterList over the
    // AptCharacterAnimation embedded in the current render item's character (the movie
    // root at char + KU_AptEmbeddedMovieOff). IncCharacterList takes the AptFilePtr BY
    // VALUE (a shallow copy; its body does not release the arg -- see the by-value
    // signature in AptCharacterAnimation::IncCharacterList), so the pin's incref is the
    // argument hand-off; the asm re-reads *a3, increfs, calls, then drops it below.
    // The render item + its character are always live here (Manager_CreateItem built the
    // render item in the base ctor). This walk assumes a converter-clean, uniformly-64-bit
    // movie def-base; on the current TITLE_SCREEN02 bundle it is NOT (char[1] un-widened +
    // the def-base charCount/table at +0x18/+0x20 vs the runtime struct's +0x0C/+0x10), so
    // the host facade DEFERS calling this factory until the converter fix (see
    // BrnAptRuntimeBringUp: the movie-root instantiation is held off with the tick). The
    // body here stays the faithful, un-gated console decompile.
    CgsApt_GalProbe("MakeCAI: IncCharacterList (renderItem)", pInst->mpRenderItem);
    AptFilePtr laIncArg;
    laIncArg.pData = laHeldFile.pData;                // v16[0] = *a3
    if (laIncArg.pData != nullptr)
        AptSharedPtrIncRef(laIncArg.pData);           // lwarx/+1/stwcx. @0x82AFFE50
    AptMovieCharacter_GetAnimation(pInst->mpRenderItem->mpCharacter)
        ->IncCharacterList(laIncArg);                 // IncCharacterList(*(mpRenderItem->mpChar)+embed, v16) @0x82AFFE74

    // Drop the by-value pin (v16[0]): the asm decrefs it after the call and deletes at
    // zero (IncCharacterList kept its own table references, not this arg).
    {
        AptFile* lpArg = laIncArg.pData;
        laIncArg.pData = nullptr;                     // v16[0] = 0
        if (lpArg != nullptr && AptSharedPtrDecRef(lpArg) == 0)
            AptSharedPtrDelete(lpArg);
    }

    // (4) consume the passed-in reference (the ctor's a3): read held, null it, decref,
    // delete at zero -- the caller handed the ctor an incref'd AptFilePtr(pFile) it no
    // longer owns. (In the folded X360 ctor this is the caller's a3 lifetime; the Make*
    // wrapper owns that hand-off here.)
    {
        AptFile* lpHeld = laHeldFile.pData;
        laHeldFile.pData = nullptr;
        if (lpHeld != nullptr && AptSharedPtrDecRef(lpHeld) == 0)
            AptSharedPtrDelete(lpHeld);
    }

    return pInst;   // ctor returns `this`
}
