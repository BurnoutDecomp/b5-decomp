#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptString.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8246D9A0.

namespace CgsGui
{
    // The un-set-up sentinel pair the leading type-tag holds before the object has
    // been set up. The X360 reads these from a module-static pair (dword_82FB3C50 /
    // dword_82FB3C54); their concrete values are not recoverable from the accessor's
    // rodata alone, so they are modelled as a named pair the SetUp path is expected to
    // move the tag off of. Zero is the natural default-construction value.
    const u32 CgsAptString::KU_UNSET_TAG0 = 0;
    const u32 CgsAptString::KU_UNSET_TAG1 = 0;

    // @ 0x8246D9A0 - if the leading tag pair still equals the un-set-up sentinel pair,
    // fire the "Text Object hasn't been set up yet" assert; either way return the
    // cached text pointer at +0x74.
    CgsAptString::TextHandle CgsAptString::GetText() const
    {
        const bool lbUnset = (muTypeTag0 == KU_UNSET_TAG0) && (muTypeTag1 == KU_UNSET_TAG1);
        CGS_ASSERT(!lbUnset, "Can't GetText() - Text Object hasn't been set up yet\n");
        return muText;
    }
}
