#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::OnlinePreEvent - the online pre-event (messages / fly-by) screen state. This
// leaf header carries the class shape and the one inline resource accessor attributed to
// the header (the single ledger function for this TU). The GUI cache, messages component,
// internal-state machine, fly-by timing and all out-of-line virtual/handler machinery
// (OnEnter/OnLeave/Update, the Update*/Handle* helpers) are reconstructed with the
// class:BrnGui::OnlinePreEvent TU. Layout/virtuals and the CgsGui::State derivation are
// from the DecFIGS DWARF (BrnOnlinePreEvent.h).
namespace BrnGui
{
    struct OnlinePreEvent : public CgsGui::State
    {
        // @ 0x82500698 - hands the online pre-event screen's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];   // @ 0x8205F994 (unk_8205F994, .rdata)
        static const u32                    muNumResourcesToLoad;  // @ 0x8205F99C (dword_8205F99C, .rdata) == 1
    };
}
