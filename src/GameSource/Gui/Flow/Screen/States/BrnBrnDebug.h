#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"

// BrnGui::BrnDebug - the debug GUI screen state (DWARF home BrnBrnDebug.h:42). It
// observes two GUI events while active. Virtual set from the DecFIGS DWARF, gated on
// the X360 ledger: OnEnter/OnLeave are bodied in BrnBrnDebug.cpp (this TU); Update
// (DWARF cpp:73, a separate ledger function) and GetResourcesToLoad (DWARF h:60) are
// declaration-only here.
namespace BrnGui
{
    struct BrnDebug : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;

    private:
        static const s32 maiEventToObserve[];   // @ 0x82065DB0 (.data): { 14, 6 }
        static const s32 miNumEventsObserved;   // == 2
    };
}
