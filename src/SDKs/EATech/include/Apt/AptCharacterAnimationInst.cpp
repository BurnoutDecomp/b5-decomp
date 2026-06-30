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

// gAptSkipCharList -- the character-list-bookkeeping skip gate (mirrors gAptSkipTimeline). On the
// native-8 (GUIAPT64) faithful path the movie's embedded AptCharacterAnimation character TABLE
// (mpCharacterTable / mnCharacterCount, read from the def-base region) is only PARTLY relocated --
// FixupInPlace skipped its records -- so IncCharacterList's `mpCharacterTable[i]` reads serialized
// 4-byte offsets as 64-bit pointers (garbage) and AVs on the first entry. IncCharacterList is pure
// AS-lookup bookkeeping (it ref-counts the embedded characters + binds their animation-file slot);
// it is NOT needed for the tick/render path right now, so the host sets this to 1 on the native-8
// path to SKIP it cleanly (with the ref-count balance preserved). The registration resumes once the
// movie's character records are fully widened/relocated. Default 0 (console/4-byte path runs it).
unsigned int gAptSkipCharList = 0u;

// CharList probe sink (host-implemented; weak no-op default just below). Logs the embedded animation's
// table head/count just before IncCharacterList would walk it, so the skip decision is observable.
extern "C" void CgsApt_CharListProbe(const void* pAnim, const void* pTable, int nCount, unsigned int uSkip);
#if defined(_MSC_VER)
extern "C" void CgsApt_CharListProbeDefault(const void*, const void*, int, unsigned int) {}
#pragma comment(linker, "/alternatename:CgsApt_CharListProbe=CgsApt_CharListProbeDefault")
#endif

#include "SDKs/EATech/include/Apt/AptCharacterInst.h"            // GetRenderItemWritable / mpRenderItem
#include "SDKs/EATech/include/Apt/AptRenderItem.h"               // mpCharacter
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"       // ClearCharacterList / ResetInitIndicators / IncCharacterList
#include "SDKs/EATech/include/Apt/AptDisplayList.h"              // mDisplayList.clear
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"                // AptSharedPtrIncRef / AptSharedPtrDecRef / AptSharedPtrDelete
#include "SDKs/EATech/include/Apt/AptFile.h"                     // AptFile (the shared pointee)
#include "SDKs/EATech/include/Apt/AptCharacter.h"                // AptCharacter base (the embedded movie root follows it)
#include "SDKs/EATech/include/Apt/AptCIH.h"                      // gAptCharMovieOffset (native-8 header size 0x20)

