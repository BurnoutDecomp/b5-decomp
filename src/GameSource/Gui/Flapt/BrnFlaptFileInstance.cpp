#include "GameSource/Gui/Flapt/BrnFlaptFileInstance.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnFlapt::FlaptFileInstance member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU (class:BrnFlapt::FlaptFileInstance) bodies the
// one X360-emitted function:
//
//   GetRootMovieClip @ 0x8246B360
//
// The X360 body guards three conditions then constructs a MovieClipRef in the
// caller-provided out buffer:
//   1. IsActive()                 (lbz 0(this))          -- BrnFlaptFileInstance.h:135
//   2. mpRootMovieClipInstance     (lwz 0x24(this))       -- BrnFlaptFileInstance.h:136
//   3. lpMovieClipInst (the same   (re-read 0x24(this))   -- BrnFlaptMovieClipRef.h:217
//      pointer, asserted again by the inlined MovieClipRef constructor)
// then writes the MovieClipRef: *out = mpRootMovieClipInstance; out[1] = 0.
// The X360-baked file/line cites are discarded per project convention.

namespace BrnFlapt
{

void FlaptFileInstance::Construct(const RGBA* lpAlternateTextColours,
                                  s32 liNumAlternateColours)
{
    CGS_ASSERT(lpAlternateTextColours != 0, "lpAlternateTextColours");

    mbIsActive = false;
    mpRootMovieClipInstance = 0;
    mpAlternateTextColours = lpAlternateTextColours;
    miNumAlternateColours = liNumAlternateColours;
    mpLinearAlloc = 0;
}

void FlaptFileInstance::Prepare(CgsMemory::LinearMalloc* lpFlaptAllocator)
{
    mpLinearAlloc = lpFlaptAllocator;
}

void FlaptFileInstance::Destruct()
{
    mbIsActive = false;
    mpRootMovieClipInstance = 0;
    mpLinearAlloc = 0;
}

// ---- GetRootMovieClip @ 0x8246B360 ---------------------------------------
MovieClipRef* FlaptFileInstance::GetRootMovieClip(MovieClipRef* lpOutRef) const
{
    CGS_ASSERT(mbIsActive, "IsActive()");
    CGS_ASSERT(mpRootMovieClipInstance != 0, "mpRootMovieClipInstance");

    // Inlined MovieClipRef construction: re-asserts the clip pointer is non-null
    // ("lpMovieClipInst") and seeds the transform slot to 0 (a root clip has no
    // Im2dTransform). The root instance is held opaquely here (void*); the Ref
    // names it as a MovieClipInstance*, so bridge the pointer type.
    void* lpMovieClipInst = mpRootMovieClipInstance;
    CGS_ASSERT(lpMovieClipInst != 0, "lpMovieClipInst");

    lpOutRef->mpMovieClipInst =
        static_cast<BrnFlapt::MovieClipInstance*>(lpMovieClipInst);
    lpOutRef->mpTransform = 0;
    return lpOutRef;
}

}
