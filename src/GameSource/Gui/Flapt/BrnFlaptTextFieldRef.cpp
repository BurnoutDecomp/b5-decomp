#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnFlapt::TextFieldRef member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (class:BrnFlapt::TextFieldRef) bodies the one
// X360-emitted function:
//
//   Construct @ 0x8246AFF0
//
// X360 body: asserts each of the three incoming pointers is non-null
//   ("lpTextFieldInst", "lpParentMovie", "lpTransform")
// then stores them into the ref (stw at +0x00, +0x04, +0x08) and returns the ref.
// The X360-baked BrnFlaptTextFieldRef.h file/line cites are discarded per project
// convention.

namespace BrnFlapt
{

// ---- Construct @ 0x8246AFF0 ----------------------------------------------
TextFieldRef* TextFieldRef::Construct(void* lpTextFieldInstance,
                                      void* lpParentMovie,
                                      void* lpTransform)
{
    CGS_ASSERT(lpTextFieldInstance != 0, "lpTextFieldInst");
    CGS_ASSERT(lpParentMovie != 0, "lpParentMovie");
    CGS_ASSERT(lpTransform != 0, "lpTransform");

    mpTextFieldInstance = lpTextFieldInstance;
    mpParentMovie       = lpParentMovie;
    mpTransform         = lpTransform;
    return this;
}

}
