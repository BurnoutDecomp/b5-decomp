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

        // @ 0x827E28A8 -- assert the queue is non-null, then forward it to the owned state machine.
        // Bodied out-of-line in BrnBaseFlow.cpp.
        virtual void SetInEventQueue(InputBuffer::GuiEventQueue* lpInEventQueue);

    protected:
        CgsGui::StateMachine mStateMachine;        // BrnBaseFlow.h:128 (X360 +0x10020)
        ReleaseStage         mReleaseStage;        // BrnBaseFlow.h:129
        GuiCache*            mpGuiCache;           // BrnBaseFlow.h:130
        u32                  muFsmSequenceNumber;  // BrnBaseFlow.h:132
        EStreamingMode       meStreamingMode;      // BrnBaseFlow.h:134
    };
}
