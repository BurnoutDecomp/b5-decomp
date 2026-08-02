// ===================================================================================
// BrnGui::OnlineGameOptions -- wave-I partfile 03: the three small in-queue handlers.
//   HandleControllerInput    @0x824AD758
//   HandleInGameEvent        @0x8249C9D0
//   HandleInGameFailedEvent  @0x8249CAE0
//
//
// The committed leaf header BrnOnlineGameOptions.h is still the MINIMAL pre-wave version
// (the GetResourcesToLoad inline plus the two resource statics). The wave-I spec's §H1
// class extension had not been applied when this partfile was written, and headers are
// frozen for implementers, so none of the three bodies can name themselves as members, nor
// reach meSubState / ESubState / mpGuiCache / the two HandleControllerInput* callees.
// HandleInGameFailedEvent additionally needs a second, SEPARATE header change: GuiCache's
// mbOnlineStartPending (BrnGuiCache.h:759) is PRIVATE with no read accessor, so this class
// needs the same `friend struct OnlineGameOptions;` grant the wave-C HudMessageAnalyzer and
// wave-H OnlineGameRoomPlayerInfo keystones already hold. Measured with the compile gate
// and a stand-in-class compile probe, not assumed.
//
// The three complete bodies live at, each with a banner naming the exact declaration lines
// that unblock it:
// They concatenate into this file (single `namespace BrnGui { ... }`) once §H1 and the
// BrnGuiCache.h friendship land. MERGE NOTE: the InGameEvent and InGameFailedEvent files
// each carry their own anonymous-namespace `const s32 KI_CHANNEL_GUI_OUT = 40;` -- keep
// exactly one of the two on merge; the union of the three include sets is
//   BrnOnlineGameOptions.h, CgsAssert.h, CgsGuiEvent.h, CgsGuiStateInterface.h,
//   CgsVariableEventQueue.h, BrnGuiCache.h, BrnGuiOverlaysDirector.h.
//
// LINK NOTE for the conductor: HandleControllerInput dispatches to
// HandleControllerInputCreateGame (@0x824A7878, wave-I group 09) and
// HandleControllerInputLoadOptions (@0x824A7E48, FOREIGN -- ledger `reviewed` but defined
// nowhere in the tree). Both are declaration-only at the moment, so the eventual link will
// miss HandleControllerInputLoadOptions until its own TU lands.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::Event
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiOverlaysDirector.h"                        // GuiOverlayWaitFinishRequest (id 188)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventNetworkSuspension
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache

