#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/FSM/CgsScriptedState.h"

// InputBuffer is the global event-queue NAMESPACE (the game-state headers add GameActionQueue /
// TakedownEventQueue / ... to the same namespace); the GUI state only holds a pointer to the
// nested GuiEventQueue, so an incomplete member declaration is enough. It MUST be modelled as a
// namespace (not a class) so it merges with the game-state's `namespace InputBuffer` rather than
// clashing -- a global `class InputBuffer` vs `namespace InputBuffer` is a C2757 in any TU (e.g.
// BrnMain) that includes both the GUI and the game-state headers.
namespace InputBuffer
{
    class GuiEventQueue;
}

// CgsGui::State - base of every GUI screen/HUD state. Extends the scripted-FSM
// state with the input-event queue it reads, the StateInterface it drives, and
// the small bookkeeping the GUI module needs (pending state change, the event
// name buffer, and the video / save-load flags). Layout, virtuals and the
// non-virtual helper set are from the DecFIGS DWARF (CgsGuiState.h).
namespace CgsGui
{
    struct StateInterface;   // defined as a struct in CgsGuiStateInterface.h (match for correct name mangling)
    struct sResourceTuple;

    struct State : public CgsFsm::ScriptedState
    {
        State();

        virtual void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm);
        virtual void PreWorldUpdate();
        virtual void GetResourcesToLoad(const sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const;
        virtual void PreUpdate();
        virtual void PostUpdate();

        void SetStateInterface(StateInterface* lpStateInterface);
        void SetInEventQueue(InputBuffer::GuiEventQueue* lpInGuiEventQueue);
        void SendStateEvent(const char* lpacEvent);

        bool IsVideoState() const { return mbIsVideoState; }
        bool IsSaveLoadState() const { return mbIsSaveLoadState; }

        // The state-event channel the owning flow/controller drains: SendStateEvent records a pending
        // transition (mbStateChangePending + the event name in macEvent); the controller reads it to run
        // the event through the FSM and, for the single-state boot FSMs, sequence to the next phase.
        bool        IsStateChangePending() const { return mbStateChangePending; }
        const char* GetPendingEventName()  const { return macEvent; }
        void        ClearStateChange()           { mbStateChangePending = false; }

    protected:
        InputBuffer::GuiEventQueue* mpInGuiEventQueue;
        StateInterface*             mpStateInterface;
        bool                        mbStateChangePending;
        char                        macEvent[16];
        bool                        mbIsSaveLoadState;
        bool                        mbIsVideoState;
    };
}
