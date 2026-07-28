// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 08: the settings page.
//   HandleControllerInputSettingsSubState @0x824AD248  cpp:2004
//   ShowSettingsScreen                    @0x8249A780  cpp:4268 (assert cpp:4311)
//   TriggerSound                          @0x8249ADB0  cpp:4631 (assert cpp:4709)
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm; see
// scratchpad/waveB/...BrnOnlineGameRoomPlayerInfo.cpp.asm.txt). These three are the
// lobby's "settings" page: ShowSettingsScreen arms the load, the sub-state handler runs
// the 4-row toggle group and commits/cancels it, and TriggerSound is the per-move audio
// cue both of the highlight paths raise.
//
// VTABLE SLOT -> METHOD, dumped rather than inferred. The four highlight vcalls in the
// sub-state handler go through the MenuToggleGroupVarSize<4> vtable @0x82074750, read out
// of the image (scratchpad/waveH08/dump_vtbl_out.txt):
//     +0x18 (slot  6) MenuToggleGroupVarSize<4>::Clear
//     +0x28 (slot 10) SelectableGroup::HighlightNext(bool)
//     +0x2C (slot 11) SelectableGroup::HighlightPrevious(bool)
//     +0x34 (slot 13) MenuToggleGroupVarSize<4>::HighlightNextItem()
//     +0x38 (slot 14) MenuToggleGroupVarSize<4>::HighlightPreviousItem()
// so action 0x29 moves the highlighted ROW backwards and 0x2A forwards (both are the
// one-argument base virtuals -- the asm sets r4 = 0 for them and leaves r4 untouched for
// the no-argument item virtuals), while 0x2B/0x2C change the highlighted row's VALUE and
// are the only pair that reports the change through HandleSettingChanged(). This INVERTS
// the slot map the keystone spec carried; the dumped vtable is the authority.
//
// The out-queue records are written as typed wire views with HOST sizeof/offset
// expressions -- never the console's baked 100 / 8 / 4 / 12 / 20 / 112 immediates, which
// are 32-bit-console record sizes. Ids 457 / 463 / 474 are homed only in
// BrnGuiDemangledEventTypes.h, which hard-collides with BrnGuiEventTypeDefs.h (needed
// here for GuiOverlayRequest / GuiAudioTriggerEvent / GuiEventActivateCrashNav), so they
// are carried as file-local wire views -- the same accommodation BrnInGame.cpp,
// BrnCarSelectMain_wG_02.cpp and BrnOnlineGameRoomPlayerInfo_wH_07.cpp make.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface out-queue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // GuiCache::GetOptionsDataProfile
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                      // OptionsDataProfile volume getters
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiOverlayRequest / GuiAudioTriggerEvent / GuiEventActivateCrashNav

namespace BrnGui
{
    namespace
    {
        // ---- AddEvent channels (the out-queue selector word) --------------------------
        const s32 KI_CHANNEL_GUI_OUT = 40;   // OutputGuiEvent

        // ---- controller action ids (the in-queue payload's second word) ----------------
        // FLAG: the producer-side enum name (EGameInputActions) is not in the recovered
        // DWARF slice, so these are named for the role the X360 code gives them. The eight
        // ids TriggerSound knows split into exactly two families by the audio label it
        // picks: 0x25/0x26/0x29/0x2A are ROW moves ("MenuToggleDefault") and
        // 0x27/0x28/0x2B/0x2C are toggle-VALUE moves ("MenuItemToggleDefault"). Only the
        // 0x29..0x2C half is dispatched by this screen's settings handler, so only that
        // half's prev/next direction is attested (by the vtable slot each case calls);
        // 0x25..0x28 are known by family only.
        const s32 KI_ACTION_MENU_MOVE_A = 0x25;   // '%'  row-move family, direction unattested
        const s32 KI_ACTION_MENU_MOVE_B = 0x26;   // '&'  row-move family, direction unattested
        const s32 KI_ACTION_ITEM_MOVE_A = 0x27;   // '\'' value-move family, direction unattested
        const s32 KI_ACTION_ITEM_MOVE_B = 0x28;   // '('  value-move family, direction unattested
        const s32 KI_ACTION_MENU_PREVIOUS = 0x29; // ')'  -> SelectableGroup::HighlightPrevious
        const s32 KI_ACTION_MENU_NEXT     = 0x2A; // '*'  -> SelectableGroup::HighlightNext
        const s32 KI_ACTION_ITEM_PREVIOUS = 0x2B; // '+'  -> HighlightPreviousItem
        const s32 KI_ACTION_ITEM_NEXT     = 0x2C; // ','  -> HighlightNextItem
        const s32 KI_ACTION_SELECT        = 0x31; // '1'  accept the page
        const s32 KI_ACTION_BACK          = 0x32; // '2'  back out of the page

