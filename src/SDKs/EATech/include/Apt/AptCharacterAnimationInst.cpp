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

#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h"
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h"  // base dtor (chained)
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"            // GetRenderItemWritable / mpRenderItem
#include "SDKs/EATech/include/Apt/AptRenderItem.h"               // mpCharacter
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"       // ClearCharacterList / ResetInitIndicators
#include "SDKs/EATech/include/Apt/AptDisplayList.h"              // mDisplayList.clear
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"                // AptSharedPtrDecRef / AptSharedPtrDelete
#include "SDKs/EATech/include/Apt/AptFile.h"                     // AptFile (the shared pointee)

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
