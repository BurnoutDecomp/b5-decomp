// ===================================================================================
// BrnGui::FlaptButtonIconComponent -- apt button-icon setup
//   GameSource/Gui/Flow/Shared/FlaptComponents/BrnFlaptButtonIcon.cpp
//
//   Setup @ 0x8241C788
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Non-static member (asm: r3 = this).
//
// Setup primes the icon's apt clips after the base component has bound its movie
// clip (mAptRef, inherited from BrnFlaptComponent @ this+0x04):
//   1. default meButton (+0x0C) to E_PADBUTTON_INVISIBLE (15),
//   2. drive the base clip onto the "ps3" platform-skin label,
//   3. bind the named "button" child clip into mAptButtonRef (+0x10) -- the X360
//      writes a stack temp then copies both handle words, which is exactly a
//      MovieClipRef write into mAptButtonRef,
//   4. stop that glyph clip on its "invisible" label.
// The X360 emits no asserts here. Member access is BY NAME throughout.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnFlaptButtonIcon.h"

namespace BrnGui
{
    // @ 0x8241C788 -- initialise the button-icon apt clips.
    void FlaptButtonIconComponent::Setup()
    {
        meButton = E_PADBUTTON_INVISIBLE;

        // Select the platform button-skin label on the base movie clip.
        mAptRef.GotoAndStopLabel("ps3");

        // Bind the "button" child glyph clip and park it on its hidden label.
        mAptRef.FindChildMovieClip(&mAptButtonRef, "button");
        mAptButtonRef.GotoAndStopLabel("invisible");
    }
}
