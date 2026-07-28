// ===================================================================================
// BrnGui::OnlineGameRoomPlayerInfo -- wave-H partfile 10: the pause / security menus.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX (pseudocode + asm,
// scratchpad/waveB/GameSource_Gui_Flow_Screen_States_BrnOnlineGameRoomPlayerInfo.cpp.asm.txt):
//   ShowPauseOptions                            @ 0x8248BFE0 (cpp:3033)
//   ShowSecurityOptions                         @ 0x8248C2F8 (cpp:3121)
//   HandleControllerInputSecurityOptionsSubState @ 0x824AD120 (cpp:1953)
//
// All three drive the one shared pause menu component (mPauseComponent): ShowPauseOptions
// builds the top-level pause option list from the host / game-mode / challenge-manager /
// ranked state, ShowSecurityOptions replaces it with the two security settings the lobby
// is NOT currently on, and the sub-state input handler navigates and commits it.
//
// The class shape, the member map and the rodata tables live in the wave-H keystone
// scaffold BrnOnlineGameRoomPlayerInfo.{h,cpp}. X360 offsets appear only in comments --
// the host layout is name-based, and no console offset / stride / sizeof is reproduced
// as a value here.
//
// COMPONENT VTABLE (dumped from the image this pass, not assumed): MenuComponent's ctor
// @0x824FFEF8 stores &off_82074068 at this+0, so the component vtable slots are
//   +0x14 slot5  SelectableGroup::Update          +0x18 slot6  MenuComponent::Clear
//   +0x20 slot8  SelectableGroup::Enable          +0x24 slot9  SelectableGroup::Disable
//   +0x28 slot10 SelectableGroup::HighlightNext(bool)
//   +0x2C slot11 SelectableGroup::HighlightPrevious(bool)
//   +0x30 slot12 SelectableGroup::HighlightIndex
//   +0x34 slot13 0x824E4DE0 == HighlightNext(false)      (MenuComponent::HighlightNext)
//   +0x38 slot14 0x824E4DE8 == HighlightPrevious(false)  (MenuComponent::HighlightPrevious)
// So the disable loops here (vcall +0x24) are SelectableGroup::Disable -- NOT the
// non-virtual MenuComponent::DisableSelectable @0x824E2E38, which is absent from the
// table -- and the input handler's +0x38 / +0x34 pair is PREVIOUS / NEXT in that order.
// The x64 build lays out its own vtable, so every site below is a by-name call.
//
// The pause option ids are stored with a full-width `std` of a small integer, so each
// id's numeric value IS the option enum and also its KPAC_PAUSE_OPTIONS_STRING_ID row
// (`ld` + `slwi 2` at the SetText sites). SelectableGroup::GetHighlightedId likewise
// returns the whole id in one register (`ld 0x10(r3)`), and the select case narrows it
// with `clrlwi r4, r11, 0` -- the LOW word, i.e. the option enum. (Hex-Rays renders that
// narrowing as `>> 32` because it models the __int64 return in a register PAIR; the asm
// is the arbiter, per the project rule.)
//
// The X360 inlines two helpers here; both are restored to the call they were written as
// (same treatment as the committed sibling partfile 02):
//   * GameStateModuleIO::IsOnlineFreeBurnLobby -- flattened to compares against 15/16;
//   * GuiCache::GetFreeburnChallengeManager -- flattened to the null assert
//     ("mpChallengeManager", BrnGuiCache.h:2390) plus the reload of the member. The
//     out-of-line copy @0x8240F168 is instruction-for-instruction that same sequence.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameRoomPlayerInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsModule::Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::OutputGuiEvent
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"       // GuiComponent::AddOutputAptViewState
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GsmIO::EGameModeType / IsOnlineFreeBurnLobby
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiAudioTriggerEvent
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"                // BrnGui::FreeburnChallengeManager

namespace BrnGui
{
    namespace
    {
        // ---- controller actions this sub-state answers (event-6 payload word +4;
        //      EGameInputActions -- 41 GUI_UP / 42 GUI_DOWN / 49 GUI_SELECT / 50 GUI_CANCEL)
        const s32 KI_ACTION_MENU_PREVIOUS = 0x29;   // ')'
        const s32 KI_ACTION_MENU_NEXT     = 0x2A;   // '*'
        const s32 KI_ACTION_SELECT        = 0x31;   // '1'
        const s32 KI_ACTION_BACK          = 0x32;   // '2'

