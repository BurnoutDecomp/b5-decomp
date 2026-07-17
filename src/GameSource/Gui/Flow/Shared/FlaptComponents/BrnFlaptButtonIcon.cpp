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
    // .rdata @ 0x8204D190 (values read from the decrypted XEX): the per-button apt
    // timeline-label names, indexed by EPadButton. FlaptHelpItem::SetItem stops the
    // glyph clip on maButtonIdentifiers[button].
    const char* const FlaptButtonIconComponent::maButtonIdentifiers[E_PADBUTTON_COUNT] =
    {
        "up",          // E_PADBUTTON_UP
        "down",        // E_PADBUTTON_DOWN
        "left",        // E_PADBUTTON_LEFT
        "right",       // E_PADBUTTON_RIGHT
        "select",      // E_PADBUTTON_SELECT
        "back",        // E_PADBUTTON_BACK
        "option0",     // E_PADBUTTON_OPTION0
        "option1",     // E_PADBUTTON_OPTION1
        "lshoulder",   // E_PADBUTTON_LSHOULDER
        "rshoulder",   // E_PADBUTTON_RSHOULDER
        "ltrigger",    // E_PADBUTTON_LTRIGGER
        "rtrigger",    // E_PADBUTTON_RTRIGGER
        "start",       // E_PADBUTTON_START
        "lthumb",      // E_PADBUTTON_LTHUMB
        "rthumb",      // E_PADBUTTON_RTHUMB
        "invisible",   // E_PADBUTTON_INVISIBLE
    };

    // X360 header-inline (no standalone symbol; the base init is carried inlined at
    // e.g. FlaptHelpItem::Construct @0x8241D198): bind the state interface and
    // invalidate the movie-clip handle. The name/parent args are unused by the body.
    void FlaptButtonIconComponent::Construct(const char* /*lacName*/,
                                             CgsGui::StateInterface* lpStateInterface,
                                             const char* /*lpcParentName*/)
    {
        BrnFlaptComponent::Construct(lpStateInterface);
    }

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
