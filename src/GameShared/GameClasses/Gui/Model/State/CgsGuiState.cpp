#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"   // CgsGui::State (+ ScriptedState base, StateInterface fwd, sResourceTuple fwd)

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsGui::State base bodies. The X360 keeps these out-of-line in CgsGuiState.cpp; the GUI virtuals are
// no-op defaults (derived states override OnEnter/OnLeave/Update/GetResourcesToLoad) and the setters just
// store the channel/queue. Reconstructed minimally to close the link for the boot HUD-flow states.
//
// SetStateInterface @ 0x82846838 is the X360-emitted out-of-line setter for this class (its own ledger
// TU): assert the incoming interface is non-null, then store it to mpStateInterface (this+0x1C, the
// `stw r28, 0x1C(r27)` tail store). The original streamed the d:\p4-baked file/line via CgsDev::Assert;
// under CGS_ASSERT the failure path forwards the plain condition string (file/line dropped per policy).

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
        mpInGuiEventQueue = 0;       // ARTIST 0x82848508: stw 0,0x18
        mpStateInterface = 0;        // stw 0,0x1C
        mbIsSaveLoadState = false;   // stb 0,0x31  (FLAG: was missing -- ARTIST zeroes both bools here)
        mbIsVideoState = false;      // stb 0,0x32
        mbStateChangePending = false;// stb 0,0x20
    }

    void State::PreWorldUpdate() {}
    void State::PreUpdate()      {}
    void State::PostUpdate()     {}

    void State::GetResourcesToLoad(const sResourceTuple** lppResourceTuples, u32* lpuNumberOfResources) const
    {
        *lppResourceTuples = 0;
        *lpuNumberOfResources = 0;
    }

    // X360 0x82846838. Validate the interface pointer, then store it (this+0x1C).
    void State::SetStateInterface(StateInterface* lpStateInterface)
    {
        CGS_ASSERT(lpStateInterface != 0,
                   "Invalid state interface sent to State::SetStateInterface");
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
