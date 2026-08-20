// ===================================================================================
// BrnGui::HudMessageDirector -- implementation (stop-flag / payback setters)
//   class:BrnGui::HudMessageDirector
//
// Reconstructed store-for-store from the X360 ARTIST build. Member-by-name access; the
// stop-flag events are published through mHudMessageQueue (the first member, so the X360
// passes the director `this` straight to VariableEventQueue<18432,16>::AddEvent).
// ===================================================================================
#include "GameSource/Gui/BrnGuiHudMessageDirector.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // gpDebugPrint, gxMessageFilterFlags

namespace BrnGui
{
    namespace
    {
        // The three queuing functions build a stop-flag event on the stack (a 16-byte record,
        // X360 `addi r4, r1, var_20`), then AddEvent it with type 0x9C and payload size 1. The
        // record bytes are not initialised by these functions (the event carries no payload of
        // its own beyond the size-1 marker), matching the X360.
        struct StopFlagEvent : public CgsModule::Event
        {
            u8 maRecord[16];
        };

        inline void QueueStopFlagEvent(CgsModule::VariableEventQueue<18432, 16>& lrQueue)
        {
            StopFlagEvent lEvent;
            lrQueue.AddEvent(&lEvent, HudMessageDirector::KI_STOP_FLAG_EVENT_TYPE, 1);
        }
    }

    // @ 0x8250C830 -- record the camera-sequence stop flag (stb arg, 0x54D0). When set, log
    // (gated on the message filter) and queue the stop-flag event.
    void HudMessageDirector::SetCameraSequenceFilter(bool lbShowing)
    {
        mbStopFlagCamera = lbShowing;
        if (lbShowing)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "HUD MSGS - D: MSG FILTER SET : CAMERA SEQUENCE IS SHOWING \n\n";
            }
            QueueStopFlagEvent(mHudMessageQueue);
        }
    }

    // @ 0x824EBEF8 -- store the active controller (stw arg, 0x4814) after asserting it is
    // non-null (BrnGuiHudMessageDirector.h:174).
    void HudMessageDirector::SetController(const BrnResource::HudMessageController* lpController)
    {
        CGS_ASSERT(lpController != 0,
                   "Invalid controller passed in to HudMessageDirector::SetController");
        mpController = lpController;
    }

    // @ 0x8250C8A8 -- begin the aggressor payback sequence (stw 1, 0x54D4). Log (gated) and
    // queue the stop-flag event.
    void HudMessageDirector::StartPaybackAggressor()
    {
        meStopFlagPaybackSequence = E_PAYBACK_AGGRESSOR;
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "HUD MSGS - D: MSG FILTER SET : AGGRESSOR SEQUENCE PLAYING \n\n";
        }
        QueueStopFlagEvent(mHudMessageQueue);
    }

    // @ 0x8250C920 -- begin the victim payback sequence (stw 2, 0x54D4). Log (gated) and
    // queue the stop-flag event.
    void HudMessageDirector::StartPaybackVictim()
    {
        meStopFlagPaybackSequence = E_PAYBACK_VICTIM;
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "HUD MSGS - D: MSG FILTER SET : VICTIM SEQUENCE PLAYING \n\n";
        }
        QueueStopFlagEvent(mHudMessageQueue);
    }
}
