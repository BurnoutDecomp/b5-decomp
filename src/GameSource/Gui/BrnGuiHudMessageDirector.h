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
#include "BrnCommonTypes.h"                                       // CgsID (u64), IsMessageAllowed param
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // VariableEventQueue<18432,16>
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                   // [gateui] GuiHudMessage (mCachedMessage, by value)

// [gateui] The HUD-message controller is BrnResource::HudMessageController
// (SharedClasses/DataLists/BrnHudMessageController.h). It used to be forward-declared a
// SECOND time as `BrnGui::HudMessageController`, an unrelated empty type that the two
// consumers then reinterpret_cast across; the X360 asm settles it (the director's
// mpController is passed straight to BrnResource::HudMessageController::
// GetIndexFromMessageHash @0x8267D4C8 in FilterAndSendOffMessage @0x82511640), so the fork
// is gone and the real type is named here.
namespace BrnResource { struct HudMessageController; }

// [gateui] DWARF BrnGuiHudMessageDirector.h:131 gives mpModelModule as CgsGui::ModelModule*
// and h:62 gives Update(InputBuffer*) -- the model-module IO input buffer (X360
// GuiModule::Update @0x82527A58 line 1035 passes the same buffer it hands GuiCache::Update).
// Pointer-only here so this boundary header does not drag CgsModelModuleIO.h (and with it the
// 32 KB GUI event-queue templates) into every GuiModule consumer; the .cpp includes it.
namespace CgsGui
{
    class ModelModule;
    namespace ModelIO { struct InputBuffer; }
}

namespace BrnGui
{
    class GuiCache;

    struct GuiHudMessage;   // GameSource/Gui/BrnGuiEventTypeDefs.h

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

        // [gateui] The event type FilterAndSendOffMessage @0x82511824 publishes a whole
        // GuiHudMessage record under (`li r5, 0x9A`, size `li r6, 0x348` == the X360
        // sizeof(GuiHudMessage)).
        static const s32 KI_HUD_MESSAGE_EVENT_TYPE = 0x9A;  // 154

        // [gateui] The three bits CheckMessageIsAvailable @0x824F2B28 tests in a message's
        // GuiHudMessageData::muAvailabilityBitSet (X360 `andi. 8` / `andi. 0x10` /
        // `andi. 0x20`). FLAG: consumer-named -- the console has no enum for them in any
        // recovered source; the names come from the log line each failing test prints
        // ("Not available offline : " / "Not available online : " /
        // "Not available while crashed").
        static const u32 KU_AVAILABLE_OFFLINE        = 0x08;
        static const u32 KU_AVAILABLE_ONLINE         = 0x10;
        static const u32 KU_AVAILABLE_WHILE_CRASHED  = 0x20;

        // [gateui r2] @ 0x82508FA0 (DWARF h:58, cpp:80/81) -- adopt the model module + the
        // cache, null the controller and the two frame-state bytes, ZERO the 300-slot
        // repeat-rate table, Construct+Clear mHudMessageQueue, and clear the three stop
        // flags. This is the never-Constructed-object fix from the round-1 verify's
        // SUSPECT-1: nothing in the tree ran this, so mauMessagesLastTriggered was read
        // uninitialised by the very first FilterAndSendOffMessage rate gate.
        // (mfBlackBarSize is NOT written by the console's Construct -- reproduced.)
        void Construct(CgsGui::ModelModule* lpModelModule, const GuiCache* lpGuiCache);

        // [gateui r2] @ 0x82516100 (DWARF h:62) -- the per-frame drive, called from
        // GuiModule::Update. Two halves:
        //   1. the DEFERRED-message arm: when the gameplay HUD has just come up
        //      (mpGuiCache->mbGameplayHudActive == 1 with mbLastFrameHudMessageState still
        //      false) and a message is pending, send mCachedMessage; then latch the HUD
        //      state and, if the HUD is up while a message is still pending, wipe the cache.
        //   2. the DRAIN: if mHudMessageQueue holds anything, append the whole queue into
        //      the model module's input GUI-event queue (under its write lock) and Clear.
        void Update(CgsGui::ModelIO::InputBuffer* lpModelInputBuffer);

        // @ 0x825161E8 (DWARF h:70) -- push one HUD message into the director: assert the
        // message + the controller, then hand it to FilterAndSendOffMessage. lbForce
        // bypasses the availability/repeat-rate gate (the HudMessageAnalyzer's two
        // TriggerMessage overloads @0x825179E8/@0x82517AF8 pass false).
        void AddMessage(const GuiHudMessage* lpMessage, bool lbForce);

        // @ 0x824F2D30 (DWARF h:109) -- is the message with this hash still permitted to show
        // given the current stop-flag state? While the black bar is up ONLY
        // KA_BLACK_BAR_EXEMPT_MSGS may show; a camera sequence blocks everything; a payback
        // sequence admits only KA_PAYBACK_EXEMPT_MSGS. (InGameMessagesComponent::
        // TerminateMessages @0x824376F0 also queries it per live slot, to retire messages the
        // director no longer allows.)
        bool IsMessageAllowed(CgsID lMessageId) const;