        // The GuiAudioTriggerEvent action + label the back press fires
        // (X360 `li r4, 7`, aGoBack "GO_BACK", and the empty rodata string unk_820046A7
        // passed for BOTH the component and the movie name).
        const s32  KI_AUDIO_ACTION_BACK   = 7;
        const char KAC_SOUND_GO_BACK[]    = "GO_BACK";
        const char KAC_EMPTY_STRING[]     = "";

        // The apt clip the pause button prompts transition through (X360 aAptTransition_1,
        // the AddOutputAptViewState first argument at 0x8248C2D4 / 0x8248C4A4).
        const char KAC_APT_TRANSITION[]   = "apt_Transition";

        // ---- in-queue payload view (the queue delivers the HEADER-STRIPPED payload, so
        //      the DWARF's typed event pointer would mislead every field by 12 --
        //      BrnInGame.cpp / BrnBootProfile.cpp idiom, used by every sibling partfile).
        // CgsGui::GuiEventControllerInputPressed (GuiEvent<6>) minus its 12-byte queue
        // header; field names from that type's home, CgsGuiEventTypeDefs.h. This handler
        // reads only the second word.
        struct GuiEventControllerInputPressedPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (EGameInputActions)
        };

        // BrnGui::GuiEventPerformOnlinePauseOption::EPauseOptions -- DWARF
        // BrnGuiEventTypeDefs.h:6520. The event type's only home in this tree is
        // BrnGuiDemangledEventTypes.h, which HARD-COLLIDES with BrnGuiEventTypeDefs.h
        // (pulled in through the class header) and models the event as an opaque 4-byte
        // payload, so the enum is mirrored here as a file-local view exactly the way the
        // sibling partfiles mirror the demangled-only payload shapes. Every value below
        // is an X360 literal in ShowPauseOptions / ShowSecurityOptions, and each doubles
        // as its row index into KPAC_PAUSE_OPTIONS_STRING_ID.
        enum EPauseOptions
        {
            E_PAUSE_OPTIONS_LAUNCH                = 0,
            E_PAUSE_OPTIONS_CREATE_EVENT          = 1,
            E_PAUSE_OPTIONS_CHANGE_TEAMS          = 2,
            E_PAUSE_OPTIONS_VIEW_LOBBY            = 3,
            E_PAUSE_OPTIONS_LEAVE_GAME            = 4,
            E_PAUSE_OPTIONS_VIEW_CHALLENGES       = 5,
            E_PAUSE_OPTIONS_VIEW_EVENT            = 6,
            E_PAUSE_OPTIONS_CANCEL_EVENT          = 7,
            E_PAUSE_OPTIONS_VIEW_MAP              = 8,
            E_PAUSE_OPTIONS_CHANGE_SECURITY       = 9,
            E_PAUSE_OPTIONS_SECURITY_ALLCOMERS    = 10,
            E_PAUSE_OPTIONS_SECURITY_FRIENDS_ONLY = 11,
            E_PAUSE_OPTIONS_SECURITY_INVITE_ONLY  = 12,
            E_PAUSE_OPTIONS_SETTINGS              = 13,
            E_PAUSE_OPTIONS_COUNT                 = 14,
        };

