#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::Video - the screen "video" flow state. This leaf header carries the class shape
// and the one inline resource accessor attributed to the header (the single ledger
// function for this TU). The screen / video-playback wiring and the out-of-line state and
// virtual machinery (OnEnter / OnLeave / Update / HandleControllerInput) are reconstructed
// with the class:BrnGui::Video TU. The base derivation (CgsGui::State) and the virtual
// layout are from the DecFIGS DWARF (BrnVideo.h).
namespace BrnGui
{
    struct Video : public CgsGui::State
    {
        // @ 0x825011B0 - this state loads no GUI resources of its own: the X360 body only
        // stores zero into the count out-param (r5) and never touches the tuple pointer
        // (r4). Mirror that exactly - leave *lppResourceTuples untouched.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            (void)lppResourceTuples;
            *lpuNumberOfResources = 0;
        }
    };
}
