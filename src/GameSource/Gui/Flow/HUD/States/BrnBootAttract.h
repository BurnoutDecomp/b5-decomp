#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"

// BrnGui::BootAttract - the boot attract-loop GUI state. Layout/virtual set from
// the DecFIGS DWARF (BrnBootAttract.h): it observes a single GUI event while active.
namespace BrnGui
{
    struct BootAttract : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;

    private:
        static const s32 maiEventToObserve[];   // @ 0x8205A8EC (.rdata)
        static const s32 miNumEventsObserved;   // == 1
    };
}
