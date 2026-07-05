#include "GameSource/Gui/Flow/HUD/Components/BrnEventInfo.h"

namespace BrnGui
{

// @0x8241EDF8 (decl BrnEventInfo.h:373; asserts at BrnEventInfo.h:485) -- drive the
// position/medal text-state animator (mTextStateAnimatorRace, X360 this+0x0F8) to the
// medal frame implied by the finishing position: 1 -> "Gold", 2..3 -> "Normal",
// 4..8 -> "Warning".
void EventInfoComponent::SetPositionTextState(s32 liPosition)
{
    CGS_ASSERT(liPosition > 0 && liPosition <= 8,
               "Invalid player position provided - ");

    if (liPosition == 1)
    {
        mTextStateAnimatorRace.Run("Gold");
    }
    else if (liPosition < KI_POSITION_WARNING_THRESHOLD)
    {
        mTextStateAnimatorRace.Run("Normal");
    }
    else
    {
        mTextStateAnimatorRace.Run("Warning");
    }
}

// @0x824218A8 (asserts at BrnEventInfo.cpp:2227) -- pick single- vs multi-digit layout
// for the road-rage takedown counter and drive the digits animator (mDistanceAnimatorRace,
// X360 this+0x168) to it. 10+ takedowns need two digits.
// NOTE: absent from the (PS3-derived) DWARF method list, which instead carries the
// distinct SetTakedownsTextState StrStream method; this body is the X360-attested function.
void EventInfoComponent::SetTakedownsDigitsState()
{
    CGS_ASSERT(miCurrentTakedowns >= 0,
               "Invalid takedown count provided - ");

    if (miCurrentTakedowns >= 10)
    {
        mDistanceAnimatorRace.Run("MultipleDigits");
    }
    else
    {
        mDistanceAnimatorRace.Run("SingleDigit");
    }
}

}
