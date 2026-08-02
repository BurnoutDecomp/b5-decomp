// wave-I partfile

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"
#include <cstring>                                                       // std::memcpy
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                      // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface / GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                  // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // GuiOverlayRequest
#include "GameSource/Gui/BrnGuiOverlaysDirector.h"                       // GuiOverlayWaitFinishRequest (the 188 payload)
#include "GameShared/GameClasses/Core/CgsID.h"                           // CgsID / CgsIDCompress
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // GuiOverlayCompleteEvent::LeaveMethod
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // GuiOverlayRequest / GuiEventActivateCrashNav / GuiOverlayCompleteEvent

namespace BrnGui
{

    // MERGE NOTE: this anonymous-namespace block is the SUBSET of
    // _07_HandleShowLoginQuestion.cpp's that this body uses. Drop it when merging.
    namespace
    {
        const s32 KI_CHANNEL_GUI_OUT = 40;   // GuiEventOut -- the AddEvent channel every site here uses

        // { 8, 188, 16, <pad>, the compressed overlay id }, channel 40, 24 bytes.
        struct GuiOverlayWaitFinishWire : public CgsGui::GuiEvent<188>
        {
            u32                         muPad0C;    // +0x0C (the payload is 8-aligned)
            GuiOverlayWaitFinishRequest mRequest;   // +0x10

            GuiOverlayWaitFinishWire()
                : CgsGui::GuiEvent<188>(static_cast<u32>(sizeof(GuiOverlayWaitFinishRequest)), 16)
                , muPad0C(0)
            {
            }
        };

        // { 288, 184, 16, <pad>, the 288-byte request }, channel 40, 304 bytes -- the
        // shape the out-of-line OutputGuiEvent<GuiOverlayRequest> instantiation
        // @0x82436BE0 builds (memcpy 0x120 into record+0x10, then the three header
        // words, then AddEvent(queue, record, 0x28, 0x130)).
        struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
        {
            u32               muPad0C;    // +0x0C (the payload is 16-aligned past the header)
            GuiOverlayRequest mRequest;   // +0x10

            GuiOverlayRequestWire()
                : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
                , muPad0C(0)
            {
            }
        };

        // Record-size pins: every payload here is pointer-free, so the X360's AddEvent
        // size immediates must survive unchanged on the x64 host.
        static_assert(sizeof(GuiOverlayWaitFinishWire) == 24, "wait-finish record is 24 bytes");
        static_assert(sizeof(GuiOverlayRequestWire) == 304, "overlay request record is 304 bytes");
        static_assert(sizeof(CgsGui::GuiEventNetworkSuspension) == 16, "suspension record is 16 bytes");

        const char KAC_STATE_EVENT_GO_BACK[]      = "GO_BACK";
        const char KAC_STATE_EVENT_GO_BACK_EASY[] = "GO_BACK_EASY";

        // The message-param slot this handler fills with the disconnect reason text
        // (the X360's `li r4, 1` at 0x824D89E0).
        const u32 KU_DISCONNECT_REASON_MESSAGE_PARAM = 1;
    }