// ---------------------------------------------------------------------------
// AptMovieCharacter_GetAnimation -- a movie/animation AptCharacter embeds its
// AptCharacterAnimation movie root by value immediately after the AptCharacter base
// (X360: `addi r3, mpCharacter, 0x10; blr` -- the embedded timeline at char+0x10,
// the same "char+16" embedded movie AptMovie.h documents). The owning character-
// subtype that would carry it as a named `AptCharacterAnimation mAnimation;` member
// has no home header yet, so -- per the header FLAG -- the embedded root is reached
// through this single accessor instead of a raw cast at the call sites.
//
// FLAG (x64 fork): the console embeds it at the literal +0x10 (== console
// sizeof(AptCharacter)); on the x64 gate the SERIALIZED header widens to 0x20 under the
// 8-byte pointer rule (the .apt converter's GUIAPT64 layout), which is NOT necessarily
// the host C++ sizeof(AptCharacter). The embedded-root offset is therefore taken from the
// host-set gAptCharMovieOffset (the SAME def-base offset AptCIH_GetClipMovie uses: 0x20
// native-8 / 0x10 console) so the returned AptCharacterAnimation* lands on the real
// def-base region (where mnFrameCount=103 / the character table live). Using the literal
// console sizeof here mis-pointed the embedded root on native-8 and AV'd. Null-safe.
// ---------------------------------------------------------------------------
AptCharacterAnimation* AptMovieCharacter_GetAnimation(AptCharacter* pCharacter)
{
    if (pCharacter == nullptr)
        return nullptr;
    return reinterpret_cast<AptCharacterAnimation*>(
        reinterpret_cast<char*>(pCharacter) + gAptCharMovieOffset);   // [c:0x10] addi r3, mpCharacter, <hdrSize>
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
//   AptSharedPtr<AptFile>::operator=(a2+0xC, a3);   // (a2's embedded AptFilePtr) = a3
//   AptSharedPtr<AptFile>::operator=(this+0x28, a3);// mAnimationFilePtr = a3
//   held = a3; if(held) incref(held);               // pinned copy of the file
//   AptCharacterAnimation::IncCharacterList(this->mpRenderItem->mpCharacter + 0x10, &held);
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

    // FLAG (raw-offset poke into the loaded movie root): the X360 also assigns the
    // held file into the AptFilePtr the movie root carries at +0x0C
    // (AptSharedPtr<AptFile>::operator=(character+0xC, &held)). That slot is part of
    // the loaded .apt data the parser produces; reached only once mpData is non-null
    // (a resolved load). Modelled as the same ref-counted store the operator= emits.
    // 8-byte SKIP (gAptSkipCharList): the native-8 AptCharacter header is widened
    // (0x10->0x20), so the +0x0C console offset is WRONG -- it lands in the char
    // header (signature/count), and the garbage lpOld would AV in AptSharedPtrDecRef.
    // The slot's data is also un-relocated (FixupInPlace skipped the movie records).
    // Skip the store; ref-balance is preserved by the mAnimationFilePtr assign below +
    // the stack laHeldFile dtor. Resumes once the char record offsets are widened.
    if (lpCharacter != nullptr && gAptSkipCharList == 0u)
    {
        AptFilePtr* lpRootFilePtr =
            reinterpret_cast<AptFilePtr*>(reinterpret_cast<char*>(lpCharacter) + 0x0C);   // a2+0xC
        AptFile* lpOld = lpRootFilePtr->pData;
        lpRootFilePtr->pData = laHeldFile.pData;
        if (laHeldFile.pData != nullptr)
            AptSharedPtrIncRef(laHeldFile.pData);
        if (lpOld != nullptr && AptSharedPtrDecRef(lpOld) == 0)
            AptSharedPtrDelete(lpOld);
    }

    // mAnimationFilePtr = held (ref-counted assign).
    {
        AptFile* lpOld = pInst->mAnimationFilePtr.pData;
        pInst->mAnimationFilePtr.pData = laHeldFile.pData;
        if (laHeldFile.pData != nullptr)
            AptSharedPtrIncRef(laHeldFile.pData);
        if (lpOld != nullptr && AptSharedPtrDecRef(lpOld) == 0)
            AptSharedPtrDelete(lpOld);
    }

    // (3)+(4) @0x82AFFE38..74: form the by-value IncCharacterList argument (copy held
    // + incref it -- the pin sp+0x50), then IncCharacterList over the movie embedded in
    // the current render item's character (char+0x10). IncCharacterList takes the
    // AptFilePtr BY VALUE (a shallow copy; its body does not release the arg), so the
    // pin's incref is the argument's hand-off -- the asm leaves sp+0x50 un-decref'd
    // after the call (it is balanced by the table reference IncCharacterList keeps),
    // so no separate drop is emitted here.
    // NULL-SAFE (x64 bring-up): this reaches the movie's character list through the instance's RENDER
    // ITEM (pInst->mpRenderItem->mpCharacter). With the render-tree manager the null stub, mpRenderItem
    // is null, so skip IncCharacterList (it only ref-counts the embedded character table) when there is
    // no render item / character. FLAG: the character-list incref resumes once the RTM lands.
    CgsApt_GalProbe("MakeCAI: IncCharacterList (renderItem)", pInst->mpRenderItem);
    if (pInst->mpRenderItem != nullptr && pInst->mpRenderItem->mpCharacter != nullptr)
    {
        AptFilePtr laIncArg;
        laIncArg.pData = laHeldFile.pData;
        if (laIncArg.pData != nullptr)
            AptSharedPtrIncRef(laIncArg.pData);       // lwarx/+1/stwcx. @0x82AFFE50

        // SKIP GATE (native-8) -- checked BEFORE touching the embedded animation. On the native-8 path
        // the whole AS character-list registration is deferred: we do NOT call AptMovieCharacter_GetAnimation
        // and do NOT read the animation's mpCharacterTable/mnCharacterCount at all (the char records the
        // table points at are un-relocated -- both the table walk in IncCharacterList AND any read of the
        // table head would touch un-widened data). We just keep the ref-count balanced: IncCharacterList
        // takes the arg by value and does NOT release it (the table would keep the reference), so when the
        // walk is skipped that table reference is never taken -- therefore drop the pin we just incref'd.
        if (gAptSkipCharList != 0u)
        {
            // Confirm the corrected embedded-animation pointer (pure pointer arithmetic via the fixed
            // gAptCharMovieOffset -- NO deref of the un-relocated table), so the run shows GetAnimation now
            // lands on the def-base. Logged through the GalProbe sink (anim ptr only).
            CgsApt_GalProbe("MakeCAI: GetAnimation(def-base, native-8)",
                            AptMovieCharacter_GetAnimation(pInst->mpRenderItem->mpCharacter));
            CgsApt_CharListProbe(nullptr, nullptr, -1, gAptSkipCharList);   // (no table deref on skip)
            if (laIncArg.pData != nullptr && AptSharedPtrDecRef(laIncArg.pData) == 0)
                AptSharedPtrDelete(laIncArg.pData);
        }
        else
        {
            // Console / fully-relocated path: resolve the embedded animation (now via the correct
            // gAptCharMovieOffset def-base) and run the faithful character-list registration.
            AptCharacterAnimation* lpAnim =
                AptMovieCharacter_GetAnimation(pInst->mpRenderItem->mpCharacter);
            CgsApt_CharListProbe(lpAnim,
                                 lpAnim != nullptr ? static_cast<const void*>(lpAnim->mpCharacterTable) : nullptr,
                                 lpAnim != nullptr ? lpAnim->mnCharacterCount : -1,
                                 gAptSkipCharList);
            if (lpAnim != nullptr)
                lpAnim->IncCharacterList(laIncArg);   // @0x82AFFE74
            else if (laIncArg.pData != nullptr && AptSharedPtrDecRef(laIncArg.pData) == 0)
                AptSharedPtrDelete(laIncArg.pData);   // balance the incref if there is no animation
        }
    }

    // (5) @0x82AFFE78..AC: consume the passed-in reference -- read held, null it,
    // decref, delete at zero. This is the ctor consuming a3 (the caller handed it an
    // incref'd AptFilePtr(pFile) it no longer owns).
    {
        AptFile* lpHeld = laHeldFile.pData;
        laHeldFile.pData = nullptr;
        if (lpHeld != nullptr && AptSharedPtrDecRef(lpHeld) == 0)
            AptSharedPtrDelete(lpHeld);
    }

    return pInst;   // ctor returns `this`
}