        // The lobby's security setting (GuiCache::meOnlineSecurity, X360 cache +0xA9C0 --
        // BrnNetwork::EBrnGameSecurity per its home header's annotation: 0 public /
        // 1 private / 2 closed). The three values are the unsigned compare set the X360
        // switch emits (`cmplwi 1` / `cmplwi 3`) at 0x8248C324.
        const s32 KI_SECURITY_PUBLIC  = 0;
        const s32 KI_SECURITY_PRIVATE = 1;
        const s32 KI_SECURITY_CLOSED  = 2;
    }

    // ------------------------------------------------------ ShowPauseOptions @0x8248BFE0
    // Build the online pause menu. The rows are gathered in X360 order: the host's
    // launch / create-event pair (which of the two depends on whether the session is a
    // free-burn lobby), then team change (road rage only), event cancel (host, outside a
    // lobby), the freeburn-challenge page (unless a challenge is running or showing its
    // results), the security page (unranked host only), and finally the four rows every
    // pause menu carries. The unfilled rows of the fixed 11-row menu are disabled.
    void OnlineGameRoomPlayerInfo::ShowPauseOptions()
    {
        // The console reserves the id buffer for the whole pause-option bound; the ids
        // past liOptionIndex are left as SetupMenu found them, exactly as the X360 does
        // (SelectableGroup::SetIds walks the group's selectable count, not the fill).
        u64 lau64OptionIds[KI_MAX_PAUSE_OPTIONS];
        s32 liOptionIndex = 0;

        if (mpGuiCache->mbIsOnlineHost)
        {
            // Inlined by the X360 into compares against 15/16 (0x8248C024).
            if (BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(
                    static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                        mpGuiCache->meOnlineGameMode)))
            {
                lau64OptionIds[0] = E_PAUSE_OPTIONS_CREATE_EVENT;
                liOptionIndex     = 1;
            }
            else
            {
                lau64OptionIds[0] = E_PAUSE_OPTIONS_LAUNCH;
                lau64OptionIds[1] = E_PAUSE_OPTIONS_VIEW_EVENT;
                liOptionIndex     = 2;
            }
        }
        else if (!BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(
                     static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                         mpGuiCache->meOnlineGameMode)))
        {
            lau64OptionIds[0] = E_PAUSE_OPTIONS_VIEW_EVENT;
            liOptionIndex     = 1;
        }

        if (mpGuiCache->meOnlineGameMode
                == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
        {
            lau64OptionIds[liOptionIndex] = E_PAUSE_OPTIONS_CHANGE_TEAMS;
            ++liOptionIndex;
        }

        if (mpGuiCache->mbIsOnlineHost
            && !BrnGameState::GameStateModuleIO::IsOnlineFreeBurnLobby(
                   static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                       mpGuiCache->meOnlineGameMode)))
        {
            lau64OptionIds[liOptionIndex] = E_PAUSE_OPTIONS_CANCEL_EVENT;
            ++liOptionIndex;
        }

        // The accessor carries the "mpChallengeManager" assert (BrnGuiCache.h:2390) the
        // X360 inlined here ahead of the state read.
        const FreeburnChallengeManager* lpChallengeManager =
            mpGuiCache->GetFreeburnChallengeManager();
        if (!lpChallengeManager->IsRunning() && !lpChallengeManager->IsShowingResults())
        {
            lau64OptionIds[liOptionIndex] = E_PAUSE_OPTIONS_VIEW_CHALLENGES;
            ++liOptionIndex;
        }

        if (mpGuiCache->mbIsOnlineHost && !mpGuiCache->mbOnlineRanked)
        {
            lau64OptionIds[liOptionIndex] = E_PAUSE_OPTIONS_CHANGE_SECURITY;
            ++liOptionIndex;
        }

        lau64OptionIds[liOptionIndex]     = E_PAUSE_OPTIONS_VIEW_LOBBY;
        lau64OptionIds[liOptionIndex + 1] = E_PAUSE_OPTIONS_VIEW_MAP;
        lau64OptionIds[liOptionIndex + 2] = E_PAUSE_OPTIONS_SETTINGS;
        lau64OptionIds[liOptionIndex + 3] = E_PAUSE_OPTIONS_LEAVE_GAME;
        liOptionIndex += 4;

        // Both asserts fire from the one overflow test in the asm (`cmpwi r28, 0xB` at
        // 0x8248C1F0), cpp:3102 then cpp:3103. Both bounds are 11 on ARTIST.
        CGS_ASSERT(liOptionIndex <= KI_MAX_DISPLAYABLE_PAUSE_OPTIONS,
                   "liOptionIndex <= KI_MAX_DISPLAYABLE_PAUSE_OPTIONS");
        CGS_ASSERT(liOptionIndex <= KI_MAX_PAUSE_OPTIONS,
                   "liOptionIndex <= KI_MAX_PAUSE_OPTIONS");

        mPauseComponent.SetupMenu(KI_MAX_DISPLAYABLE_PAUSE_OPTIONS, true);

        s32 liRow = 0;
        for (; liRow < liOptionIndex; ++liRow)
        {
            mPauseComponent.SetText(
                liRow,
                KPAC_PAUSE_OPTIONS_STRING_ID[static_cast<s32>(lau64OptionIds[liRow])]);
        }
        for (; liRow < KI_MAX_DISPLAYABLE_PAUSE_OPTIONS; ++liRow)
        {
            mPauseComponent.Disable(liRow);   // component vtable slot 9
        }

        mPauseComponent.SetIds(lau64OptionIds);
        mPauseButtonPromptsAnimation.AddOutputAptViewState(
            KAC_APT_TRANSITION, KAPC_PAUSE_BUTTON_PROMPT_ANIMATION_STATES[0], false);
    }

    // --------------------------------------------------- ShowSecurityOptions @0x8248C2F8
    // Replace the pause menu with the security page: the two settings the lobby is not
    // currently on, then the sub-state flip and the button-prompt transition.
    void OnlineGameRoomPlayerInfo::ShowSecurityOptions()
    {
        u64 lau64OptionIds[KI_MAX_PAUSE_OPTIONS];
        s32 liOptionIndex = 0;

        switch (mpGuiCache->meOnlineSecurity)
        {
        case KI_SECURITY_PUBLIC:
            lau64OptionIds[0] = E_PAUSE_OPTIONS_SECURITY_FRIENDS_ONLY;
            lau64OptionIds[1] = E_PAUSE_OPTIONS_SECURITY_INVITE_ONLY;
            liOptionIndex     = 2;
            break;

        case KI_SECURITY_PRIVATE:
            lau64OptionIds[0] = E_PAUSE_OPTIONS_SECURITY_ALLCOMERS;
            lau64OptionIds[1] = E_PAUSE_OPTIONS_SECURITY_INVITE_ONLY;
            liOptionIndex     = 2;
            break;

        case KI_SECURITY_CLOSED:
            lau64OptionIds[0] = E_PAUSE_OPTIONS_SECURITY_ALLCOMERS;
            lau64OptionIds[1] = E_PAUSE_OPTIONS_SECURITY_FRIENDS_ONLY;
            liOptionIndex     = 2;
            break;

        default:
            // cpp:3167. The X360 streams "Unknown security value of " << meOnlineSecurity
            // << "\n" into CgsDev::Assert::gpcMessageBuffer before FireAssert; modelled
            // through CGS_ASSERT per this tree's assert-machinery convention (the live
            // value is not re-streamed). No ids are written on this path.
            CGS_ASSERT(false, "Unknown security value");
            break;
        }

        mPauseComponent.SetupMenu(KI_MAX_DISPLAYABLE_PAUSE_OPTIONS, true);

        s32 liRow = 0;
        for (; liRow < liOptionIndex; ++liRow)
        {
            mPauseComponent.SetText(
                liRow,
                KPAC_PAUSE_OPTIONS_STRING_ID[static_cast<s32>(lau64OptionIds[liRow])]);
        }
        for (; liRow < KI_MAX_DISPLAYABLE_PAUSE_OPTIONS; ++liRow)
        {
            mPauseComponent.Disable(liRow);   // component vtable slot 9
        }

        mPauseComponent.SetIds(lau64OptionIds);
        meSubState = E_SUBSTATE_SECURITY_OPTIONS;
        mPauseButtonPromptsAnimation.AddOutputAptViewState(
            KAC_APT_TRANSITION, KAPC_PAUSE_BUTTON_PROMPT_ANIMATION_STATES[1], false);
    }

    // ------------------------- HandleControllerInputSecurityOptionsSubState @0x824AD120
    // The security page's input map: move the highlight, commit the highlighted setting
    // (its id's low word is the pause option) and go back to the pause screen, or back
    // out with the go-back sound. Every other action is ignored.
    void OnlineGameRoomPlayerInfo::HandleControllerInputSecurityOptionsSubState(
        const CgsModule::Event* lpEvent)
    {
        const GuiEventControllerInputPressedPayload* lpInput =
            reinterpret_cast<const GuiEventControllerInputPressedPayload*>(lpEvent);

        switch (lpInput->miButtonId)
        {
        case KI_ACTION_MENU_PREVIOUS:
            mPauseComponent.HighlightPrevious();   // component vtable slot 14
            break;

        case KI_ACTION_MENU_NEXT:
            mPauseComponent.HighlightNext();       // component vtable slot 13
            break;

        case KI_ACTION_SELECT:
            PerformPauseOption(static_cast<s32>(mPauseComponent.GetHighlightedId()));
            ShowPauseScreen();
            break;

        case KI_ACTION_BACK:
        {
            GuiAudioTriggerEvent lAudioEvent;
            lAudioEvent.Construct(KI_AUDIO_ACTION_BACK, KAC_EMPTY_STRING,
                                  KAC_SOUND_GO_BACK, KAC_EMPTY_STRING);
            mpStateInterface->OutputGuiEvent(lAudioEvent);
            ShowPauseScreen();
            break;
        }

        default:
            break;
        }
    }
}
