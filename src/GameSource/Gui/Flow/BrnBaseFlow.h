#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/CgsEventObserver.h"        // CgsGui::EventObserver (base)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateMachine.h"// CgsGui::StateMachine (embedded), InputBuffer::GuiEventQueue

// BrnGui::BrnBaseFlow - the base of every GUI "flow" (HUD flow, screen flow, ...). A flow is a
// CgsGui::EventObserver that owns a CgsGui::StateMachine driving the flow's screen/HUD states,
// plus the streaming-mode + release-stage bookkeeping the GUI module sequences it with. Class
// shape, the EState/ReleaseStage enums and the member set are from the DecFIGS DWARF
// (BrnBaseFlow.h); the one ledger function attributed to this header (SetInEventQueue @
// 0x827E28A8) is bodied out-of-line in BrnBaseFlow.cpp.
//
// LAYOUT NOTE (X360): the embedded mStateMachine is the first member after the EventObserver
// base, and the X360 SetInEventQueue reaches it at this+0x10020 (`addis r3,this,1; addi r3,r3,0x20`
// == +65568). That offset is the size of the EventObserver base subobject -- dominated by the
// StateInterface's embedded GuiEventQueueLarge output queue -- so member-by-name access through the
// real base + member reproduces it; the numeric offset is not hardcoded here.
namespace CgsResource { struct LuaCodeResource; }   // GameShared/.../Fsm/Resources (PrepareLua arg, by ptr)
namespace CgsMemory   { class  HeapMalloc; }        // GameShared/.../Memory (PrepareLua arg, by ptr)
namespace rw          { struct IResourceAllocator; }

namespace BrnGui
{
    class GuiCache;   // GameSource/Gui/BrnGuiCache.h (held by pointer only)

    struct BrnBaseFlow : public CgsGui::EventObserver
    {
        // BrnBaseFlow.h:51 (DWARF) -- staged teardown the GUI module walks during Release().
        enum ReleaseStage
        {
            E_RELEASESTAGE_START              = 0,
            E_RELEASESTAGE_LEAVESTATE         = 1,
            E_RELEASESTAGE_RELEASESTATEMACHINE = 2,
            E_RELEASESTAGE_DONE               = 3,
        };

        // BrnBaseFlow.h:101 (DWARF) -- which resource set the flow streams.
        enum EStreamingMode
        {
            E_STREAMING_OFF     = 0,
            E_STREAMING_CURRENT = 1,
            E_STREAMING_PRELOAD = 2,
            E_STREAMING_COUNT   = 3,
        };

        CgsGui::StateMachine& GetStateMachine() { return mStateMachine; }   // BrnBaseFlow.h:171

        // ---- lifecycle (reconstructed from BURNOUT_X360_ARTIST.XEX; bodies in BrnBaseFlow.cpp) ----

        // @ 0x824F1BC0 -- EventObserver::Construct(), bring the embedded state machine up, then
        // stash the GUI cache and reset the streaming/release bookkeeping. Virtual (the concrete
        // flow, e.g. BrnHudFlow, overrides to chain in its own Construct).
        virtual void Construct(GuiCache* lpGuiCache);

        // @ 0x824F1C38 -- EventObserver::Prepare(access pointers, allocator) then wire the owned
        // state machine to the flow's StateInterface. (BrnHudFlow adds a wider Prepare overload
        // that also builds the state pool.)
        virtual bool Prepare(CgsGui::GuiAccessPointers* lpAccessPointers,
                             rw::IResourceAllocator* lpAllocator);

        // @ 0x824F1DC0 -- staged teardown driven by mReleaseStage (LeaveState -> ReleaseStateMachine
        // -> Done); returns true once the state machine has been released.
        virtual bool Release();

        // @ 0x82507FD8 -- per-frame: pump the FSM (current state Pre/Update/Post), and when the
        // script's state sequence changes, kick the next-state resource streaming.
        virtual void Update();

        // @ 0x82514DB8 -- forward PreWorldUpdate to the current state, then (video/save-load states)
        // raise the matching GUI notification event.
        virtual void PreWorldUpdate();

        // @ 0x827E28A8 -- assert the queue is non-null, then forward it to the owned state machine.
        virtual void SetInEventQueue(InputBuffer::GuiEventQueue* lpInEventQueue);

        // @ 0x824F1C78 -- compile + enter the FSM's Lua script: assert the resource + heap, run
        // ScriptedFsm::Prepare(luaCode, heap, initialStateId), and on success reset the release
        // stage to START. The controller calls this once the flow's LuaCode resource has loaded.
        bool PrepareLua(CgsResource::LuaCodeResource* lpLuaCodeResource,
                        CgsMemory::HeapMalloc* lpHeapMalloc, CgsID lInitialStateId);

    protected:
        // @ 0x824FFD48 -- preload the resources the current (and next reachable) states need.
        void UpdateStreaming(s32 liStateIndex, EStreamingMode meStreamingMode);

        CgsGui::StateMachine mStateMachine;        // BrnBaseFlow.h:128 (X360 +0x10020)
        ReleaseStage         mReleaseStage;        // BrnBaseFlow.h:129
        GuiCache*            mpGuiCache;           // BrnBaseFlow.h:130
        u32                  muFsmSequenceNumber;  // BrnBaseFlow.h:132
        EStreamingMode       meStreamingMode;      // BrnBaseFlow.h:134
    };
}
