#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::CrashNavColourCalibrate - the crash-nav colour-calibration screen flow state
// (brightness toggle over a grey/white calibration card). This header carries the class
// shape and the inline resource accessor (the one ledger function attributed to the
// header); the large component/substate machinery and the out-of-line state/virtual
// machinery (OnEnter/OnLeave/Update + the Update* / Handle* helpers and the
// observed-event/option .rdata tables) are reconstructed with the
// class:BrnGui::CrashNavColourCalibrate TU. Layout/virtuals from the DecFIGS DWARF
// (BrnCrashNavColourCalibrate.h).
namespace BrnGui
{
    struct CrashNavColourCalibrate : public CgsGui::State
    {
        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x825001C0 - hands the colour-calibrate state's static resource list to the
        // loader (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        static const CgsGui::sResourceTuple maResourcesToLoad[];  // @ 0x82F27008 (.rdata, 2 entries)
        static const u32                    muNumResourcesToLoad; // @ 0x82F27018 (.rdata)
    };
}
