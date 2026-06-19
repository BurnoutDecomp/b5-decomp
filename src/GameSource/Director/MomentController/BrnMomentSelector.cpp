#include "GameSource/Director/MomentController/BrnMomentSelector.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// BrnDirector::MomentSelector - five accessor/mutator methods reconstructed from the
// X360 pseudocode (no DWARF / no Feb-2007 source for this TU). Each is an assert guard
// plus a small state mutation; the member layout (in BrnMomentSelector.h) is a
// semantic-parity model inferred from the assert strings. See the header for FLAGS.

namespace BrnDirector
{

// @ 0x821F57F0  (X360 sig (float* result, double a2): the double is the X360
// float-arg ABI; modelled as f32 lfRecencyFactor01.)
void MomentSelector::SetRecencyFactor(f32 lfRecencyFactor01)
{
    CGS_ASSERT(lfRecencyFactor01 >= 0.0f && lfRecencyFactor01 < 1.0f,
               "lfRecencyFactor01 >= 0.0f && lfRecencyFactor01 < 1.0f");
    mfRecencyFactor = lfRecencyFactor01;
}

// @ 0x82208570
void MomentSelector::SetMaxActiveMoments(u32 luMaxMoments)
{
    CGS_ASSERT(!mbPrepared, "!mbPrepared");
    // The X360 "Array used before Construct/Clear was called" assert seen here is
    // INTERNAL to Array::GetLength() (CgsArray.h) - calling GetLength() reproduces it,
    // so no separate assert is added.
    CGS_ASSERT(luMaxMoments < mMomentHandleArray.GetLength(),
               "luMaxMoments < mMomentHandleArray.GetLength()");
    muMaxActiveMoments    = luMaxMoments;
    mbMaxActiveMomentsSet = true;
}

// @ 0x82254DA8
int MomentSelector::SelectBestMoment(int liContext)
{
    CGS_ASSERT(!HasSelectedMoment(), "!HasSelectedMoment()");
    return SelectBestMomentWithExclusion(liContext, -1);
}

// @ 0x82254E18
int MomentSelector::SelectNewBestMoment(int liContext)
{
    CGS_ASSERT(HasSelectedMoment(), "HasSelectedMoment()");
    // X360 reads +472 into v4 BEFORE clearing +480 - preserve that order.
    const int liExclude = miLastSelectedMomentIndex;
    mbHasSelectedMoment = false;
    return SelectBestMomentWithExclusion(liContext, liExclude);
}

// @ 0x821F5868
void MomentSelector::CancelSelection()
{
    CGS_ASSERT(HasSelectedMoment(), "HasSelectedMoment()");
    mbHasSelectedMoment = false;
}

} // namespace BrnDirector