        // @ 0x8250C830 -- set the camera-sequence stop flag; when set, queue the stop event.
        void SetCameraSequenceFilter(bool lbShowing);
        // @ 0x824EBEF8 -- set the active controller (asserts non-null).
        void SetController(const BrnResource::HudMessageController* lpController);
        // @ 0x8250C8A8 -- begin the aggressor payback sequence; queue the stop event.
        void StartPaybackAggressor();
        // @ 0x8250C920 -- begin the victim payback sequence; queue the stop event.
        void StartPaybackVictim();

    private:
        // @ 0x82511640 (DWARF h:152) -- the real gate behind AddMessage: resolve the message
        // hash to a controller index, check availability + the per-message repeat rate
        // (mauMessagesLastTriggered), check the stop flags, then publish the whole 840-byte
        // (X360) message record onto mHudMessageQueue as event 154. Returns whether it went.
        bool FilterAndSendOffMessage(const GuiHudMessage* lpMessage, bool lbForce);

        // @ 0x824F2B28 (DWARF h:156) -- test one message's availability bitset against the
        // cache's current game mode / crash state.
        bool CheckMessageIsAvailable(u32 luAvailabilityBitSet) const;

        // [gateui] DWARF h:124/:125/:127/:128 -- the two stop-flag exemption tables and their
        // counts. Values are the X360 .data blocks qword_82FB5610 (7) and qword_82FB5668 (14),
        // both zero in the image and filled by their dynamic initialisers @0x82C55E48 /
        // @0x82C55F00; definitions in BrnGuiHudMessageDirector_gUI_00.cpp.
        static const s32   KI_NUM_PAYBACK_EXEMPT_MSGS   = 7;    // h:125
        static const s32   KI_NUM_BLACK_BAR_EXEMPT_MSGS = 14;   // h:128
        static const CgsID KA_PAYBACK_EXEMPT_MSGS[KI_NUM_PAYBACK_EXEMPT_MSGS];     // h:124
        static const CgsID KA_BLACK_BAR_EXEMPT_MSGS[KI_NUM_BLACK_BAR_EXEMPT_MSGS]; // h:127

        // h:122 -- SetBlackBarSize's threshold (its body is another TU). [gateui r2] the
        // VALUE is now recovered and defined out-of-line in BrnGuiHudMessageDirector_gUI_00.cpp:
        // SetBlackBarSize @0x82511848 loads `flt_82004014` (0.1f) once and compares BOTH the
        // incoming size and the stored mfBlackBarSize against it (`lfs f0, flt_82004014`
        // @0x82511868, `fcmpu cr6, f31, f0` / `fcmpu cr6, f13, f0`), i.e. the bar counts as
        // "up" at >= 0.1. Previously declared with no definition anywhere -- an LNK2001 the
        // day SetBlackBarSize lands (round-1 verify NOTE-4).
        static const f32   KF_MINIMUM_BLACK_BAR_STOP_SIZE;

    public:
        // ----- recovered layout (member-by-name; DWARF member order) -----
        // mHudMessageQueue is the first member: the X360 passes the director `this` directly as
        // the queue `this` to AddEvent, so &mHudMessageQueue == this (offset +0x00).
        CgsModule::VariableEventQueue<18432, 16> mHudMessageQueue;        // +0x00
        CgsGui::ModelModule*        mpModelModule;                        // (after the queue)
        const BrnResource::HudMessageController* mpController;            // X360 +0x4814
        const BrnGui::GuiCache*     mpGuiCache;
        bool                        mbLastFrameHudMessageState;           // DWARF :136 (X360 +0x481C)
        bool                        mbMessagePending;                     // DWARF :137 (X360 +0x481D)
        // [gateui] mCachedMessage was a 64-byte opaque reserve. It is a REAL GuiHudMessage and
        // the X360 arithmetic proves it exactly: FilterAndSendOffMessage @0x825117DC indexes
        // mauMessagesLastTriggered as `(liIndex + 0x96D) * 8` off `this`, i.e. base
        // 0x96D*8 == 19304; mpGuiCache sits at 18456, so 18456 + 4 (ptr) + 2 (bools) + 2 (pad)
        // == 18464 and 19304 - 18464 == 840 == the X360 sizeof(GuiHudMessage). Named for real.
        GuiHudMessage               mCachedMessage;                       // DWARF :138 (X360 +0x4820, 840B)
        u64                         mauMessagesLastTriggered[300];        // DWARF :140 (X360 +0x4B68)
        bool                        mbStopFlagBlackBar;
        f32                         mfBlackBarSize;
        bool                        mbStopFlagCamera;                     // X360 +0x54D0
        EPayback                    meStopFlagPaybackSequence;            // X360 +0x54D4
    };
}