        // The GuiAudioTriggerEvent::meAction value every menu cue in the GUI flow uses
        // (X360 `li r4, 7`; the same literal the committed BrnBootProfile.cpp passes).
        // FLAG: the action enum's name is not in the recovered DWARF slice.
        const s32 KI_AUDIO_ACTION_MENU_CUE = 7;

        // The two audio labels the X360 selects between (rodata literals).
        const char KAC_SOUND_LABEL_MENU_TOGGLE[]      = "MenuToggleDefault";
        const char KAC_SOUND_LABEL_MENU_ITEM_TOGGLE[] = "MenuItemToggleDefault";

        // The "cancel the settings edit?" overlay this page raises when the player backs
        // out with unsaved changes.
        const char KAC_OVERLAY_CANCEL_CONFIRM[] = "CNCnclCnfrm";

        // The audio label the clean back-out path plays.
        const char KAC_SOUND_LABEL_GO_BACK[] = "GO_BACK";

        // ---- in-queue payload view ----------------------------------------------------
        // ARTIST controller event payload: pad id at +0x00, the button/action id at +0x04.
        // The in-queue hands the state the HEADER-STRIPPED payload, so this view starts at
        // the pad id (BrnInGame.cpp / wave-H 05 / wave-H 07 precedent). Only the action
        // word is read here (`lwz r30, 4(r4)`).
        struct ControllerInputPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04
        };

        // ---- out-queue wire records ---------------------------------------------------

        // The record the inlined OutputGuiEvent<BrnGui::GuiAudioTriggerEvent> builds (the
        // out-of-line instantiation @0x82436890 builds the identical one):
        // { 100, 457, 12, the 100-byte payload }, channel 40, 112 bytes. The committed PC
        // GuiAudioTriggerEvent already carries that 12-byte queue header in front of its
        // 100-byte payload, so the event object IS the record -- but its GuiEvent<201>
        // base seeds the payload-size word 0 and the id 201, while the X360 wire words are
        // the payload size and id 457. This view restores them without forking the payload
        // type. (Cross-TU finding: 201 is GuiEventHideDriveThru's id; 457 is the id both
        // this record and BrnGuiDemangledEventTypes.h attest for the audio trigger.)
        const u32 KU_WIRE_ID_AUDIO_TRIGGER = 457;

        struct GuiAudioTriggerWire : public GuiAudioTriggerEvent
        {
            GuiAudioTriggerWire()
            {
                muHeader0 = static_cast<u32>(sizeof(GuiAudioTriggerEvent) -
                                             sizeof(CgsGui::GuiEvent<201>));
                muEventType = KU_WIRE_ID_AUDIO_TRIGGER;
            }
        };

        // Id 463 == BrnGui::GuiEventAudioSettings. The record
        // OutputGuiEvent<GuiEventAudioSettings> @0x82493728 builds: { 8, 463, 12, the two
        // volume words }, channel 40, 20 bytes. Field order is the X360 store order --
        // GetMusicVolume lands in the first word, GetSFXVolume in the second.
        typedef CgsGui::GuiEvent<463> AudioSettingsHeader;
        struct GuiEventAudioSettingsWire : public AudioSettingsHeader
        {
            s32 miMusicVolume;   // +0x0C
            s32 miSFXVolume;     // +0x10

            GuiEventAudioSettingsWire(s32 liMusicVolume, s32 liSFXVolume)
                : AudioSettingsHeader(static_cast<u32>(2 * sizeof(s32)),
                                      static_cast<u32>(sizeof(AudioSettingsHeader)))
                , miMusicVolume(liMusicVolume)
                , miSFXVolume(liSFXVolume)
            {
            }
        };

        // Id 474 == BrnGui::GuiEventVoipSettings. The record
        // OutputGuiEvent<GuiEventVoipSettings> @0x82493CE8 builds: { 4, 474, 12, the voip
        // volume word }, channel 40, 16 bytes. (The X360 reuses the audio-settings stack
        // slot and overwrites its FIRST word with the voip volume, so the voip volume is
        // payload word 0 and the trailing sfx word is outside this record.)
        typedef CgsGui::GuiEvent<474> VoipSettingsHeader;
        struct GuiEventVoipSettingsWire : public VoipSettingsHeader
        {
            s32 miVoipVolume;    // +0x0C

            explicit GuiEventVoipSettingsWire(s32 liVoipVolume)
                : VoipSettingsHeader(static_cast<u32>(sizeof(s32)),
                                     static_cast<u32>(sizeof(VoipSettingsHeader)))
                , miVoipVolume(liVoipVolume)
            {
            }
        };

