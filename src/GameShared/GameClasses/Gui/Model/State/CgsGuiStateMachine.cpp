#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateMachine.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsGui::StateMachine, reconstructed from BURNOUT_X360_ARTIST.XEX. This home bodies
// the two X360-emitted StateMachine functions split across the GUI state-machine TUs:
//
//   GetState(i)  @ 0x827DDDB0  (CgsGuiStateMachine.h -- the ScriptedFsm virtual override)
//   SetStates()  @ 0x82848700  (CgsGuiStateMachine.cpp -- installs the state table)
//
// The X360 asserts forward through CGS_ASSERT (Begin/Fire/EndAssert); the original
// streamed the count/max into the message via CgsDev::StrStream -- under CGS_ASSERT the
// failure path forwards the plain condition string, and the X360-baked d:\p4 file paths /
// line numbers (148/149, 47/49/55) are dropped per project policy.

namespace CgsGui
{
    // X360 0x827DDDB0. Bounds-check the index against miNumStates (this+0x14), then assert
    // the slot is populated, and return mapGuiStates[liIndex] (this+0x18). Covariant return
    // (CgsGui::State* refines CgsFsm::ScriptedState*).
    State* StateMachine::GetState(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= 0 && liIndex < miNumStates,
                   "Invalid index range in StateMachine::GetState");
        CGS_ASSERT(mapGuiStates[liIndex] != 0,
                   "Attempting to get invalid state in StateMachine::GetState");
        return mapGuiStates[liIndex];
    }

    // X360 0x82848700. Validate the requested count and the wired-up interface, clamp the
    // count to the table capacity, then copy each source State* into mapGuiStates and wire
    // it to mpStateInterface. The final store sets miNumStates (this+0x14).
    void StateMachine::SetStates(State** lpapStates, s32 liNumStates)
    {
        CGS_ASSERT(liNumStates <= KI_MAX_NUM_STATES,
                   "Attempting to add to many states in StateMachine::SetStates");
        CGS_ASSERT(mpStateInterface != 0,
                   "You must set a valid state interface before calling StateMachine::SetStates");

        if (liNumStates >= KI_MAX_NUM_STATES)
            liNumStates = KI_MAX_NUM_STATES;

        for (s32 li = 0; li < liNumStates; ++li)
        {
            CGS_ASSERT(lpapStates[li] != 0, "Invalid State pointer in StateMachine::SetStates");
            mapGuiStates[li] = lpapStates[li];
            mapGuiStates[li]->SetStateInterface(mpStateInterface);
        }

        miNumStates = liNumStates;
    }
}
