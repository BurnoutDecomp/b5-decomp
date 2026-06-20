#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"   // CgsGui::State (+ ScriptedState base, StateInterface fwd, sResourceTuple fwd)

// CgsGui::State base bodies. The X360 keeps these out-of-line in CgsGuiState.cpp; the GUI virtuals are
// no-op defaults (derived states override OnEnter/OnLeave/Update/GetResourcesToLoad) and the setters just
// store the channel/queue. Reconstructed minimally to close the link for the boot HUD-flow states.

namespace CgsGui
{
    State::State()
        : mpInGuiEventQueue(0)
        , mpStateInterface(0)
        , mbStateChangePending(false)
        , mbIsSaveLoadState(false)
        , mbIsVideoState(false)
    {
        macEvent[0] = 0;
    }

    void State::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
    {
        mId = liId;     // CgsFsm::ScriptedState protected members
        mpFsm = lpFsm;
        mpInGuiEventQueue = 0;
        mpStateInterface = 0;
        mbStateChangePending = false;
    }

    void State::PreWorldUpdate() {}
    void State::PreUpdate()      {}
    void State::PostUpdate()     {}

    void State::GetResourcesToLoad(const sResourceTuple** lppResourceTuples, u32* lpuNumberOfResources) const
    {
        *lppResourceTuples = 0;
        *lpuNumberOfResources = 0;
    }

    void State::SetStateInterface(StateInterface* lpStateInterface)
    {
        mpStateInterface = lpStateInterface;
    }

    void State::SetInEventQueue(InputBuffer::GuiEventQueue* lpInGuiEventQueue)
    {
        mpInGuiEventQueue = lpInGuiEventQueue;
    }

    void State::SendStateEvent(const char* lpacEvent)
    {
        // [stub] record the event name + flag a pending change. The X360 routes it through the owning
        // ScriptedFsm to drive the state transition; the minimal boot driver does not transition via this.
        u32 li = 0;
        if (lpacEvent != 0)
            for (; li < sizeof(macEvent) - 1u && lpacEvent[li] != 0; ++li)
                macEvent[li] = lpacEvent[li];
        macEvent[li] = 0;
        mbStateChangePending = true;
    }
}
