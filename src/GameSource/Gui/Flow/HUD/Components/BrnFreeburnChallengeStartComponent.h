#pragma once

// BrnFreeburnChallengeStartComponent.h
// Home of BrnGui::FreeburnChallengeStartComponent -- the HUD prompt shown in freeburn when
// both shoulder triggers can be held to start a challenge. It tracks whether it is active
// and whether each of the two trigger buttons (L2/R2) is currently held; when it is active
// and both are held it posts a "start challenge" GUI event onto the owning state's output
// queue. Derives from CgsGui::GuiComponent (the apt-view-state output plumbing). Layout/
// method set from the DecFIGS DWARF (BrnFreeburnChallengeStartComponent.h:54/:91..:93).
//
// LAYOUT (X360 authoritative; offsets are the store displacements in Construct):
//   +0x8C  mbActive  -- bool: the prompt is shown / armed (stb at 0x8C)
//   +0x8D  mbDownL2  -- bool: the L2 trigger (button 56) is held (stb at 0x8D)
//   +0x8E  mbDownR2  -- bool: the R2 trigger (button 57) is held (stb at 0x8E)

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h" // CgsGui::GuiComponent base

namespace CgsGui { class StateInterface; }

namespace BrnGui
{
    struct FreeburnChallengeStartComponent : public CgsGui::GuiComponent
    {
        // @ 0x824157D8 -- base Construct, then clear all three flags.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName) override;

        // @ 0x82415818 -- show the prompt: when it was hidden, publish the "visible"
        // apt_state; then set mbActive.
        void Show();

        // @ 0x82415870 -- hide the prompt: when it was shown, publish the "invisible"
        // apt_state; then clear mbActive.
        void Hide();

        // @ 0x82439AF8 -- a controller button went down. L2 (56) sets mbDownL2, R2 (57) sets
        // mbDownR2; when the prompt is active and both are now held, post the start event.
        void HandleButtonPress(s32 liButton);

        // @ 0x824158C8 -- a controller button came up. L2 (56) clears mbDownL2, R2 (57)
        // clears mbDownR2.
        void HandleButtonRelease(s32 liButton);

    private:
        bool mbActive;   // +0x8C
        bool mbDownL2;   // +0x8D
        bool mbDownR2;   // +0x8E
    };
}
