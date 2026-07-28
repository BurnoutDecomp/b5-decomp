// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 07: the freeburn controller
// handler and the two pause/HUD transitions.
//   HandleControllerInputFreeBurnSubState @0x824A4A08  cpp:1520 (assert cpp:1532)
//   ShowPauseScreen                       @0x82484DE8  cpp:3792 (assert cpp:3826)
//   HideMenusAndReturnToHUD               @0x82499B80  cpp:3732 (assert cpp:3776)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm; see
// scratchpad/waveB/...BrnOnlineGameRoomPlayerInfo.cpp.asm.txt). Together these are the
// "player is driving in the online lobby" branch of the sub-state machine: the freeburn
// handler is the only input path live while the menus are down, ShowPauseScreen arms the
// pause load, and HideMenusAndReturnToHUD tears the whole menu stack back down.
//
// Both asserts the X360 fires here are NON-fatal (BeginAssert / FireAssert / EndAssert
// with no early-out), so control continues through them exactly as written -- the same
// shape the sibling BrnOnlineGameRoomPlayerInfo_wH_05.cpp and BrnCarSelectMain_wG_02.cpp
// use. HideMenusAndReturnToHUD's default-case assert is the streamed form (a stack
// CgsDev::StrStream over CgsDev::Assert::gpcMessageBuffer, "Substate " << meSubState <<
// " should not be leaving the menus.\n"); per project policy it is lowered to CGS_ASSERT
// with the static text and the streamed value is the switch scrutinee itself.
//
// The five out-queue posts at the tail of HideMenusAndReturnToHUD are written as typed
// records, NOT as the console's baked immediates: every payload-size / payload-offset
// header word is a host sizeof expression, and the AddEvent size argument is sizeof(the
// record). The X360's 16 / 20 byte literals are the 32-bit console record sizes.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue / PlayAptMovie
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache (freeburn gate flags)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiEventActivateCrashNav (id 191)

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // OutputGuiEvent

        // ---- controller action ids (the payload's second word) ------------------------
        // FLAG: the producer-side enum name (EGameInputActions) is not in the recovered
        // DWARF slice, so these four names are role-derived -- read off what THIS handler
        // does with each id, nothing else. All four literals appear exactly once in the
        // whole TU (this switch); no other handler in the TU compares the action word
        // against them, so their meaning outside this function is not attested here.
        const s32 KI_ACTION_PAUSE          = 0x2D;   // '-'
        const s32 KI_ACTION_MAP            = 0x2E;   // '.'
        const s32 KI_ACTION_EA_TRACK_HOLD  = 0x36;   // '6' -- arms mfTimeToDisableNextEATrack
        const s32 KI_ACTION_EA_TRACK_SKIP  = 0x37;   // '7' -- arms mfTimeUntilNextEATrack

        // The EA-trax debounce window both cases load (X360 flt_8205E5E0 == 0.30000001f);
        // the zero the skip case compares against is flt_82001CC0 == 0.0f.
        const f32 KF_EA_TRACK_DEBOUNCE_TIME = 0.3f;

        // ---- in-queue payload view ----------------------------------------------------
        // ARTIST controller event payload: pad id at +0x00, the button/action id at +0x04.
        // The in-queue hands the state the HEADER-STRIPPED payload, so this view starts at
        // CgsGui::GuiEventControllerInputPressed's miPadId (BrnInGame.cpp / wave-H 05
        // precedent). Only the action word is read here (`lwz r11, 4(r30)`).
        struct ControllerInputPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04
        };

        // ---- out-queue wire records ---------------------------------------------------
        // Each is a CgsGui::GuiEvent<N> header { payload size, event id, payload offset }
        // followed by the payload, published through
        // mpStateInterface->GetOutputEventQueue()->AddEvent(record, channel, sizeof(record)).
        // BrnGuiDemangledEventTypes.h (the home of ids 65 and 148) hard-collides with
        // BrnGuiEventTypeDefs.h, which this file needs for GuiEventActivateCrashNav, so the
        // two are carried as file-local wire views -- the same accommodation BrnInGame.cpp,
        // BrnCarSelectMain_wG_02.cpp and BrnOnlineGameRoomPlayerInfo_wH_05.cpp make.

        // Id 65 == BrnGui::GuiRequestCarControlChangeEvent (payload size 1). X360 stores 1
        // in the payload byte. FLAG: the payload field has no name in the recovered DWARF
        // slice; the polarity reading (1 == hand car control back) comes from the event name
        // and from this handler being the return-to-HUD path.
        typedef CgsGui::GuiEvent<65> RequestCarControlChangeHeader;
        struct GuiRequestCarControlChangeWire : public RequestCarControlChangeHeader
        {
            bool mbEnableCarControl;   // +0x0C

            explicit GuiRequestCarControlChangeWire(bool lbEnableCarControl)
                : RequestCarControlChangeHeader(static_cast<u32>(sizeof(bool)),
                                                static_cast<u32>(sizeof(RequestCarControlChangeHeader)))
                , mbEnableCarControl(lbEnableCarControl)
            {
            }
        };

        // Id 533 -- the screen-flow command the online back-out paths post (the committed
        // BrnOnlinePlay.cpp posts the identical { 1, 533, 12, <payload byte> } record on
        // channel 40 from its own GO_BACK path). Payload size 1.
        typedef CgsGui::GuiEvent<533> ScreenFlowCommandHeader;
        struct GuiScreenFlowCommandWire : public ScreenFlowCommandHeader
        {
            // The X360 posts this record WITHOUT writing the payload byte: its block
            // (0x82499D88..0x82499DAC) stores only record+0 = 1, record+4 = 0x215 and
            // record+8 = 12 before AddEvent(..., 40, 16). The byte is still DETERMINISTIC,
            // because the preceding id-191 record left it: 0x82499D58 stw r29(=1) -> [+0],
            // 0x82499D64 stw r26 -> [+4], 0x82499D68 ld r11 <- [+0], 0x82499D78 std r11 ->
            // [+0x0C], i.e. the big-endian word 0x00000001 lands at [+0x0C..+0x0F] and the
            // byte AT +0x0C is 0x00. The published value is therefore 0.
            //
            // That 0 is an ENDIANNESS artefact of the console, not a stored constant: the
            // identical little-endian aliasing on the host would leave 0x01 there. So the
            // byte is written explicitly to reproduce what the X360 publishes, rather than
            // left to inherit whatever the host's stack reuse happens to put in it.
            u8 muPayload;   // +0x0C (never stored by the X360; inherited value is 0)

            GuiScreenFlowCommandWire()
                : ScreenFlowCommandHeader(static_cast<u32>(sizeof(u8)),
                                          static_cast<u32>(sizeof(ScreenFlowCommandHeader)))
                , muPayload(0)
            {
            }
        };

        // Id 148 == BrnGui::GuiEventShowHideHud (payload size 1). X360 stores 1.
        typedef CgsGui::GuiEvent<148> ShowHideHudHeader;
        struct GuiEventShowHideHudWire : public ShowHideHudHeader
        {
            bool mbShowHud;   // +0x0C

            explicit GuiEventShowHideHudWire(bool lbShowHud)
                : ShowHideHudHeader(static_cast<u32>(sizeof(bool)),
                                    static_cast<u32>(sizeof(ShowHideHudHeader)))
                , mbShowHud(lbShowHud)
            {
            }
        };
    }

    // ---- HandleControllerInputFreeBurnSubState @ 0x824A4A08 ------------------------
    // The only input path live while the player is driving with the lobby menus down.
    // Everything is gated on the cache's freeburn input flag; the pause and map entries
    // are additionally gated on the menu lock, and the two EA-trax buttons just arm their
    // debounce timers (Update runs them down).
    void OnlineGameRoomPlayerInfo::HandleControllerInputFreeBurnSubState(const CgsModule::Event* lpControllerEvent)
    {
        CGS_ASSERT(lpControllerEvent != 0, "lpControllerEvent");   // cpp:1532 (non-fatal)

        if (mpGuiCache == 0 || mpGuiCache->mbFreeBurnInputDisabled)
        {
            return;
        }

        const s32 liAction =
            reinterpret_cast<const ControllerInputPayload*>(lpControllerEvent)->miButtonId;

        switch (liAction)
        {
            case KI_ACTION_PAUSE:
                if (!mpGuiCache->mbFreeBurnMenuLocked)
                {
                    HideHUDAndEnterMenus();
                    ShowPauseScreen();
                }
                break;

            case KI_ACTION_MAP:
                if (!mpGuiCache->mbFreeBurnMenuLocked)
                {
                    ShowMapScreen();
                    mbShownFromPause = false;
                }
                break;

            case KI_ACTION_EA_TRACK_HOLD:
                mfTimeToDisableNextEATrack = KF_EA_TRACK_DEBOUNCE_TIME;
                break;

            case KI_ACTION_EA_TRACK_SKIP:
                if (mfTimeUntilNextEATrack <= 0.0f)
                {
                    mfTimeUntilNextEATrack = KF_EA_TRACK_DEBOUNCE_TIME;
                }
                break;

            default:
                break;
        }
    }

    // ---- ShowPauseScreen @ 0x82484DE8 ----------------------------------------------
    // Arm the pause-screen load. Every page that can raise the pause menu is listed in
    // the assert; the state itself is the whole body.
    void OnlineGameRoomPlayerInfo::ShowPauseScreen()
    {
        // cpp:3826 -- the X360 fires the bare expression text (non-fatal).
        CGS_ASSERT(meSubState == E_SUBSTATE_FREE_BURNING ||
                   meSubState == E_SUBSTATE_MAIN ||
                   meSubState == E_SUBSTATE_MAP ||
                   meSubState == E_SUBSTATE_CHALLENGES ||
                   meSubState == E_SUBSTATE_SECURITY_OPTIONS ||
                   meSubState == E_SUBSTATE_VIEW_EVENT ||
                   meSubState == E_SUBSTATE_SETTINGS,
                   "( meSubState == E_SUBSTATE_FREE_BURNING ) || ( meSubState == E_SUBSTATE_MAIN ) || "
                   "( meSubState == E_SUBSTATE_MAP ) || ( meSubState == E_SUBSTATE_CHALLENGES) || "
                   "( meSubState == E_SUBSTATE_SECURITY_OPTIONS) || ( meSubState == E_SUBSTATE_VIEW_EVENT) || "
                   "( meSubState == E_SUBSTATE_SETTINGS)");

        meSubState = E_SUBSTATE_LOADING_PAUSE_SCREEN;
    }

    // ---- HideMenusAndReturnToHUD @ 0x82499B80 --------------------------------------
    // Drop the whole menu stack and put the player back behind the wheel: leave the map
    // page if that is where we are, park the machine in the controller-disable delay for
    // KI_CONTROLLER_DISABLE_DELAY_FRAME_COUNT frames, then publish the five view/flow
    // commands that unmount the lobby movie and bring the HUD back.
    void OnlineGameRoomPlayerInfo::HideMenusAndReturnToHUD()
    {
        switch (meSubState)
        {
            case E_SUBSTATE_LOADING_PAUSE_SCREEN:       // 1
            case E_SUBSTATE_LOADING_PAUSE_COMPONENTS:   // 2
            case E_SUBSTATE_FREE_BURNING:               // 11
            case E_SUBSTATE_PAUSE:                      // 14
            case E_SUBSTATE_SECURITY_OPTIONS:           // 20
                break;

            case E_SUBSTATE_MAP:                        // 17
                CrashNavMap::OnLeave();
                break;

            default:
                // cpp:3776 -- streamed as "Substate " << meSubState << " should not be
                // leaving the menus.\n"; lowered to the static text per project policy.
                CGS_ASSERT(false, "Substate  should not be leaving the menus.\n");
                break;
        }

        meSubState        = E_SUBSTATE_CONTROLLER_DISABLE_DELAY;
        miDelayFrameCount = KI_CONTROLLER_DISABLE_DELAY_FRAME_COUNT;

        // 1. Hand car control back to the player (X360 record size 16, channel 40).
        GuiRequestCarControlChangeWire lCarControl(true);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lCarControl), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lCarControl)));

        // 2. Unmount the lobby apt movie: the inlined CgsGui::GuiEventPlayAptMovie record
        //    { 8, 18, 12, "", 3 } on channel 41 (the empty name is the &unk_820046A7
        //    rodata sentinel). X360 record size 20.
        mpStateInterface->PlayAptMovie("", 3);

        // 3. Re-activate CrashNav (X360 record { 8, 191, 12, 1, 0 }, size 20, channel 40).
        GuiEventActivateCrashNav lActivateCrashNav(true);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lActivateCrashNav), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lActivateCrashNav)));

        // 4. The id-533 screen-flow command (X360 record size 16, channel 40).
        GuiScreenFlowCommandWire lScreenFlowCommand;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lScreenFlowCommand), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lScreenFlowCommand)));

        // 5. Show the HUD again (X360 record size 16, channel 40).
        GuiEventShowHideHudWire lShowHud(true);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lShowHud), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lShowHud)));
    }
}
