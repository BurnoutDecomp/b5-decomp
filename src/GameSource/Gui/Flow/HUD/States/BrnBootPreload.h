#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::BootPreload - the boot preload GUI state (BF_PRELOAD, the FIRST HUD-flow
// state). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   OnEnter @0x82473A10  OnLeave @0x82473A60  Update @0x82477F28
//   GetResourcesToLoad @0x82508070
//
// The state waits for the GUI cache's preload resources, plays the AS FRAMEWORK movie
// "main" at display level 0 (the asm string xref at 0x82478110 pins the name), raises
// the loading screen, and posts the phase-complete command 70 (channel 40) plus the
// preload-done command 72 (channels 42 + 40) once the preload set is resident.
namespace BrnGui
{
    class GuiCache;   // GameSource/Gui/BrnGuiCache.h (held by pointer only)

    struct BootPreload : public CgsGui::State
    {
        // The X360 Update's internal stage values (this+60; OnEnter seeds 2).
        enum EUpdateStage
        {
            E_PRELOAD_WAIT_CACHE     = 2,   // wait for the cache + play "main" @ level 0
            E_PRELOAD_LOADING_SCREEN = 4,   // wait resources + raise the loading screen
            E_PRELOAD_SETTLE         = 5,   // one-frame settle
            E_PRELOAD_SIGNAL_DONE    = 6,   // wait resources + post 70 / 72
            E_PRELOAD_DRAIN          = 7,   // one-frame drain
            E_PRELOAD_DONE           = 8,   // idle
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x82508070 - hands the preload screen's second-phase resource list out.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maSecondPhaseResourcesToLoad;
            *lpuNumberOfResources = muSecondPhaseNumResourcesToLoad;
        }

    private:
        static const s32 maiEventToObserve[];                                // @ .rdata (== { 64 })
        static const s32 miNumEventsObserved;                                // == 1
        static const CgsGui::sResourceTuple maSecondPhaseResourcesToLoad[];  // @ .rdata
        static const u32                    muSecondPhaseNumResourcesToLoad; // @ .rdata

        GuiCache*    mpGuiCache;     // this+56 (filled from the per-frame cache event 64)
        EUpdateStage meUpdateStage;  // this+60 (OnEnter seeds E_PRELOAD_WAIT_CACHE)
    };
}
