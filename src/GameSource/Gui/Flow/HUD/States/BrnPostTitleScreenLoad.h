#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"   // CgsGui::State, sResourceTuple

// BrnGui::PostTitleScreenLoad - the boot HUD-flow state that runs immediately after the
// title screen has loaded: it plays the post-title video and, when finished, hands off to
// the front-end. Class shape / member set / EState from the DecFIGS DWARF
// (BrnPostTitleScreenLoad.h); the one ledger function attributed to this header
// (GetResourcesToLoad @ 0x825080B0) is bodied out-of-line in BrnPostTitleScreenLoad.cpp.
//
// This state does NOT participate in the FSM's static-resource-list loading path -- its
// GetResourcesToLoad override is a "Should not get here" assert tripwire (the state loads via
// its own mpGuiCache / miNumFilesToLoad bookkeeping, not the sResourceTuple table mechanism),
// unlike e.g. BrnGui::BootLegal which returns a real table.
namespace BrnGui
{
    class GuiCache;   // GameSource/Gui/BrnGuiCache.h (held by pointer only)

    struct PostTitleScreenLoad : public CgsGui::State
    {
        // BrnPostTitleScreenLoad.h:79 (DWARF) -- the state's internal phase.
        enum EState
        {
            E_IDLE          = 0,
            E_PLAYING_VIDEO = 1,
            E_FINISHED_VIDEO = 2,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x825080B0 -- this state never loads through the FSM resource-tuple path; the X360
        // body just fires "Should not get here" (BrnPostTitleScreenLoad.h:63) and returns. Bodied
        // out-of-line in BrnPostTitleScreenLoad.cpp.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;

        void HandleIncomingEvents();

    private:
        static const s32 miNumEventsObserved = 3;
        static const s32 maiEventToObserve[miNumEventsObserved];

        GuiCache* mpGuiCache;        // BrnPostTitleScreenLoad.h:71
        s32       miNumFilesToLoad;  // BrnPostTitleScreenLoad.h:73
        EState    meState;           // BrnPostTitleScreenLoad.h:86
        bool      mbVideoFinished;   // BrnPostTitleScreenLoad.h:88
    };
}
