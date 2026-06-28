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
// LAYOUT (proven by Setup @ 0x8241C788; offsets are within the component object).
// FlaptButtonIconComponent has NO virtuals, so the (non-polymorphic) base sits at
// +0x00 directly:
//   +0x00  BrnFlaptComponent base (mpStateInterface @ +0x00, mAptRef @ +0x04)  -- 0x0C
//   +0x0C  meButton        (EPadButton; Setup writes E_PADBUTTON_INVISIBLE)
//   +0x10  mAptButtonRef   (BrnFlapt::MovieClipRef; the glyph clip)
//   sizeof 0x18
//
// This header declares what FlaptButtonIconComponent::Setup needs plus the members
// FlaptHelpItem pokes inline (the X360 inlined the per-button "set button" logic --
// it has no standalone SetButton in the ledger -- into FlaptHelpItem::SetItem; that
// poke is modelled as direct named access, granted via the friend below). Grow this
// header additively as further ButtonIcon member TUs land. All member access is BY NAME.
// ============================================================================

namespace BrnGui
{
    class FlaptHelpItem;   // friend: SetItem pokes meButton / mAptButtonRef inline

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

        // Construct(const char*, StateInterface*, const char*) (DWARF
        // BrnFlaptButtonIcon.h:87) -- base init (bind the state interface, invalidate
        // the movie-clip handle). Always inlined by the X360 (no standalone symbol);
        // declared here so callers (FlaptHelpItem::Construct) de-inline to a call.
        // The body lands with ButtonIcon's own member TU.
        void Construct(const char* lacName,
                       CgsGui::StateInterface* lpStateInterface,
                       const char* lpcParentName);

        // Setup @ 0x8241C788 -- initialise the icon's apt clips: select the "ps3"
        // platform skin on the base clip, bind the "button" child glyph clip into
        // mAptButtonRef, default the button to invisible, and stop that glyph clip on
        // its "invisible" label.
        void Setup();

        // off_8204D190 (X360) == maButtonIdentifiers[16] (DWARF BrnFlaptButtonIcon.h:121)
        // -- per-button apt timeline-label names, indexed by EPadButton (index 0 ==
        // "up"); the glyph clip is goto-and-stopped on maButtonIdentifiers[button].
        // This static label table is OWNED+DEFINED by ButtonIcon's own data TU (the
        // full 16-entry string set is serialised data not in the function export, so
        // it is declared here and defined there, not guessed); FlaptHelpItem::SetItem
        // indexes it.
        static const char* const maButtonIdentifiers[E_PADBUTTON_COUNT];

    private:
        friend class FlaptHelpItem;

        EPadButton             meButton;        // +0x0C
        BrnFlapt::MovieClipRef mAptButtonRef;   // +0x10
    };
}

#endif // BRN_FLAPT_BUTTON_ICON_H
