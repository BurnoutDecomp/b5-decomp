#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/FSM/CgsScriptedState.h"

// InputBuffer is a class with a nested GuiEventQueue; the GUI state only holds a
// pointer to that nested queue, so an incomplete nested declaration is enough.
class InputBuffer
{
public:
    class GuiEventQueue;
};

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

    protected:
        InputBuffer::GuiEventQueue* mpInGuiEventQueue;
        StateInterface*             mpStateInterface;
        bool                        mbStateChangePending;
        char                        macEvent[16];
        bool                        mbIsSaveLoadState;
        bool                        mbIsVideoState;
    };
}