    // ---- HandleDisconnectedEvent @ 0x824D8830 (cpp:873) -----------------------------
    // In-queue event 44. The sign-in attempt fell over. While the screen is still
    // bringing itself up there is nothing to show the message on, so the whole payload
    // is cached and replayed by CheckForCompletedLoads once the components exist.
    // Otherwise the disconnect either backs the player out without a popup (the paths
    // that set mbIgnoreDisconnectedPopup asked for the disconnect themselves) or raises
    // the failure overlay / the login-error page.
    void CrashNavEnterOnlineBase::HandleDisconnectedEvent(const CgsModule::Event* lpDisconnectedEvent)
    {
        CGS_ASSERT(lpDisconnectedEvent != 0, "lpDisconnectedEvent");   // cpp:878

        // The in-queue hands out CgsGui::GuiEventNetworkDisconnected itself: the publisher
        // @0x825660B0 posts the object raw (size 0x84), so there is no header to step over
        // and meLastError really is at payload+0.
        const CgsGui::GuiEventNetworkDisconnected* lpPayload =
            reinterpret_cast<const CgsGui::GuiEventNetworkDisconnected*>(lpDisconnectedEvent);

        // Two separate equality tests in the asm (beq on 0 then beq on 1 at
        // 0x824D8874..0x824D8880), not an unsigned `< 2` range check.
        if (meSubState == E_SUBSTATE_LOADING_SCREEN ||
            meSubState == E_SUBSTATE_LOADING_COMPONENTS)
        {
            // Stash the record verbatim -- the X360's memcpy of 0x84 bytes; the payload
            // is pointer-free so the host sizeof is the same 132.
            std::memcpy(&mCachedHandleDisconnectEvent, lpPayload,
                        sizeof(CgsGui::GuiEventNetworkDisconnected));
            mbCachedDisconnectEventToHandle = true;
            return;
        }

        {
            GuiOverlayWaitFinishWire lWaitConnecting;
            lWaitConnecting.mRequest.Construct(KAC_CONNECTING_STRING_ID);

            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWaitConnecting), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lWaitConnecting)));
        }

        // cpp:892 -- the second, streamed assert on the same pointer (the X360 fires
        // both, the bare one above and this one after the overlay handshake).
        CGS_ASSERT(lpDisconnectedEvent != 0,
                   "Invalid event sent to CrashNavEnterOnlineBase::HandleDisconnectedEvent");

        if (mbIgnoreDisconnectedPopup)
        {
            // The disconnect was asked for: leave without raising a popup.
            if (mbJunkyardEntered)
            {
                HandleEnteringJunkyard();
            }
            else if (mpGuiCache->IsOnlineStartPending())
            {
                // A queued online start is still pending -- park the network side and
                // take the cheap route back out. The X360 stores one CLEAR payload word
                // (stw r27, with r27 == 0, at 0x824D8964) into the record it hands the
                // out-of-line OutputGuiEvent<GuiEventNetworkSuspension> instantiation,
                // which posts { 4, 45, 12, 0 } on channel 40.
                CgsGui::GuiEventNetworkSuspension lSuspension(false);

                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lSuspension), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lSuspension)));

                mpGuiCache->SetOnlineStartPending(false);
                SendStateEvent(KAC_STATE_EVENT_GO_BACK_EASY);
            }
            else
            {
                SendStateEvent(KAC_STATE_EVENT_GO_BACK);
            }

            // All three arms drop the cached record (the X360 repeats the same three
            // stores per arm -- stb +0x3754, stw +0x3750, stb +0x37EB).
            mCachedHandleDisconnectEvent.macReason[0] = 0;
            mCachedHandleDisconnectEvent.meLastError  = 0;
            mbCachedDisconnectEventToHandle           = false;
            return;
        }

        GuiOverlayRequestWire lWire;
        if (lpPayload->macReason[0] != 0)
        {
            // The server sent wording of its own: show it as the overlay's message.
            lWire.mRequest.Construct(KAC_DISCONNECTED_WITH_REASON_STRING_ID);
            lWire.mRequest.AddMessageParam(KU_DISCONNECT_REASON_MESSAGE_PARAM, lpPayload->macReason);
        }
        else if (lpPayload->meLastError != 0)
        {
            // A coded failure goes to the dedicated login-error page instead of an
            // overlay -- and the X360 returns straight out of here (0x824D8A04), so the
            // cached record is deliberately NOT cleared on this arm.
            ShowLoginError(lpPayload->meLastError);
            return;
        }
        else
        {
            lWire.mRequest.Construct(KAC_DISCONNECTED_STRING_ID);
        }

        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lWire)));

        // Everything the sign-in flow had on screen goes away behind the overlay, in the
        // X360's own call order (0x824D8A40 onwards). off_82F26E68 is
        // KAPC_ANIMATION_STATES[1] == "Invisible".
        mShareInfoTogglesAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mButtonPromptAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mSignInBackgroundAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mTOSDisplayAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mTOSText.SetText("");
        mMessageAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);
        mMessageButtonsAnimation.AddOutputAptViewState("apt_Transition", KAPC_ANIMATION_STATES[1], false);

        mCachedHandleDisconnectEvent.macReason[0] = 0;
        mCachedHandleDisconnectEvent.meLastError  = 0;
        mbCachedDisconnectEventToHandle           = false;
        meSubState                                = E_SUBSTATE_DISCONNECTED;
    }

    // MERGE NOTE: this anonymous-namespace block is the SUBSET of
    // _07_HandleShowLoginQuestion.cpp's that this body uses. Drop it when merging.
    namespace
    {

        // { 8, 188, 16, <pad>, the compressed overlay id }, channel 40, 24 bytes.
        static_assert(sizeof(GuiOverlayWaitFinishWire) == 24, "wait-finish record is 24 bytes");
        static_assert(sizeof(CgsGui::GuiEventNetworkSuspension) == 16, "suspension record is 16 bytes");

        // Event 189 ("overlay complete") payload view: the compressed overlay id then how
        // the overlay was left. The X360 compares the id with a FULL 64-bit `cmpld` after
        // an `ld r11, 0(r30)` (0x824D8B50 / 0x824D8B68) -- the pseudocode's 32-bit
        // `*(a2 + 4)` read is a Hex-Rays artifact. The queue hands out the
        // HEADER-STRIPPED payload, so the view starts at the id.
        struct GuiOverlayCompletePayload : public CgsModule::Event
        {
            CgsID                                mOverlayId;     // +0x00
            GuiOverlayCompleteEvent::LeaveMethod meLeaveMethod;  // +0x08
        };

    }

    // ---- HandleOverlayCompleteEvent @ 0x824D8B00 (cpp:981) --------------------------
    // In-queue event 189. Only the two connection-failure overlays this screen raises
    // are of interest, and only when the player acknowledged them: that is the cue to
    // leave the sign-in flow the same way the no-popup disconnect path does.
    void CrashNavEnterOnlineBase::HandleOverlayCompleteEvent(const CgsModule::Event* lpOverlayCompleteEvent)
    {
        CGS_ASSERT(lpOverlayCompleteEvent != 0, "lpOverlayCompleteEvent");   // cpp:981

        const GuiOverlayCompletePayload* lpPayload =
            reinterpret_cast<const GuiOverlayCompletePayload*>(lpOverlayCompleteEvent);

        // Full 64-bit id compares (cmpld); the second CgsIDCompress is only evaluated
        // when the first misses, exactly as the asm chains them.
        const bool lbOurOverlay =
            lpPayload->mOverlayId == CgsIDCompress(KAC_DISCONNECTED_STRING_ID) ||
            lpPayload->mOverlayId == CgsIDCompress(KAC_DISCONNECTED_WITH_REASON_STRING_ID);

        if (!lbOurOverlay || lpPayload->meLeaveMethod != GuiOverlayCompleteEvent::E_LEAVEMETHOD_OK)
        {
            return;
        }

        {
            GuiOverlayWaitFinishWire lWaitConnecting;
            lWaitConnecting.mRequest.Construct(KAC_CONNECTING_STRING_ID);

            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWaitConnecting), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lWaitConnecting)));
        }

        // The same leave tail HandleDisconnectedEvent's ignore-popup path runs -- but
        // without its cached-record clears (the X360 has none on this path).
        if (mbJunkyardEntered)
        {
            HandleEnteringJunkyard();
        }
        else if (mpGuiCache->IsOnlineStartPending())
        {
            // One CLEAR payload word (stw r30, with r30 == 0, at 0x824D8BFC) handed to
            // the out-of-line OutputGuiEvent<GuiEventNetworkSuspension> instantiation,
            // which posts { 4, 45, 12, 0 } on channel 40.
            CgsGui::GuiEventNetworkSuspension lSuspension(false);

            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lSuspension), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lSuspension)));

            mpGuiCache->SetOnlineStartPending(false);
            SendStateEvent(KAC_STATE_EVENT_GO_BACK_EASY);
        }
        else
        {
            SendStateEvent(KAC_STATE_EVENT_GO_BACK);
        }
    }

