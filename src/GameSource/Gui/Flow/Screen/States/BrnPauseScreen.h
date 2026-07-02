#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"   // BrnGui::MenuComponent (by value)

namespace CgsModule { struct Event; }

// BrnGui::PauseScreen - the in-game pause GUI screen state (DWARF home BrnPauseScreen.h:44).
// A three-substate screen (loading -> initialising -> prompt) that owns the pause-options
// menu. Virtual/member set from the DecFIGS DWARF, gated on the X360 ledger:
// HandleControllerInput is bodied in BrnPauseScreen.cpp (this TU); the ctor, OnEnter,
// OnLeave, Update and GetResourcesToLoad are declaration-only (their own ledger functions).
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

        // ---- statics (DWARF cpp:26-51; .rdata values not recovered -- resolved at link
        //      time, as the sibling GUI states) ----
        static const s32                 maiEventToObserve[];        // cpp:26 (2 entries)
        static const s32                 miNumEventsObserved;        // == 2
        static const CgsGui::sResourceTuple maResourcesToLoad[];     // cpp:35 (2 entries)
        static const u32                 muNumResourcesToLoad;       // cpp:43
        static const char                KAC_OPTION_CPT_NAME_BASE[]; // cpp:47 ("Option" base name)
        static const char*               KAPC_PAUSE_OPTION_STRING_IDS[]; // cpp:51 (1 entry)
    };
}
