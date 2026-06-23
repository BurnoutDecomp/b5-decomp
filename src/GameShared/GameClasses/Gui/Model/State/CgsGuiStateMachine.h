#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/FSM/CgsScriptedFsm.h"          // CgsFsm::ScriptedFsm (base)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h" // CgsGui::State, StateInterface, InputBuffer::GuiEventQueue

// CgsGui::StateMachine - the GUI screen/HUD finite-state machine. A ScriptedFsm that
// owns a fixed array of up to KI_MAX_NUM_STATES (128) CgsGui::State pointers plus the
// StateInterface every owned state is wired to. SetStates installs the state table
// (validating the count, the interface and each pointer) and GetState (the ScriptedFsm
// virtual override) indexes it. Layout, the KI_MAX_NUM_STATES cap and the method set
// are from the DecFIGS DWARF (CgsGuiStateMachine.h); the member offsets are pinned by
// the X360 bodies:
//   +0x14  miNumStates       (SetStates' final store stw r24,0x14(this); GetState count check)
//   +0x18  mapGuiStates[128] (GetState index 4*(i+6) == this+0x18; SetStates this+6 words)
//   +0x218 mpStateInterface  (SetStates tests word 134 == this+0x218; == 0x18 + 128*4)
namespace CgsGui
{
    // CgsGuiStateMachine.h:81 -- the fixed state-table capacity.
    const s32 KI_MAX_NUM_STATES = 128;

    struct StateMachine : public CgsFsm::ScriptedFsm
    {
        StateMachine();

        void SetStateInterface(StateInterface* lpStateInterface);
        void SetInEventQueue(InputBuffer::GuiEventQueue* lpInGuiEventQueue);

        State* GetCurrentState();

        // ---- ScriptedFsm virtual overrides ---------------------------------------------
        // X360 0x827DDDB0: bounds-checked + non-null-checked accessor into mapGuiStates.
        // Return type is covariant (CgsGui::State* : CgsFsm::ScriptedState*).
        virtual State* GetState(s32 liIndex);
        virtual s32    GetStateCount();

        // ---- bodied by this group ------------------------------------------------------
        // X360 0x82848700: install the state table (CgsGuiStateMachine.cpp).
        void SetStates(State** lpapStates, s32 liNumStates);

        const State* GetStateByIndex(s32 liIndex);

    private:
        s32             miNumStates;                    // +0x14
        State*          mapGuiStates[KI_MAX_NUM_STATES]; // +0x18
        StateInterface* mpStateInterface;               // +0x218
    };
}