        // The OutputGuiEvent<BrnGui::GuiOverlayRequest> wire record: { 288, 184, 16,
        // <pad>, the 288-byte request }, channel 40, 304 bytes (identical shape to
        // BrnInGame.cpp's / BrnCarSelectMain_wG_02.cpp's; the payload is 8-aligned, so the
        // payload-offset word is 16, not 12).
        struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
        {
            u32               muPad0C;    // +0x0C (payload is 16-aligned past the header)
            GuiOverlayRequest mRequest;   // +0x10

            GuiOverlayRequestWire()
                : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
                , muPad0C(0)
            {
            }
        };

        // Id 148 == BrnGui::GuiEventShowHideHud (payload size 1); ShowSettingsScreen posts
        // it with the flag clear. Same wire view as BrnOnlineGameRoomPlayerInfo_wH_07.cpp.
        typedef CgsGui::GuiEvent<148> ShowHideHudHeader;
        struct GuiEventShowHideHudWire : public ShowHideHudHeader
        {
            bool mbShowHud;   // +0x0C
            u8   maPad[3];    // +0x0D (the record is 16 bytes on the wire)

            explicit GuiEventShowHideHudWire(bool lbShowHud)
                : ShowHideHudHeader(static_cast<u32>(sizeof(bool)),
                                    static_cast<u32>(sizeof(ShowHideHudHeader)))
                , mbShowHud(lbShowHud)
            {
                maPad[0] = 0;
                maPad[1] = 0;
                maPad[2] = 0;
            }
        };

