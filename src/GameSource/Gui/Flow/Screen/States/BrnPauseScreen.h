#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"   // BrnGui::MenuComponent (by value)

namespace CgsModule { struct Event; }

// BrnGui::PauseScreen - the in-game pause GUI screen state (DWARF home BrnPauseScreen.h:44).
// A three-substate screen (loading -> initialising -> prompt) that owns the pause-options
// menu. Virtual/member set from the DecFIGS DWARF, gated on the X360 ledger. The full
// surface is bodied in BrnPauseScreen.cpp:
//   OnEnter @0x824CFCE0   OnLeave @0x824CFDF0   Update @0x824DA0C0
//   HandleControllerInput @0x824CFE78
//   GetResourcesToLoad = the ICF fold @0x825011B0 (the vtable slot @0x82074298)
// (The ctor is the vtable-establishing default, inlined into BrnScreenFlow::Prepare
// @0x82523E50 on the X360 -- no standalone export; the PC implicit default matches.)
namespace BrnGui
{
    struct PauseScreen : public CgsGui::State
    {
        // DWARF BrnPauseScreen.h:70.
        enum ESubState
        {
            E_SUBSTATE_LOADING      = 0,
            E_SUBSTATE_INITIALISING = 1,
            E_SUBSTATE_PROMPT       = 2,
            E_SUBSTATE_COUNT        = 3,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;

    private:
        // @0x824CFE78 (this TU) -- act on a controller-action event while the prompt is up.
        void HandleControllerInput(const CgsModule::Event* lpEvent);

        // ---- members (DWARF h:89/h:107; X360 offsets +0x38 / +0x40) ----
        ESubState     meSubState;      // +0x38 (HandleControllerInput gates on E_SUBSTATE_PROMPT)
        MenuComponent mPauseOptions;   // +0x40 (8-aligned by its u64 id slot; the one-option pause menu)

        // ---- statics (DWARF cpp:26-51; values read from the decrypted XEX at the
        //      addresses noted on the definitions in the .cpp) ----
        static const s32                 maiEventToObserve[2];       // cpp:26 @0x82066660 {6, 64}
        static const s32                 miNumEventsObserved;        // @0x82066668 == 2
        static const CgsGui::sResourceTuple maResourcesToLoad[2];    // cpp:35 @0x82F2728C
        static const u32                 muNumResourcesToLoad;       // cpp:43 @0x82F2729C == 2
        static const char                KAC_OPTION_CPT_NAME_BASE[7];    // cpp:47 @0x8206666C "Option"
        static const char*               KAPC_PAUSE_OPTION_STRING_IDS[1]; // cpp:51 @0x82F272A0
    };
}
