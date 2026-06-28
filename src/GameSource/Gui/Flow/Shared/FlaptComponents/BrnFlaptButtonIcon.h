#ifndef BRN_FLAPT_BUTTON_ICON_H
#define BRN_FLAPT_BUTTON_ICON_H

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"  // BrnGui::BrnFlaptComponent (base)
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipRef.h"                        // BrnFlapt::MovieClipRef (embedded)

// ============================================================================
// GameSource/Gui/Flow/Shared/FlaptComponents/BrnFlaptButtonIcon.h
//
// BrnGui::FlaptButtonIconComponent -- an apt-driven controller-button icon. It
// drives one apt movie clip (the inherited base mAptRef) holding the per-platform
// button artwork, plus a second clip ref (mAptButtonRef) for the specific button
// glyph it currently shows. Derives from BrnGui::BrnFlaptComponent. Reconstructed
// from BURNOUT_X360_ARTIST.XEX; enums/member names/types pinned from the DecFIGS
// DWARF (BrnFlaptButtonIcon.h), gated on the X360 ledger.
//
// LAYOUT (proven by Setup @ 0x8241C788; offsets are within the component object):
//   +0x00  vptr + BrnFlaptComponent base (mAptRef MovieClipRef @ +0x04)  -- 0x0C
//   +0x0C  meButton        (EPadButton; Setup writes E_PADBUTTON_INVISIBLE)
//   +0x10  mAptButtonRef   (BrnFlapt::MovieClipRef; the glyph clip)
//
// This header presently declares only what FlaptButtonIconComponent::Setup (the one
// function bodied in BrnFlaptButtonIcon.cpp) needs to compile: the two button enums,
// the two instance members, and Setup() itself. Grow it additively (the static
// identifier tables, Construct / the two Prepare overloads / SetButton / SetState /
// GetButton) as those member TUs land. All member access is BY NAME.
// ============================================================================

namespace BrnGui
{
    class FlaptButtonIconComponent : public BrnFlaptComponent
    {
    public:
        // BrnFlaptButtonIcon.h:48 (DWARF) -- which controller button this icon
        // represents. E_PADBUTTON_INVISIBLE is the "no button shown" sentinel
        // Setup installs before any concrete button is selected.
        enum EPadButton
        {
            E_PADBUTTON_UP        = 0,
            E_PADBUTTON_DOWN      = 1,
            E_PADBUTTON_LEFT      = 2,
            E_PADBUTTON_RIGHT     = 3,
            E_PADBUTTON_SELECT    = 4,
            E_PADBUTTON_BACK      = 5,
            E_PADBUTTON_OPTION0   = 6,
            E_PADBUTTON_OPTION1   = 7,
            E_PADBUTTON_LSHOULDER = 8,
            E_PADBUTTON_RSHOULDER = 9,
            E_PADBUTTON_LTRIGGER  = 10,
            E_PADBUTTON_RTRIGGER  = 11,
            E_PADBUTTON_START     = 12,
            E_PADBUTTON_LTHUMB    = 13,
            E_PADBUTTON_RTHUMB    = 14,
            E_PADBUTTON_INVISIBLE = 15,
            E_PADBUTTON_COUNT     = 16,
        };

        // BrnFlaptButtonIcon.h:71 (DWARF) -- the visual state of the button glyph.
        enum EPadButtonState
        {
            E_PADBUTTON_STATE_ACTIVE       = 0,
            E_PADBUTTON_STATE_HIGLIGHTED   = 1,
            E_PADBUTTON_STATE_PRESSED      = 2,
            E_PADBUTTON_STATE_UNSELECTABLE = 3,
            E_PADBUTTON_STATE_COUNT        = 4,
        };

    private:
        // Setup @ 0x8241C788 -- initialise the icon's apt clips: select the "ps3"
        // platform skin on the base clip, bind the "button" child glyph clip into
        // mAptButtonRef, default the button to invisible, and stop that glyph clip on
        // its "invisible" label.
        void Setup();

        EPadButton             meButton;        // +0x0C
        BrnFlapt::MovieClipRef mAptButtonRef;   // +0x10
    };
}

#endif // BRN_FLAPT_BUTTON_ICON_H
