// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 05: the controller-input entry
// points.
//   HandleControllerInputPressed          @0x824AF018  cpp:1282 (assert cpp:1294)
//   HandleControllerInputReleased         @0x824A45B0  cpp:1361 (assert cpp:1373)
//   HandleControllerInputReleasedMapSubState @0x824982A8 cpp:2343
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm). The
// first two are pure dispatchers: both open with the streamed "Invalid event sent to
// ..." assert on a null payload (the CgsDev::Assert::BeginAssert / StrStream / FireAssert
// / EndAssert idiom the X360 inlines -- written here as CGS_ASSERT, the same shape the
// sibling BrnCarSelectMain_wG_02.cpp uses for the copy-pasted twins of these asserts),
// then branch on meSubState. Note the asserts are NON-fatal in the X360 build: the
// dispatch runs whether or not the payload was null, so there is no early return here
// either.
//
// The released-map handler is the only one with a body of its own: it consumes the two
// map-zoom RELEASE actions off the controller event's payload (the file-local
// ControllerInputPayload view -- BrnBootProfile.cpp / BrnInGame.cpp precedent, since the
// in-queue hands the state the header-stripped payload) and drops the map back to the
// medium zoom / icon-selecting cursor.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------
        // OutputViewState / OutputInternalState -- the zoom-out release publishes the SAME
        // record on both. (The X360 re-stores the four identical words onto the same stack
        // slot before the second AddEvent, since AddEvent copies the record out; one local
        // reused across both calls is the same behaviour.)
        const s32 KI_CHANNEL_VIEW_STATE     = 41;
        const s32 KI_CHANNEL_GUI_INTERNAL   = 42;

        // ---- controller action ids (the payload's second word) -----------------------
        // The X360 compares the raw ids; the release pair is the map zoom-in / zoom-out
        // buttons coming back UP. FLAG: the producer-side enum name for these is not in
        // the recovered DWARF slice, so they are named after the role the handler gives
        // them (the map-substate press handler owns the matching 0x36/0x37 down ids).
        const s32 KI_ACTION_ZOOM_IN_RELEASE  = 56;   // 0x38
        const s32 KI_ACTION_ZOOM_OUT_RELEASE = 57;   // 0x39

        // ---- in-queue payload view ----------------------------------------------------
        // ARTIST controller event payload: pad id at +0x00, the button/action id at +0x04.
        // Only the action word is read here (`lwz r11, 4(r4)`).
        struct ControllerInputPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04
        };

        // ---- out-queue wire record ----------------------------------------------------
        // The X360 stack-builds { 4, 558, 12, 0 } and AddEvent()s it with size 16 on
        // channels 41 and 42. Event 558 is the "set inspected event icon" view-state
        // event (a single 4-byte payload word == the event id being inspected, zero here
        // to clear it) -- its declaration lives in BrnGuiDemangledEventTypes.h, which
        // cannot be included alongside BrnGuiEventTypeDefs.h (they hard-collide on four
        // types), so this TU carries the file-local wire view, the same accommodation
        // BrnInGame.cpp and BrnCarSelectMain_wG_02.cpp make.
        //
        // The two header words are HOST expressions, not the console's baked immediates:
        // the payload size is sizeof(the payload word) and the payload offset is the host
        // sizeof of the 3-word GuiEvent header (12 on the X360, and 12 here too because
        // CgsModule::Event is empty -- but written as sizeof so it tracks the host layout).
        typedef CgsGui::GuiEvent<558> SetInspectedEventIconHeader;

        struct GuiEventSetInspectedEventIconWire : public SetInspectedEventIconHeader
        {
            u32 muEventID;   // +0x0C (X360 stores 0)

            explicit GuiEventSetInspectedEventIconWire(u32 luEventID)
                : SetInspectedEventIconHeader(static_cast<u32>(sizeof(u32)),
                                              static_cast<u32>(sizeof(SetInspectedEventIconHeader)))
                , muEventID(luEventID)
            {
            }
        };
    }

    // ---- HandleControllerInputPressed @ 0x824AF018 ---------------------------------
    // Route a controller-press event to the handler for the current sub-state. The X360
    // compiles this as a jump table over (meSubState - 10) with a 13-entry span; the
    // holes (12 E_SUBSTATE_LEAVING, 15/16 the map-load pair, 19 the input-disable delay)
    // and everything outside the span fall through the default and consume the press.
    void OnlineGameRoomPlayerInfo::HandleControllerInputPressed(const CgsModule::Event* lpEvent)
    {
        // cpp:1294. Streamed into the assert buffer by the X360; non-fatal.
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineGameRoomPlayerInfo::HandleControllerInputPressed");

        switch (meSubState)
        {
            case E_SUBSTATE_MAIN:
                HandleControllerInputMainSubState(lpEvent);
                break;
            case E_SUBSTATE_FREE_BURNING:
                HandleControllerInputFreeBurnSubState(lpEvent);
                break;
            case E_SUBSTATE_PLAYER_OPTIONS:
                HandleControllerInputPlayerOptionsSubState(lpEvent);
                break;
            case E_SUBSTATE_PAUSE:
                HandleControllerInputPauseSubState(lpEvent);
                break;
            case E_SUBSTATE_MAP:
                HandleControllerInputPressedMapSubState(lpEvent);
                break;
            case E_SUBSTATE_CHALLENGES:
                HandleControllerInputChallengesSubState(lpEvent);
                break;
            case E_SUBSTATE_SECURITY_OPTIONS:
                HandleControllerInputSecurityOptionsSubState(lpEvent);
                break;
            case E_SUBSTATE_SETTINGS:
                HandleControllerInputSettingsSubState(lpEvent);
                break;
            case E_SUBSTATE_VIEW_EVENT:
                HandleControllerInputViewEventSubState(lpEvent);
                break;
            default:
                break;
        }
    }

    // ---- HandleControllerInputReleased @ 0x824A45B0 --------------------------------
    // The release counterpart. Only the map sub-state cares about button releases.
    void OnlineGameRoomPlayerInfo::HandleControllerInputReleased(const CgsModule::Event* lpEvent)
    {
        // cpp:1373. Streamed into the assert buffer by the X360; non-fatal.
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineGameRoomPlayerInfo::HandleControllerInputReleased");

        if (meSubState == E_SUBSTATE_MAP)
        {
            HandleControllerInputReleasedMapSubState(lpEvent);
        }
    }

    // ---- HandleControllerInputReleasedMapSubState @ 0x824982A8 ---------------------
    // Both map-zoom buttons coming up return the map to the medium zoom with the cursor
    // back in icon-selecting mode. Letting go of ZOOM OUT additionally drops whatever
    // event icon was being inspected -- locally, and on the view-state and internal
    // channels via the id-558 record.
    void OnlineGameRoomPlayerInfo::HandleControllerInputReleasedMapSubState(const CgsModule::Event* lpEvent)
    {
        const s32 liAction =
            reinterpret_cast<const ControllerInputPayload*>(lpEvent)->miButtonId;

        if (liAction != KI_ACTION_ZOOM_IN_RELEASE && liAction != KI_ACTION_ZOOM_OUT_RELEASE)
        {
            return;
        }

        if (liAction == KI_ACTION_ZOOM_OUT_RELEASE)
        {
            muInspectingEventID = 0;

            GuiEventSetInspectedEventIconWire lClearInspected(0);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lClearInspected),
                KI_CHANNEL_VIEW_STATE,
                static_cast<s32>(sizeof(lClearInspected)));      // X360 record size 16

            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lClearInspected),
                KI_CHANNEL_GUI_INTERNAL,
                static_cast<s32>(sizeof(lClearInspected)));      // X360 record size 16
        }

        // r4 == 1 == E_ZOOMFACTOR_MEDIUM, f1 == flt_82001CC0 == 0.0f, r6 == 0.
        mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_MEDIUM, 0.0f, false);
        meCursorMode = E_CURSORMODE_SELECTING_ICONS;
    }
}
