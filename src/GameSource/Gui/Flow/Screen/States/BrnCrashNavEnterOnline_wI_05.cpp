// wave-I partfile

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavEnterOnline.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"               // CgsGui::ELoginQuestion
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache

namespace BrnGui
{

    namespace
    {
        // ---- axis ids (the axis payload's first word) ---------------------------------
        // The TOS page scrolls off the two right-hand-stick axes; axis 0 (the left stick)
        // is ignored, and so is anything else (@0x824B652C-0x824B653C).
        const s32 KI_AXIS_RIGHT_STICK_A = 1;
        const s32 KI_AXIS_RIGHT_STICK_B = 2;

        // CgsGui::GuiEventControllerAxis, minus the 12-byte GuiEvent header
        // (BrnCarSelectVehicle_Input.cpp carries the identical view for the same event).
        struct ControllerAxisPayload : public CgsModule::Event
        {
            s32 miAxis;       // +0x00
            f32 mfXAxis;      // +0x04 (not read by this screen)
            f32 mfYAxis;      // +0x08
        };
    }

    // ---- HandleControllerAxis @ 0x824B6480 (cpp:1032) ------------------------------
    // Sample the analogue stick for the TOS page's text scroll. The value is latched raw
    // (the per-frame Update turns it into scroll lines); anything inside the dead zone
    // latches zero, and so does every sub-state / question that is not the TOS page.
    //
    // The parameter type is the DWARF's (h:436) -- unlike its HandleControllerInput
    // sibling this one is declared over the concrete event type. What the in-queue
    // actually hands it is still the HEADER-STRIPPED payload, so the reads below are at
    // +0 and +8 (`lwz r11, 0(r28)` @0x824B652C, `lfs f0, 8(r28)` @0x824B6544) and NOT at
    // GuiEventControllerAxis's own miAxis +0x0C / mfYAxis +0x14. That is why the pointer
    // goes through the local payload view instead of being dereferenced directly; the
    // sibling screen BrnOnlineNews::HandleControllerAxis does the same thing.
    void CrashNavEnterOnlineBase::HandleControllerAxis(const CgsGui::GuiEventControllerAxis* lpEvent)
    {
        // cpp:1032. Streamed into the assert buffer by the X360; non-fatal (the sampling
        // below runs either way), and the "SelectRoutes" in the text is the original's own
        // copy-paste from the route-select screen -- kept verbatim.
        CGS_ASSERT(lpEvent != 0, "Invalid event in SelectRoutes::HandleControllerAxis");

        if (meSubState == E_SUBSTATE_LOGIN_QUESTION &&
            meCurrentLoginQuestion == CgsGui::E_LOGIN_QUESTION_TOS)
        {
            const ControllerAxisPayload* lpAxis =
                reinterpret_cast<const ControllerAxisPayload*>(lpEvent);

            // Any other axis leaves the latch ALONE -- the X360 branches past the store
            // entirely (@0x824B653C -> loc_824B6570), it does not zero it.
            if (lpAxis->miAxis != KI_AXIS_RIGHT_STICK_A &&
                lpAxis->miAxis != KI_AXIS_RIGHT_STICK_B)
            {
                return;
            }

            // The two compares are `fcmpu` + `blt` / `bgt` (@0x824B654C-0x824B6560), both
            // ORDERED branches, so an unordered (NaN) sample falls through to the zero
            // store -- which is exactly what plain C++ < and > do with a NaN operand, so
            // the direct transliteration keeps the console's NaN behaviour.
            const f32 lfYAxis = lpAxis->mfYAxis;
            if (lfYAxis < -KF_AXIS_DEAD_ZONE || lfYAxis > KF_AXIS_DEAD_ZONE)
            {
                mfLastAxisValue = lfYAxis;
            }
            else
            {
                mfLastAxisValue = 0.0f;
            }
        }
        else
        {
            mfLastAxisValue = 0.0f;
        }
    }

    namespace
    {
        // ---- controller action ids (the press payload's second word) ------------------
        // EGameInputActions GUI_SELECT / GUI_BACK. The X360 tests the login-error arm as
        // the unsigned range `(id - 0x31) <= 1` (@0x824DFF10), i.e. exactly these two.
        const s32 KI_ACTION_ACCEPT = 49;   // 0x31
        const s32 KI_ACTION_BACK   = 50;   // 0x32

