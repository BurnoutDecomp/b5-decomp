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

#include "SDKs/EATech/include/Apt/AptCharacterInst.h"            // GetRenderItemWritable / mpRenderItem
#include "SDKs/EATech/include/Apt/AptRenderItem.h"               // mpCharacter
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"       // ClearCharacterList / ResetInitIndicators / IncCharacterList
#include "SDKs/EATech/include/Apt/AptDisplayList.h"              // mDisplayList.clear
#include "SDKs/EATech/include/Apt/AptFile.h"                     // AptFile (the shared pointee)
#include "SDKs/EATech/include/Apt/AptCharacter.h"                // AptCharacter base (the embedded movie root follows it)
#include "SDKs/EATech/include/Apt/AptCIH.h"                      // KU_AptEmbeddedMovieOff

// ---------------------------------------------------------------------------
// AptGetMovieCharacterAnimation -- a movie/animation AptCharacter embeds its
// AptCharacterAnimation movie root by value immediately after the AptCharacter base
// (X360: `addi r3, mpCharacter, 0x10; blr` -- the embedded timeline, the same embedded
// movie AptMovie.h documents). The serialized record has no owning character-subtype
// header (the blob IS the runtime layout), so the embedded root is reached through
// this single accessor instead of a raw cast at the call sites.
//
// x64 fork (VERIFIED): the console embeds it at the literal +0x10 (== console
// sizeof(AptCharacter)); on the x64 gate the SERIALIZED header widens to 0x20 under the
// 8-byte pointer rule (the .apt converter's GUIAPT64 "1:7:8" layout). The embedded-root
// offset is therefore KU_AptEmbeddedMovieOff (0x20 -- the SAME def-base offset
// AptGetClipMovie uses; VERIFIED vs TITLE_SCREEN02.bundle), so the returned
// AptCharacterAnimation* lands on the real def-base region (where the frame count / the
// fixed-up character table live). Null-safe.
// ---------------------------------------------------------------------------
AptCharacterAnimation* AptGetMovieCharacterAnimation(AptCharacter* pCharacter)
{
    if (pCharacter == 0)
        return 0;
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
// AptGetMovieCharacterAnimation -- see the header note). The first call reads the
// CURRENT render item's character directly (this->mpRenderItem); the init-indicator
// reset uses the tick-WRITABLE render item (GetRenderItemWritable, evaluated twice
// in the asm: once for the null guard, once for the character pointer).
//
// The owning file member is destroyed automatically before the base destructor.
// ---------------------------------------------------------------------------
AptCharacterAnimationInst::~AptCharacterAnimationInst()
{
    // Tear down the imported movie's character list (drop a character reference per
    // table entry). The movie root is embedded in the current render item's
    // character at char+0x10.
    AptGetMovieCharacterAnimation(mpRenderItem->mpCharacter)->ClearCharacterList();

    // If the writable render item still owns a character, restore that movie's init
    // indicators (the instance is going away before all its init actions ran).
    if (GetRenderItemWritable()->mpCharacter)
    {
        AptGetMovieCharacterAnimation(GetRenderItemWritable()->mpCharacter)->ResetInitIndicators();
    }

    // ~AptCharacterSpriteInstBase() (the embedded display list + the AptCharacterInst
    // base teardown) is chained automatically by the compiler.
}

// ARTIST @0x82AFFDE8 and DecFIGS @0xF5B330.
AptCharacterAnimationInst::AptCharacterAnimationInst(AptCharacter* pCharacter, AptFilePtr file)
    : AptCharacterSpriteInstBase(pCharacter), mnAccumulatedUpdateMs(0)
{
    // The animation-file slot belongs to the serialized character layout.
    // Rebind its counted reference without constructing an owner in the blob.
    AptFile* oldFile = pCharacter->mpAnimationFile;
    pCharacter->mpAnimationFile = file.pData;
    if (file.pData != 0)
        AptSharedPtrIncRef(file.pData);
    if (oldFile != 0 && AptSharedPtrDecRef(oldFile) == 0)
        AptSharedPtrDelete(oldFile);

    mAnimationFilePtr = file;
    AptGetMovieCharacterAnimation(mpRenderItem->mpCharacter)->IncCharacterList(file);
}

AptCharacterAnimationInst* MakeCharacterAnimationInst(const AptFilePtr& file)
{
    void* pMem = gpAptSharedPtrPool->Allocate(sizeof(AptCharacterAnimationInst));
    if (pMem == 0)
        return 0;
    return ::new (pMem) AptCharacterAnimationInst(static_cast<AptCharacter*>(file.pData->mpData), file);
}
