#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnTextSelection.h"

// BrnGui::CrashNavColourCalibrate - the crash-nav colour-calibration screen flow state
// (brightness toggle over a grey/white calibration card). This header carries the class
// shape and the inline resource accessor (the one ledger function attributed to the
// header); the large component/substate machinery and the out-of-line state/virtual
// machinery (OnEnter/OnLeave/Update + the Update* / Handle* helpers and the
// observed-event/option .rdata tables) are reconstructed with the
// class:BrnGui::CrashNavColourCalibrate TU. Layout/virtuals from the DecFIGS DWARF
// (BrnCrashNavColourCalibrate.h).
//
// The ctor (@0x82500120) is the compiler-emitted construction of this state plus its
// embedded GUI sub-objects: it writes the state's own vtable (+0x000), two embedded
// sub-object vtables (+0x038, +0x050), constructs the embedded BrnGui::TextSelection
// widget (X360 r3 = this+0xE0), then sets four more embedded sub-object slots
// (+0xDA0, +0xEE4, +0xF70, +0x1000). Those per-sub-object vtable stores are the
// initialisation of widget types not individually recoverable from this ctor; the
// recovered, modelled effect is "construct the CgsGui::State base and the embedded
// TextSelection". The unrecovered sub-object body is reserved storage (the X360 offsets
// above are documentary -- the x64 gate's 8-byte pointers do not reproduce them). All
// access is by name.
namespace BrnGui
{
    struct CrashNavColourCalibrate : public CgsGui::State
    {
        // @ 0x82500120 - construct the colour-calibrate state and its embedded widgets.
        CrashNavColourCalibrate();

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

        // The embedded text-selection widget the ctor constructs (X360 this+0xE0). Modelled
        // by name; its preceding/trailing sibling sub-objects are unrecovered.
        TextSelection                       mTextSelection;
    };
}
