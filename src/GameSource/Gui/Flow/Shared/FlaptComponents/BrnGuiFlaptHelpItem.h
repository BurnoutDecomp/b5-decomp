#ifndef BRN_GUI_FLAPT_HELP_ITEM_H
#define BRN_GUI_FLAPT_HELP_ITEM_H

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptComponent.h"  // BrnFlaptComponent (base)
#include "GameSource/Gui/Flow/Shared/FlaptComponents/BrnFlaptButtonIcon.h"    // FlaptButtonIconComponent (embedded x2)
#include "GameSource/Gui/Flapt/BrnFlaptTextFieldRef.h"                        // BrnFlapt::TextFieldRef (embedded)

// ============================================================================
// GameSource/Gui/Flow/Shared/FlaptComponents/BrnGuiFlaptHelpItem.h
//
// BrnGui::FlaptHelpItem -- an apt-driven "help" line: a text string flanked by two
// controller-button icons (left/right). Derives from BrnFlaptComponent and embeds
// two FlaptButtonIconComponent glyph icons plus the text field. Reconstructed from
// BURNOUT_X360_ARTIST.XEX; member names/types/enums from the DecFIGS DWARF
// (BrnGuiFlaptHelpItem.h), gated on the X360 ledger.
//
// LAYOUT (X360-attested by Construct @ 0x8241D198 / Prepare @ 0x82428070 / SetItem
// @ 0x8241D338). FlaptHelpItem has NO virtuals, so the base sits at +0x00:
//   +0x00  BrnFlaptComponent base (mpStateInterface @ +0x00, mAptRef @ +0x04)  -- 0x0C
//   +0x0C  mIconLeft   (FlaptButtonIconComponent, sizeof 0x18 -> +0x0C..+0x23)
//   +0x24  mIconRight  (FlaptButtonIconComponent, sizeof 0x18 -> +0x24..+0x3B)
//   +0x3C  mTextField  (BrnFlapt::TextFieldRef, 3 words -> +0x3C..+0x47)
//   sizeof 0x48
// All member access is BY NAME.
//
// Only Construct / Prepare / SetItem (the X360-attested bodies in
// BrnGuiFlaptHelpItem.cpp) are declared here; the remaining DWARF methods
// (Prepare(const MovieClipRef*), SetText, SetIconState, GetIconButton) grow this
// header additively as their member TUs land.
// ============================================================================

namespace CgsGui
{
    struct StateInterface;   // stored by-pointer only (see BrnGuiFlaptComponent.h)
}

namespace BrnFlapt
{
    struct FileRef;          // by const-reference in Prepare; full type via the .cpp
}

namespace BrnGui
{
    class FlaptHelpItem : public BrnFlaptComponent
    {
    public:
        // BrnGuiFlaptHelpItem.h:49 (DWARF) -- which side a button icon sits on.
        enum EIconPosition
        {
            E_ICONPOSITION_LEFT  = 0,
            E_ICONPOSITION_RIGHT = 1,
            E_ICONPOSITION_COUNT = 2,
        };

        // Construct @ 0x8241D198 -- bind the state interface into the help item and
        // into both embedded icons, clear the text field. DWARF shape
        // Construct(const char*, StateInterface*, const char*); the body asserts the
        // name + state interface and uses only the state interface.
        void Construct(const char* lacName,
                       CgsGui::StateInterface* lpStateInterface,
                       const char* lpcParentName);

        // Prepare @ 0x82428070 -- bind the help item's own clip, then prepare the two
        // child button icons ("ButtonLeft"/"ButtonRight") and bind the "TextField".
        void Prepare(const char* lacName, const BrnFlapt::FileRef& lFlaptFile);

        // SetItem @ 0x8241D338 -- set the help text and both flanking button glyphs.
        void SetItem(const char* lpText,
                     FlaptButtonIconComponent::EPadButton leButtonLeft,
                     FlaptButtonIconComponent::EPadButton leButtonRight,
                     bool lbRemapButtons);

    private:
        FlaptButtonIconComponent mIconLeft;    // +0x0C
        FlaptButtonIconComponent mIconRight;   // +0x24
        BrnFlapt::TextFieldRef   mTextField;   // +0x3C
    };
}

#endif // BRN_GUI_FLAPT_HELP_ITEM_H
