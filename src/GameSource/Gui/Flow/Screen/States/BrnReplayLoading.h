#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::ReplayLoading -- the "replays loading" GUI state: a small load -> advance
// machine that streams the screen's single static resource, binds the "ReplaysLoading"
// apt movie, then sends ADVANCE once the GuiCache stops blocking. A twin of the
// committed BrnGui::Credits / BrnGui::ScreenLoading loading-screen states. Layout /
// virtual set reconstructed from BURNOUT_X360_ARTIST.XEX (BrnReplayLoading.{h,cpp}).
namespace BrnGui
{
    class GuiCache;

    struct ReplayLoading : public CgsGui::State
    {
        // Internal load-state machine (X360 this+0x3C).
        enum ELoadState
        {
            E_LOAD_WAIT_RESOURCES  = 0,
            E_LOAD_RESOURCES_READY = 1,
            E_LOAD_ADVANCE         = 2,
            E_LOAD_DONE            = 3,
        };

        // @0x824BA120 / @0x824D4978 / @0x824DB5B0 -- the FSM virtuals.
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x825009C0 -- hands the screen's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, count == 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;

    private:
        // @0x824D49F8 -- stream the single static resource; bind "ReplaysLoading" once loaded.
        bool UpdateLoadResources();

        // ---- members (X360 +0x38/+0x3C) ----
        GuiCache*  mpGuiCache;    // @ +0x38
        ELoadState meLoadState;   // @ +0x3C

        // ---- statics ----
        static const CgsGui::sResourceTuple maResourcesToLoad[];   // @ 0x82066A20 (.rdata)
        static const u32                    muNumResourcesToLoad;  // @ 0x82066A28 == 1
    };
}