namespace BrnGui
{

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x824AD758.json, asm arbitrated over Hex-Rays).
//
// The dispatch reads ONE word -- `lwz r11, 0x38(r28)` == meSubState -- then compares it
// against 2 and 3; every other sub-state falls straight through to the epilogue with no
// action. 0x38 == 56 is the CONSOLE offset of meSubState and is deliberately NOT
// reproduced: the member is reached by name so the host's own layout applies. The
// parameter is the header-stripped in-queue payload view (const CgsModule::Event*, the
// wave-H precedent); this function never dereferences it, it only null-asserts and
// forwards. No floats here, so there is no NaN-polarity decision to make.
    // ================================================================================
    //  HandleControllerInput  @ 0x824AD758  (cpp:457)
    //
    //  Update's controller-event arm: hand the pad event to whichever sub-state owns the
    //  input. Only the two interactive sub-states take it -- while the screen is still
    //  loading, or is waiting on the game, the event is consumed with no action.
    // ================================================================================
    void OnlineGameOptions::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineGameOptions::HandleControllerInput");   // cpp:457 (non-fatal)

        if (meSubState == E_SUBSTATE_SELECTING_PARAMS)
        {
            HandleControllerInputCreateGame(lpEvent);
        }
        else if (meSubState == E_SUBSTATE_LOAD_OPTIONS)
        {
            HandleControllerInputLoadOptions(lpEvent);
        }
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8249C9D0.json, asm arbitrated over Hex-Rays).
//
// The X360 fires TWO asserts back to back off the SAME null test (lines 2226 and 2228 --
// the first carries a copy-pasted OnlineSelectRoute message), then -- unconditionally,
// unlike the game-room twin, which gates on the payload's in-game byte -- Constructs the
// overlay wait-finish request onto a stack record and publishes it before advancing the
// flow. This handler never dereferences its event pointer at all.
//
// The record the asm stack-builds (0x8249CA90..CAC4) is { 8, 188, 16 } + an 8-aligned
// 8-byte payload, queued with channel 0x28 (40) and size 0x18 (24). Those console
// immediates live in comments only: the wire type below derives every one of them from a
// host sizeof/offset (verified in a scratch compile probe -- host sizeof of the record is
// 24, matching the console record size because the payload is a pointer-free CgsID).
// No floats here, so there is no NaN-polarity decision to make.
    namespace
    {
        // The AddEvent channel selector word (the queue's "type" argument): 40 is the
        // OutputGuiEvent channel this record is published on (`li r5, 0x28`).
        const s32 KI_CHANNEL_GUI_OUT = 40;

        // Id 188 -- the overlay wait-finish handshake. The X360 stack-builds { 8, 188, 16 }
        // and drops the 8-byte compressed overlay id at the 8-aligned payload slot, then
        // publishes 24 bytes on channel 40. Same wire the wave-H game-room screen posts
        // (BrnOnlineGameRoomPlayerInfo_wH_00.cpp), re-declared here because the two screens
        // are separate translation units.
        typedef CgsGui::GuiEvent<188> OverlayWaitFinishHeader;
        struct GuiOverlayWaitFinishWire : public OverlayWaitFinishHeader
        {
            u32                         muPad0C;    // +0x0C (payload alignment pad)
            GuiOverlayWaitFinishRequest mRequest;   // +0x10 (the compressed overlay id)

            GuiOverlayWaitFinishWire()
                : OverlayWaitFinishHeader(
                      static_cast<u32>(sizeof(GuiOverlayWaitFinishRequest)),
                      static_cast<u32>(sizeof(OverlayWaitFinishHeader) + sizeof(u32)))
                , muPad0C(0)
            {
            }
        };

        // Publish the id-188 record for one overlay name -- Construct the request straight
        // onto the wire record, then queue it (the X360 does exactly this inline).
        void PostOverlayWaitFinish(CgsGui::StateInterface* lpStateInterface,
                                   const char* lpacOverlayName)
        {
            GuiOverlayWaitFinishWire lWaitFinish;
            lWaitFinish.mRequest.Construct(lpacOverlayName);
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWaitFinish), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lWaitFinish)));   // X360 record size 24
        }
    }

    // ================================================================================
    //  HandleInGameEvent  @ 0x8249C9D0  (cpp:2226)
    //
    //  The match is going in-game: hold the flow until the "entering game" overlay has
    //  finished showing, then advance out of the options screen.
    // ================================================================================
    void OnlineGameOptions::HandleInGameEvent(const CgsModule::Event* lpInGameEvent)
    {
        // Two asserts fire back to back off the same null test; the first carries the
        // original's copy-pasted OnlineSelectRoute message.
        CGS_ASSERT(lpInGameEvent != 0,
                   "Invalid event sent to OnlineSelectRoute::HandleInGameEvent");   // cpp:2226 (non-fatal)
        CGS_ASSERT(lpInGameEvent != 0, "lpInGameEvent");                            // cpp:2228 (non-fatal)

        PostOverlayWaitFinish(mpStateInterface, "CNOnlEntGame");
        SendStateEvent("ADVANCE");
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8249CAE0.json, asm arbitrated over Hex-Rays).
//
// The cache byte is reached as `addis r31, r27, 1 / addi r31, r31, -0x5B00` == this+0xA500
// (42240, mpGuiCache) then `lbz r11, 0x4B53(r11)` == GuiCache+19283, mbOnlineStartPending.
// Both console offsets stay in this comment only -- the members are reached by name so the
// host's own layout applies. Store order follows the asm exactly: publish the record, THEN
// clear the flag, THEN send the state event.
//
// The record the asm stack-builds (0x8249CB94..CBB8) is { 4, 45, 12, 0 } queued with
// channel 0x28 (40) and size 0x10 (16) -- which is exactly the committed
// CgsGui::GuiEventNetworkSuspension(false) (CgsGuiStateInterface.h), whose host sizeof is
// 16, matching the console record size because the payload is a single pointer-free word
// (verified in a scratch compile probe). No floats here, so there is no NaN-polarity
// decision to make.
    namespace
    {
        // The AddEvent channel selector word (the queue's "type" argument): 40 is the
        // OutputGuiEvent channel this record is published on (`li r5, 0x28`).
    }

    // ================================================================================
    //  HandleInGameFailedEvent  @ 0x8249CAE0  (cpp:2250)
    //
    //  The match failed to go in-game. If this screen is the one that armed the online
    //  start, it owns the un-suspend and backs out on the quiet path; otherwise it just
    //  backs out.
    // ================================================================================
    void OnlineGameOptions::HandleInGameFailedEvent(const CgsModule::Event* lpInGameFailedEvent)
    {
        // Same copy-pasted OnlineSelectRoute message the in-game arm above fires.
        CGS_ASSERT(lpInGameFailedEvent != 0,
                   "Invalid event sent to OnlineSelectRoute::HandleInGameEvent");   // cpp:2250 (non-fatal)

        if (mpGuiCache->mbOnlineStartPending)
        {
            // Let the network layer resume before the flow drops back a screen. Posted
            // straight onto the out-queue rather than through
            // StateInterface::OutputGuiEvent, whose committed body passes the event id as
            // the channel argument where the X360 passes 40.
            CgsGui::GuiEventNetworkSuspension lNetworkSuspension(false);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lNetworkSuspension),
                KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lNetworkSuspension)));   // X360 record size 16

            mpGuiCache->mbOnlineStartPending = false;
            SendStateEvent("GO_BACK_EASY");
        }
        else
        {
            SendStateEvent("GO_BACK");
        }
    }
}
