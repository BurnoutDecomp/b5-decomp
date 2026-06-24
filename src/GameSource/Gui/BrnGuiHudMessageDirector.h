#pragma once

// ===================================================================================
// BrnGui::HudMessageDirector -- owning header
//   b5-decomp/src/GameSource/Gui/BrnGuiHudMessageDirector.h
//   class:BrnGui::HudMessageDirector
//
// Drives the on-screen HUD message flow: it owns the GUI event queue the messages are
// published into (mHudMessageQueue, the first member at +0x00 -- a
// CgsModule::VariableEventQueue<18432,16>), the active controller, and the "stop flag"
// state used by the payback/camera-sequence cinematics.
//
// This header bodies the four recovered setter/payback functions; the rest of the director
// (Construct/Update/AddMessage/FilterAndSendOffMessage/...) is owned by other TUs. Member
// list/order from the DecFIGS DWARF (BrnGuiHudMessageDirector.h:51). Member-by-name access
// (the gate compiles 64-bit, so guest offsets are illustrative); the unrecovered aggregate
// members the four functions do not touch (mCachedMessage, mauMessagesLastTriggered, ...) are
// modelled as reserved spans so the touched members keep their relative position.
//
// Recovered functions (X360 ARTIST):
//   SetCameraSequenceFilter(bool) @ 0x8250C830 -> mbStopFlagCamera = arg; if(arg) { log;
//                                                  mHudMessageQueue.AddEvent(&evt, 0x9C, 1) }
//   SetController(ctrl)           @ 0x824EBEF8 -> assert ctrl != null
//                                                  (BrnGuiHudMessageDirector.h:174); mpController = ctrl
//   StartPaybackAggressor()       @ 0x8250C8A8 -> meStopFlagPaybackSequence = E_PAYBACK_AGGRESSOR;
//                                                  log; mHudMessageQueue.AddEvent(&evt, 0x9C, 1)
//   StartPaybackVictim()          @ 0x8250C920 -> meStopFlagPaybackSequence = E_PAYBACK_VICTIM;
//                                                  log; mHudMessageQueue.AddEvent(&evt, 0x9C, 1)
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // VariableEventQueue<18432,16>

namespace CgsGui { class ModelModule; }

namespace BrnGui
{
    class GuiCache;
    class HudMessageController;

    struct HudMessageDirector
    {
        // DWARF BrnGuiHudMessageDirector.h:114 -- which payback cinematic is being shown.
        enum EPayback
        {
            E_PAYBACK_NONE      = 0,
            E_PAYBACK_AGGRESSOR = 1,
            E_PAYBACK_VICTIM    = 2,
            E_PAYBACK_COUNT     = 3,
        };

        // The published HUD message event type the payback/camera-filter functions queue.
        static const s32 KI_STOP_FLAG_EVENT_TYPE = 0x9C;  // 156

        // @ 0x8250C830 -- set the camera-sequence stop flag; when set, queue the stop event.
        void SetCameraSequenceFilter(bool lbShowing);
        // @ 0x824EBEF8 -- set the active controller (asserts non-null).
        void SetController(const HudMessageController* lpController);
        // @ 0x8250C8A8 -- begin the aggressor payback sequence; queue the stop event.
        void StartPaybackAggressor();
        // @ 0x8250C920 -- begin the victim payback sequence; queue the stop event.
        void StartPaybackVictim();

        // ----- recovered layout (member-by-name; DWARF member order) -----
        // mHudMessageQueue is the first member: the X360 passes the director `this` directly as
        // the queue `this` to AddEvent, so &mHudMessageQueue == this (offset +0x00).
        CgsModule::VariableEventQueue<18432, 16> mHudMessageQueue;        // +0x00
        CgsGui::ModelModule*        mpModelModule;                        // (after the queue)
        const HudMessageController* mpController;                         // X360 +0x4814
        const BrnGui::GuiCache*     mpGuiCache;
        bool                        mbLastFrameHudMessageState;
        bool                        mbMessagePending;
        // mCachedMessage (GuiHudMessage) + mauMessagesLastTriggered[300] -- not touched by the
        // four recovered functions; modelled as reserved spans (sized to the DWARF: a u64[300]
        // table plus the cached-message aggregate) so the stop-flag members keep their position.
        u8                          maCachedMessageReserved[64];          // GuiHudMessage (opaque)
        u64                         mauMessagesLastTriggered[300];        // DWARF :140
        bool                        mbStopFlagBlackBar;
        f32                         mfBlackBarSize;
        bool                        mbStopFlagCamera;                     // X360 +0x54D0
        EPayback                    meStopFlagPaybackSequence;            // X360 +0x54D4
    };
}