        // CgsGui::GuiEventControllerInputPressed, minus the 12-byte GuiEvent header
        // (the in-queue hands the state the header-stripped payload).
        struct ControllerInputPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (EGameInputActions; the only word read here)
        };
    }

    // ---- HandleControllerInput @ 0x824DFEE0 (cpp:488) ------------------------------
    // Route a controller press to the handler for the sub-state that is up. Only two of
    // the six sub-states take presses at all: the login-question pages (routed a second
    // time by which question is showing) and the login-error page (where either the
    // accept or the back button acknowledges the failure and bails out of the flow).
    //
    // Note there is no assert here -- unlike its sibling handlers the X360 body opens
    // straight on the collision-world gate byte (`lbz r11, 0x37EA(r3)`), and returns
    // without touching the event while the crash-nav collision world is being rebuilt.
    void CrashNavEnterOnlineBase::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        if (mbIsInvalidatingCollisionWorld)
        {
            return;
        }

        if (meSubState == E_SUBSTATE_LOGIN_QUESTION)
        {
            // The share-info page and the "shall I show the sign-in UI" page each own
            // their button layout; every other question is the plain two-button page.
            if (meCurrentLoginQuestion == CgsGui::E_LOGIN_QUESTION_SHARE)
            {
                HandleLoginInputShareInfo(lpEvent);
            }
            else if (meCurrentLoginQuestion == CgsGui::E_LOGIN_QUESTION_SHOW_SIGN_IN)
            {
                HandleLoginInputShowSignIn(lpEvent);
            }
            else
            {
                HandleLoginInput(lpEvent);
            }
        }
        else if (meSubState == E_SUBSTATE_LOGIN_ERROR)
        {
            const s32 liButtonId =
                reinterpret_cast<const ControllerInputPayload*>(lpEvent)->miButtonId;

            if (liButtonId == KI_ACTION_ACCEPT || liButtonId == KI_ACTION_BACK)
            {
                // Acknowledging the error takes the same route out as a real disconnect,
                // minus the popup -- the flag is raised first so the shared handler takes
                // its quiet arm.
                mbIgnoreDisconnectedPopup = true;

                // The X360 hands the handler r4 = &var_10, a stack slot it never wrote
                // (@0x824DFF20): on the ignore-popup arm the callee reads nothing out of
                // the record, so the console gets away with an uninitialised pointer.
                // A zeroed local stands in for it here.
                CgsGui::GuiEventNetworkDisconnected lDummyDisconnect;
                lDummyDisconnect.meLastError  = 0;
                lDummyDisconnect.macReason[0] = 0;

                HandleDisconnectedEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lDummyDisconnect));
            }
        }
    }

    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------
        // 42 == OutputInternalState. HandleEnteringJunkyard is the only site in this TU
        // that hides the HUD on the INTERNAL channel (the login-question site posts the
        // same record on 40) -- the X360 loads `li r5, 0x2A` @0x824CB038.
        const s32 KI_CHANNEL_GUI_INTERNAL = 42;

        // ---- out-queue wire record ----------------------------------------------------
        // The { 1, 148, 12, flag } show/hide-HUD command (BrnPauseScreen.cpp's
        // GuiEventHudComponentsCommand twin: flag 0 shuts the HUD components down). The
        // X360 stack-builds it at 0x824CB040-0x824CB060 and pushes 16 bytes on channel 42;
        // it writes only the flag byte, so the pad is modelled zeroed.
        //
        // Both header words are HOST expressions, never the console's baked immediates:
        // the payload size is sizeof(the flag byte) and the payload offset is the host
        // sizeof of the 3-word GuiEvent header (12 on the X360, and 12 here too because
        // CgsModule::Event is empty -- but written as sizeof so it tracks the host layout).
        typedef CgsGui::GuiEvent<148> ShowHideHudHeader;

        struct GuiEventShowHideHudWire : public ShowHideHudHeader
        {
            u8 mu8Show;      // +0x0C
            u8 maPad[3];

            explicit GuiEventShowHideHudWire(u8 lu8Show)
                : ShowHideHudHeader(static_cast<u32>(sizeof(u8)),
                                    static_cast<u32>(sizeof(ShowHideHudHeader)))
                , mu8Show(lu8Show)
            {
                maPad[0] = maPad[1] = maPad[2] = 0;
            }
        };
    }

    // ---- HandleEnteringJunkyard @ 0x824CAFD8 (cpp:1418) ----------------------------
    // Leave the sign-in screen straight into the junkyard. The cache's in-event colouring
    // gate doubles as the "an event is still running" latch here, so entry is refused
    // while it is up; otherwise the HUD components go down and the state event picks the
    // car-unlock flavour of car select over the plain one.
    void CrashNavEnterOnlineBase::HandleEnteringJunkyard()
    {
        // cpp:1418. Plain (unstreamed) FireAssert on the X360; non-fatal, and the body
        // re-tests the pointer afterwards (@0x824CB018) rather than early-returning.
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        if (mpGuiCache != 0 && !mpGuiCache->GetInEventColouringGate())
        {
            GuiEventShowHideHudWire lHideHud(0);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lHideHud),
                KI_CHANNEL_GUI_INTERNAL,
                static_cast<s32>(sizeof(lHideHud)));          // X360 record size 16

            SendStateEvent(mpGuiCache->IsJunkyardCarUnlockPending() ? "TO_CUNLOCK"
                                                                    : "TO_CSELECT");
        }
    }
}