// ===================================================================================
// BrnGui::CrashNavEnterOnlineBase -- wave-I partfile 07: the three in-queue handlers
// that drive the sign-in screen out of its "connecting" phase.
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline_wI_07.cpp
//
//   HandleShowLoginQuestion    @0x824CAC70  (cpp:833)  in-queue event 47
//   HandleDisconnectedEvent    @0x824D8830  (cpp:873)  in-queue event 44
//   HandleOverlayCompleteEvent @0x824D8B00  (cpp:981)  in-queue event 189
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode arbitrated by
// the raw asm in .ida-exports/BURNOUT_X360_ARTIST.XEX/0x824CAC70.json / 0x824D8830.json
// / 0x824D8B00.json). Every console offset below resolves BY NAME through the owning
// header; the offsets in the comments are documentary only.
//   this+0x1C   -> mpStateInterface (CgsGui::State)   this+0xC4   -> mpGuiCache
//   this+0xC8   -> meCurrentLoginQuestion             this+0xCC   -> meSignInType
//   this+0xD0   -> maShowControlsFunc[7]              this+0x3750 -> mCachedHandleDisconnectEvent
//   this+0x37E4 -> meSubState                         this+0x37E8 -> mbJunkyardEntered
//   this+0x37E9 -> mbIgnoreDisconnectedPopup          this+0x37EB -> mbCachedDisconnectEventToHandle
// and on the cache side cache+0x4B53 -> mbOnlineStartPending.
//
// OUT-QUEUE RECORDS. The X360 reaches them two ways -- by stack-building
// { payload size, event id, payload offset } + payload inline and calling
// VariableEventQueue::AddEvent (the wait-finish / activate-crashnav / show-hide-hud
// sites), or by an out-of-line `bl` to a StateInterface::OutputGuiEvent<T>
// instantiation (GuiOverlayRequest @0x82436BE0, GuiEventNetworkSuspension
// @0x82493A88). Both emit the identical record, and BOTH instantiations post it on
// CHANNEL 40 (`li r5, 0x28` at 0x82436C08 / 0x82493A9C) -- whereas the committed
// CgsGuiStateInterface.h OutputGuiEvent template passes lrEvent.GetEventType() as the
// channel, which is not what the X360 does. So every site below posts the wire record
// directly through mpStateInterface->GetOutputEventQueue()->AddEvent(..., 40,
// sizeof(wire)), exactly as the committed BrnCarSelectMain_wG_02.cpp /
// BrnOnlineGameRoomPlayerInfo_wH_14.cpp do. (GuiOverlayRequest cannot go through the
// template at all -- it is not CgsModule::Event-derived, measured C2664.)
//
// X360 LITERALS. The console's baked AddEvent sizes (0x18 / 0x14 / 0x10) and the
// memcpy size (0x84) are written as host sizeof() expressions. Every payload involved
// is pointer-free, so the static_asserts below pin the host records back onto the
// console sizes instead of letting a widened layout drift onto the wire unnoticed.
// ===================================================================================
    namespace
    {

        // ---- out-queue wire records ------------------------------------------------

        // The OutputGuiEvent<BrnGui::GuiOverlayWaitFinishRequest> record:
        // { 8, 188, 16, <pad>, the compressed overlay id }, channel 40, 24 bytes. All
        // three handlers post it for the "OnConnecting" overlay to tear the connecting
        // spinner down before they act.
        // The "show / hide the in-race HUD" record: { 1, 148, 12, <one payload byte> },
        // channel 40, 16 bytes. HandleShowLoginQuestion posts it with the byte clear
        // (hide the HUD) once the no-title sign-in has taken the screen over. The X360
        // stack-builds it inline at 0x824CADC4..0x824CADF4 (stb of 0 at record+0x0C).
        struct GuiEventShowHideHud : public CgsGui::GuiEvent<148>
        {
            u8 mu8Show;   // +0x0C payload byte: 0 = hide the HUD

            explicit GuiEventShowHideHud(bool lbShow)
                : CgsGui::GuiEvent<148>(1, 12), mu8Show(lbShow ? 1u : 0u)
            {
            }
        };

        // The OutputGuiEvent<BrnGui::GuiOverlayRequest> record: { 288, 184, 16, <pad>,
        // the 288-byte request }, channel 40, 304 bytes -- the shape the out-of-line
        // instantiation @0x82436BE0 builds (memcpy 0x120 into record+0x10, then the
        // three header words, then AddEvent(queue, record, 0x28, 0x130)).
        // Record-size pins: every payload here is pointer-free, so the X360's AddEvent
        // size immediates must survive unchanged on the x64 host.
        static_assert(sizeof(GuiOverlayWaitFinishWire) == 24, "wait-finish record is 24 bytes");
        static_assert(sizeof(GuiOverlayRequestWire) == 304, "overlay request record is 304 bytes");
        static_assert(sizeof(CgsGui::GuiEventNetworkSuspension) == 16, "suspension record is 16 bytes");
        static_assert(sizeof(GuiEventShowHideHud) == 16, "show/hide-hud record is 16 bytes");
        static_assert(sizeof(GuiEventActivateCrashNav) == 20, "activate-crashnav record is 20 bytes");

        // ---- in-queue payload views (the queue hands out the HEADER-STRIPPED payload,
        //      so each view starts at the first field) ---------------------------------

        // Event 47 ("show this login question"): the one word the handler both stores
        // into meCurrentLoginQuestion and re-reads to index maShowControlsFunc.
        struct GuiEventShowLoginQuestionPayload : public CgsModule::Event
        {
            CgsGui::ELoginQuestion meLoginQuestion;   // +0x00
        };

        // Event 189 ("overlay complete"): the compressed overlay id then how the overlay
        // was left. The X360 compares the id with a FULL 64-bit `cmpld` after an
        // `ld r11, 0(r30)` (0x824D8B50 / 0x824D8B68) -- the pseudocode's 32-bit
        // `*(a2 + 4)` read is a Hex-Rays artifact.
        // ---- the overlay / state-event vocabularies (the asm's rodata literals) ------

        // The message-param slot HandleDisconnectedEvent fills with the disconnect
        // reason text (the X360's `li r4, 1` at 0x824D89E0).

        // The second payload word of the deactivate-CrashNav record the no-title sign-in
        // posts (the X360's `li r11, 2` at 0x824CAD98). FLAG: the committed
        // GuiEventActivateCrashNav ctor leaves muParam 0 (the pause screen posts 0) and
        // the word's role is not recovered, so it is written explicitly here.
        const u32 KU_ACTIVATE_CRASHNAV_PARAM_SIGN_IN = 2;
    }

    // ---- HandleShowLoginQuestion @ 0x824CAC70 (cpp:833) -----------------------------
    // In-queue event 47. The network layer has decided which sign-in page the player has
    // to answer: drop the connecting overlay, remember the question, and run the page's
    // own show-controls member through maShowControlsFunc. On the no-title flavour of
    // the flow the screen additionally takes the front end over -- it deactivates the
    // CrashNav map flow and hides the HUD behind itself.
    void CrashNavEnterOnlineBase::HandleShowLoginQuestion(const CgsModule::Event* lpShowLoginQuestionEvent)
    {
        // cpp:833 -- the X360 streams this text into the assert buffer. Non-fatal: the
        // dispatch below runs regardless (the asm falls straight through EndAssert).
        CGS_ASSERT(lpShowLoginQuestionEvent != 0,
                   "Invalid event sent to CrashNavEnterOnlineBase::HandleShowLoginQuestion");

        const GuiEventShowLoginQuestionPayload* lpPayload =
            reinterpret_cast<const GuiEventShowLoginQuestionPayload*>(lpShowLoginQuestionEvent);

        // The connecting spinner has served its purpose -- tell the overlays director to
        // finish waiting on it.
        {
            GuiOverlayWaitFinishWire lWaitConnecting;
            lWaitConnecting.mRequest.Construct(KAC_CONNECTING_STRING_ID);

            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWaitConnecting), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lWaitConnecting)));
        }

        meCurrentLoginQuestion = lpPayload->meLoginQuestion;
        meSubState             = E_SUBSTATE_LOGIN_QUESTION;

        // The X360 re-loads the question out of the payload for the dispatch
        // (lwz r11, 0(r26) at 0x824CAD60) rather than re-reading the member it has just
        // written -- same value, kept as written.
        (this->*maShowControlsFunc[lpPayload->meLoginQuestion])();

        if (meSignInType == E_SIGN_IN_TYPE_NO_TITLE)
        {
            // Put the CrashNav map flow to sleep: this screen owns the front end until
            // the sign-in resolves.
            GuiEventActivateCrashNav lDeactivateCrashNav(false);
            lDeactivateCrashNav.muParam = KU_ACTIVATE_CRASHNAV_PARAM_SIGN_IN;

            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lDeactivateCrashNav), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lDeactivateCrashNav)));

            GuiEventShowHideHud lHideHud(false);

            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lHideHud), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lHideHud)));
        }
    }
}