        // Record-size pins. Every one of these payloads is pointer-free, so the X360's
        // AddEvent size immediate must survive unchanged on the x64 host; if a host
        // layout ever drifts, this is where it is caught rather than on the wire.
        static_assert(sizeof(GuiAudioTriggerWire) == 112, "audio trigger record is 112 bytes");
        static_assert(sizeof(GuiEventAudioSettingsWire) == 20, "audio settings record is 20 bytes");
        static_assert(sizeof(GuiEventVoipSettingsWire) == 16, "voip settings record is 16 bytes");
        static_assert(sizeof(GuiEventShowHideHudWire) == 16, "show/hide hud record is 16 bytes");
        static_assert(sizeof(GuiOverlayRequestWire) == 304, "overlay request record is 304 bytes");
        static_assert(sizeof(GuiEventActivateCrashNav) == 20, "activate crashnav record is 20 bytes");
    }

    // ---- TriggerSound @ 0x8249ADB0 -------------------------------------------------
    // Raise the menu-move audio cue for a controller action: pick the label for the
    // action's family and publish a GuiAudioTriggerEvent with it.
    void OnlineGameRoomPlayerInfo::TriggerSound(s32 leGameInputAction)
    {
        const char* lpcLabel = 0;

        switch (leGameInputAction)
        {
            case KI_ACTION_MENU_MOVE_A:
            case KI_ACTION_MENU_MOVE_B:
            case KI_ACTION_MENU_PREVIOUS:
            case KI_ACTION_MENU_NEXT:
                lpcLabel = KAC_SOUND_LABEL_MENU_TOGGLE;
                break;

            case KI_ACTION_ITEM_MOVE_A:
            case KI_ACTION_ITEM_MOVE_B:
            case KI_ACTION_ITEM_PREVIOUS:
            case KI_ACTION_ITEM_NEXT:
                lpcLabel = KAC_SOUND_LABEL_MENU_ITEM_TOGGLE;
                break;

            default:
                break;
        }

        // cpp:4709 -- non-fatal, and the X360 sinks it into the switch's default arm
        // (the other arms provably set lpcLabel). Either placement runs the same code:
        // the event is constructed with the null label on the unrecognised path.
        CGS_ASSERT(lpcLabel, "lpcLabel");

        GuiAudioTriggerWire lAudio;
        lAudio.Construct(KI_AUDIO_ACTION_MENU_CUE, "", lpcLabel, "");

        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAudio), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lAudio)));
    }

    // ---- ShowSettingsScreen @ 0x8249A780 -------------------------------------------
    // Arm the settings-page load: park the machine in the loading sub-state, take the
    // front-end map and the HUD down, bring the accept help-prompt up, and start the edit
    // with no pending change.
    void OnlineGameRoomPlayerInfo::ShowSettingsScreen()
    {
        // cpp:4311 -- the X360 fires the bare expression text (non-fatal).
        CGS_ASSERT((meSubState == E_SUBSTATE_PAUSE) || (meSubState == E_SUBSTATE_FREE_BURNING),
                   "(meSubState == E_SUBSTATE_PAUSE) || (meSubState == E_SUBSTATE_FREE_BURNING)");

        meSubState = E_SUBSTATE_LOADING_SETTINGS_SCREEN;

        // Deactivate CrashNav (X360 record { 8, 191, 12, 0, 0 }, size 20, channel 40).
        GuiEventActivateCrashNav lDeactivateCrashNav(false);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lDeactivateCrashNav), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lDeactivateCrashNav)));

        // Take the HUD down (X360 record { 1, 148, 12, 0 }, size 16, channel 40).
        GuiEventShowHideHudWire lHideHud(false);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lHideHud), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lHideHud)));

        mHelpItemAccept.Construct(KAC_HELPITEM_ACCEPT, mpStateInterface, 0);

        mbSettingsChanged = false;
    }

    // ---- HandleControllerInputSettingsSubState @ 0x824AD248 ------------------------
    // The settings page's input map. The two row moves and the two value moves each play
    // their cue only when the group actually moved; a value move additionally reports the
    // edit through HandleSettingChanged(). Accept commits the edited options back to the
    // profile and republishes them; back either raises the cancel-confirm overlay (when
    // there is an unsaved edit) or restores the profile's audio/voip volumes and leaves.
    // Both exit paths tear the toggle group down and then return to whichever page the
    // settings screen was entered from.
    void OnlineGameRoomPlayerInfo::HandleControllerInputSettingsSubState(const CgsModule::Event* lpControllerEvent)
    {
        const s32 liAction =
            reinterpret_cast<const ControllerInputPayload*>(lpControllerEvent)->miButtonId;

        switch (liAction)
        {
            case KI_ACTION_MENU_PREVIOUS:
                if (mSettingToggle.HighlightPrevious(false))
                {
                    TriggerSound(liAction);
                }
                break;

            case KI_ACTION_MENU_NEXT:
                if (mSettingToggle.HighlightNext(false))
                {
                    TriggerSound(liAction);
                }
                break;

            case KI_ACTION_ITEM_PREVIOUS:
                if (mSettingToggle.HighlightPreviousItem())
                {
                    HandleSettingChanged();
                    TriggerSound(liAction);
                }
                break;

            case KI_ACTION_ITEM_NEXT:
                if (mSettingToggle.HighlightNextItem())
                {
                    HandleSettingChanged();
                    TriggerSound(liAction);
                }
                break;

            case KI_ACTION_SELECT:
                mCrashNavOptionsData.SetToProfile(mpGuiCache->GetOptionsDataProfile());
                mCrashNavOptionsData.OutputEvents(mpGuiCache->GetOptionsDataProfile(),
                                                  mpStateInterface);

                mSettingToggle.Unloaded();
                mSettingToggle.Clear();

                if (mbShownFromPause)
                {
                    UnloadSettingsResources();
                    HideHUDAndEnterMenus();
                    ShowPauseScreen();
                }
                else
                {
                    UnloadSettingsResources();
                    HideSettingsPage();
                }
                break;

            case KI_ACTION_BACK:
                if (mbSettingsChanged)
                {
                    GuiOverlayRequestWire lWire;
                    lWire.mRequest.Construct(KAC_OVERLAY_CANCEL_CONFIRM);

                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
                        static_cast<s32>(sizeof(lWire)));
                }
                else
                {
                    // The X360 reads the two volumes into one stack record, plays the
                    // back-out cue, publishes the audio settings, then overwrites the
                    // record's first word with the voip volume and publishes that.
                    const s32 liMusicVolume = mpGuiCache->GetOptionsDataProfile()->GetMusicVolume();
                    const s32 liSFXVolume   = mpGuiCache->GetOptionsDataProfile()->GetSFXVolume();

                    GuiAudioTriggerWire lAudio;
                    lAudio.Construct(KI_AUDIO_ACTION_MENU_CUE, "", KAC_SOUND_LABEL_GO_BACK, "");
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lAudio), KI_CHANNEL_GUI_OUT,
                        static_cast<s32>(sizeof(lAudio)));

                    GuiEventAudioSettingsWire lAudioSettings(liMusicVolume, liSFXVolume);
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lAudioSettings), KI_CHANNEL_GUI_OUT,
                        static_cast<s32>(sizeof(lAudioSettings)));

                    GuiEventVoipSettingsWire lVoipSettings(
                        mpGuiCache->GetOptionsDataProfile()->GetVoipVolume());
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lVoipSettings), KI_CHANNEL_GUI_OUT,
                        static_cast<s32>(sizeof(lVoipSettings)));

                    mSettingToggle.Unloaded();
                    mSettingToggle.Clear();

                    if (mbShownFromPause)
                    {
                        UnloadSettingsResources();
                        HideHUDAndEnterMenus();
                        ShowPauseScreen();
                    }
                    else
                    {
                        UnloadSettingsResources();
                        HideSettingsPage();
                    }
                }
                break;

            default:
                break;
        }
    }
}
