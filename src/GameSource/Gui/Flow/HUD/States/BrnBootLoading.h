#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"

// BrnGui::BootLoading - the boot loading-screen GUI state. Observes two GUI events
// while active and tracks whether the loading screen is currently playing. Layout/
// virtual set from the DecFIGS DWARF (BrnBootLoading.h).
namespace BrnGui
{
    struct BootLoading : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;

    private:
        static const s32 maiEventToObserve[2];  // @ 0x8205ABF8 (.rdata)
        static const s32 miNumEventsObserved;   // == 2
        bool             mbScreenPlaying;
    };
}
