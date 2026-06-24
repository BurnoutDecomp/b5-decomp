#ifndef BRN_COLOUR_FIELD_H
#define BRN_COLOUR_FIELD_H

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h" // CgsGui::GuiComponent

// BrnGui::ColourField - an apt-driven swatch component that shows one solid colour or a
// two-stop gradient. It keeps two decimal colour strings (built with "%u") plus the
// hidden / gradient flags, and pushes them to the bound apt movie via OutputAptData.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::ColourField::Construct      @ 0x824E53E0
//   BrnGui::ColourField::OutputAptData  @ 0x824E5440
//
// Derives from CgsGui::GuiComponent and adds the members the asm touches:
//   +0x8C macColour1[16], +0x9C macColour2[16], +0xAC mbHidden, +0xAD mbIsGradient.

namespace CgsGui { class StateInterface; }

namespace BrnGui
{
    class ColourField : public CgsGui::GuiComponent
    {
    public:
        static const u32 KU_MAX_COLOUR_LEN = 16;

        // 0x824E53E0 -- base Construct, then format both colour buffers to "0" via "%u",
        // null-terminate them, and clear the hidden flag.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName);

    private:
        // 0x824E5440 -- push the colour/gradient/hidden state to the apt movie.
        void OutputAptData();

        // Guest +0x8C: primary colour as a decimal string (15 chars + NUL).
        char macColour1[KU_MAX_COLOUR_LEN];
        // Guest +0x9C: secondary (gradient end) colour as a decimal string.
        char macColour2[KU_MAX_COLOUR_LEN];
        // Guest +0xAC: hidden flag (apt_hideColour).
        bool mbHidden;
        // Guest +0xAD: gradient flag (selects whether colour2 is emitted).
        bool mbIsGradient;
    };
}

#endif
