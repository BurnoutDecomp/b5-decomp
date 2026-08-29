// ===================================================================================
// BrnGui::CrashNavPanel  -- implementation
//   class:BrnGui::CrashNavPanel
//
// The crash-nav map's data panel: a two-row MenuToggleGroupVarSize<3> filter header over
// five mutually-exclusive sub-panels (event / drive-thru / road-rule / rival / generic
// text). Row 0 picks the top-level filter (which sub-panel), row 1 the per-panel
// second-level filter (which game mode, which road rule), and row 2 is the in-event
// single-row variant. Every typed accessor guards its read with a CGS_ASSERT that
// mePanelType (+0x90) is the matching sub-panel.
//
// ⭐ WAVE F1 (main-menu wave, 2026-08-29). This TU used to carry FOUR of the class's
// twenty-five X360 methods; the S2 census measured fourteen link holes against it, the
// single biggest owner in the whole crash-nav family. All twenty-five are bodied here now.
//
// X360 ARTIST addresses (all attested in scratch/func_index.tsv):
//   CrashNavPanel (default ctor)     @ 0x82500FD0     Construct                @ 0x82425C60
//   AppendExpectedAptComponents      @ 0x82425EC8     SetupComponent           @ 0x82440378
//   RestoreSettings                  @ 0x82440258     StoreSettings            @ 0x82418708
//   Update                           @ 0x82418810     ChangeVisiblePanelState  @ 0x8243A548
//   RecEvent                         @ 0x82441F58     ShowBlank                @ 0x8243A820
//   SetEventPanelData                @ 0x8243A878     SetDrivethruPanelData    @ 0x8243A8F0
//   SetRoadPanelData                 @ 0x8243A978     ToggleRoadPanelScores    @ 0x8243AA38
//   SetRivalPanelData()              @ 0x8243AAC8     SetRivalPanelData(CgsID) @ 0x8243AB68
//   SetRivalPanelData(name,id)       @ 0x8243ABF0     HandleAptEvents          @ 0x82418828
//   HandleControllerInput            @ 0x824408E0     RefreshSecondLevelFilter @ 0x8243AC60
//   UpdateDataPanel                  @ 0x8242D410     TriggerSound             @ 0x8243AF30
//   IsRoadRuleFriendSelected         @ 0x824188B0     GetRoadRuleFriendSelectedName @ 0x82418938
//   GetPanelActiveGameModeType       @ 0x824BAE58     GetPanelActiveRoadRuleType    @ 0x824185C8
//   GetRoadPanelScoreMode            @ 0x82418668
// The one DWARF method with NO X360 symbol -- SetAnimState (DWARF cpp:499) -- is declared
// in the header with a blocked-reason comment and deliberately has no body here.
//
// Enum/member spellings are DWARF-supplied and corroborated by the X360 assert literals
// ("E_PANEL_EVENT == mePanelType", "mePrepareStage == E_PREPARESTAGE_CONSTRUCTED",
// "E_FILTER_LEVEL_SECOND == liCurrentlySelectedFilter", ...), which are reproduced verbatim.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/Components/BrnCrashNavPanel.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                              // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"                       // CgsGui::GuiEventControllerInputPressed
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"          // CgsGui::StateInterface (OutputGuiEvent)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h"      // CgsGui::GuiEventAptTriggerPayload
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"                   // CgsLanguage::LanguageManager::FindString
#include "GameSource/Gui/BrnGuiCache.h"                                           // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiWorldDataController.h"                             // BrnGui::WorldDataController
#include "GameSource/Gui/Events/BrnGuiEventRankProgressResponse.h"                // BrnGui::GuiEventRankProgressResponse
#include "GameSource/GameState/BrnCgsPlayerName.h"                                // CgsNetwork::PlayerName
#include "SharedClasses/Progression/BrnProgressionData.h"                         // BrnProgression::ProgressionData

#include <cstring>   // std::strstr / std::strcmp

namespace BrnGui
{
    // ---------------------------------------------------------------------------------
    // FILE-STATIC DATA (DWARF BrnCrashNavPanel.cpp:25..:64). Every string below is X360
    // rodata read straight off the asm's string comments; array LENGTHS are the DWARF's.
    // ---------------------------------------------------------------------------------

    // DWARF cpp:41..:48 -- the apt component names Construct wires. Each declared length
    // matches its literal exactly (13/14/18/13/14/16/9/9 incl. the terminator), which is
    // what corroborates the strings the X360 asm comments give.
    static const char KAC_OPTION_NAME[13]              = "filterToggle";
    static const char KAC_EVENT_INFO_PANEL_NAME[14]    = "eventPanel_mc";
    static const char KAC_DRIVETHRU_PANEL_NAME[18]     = "drivethruPanel_mc";
    static const char KAC_ROAD_RULE_PANEL_NAME[13]     = "roadPanel_mc";
    static const char KAC_RIVAL_PANEL_NAME[14]         = "rivalPanel_mc";
    static const char KAC_GENERIC_PANEL_NAME[16]       = "genericPanel_mc";
    static const char KAC_GENERIC_PANEL_TEXT_1_NAME[9] = "Generic1";
    static const char KAC_GENERIC_PANEL_TEXT_2_NAME[9] = "Generic2";

    // DWARF cpp:58 -- the apt variable the panel drives its animation phase through
    // (`AddOutputAptViewState("apt_label", ...)` @0x824408A8; char[10] == 9 + terminator).
    static const char macAnimationVarName[10] = "apt_label";

    // DWARF cpp:25 -- the two toggle-row captions (X360 off_82F251E0, 3 pointers).
    // ⚠️ FLAG UNRECOVERED: only entries [0] and [1] are attested (the asm comments them as
    // "$CN_PANEL_FILTER" @0x824403F4 and "$CN_PANEL_TYPE" @0x824405A8). Entry [2] is a
    // pointer IDA never resolves because no recovered body reads it, and the string is not
    // present in build/game or in the Xbox One image's export set. It is left NULL rather
    // than invented; no reconstructed body indexes it.
    static const char* KAPC_OPTION_HEADINGS[3] =
    {
        "$CN_PANEL_FILTER",   // [0] @0x82F251E0
        "$CN_PANEL_TYPE",     // [1] @0x82F251E4
        0,                    // [2] @0x82F251E8 -- FLAG UNRECOVERED
    };

    // DWARF cpp:33 -- the four TOP-LEVEL filter option labels (X360 off_82F251EC, 4
    // pointers), one per PanelType in E_PANEL_EVENT..E_PANEL_RIVALS order.
    // ⚠️ FLAG UNRECOVERED: only [0] is attested ("$CN_PANEL_EVENTS" @0x824403E8). The array
    // is passed WHOLE to SetupToggle, so the missing three are a real runtime gap on this
    // build (the drive-thru / road-rule / rival labels render empty) -- recorded here rather
    // than papered over with invented ids.
    static const char* KAPC_TOP_LEVEL_OPTION_LABELS[4] =
    {
        "$CN_PANEL_EVENTS",   // [0] @0x82F251EC  (E_PANEL_EVENT)
        0,                    // [1] @0x82F251F0  (E_PANEL_DRIVETHRU) -- FLAG UNRECOVERED
        0,                    // [2] @0x82F251F4  (E_PANEL_ROADSIGN)  -- FLAG UNRECOVERED
        0,                    // [3] @0x82F251F8  (E_PANEL_RIVALS)    -- FLAG UNRECOVERED
    };

    // DWARF cpp:50 -- the apt view-state names indexed by CrashNavPanel::AnimState
    // (X360 off_82F251FC, 4 pointers).
    // ⚠️ FLAG UNRECOVERED: [0] "Invisible" and [1] "transIn" are attested by the asm comment
    // pair at 0x824408B0/0x824408B4; [2] (IDLE) and [3] (TRANS_OUT) have no reader in any
    // recovered body -- SetAnimState, their only consumer, has no X360 symbol at all.
    static const char* mpacAnimationStrings[CrashNavPanel::E_ANIMSTATE_COUNT] =
    {
        "Invisible",   // [0] E_ANIMSTATE_INVISIBLE @0x82F251FC
        "transIn",     // [1] E_ANIMSTATE_TRANS_IN  @0x82F25200
        0,             // [2] E_ANIMSTATE_IDLE      -- FLAG UNRECOVERED
        0,             // [3] E_ANIMSTATE_TRANS_OUT -- FLAG UNRECOVERED
    };

    // DWARF cpp:61..:64 -- the defaults StoreSettings(true) writes (X360 immediates
    // @0x82418724..0x82418734: 0, 5, 0) and the online override RestoreSettings applies
    // (`if (onlineStart && mePanelType == 0) mePanelType = 3` @0x824402F8).
    static const CrashNavPanel::PanelType    K_DEFAULT_PANELTYPE        = CrashNavPanel::E_PANEL_EVENT;   // 0
    static const CrashNavPanel::PanelType    K_DEFAULT_PANELTYPE_ONLINE = CrashNavPanel::E_PANEL_RIVALS;  // 3
    static const EventPanel::EEventType      K_DEFAULT_EVENTMODE        = EventPanel::E_EVENT_TYPE_ALL;   // 5
    static const BrnStreetData::ScoreType    K_DEFAULT_RR_SCORETYPE     = static_cast<BrnStreetData::ScoreType>(0);

    // ---------------------------------------------------------------------------------
    // FILE-LOCAL CONSTANTS recovered from the X360 immediates. The tree has no shared
    // header for EGameInputActions (every GUI TU re-declares the ids it consumes), so the
    // four this panel reads are spelled out here with their DecFIGS-DWARF names
    // (references/DecFIGS/dwarfdump/GameSource/Input/GameInputActions.h:24).
    // ---------------------------------------------------------------------------------
    static const s32 KI_ACTION_GUI_DPAD_UP    = 37;
    static const s32 KI_ACTION_GUI_DPAD_DOWN  = 38;
    static const s32 KI_ACTION_GUI_DPAD_LEFT  = 39;
    static const s32 KI_ACTION_GUI_DPAD_RIGHT = 40;
    // TriggerSound's second sound bucket also covers these four (the analogue-stick menu
    // family), which never reach this panel's own switch but do reach its sound helper.
    static const s32 KI_ACTION_GUI_UP         = 41;
    static const s32 KI_ACTION_GUI_DOWN       = 42;
    static const s32 KI_ACTION_GUI_LEFT       = 43;
    static const s32 KI_ACTION_GUI_RIGHT      = 44;

    // The two toggle rows' indices inside mFilterToggles, and the in-event single row.
    static const s32 KI_TOGGLE_TOP_LEVEL   = 0;
    static const s32 KI_TOGGLE_SECOND_LEVEL = 1;
    static const s32 KI_TOGGLE_IN_EVENT    = 2;
    static const s32 KI_NUM_FILTER_TOGGLES = 3;   // `li r6, 3` @0x82425E44 / SetupGroup(3, 1)

    // Which toggle row HandleControllerInput's D-pad-left/right arms are stepping, read off
    // mFilterToggles.miHighlightedIndex. E_FILTER_LEVEL_SECOND is X360-attested verbatim by
    // the assert literal at cpp:894 / cpp:920; E_FILTER_LEVEL_FIRST is the 0 the same
    // compares test against -- FLAG: that spelling is inferred from its sibling.
    static const s32 KI_FILTER_LEVEL_FIRST  = 0;
    static const s32 KI_FILTER_LEVEL_SECOND = 1;

    // The audio labels TriggerSound posts (X360 rodata @0x8243AF7C / @0x8243AF94) and the
    // GuiAudioTriggerEvent action id it stamps them with (`li r4, 7` @0x8243AFB4).
    static const char KAC_SOUND_MENU_TOGGLE_DEFAULT[]      = "MenuToggleDefault";
    static const char KAC_SOUND_MENU_ITEM_TOGGLE_DEFAULT[] = "MenuItemToggleDefault";
    static const s32  KI_AUDIO_TRIGGER_ACTION              = 7;

    // The build's shared empty string literal (X360 `&unk_820046A7`).
    static const char KAC_EMPTY_STRING[] = "";

    // The "not in an event" GsmIO::EGameModeType values SetupComponent and
    // RefreshSecondLevelFilter both branch on (`cmpwi -1` / `cmpwi 15`).
    // FLAG: the enumerator SPELLINGS are not recovered -- no assert literal names them --
    // so the two immediates are carried as measured constants.
    static const s32 KI_GAME_MODE_TYPE_NONE      = -1;
    static const s32 KI_GAME_MODE_TYPE_FREEBURN  = 15;

    // The apt view state the rival re-show arm of ChangeVisiblePanelState pushes
    // (X360 rodata @0x8243A79C).
    static const char KAC_TRANSITION_IN_RIVAL[] = "transInRival";
    static const char KAC_TRANSITION_IN[]       = "transIn";
    static const char KAC_TRANSITION_OUT[]      = "transOut";

    // ---------------------------------------------------------------------------------
    // @ 0x82500FD0 -- the compiler-synthesised default ctor (the PS3 DWARF carries the
    // synthesised `CrashNavPanel()`; the one caller is CrashNavMap::CrashNavMap
    // @0x825114B8, constructing the by-value mCrashNavPanel at state+0x6E0). The X360
    // body is exactly:
    //   * its own single-slot vtable install (off_82074814 -> Construct @0x82425C60),
    //   * `bl` MenuToggleGroupVarSize<3>::MenuToggleGroupVarSize (@0x82500DB8) on
    //     mFilterToggles (this+0xA0) -- the only member ctor the compiler left
    //     out-of-line, and
    //   * 33 inlined sub-object vtable installs at the DWARF member offsets of the
    //     by-value panels / text fields / icon components.
    // It stores NO data members (mePrepareStage/mePanelType/... are written later by
    // Construct / StoreSettings). All of the above is what C++ emits IMPLICITLY for this
    // member list, so the faithful reconstruction is an empty body -- the same reading
    // the committed MenuToggleGroupVarSize<N> and TextSelection ctors carry.
    // (The mRoadPanel carve that used to be the one documented divergence here is gone:
    //  BrnRoadPanel.h now carries its DWARF member run.)
    CrashNavPanel::CrashNavPanel()
    {
    }

    // @ 0x82425C60 -- CgsGui::GuiComponent virtual, vtable slot 0.
    void CrashNavPanel::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                 const char* lpacParentName)
    {
        CGS_ASSERT(lpacName != 0,
                   "Invalid name sent to CrashNavPanel::Construct");              // cpp:91
        CGS_ASSERT(lpStateInterface != 0,
                   "Invalid state interface sent to CrashNavPanel::Construct");   // cpp:92

        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

        mePrepareStage = E_PREPARESTAGE_CONSTRUCTED;   // stw 0 -> +0x8C
        mpGuiCache     = 0;                            // stw 0 -> +0x98
        mePanelType    = E_PANEL_COUNT;                // stw 5 -> +0x90
        meVisiblePanel = E_PANEL_COUNT;                // stw 5 -> +0x94

        // `li r8, -1` -- the "no apt id" sentinel the group forwards to each row.
        mFilterToggles.Construct(KAC_OPTION_NAME, lpStateInterface, KI_NUM_FILTER_TOGGLES,
                                 GetName(), static_cast<u64>(-1));
        mFilterToggles.SetupGroup(KI_NUM_FILTER_TOGGLES, 1);

        // The four sub-panels are constructed through their own vtable slot 0 on the
        // console (`(**(this + N))(this + N, ...)`); by name here.
        mEventPanel.Construct(KAC_EVENT_INFO_PANEL_NAME, lpStateInterface, GetName());
        mDrivethruPanel.Construct(KAC_DRIVETHRU_PANEL_NAME, lpStateInterface, GetName());
        mRoadPanel.Construct(KAC_ROAD_RULE_PANEL_NAME, lpStateInterface, GetName());
        mRivalPanel.Construct(KAC_RIVAL_PANEL_NAME, lpStateInterface, GetName());

        // The generic panel carries no state-identifier table (`li r6, 0` @0x82425F60);
        // its two text fields are parented on the generic panel's own component name
        // (`addi r6, r31, 0x4FDC` == mGenericPanel.macName).
        mGenericPanel.Construct(KAC_GENERIC_PANEL_NAME, lpStateInterface, 0, GetName());
        mGenericPanelText1.Construct(KAC_GENERIC_PANEL_TEXT_1_NAME, lpStateInterface,
                                     mGenericPanel.GetName());
        mGenericPanelText2.Construct(KAC_GENERIC_PANEL_TEXT_2_NAME, lpStateInterface,
                                     mGenericPanel.GetName());
    }

    // @ 0x82425EC8
    void CrashNavPanel::AppendExpectedAptComponents(GuiFlow leFlow, GuiCache* lpGuiCache)
    {
        CGS_ASSERT(lpGuiCache != 0, "lpGuiCache");   // cpp:250

        mFilterToggles.AppendExpectedAptComponent(leFlow, lpGuiCache, true);   // `li r6, 1`
        mEventPanel.AppendExpectedAptComponents(leFlow, lpGuiCache);
        mDrivethruPanel.AppendExpectedAptComponents(leFlow, lpGuiCache);
        mRoadPanel.AppendExpectedAptComponents(leFlow, lpGuiCache);
        mRivalPanel.AppendExpectedAptComponents(leFlow, lpGuiCache);
    }

    // @ 0x82440378 -- the one-shot component set-up, run once the GuiCache has arrived.
    void CrashNavPanel::SetupComponent()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                                  // cpp:270
        CGS_ASSERT(mePrepareStage == E_PREPARESTAGE_CONSTRUCTED,
                   "mePrepareStage == E_PREPARESTAGE_CONSTRUCTED");                 // cpp:271

        // Row 0: the four top-level filters. Row 2: the empty in-event row, disabled
        // straight away (it is only switched on further down, in the in-event branch).
        mFilterToggles.SetupToggle(KI_TOGGLE_TOP_LEVEL, E_PANEL_SELECTABLE_COUNT, true,
                                   KAPC_OPTION_HEADINGS[0], KAPC_TOP_LEVEL_OPTION_LABELS, 0);
        RestoreSettings();
        mFilterToggles.SetupToggle(KI_TOGGLE_IN_EVENT, 0, true, 0, 0, 0);

        MenuToggle* lpInEventRow = mFilterToggles.GetSelectable(KI_TOGGLE_IN_EVENT);
        lpInEventRow->SetActive(false);          // vtable slot 0
        lpInEventRow->SetHighlightable(false);   // vtable slot 1
        lpInEventRow->SetSelectable(false);      // vtable slot 2

        MenuToggle* const lpTopRow = mFilterToggles.GetSelectable(KI_TOGGLE_TOP_LEVEL);

        if (mpGuiCache->IsOnlineStartInProgress())   // lbz mpGuiCache+0x4B4C
        {
            // Online: the offline-only filters (events, drive-thrus) are struck out.
            Selectable* lpOption = lpTopRow->mItemText.GetSelectable(E_PANEL_EVENT);
            lpOption->SetActive(false);
            lpOption->SetSelectable(false);

            lpOption = mFilterToggles.GetSelectable(KI_TOGGLE_TOP_LEVEL)
                           ->mItemText.GetSelectable(E_PANEL_DRIVETHRU);
            lpOption->SetActive(false);
            lpOption->SetSelectable(false);
        }
        else
        {
            // Offline: the rivals filter is struck out.
            Selectable* lpOption = lpTopRow->mItemText.GetSelectable(E_PANEL_RIVALS);
            lpOption->SetActive(false);
            lpOption->SetSelectable(false);

            const s32 leGameModeType = mpGuiCache->GetCurrentGameModeType();   // +0x9E58
            const bool lbOutOfEvent  = (leGameModeType == KI_GAME_MODE_TYPE_NONE)
                                    || (leGameModeType == KI_GAME_MODE_TYPE_FREEBURN);
            if (!lbOutOfEvent)
            {
                // IN AN EVENT. The panel collapses to the single in-event row: both real
                // filter rows are populated, locked to one entry, then deactivated, and row
                // 2 becomes the highlighted one.
                mFilterToggles.SetupToggle(KI_TOGGLE_SECOND_LEVEL, 6, true,
                                           KAPC_OPTION_HEADINGS[1],
                                           EventPanel::KAPC_EVENT_FILTER_OPTIONS, 0);

                mFilterToggles.GetSelectable(KI_TOGGLE_TOP_LEVEL)
                    ->mItemText.GetSelectable(E_PANEL_ROADSIGN)->SetActive(false);
                mFilterToggles.GetSelectable(KI_TOGGLE_TOP_LEVEL)
                    ->mItemText.GetSelectable(E_PANEL_DRIVETHRU)->SetActive(false);
                mFilterToggles.GetSelectable(KI_TOGGLE_TOP_LEVEL)
                    ->mItemText.GetSelectable(E_PANEL_EVENT)->SetSelectable(true);
                mFilterToggles.HighlightItem(KI_TOGGLE_TOP_LEVEL, E_PANEL_EVENT);

                // Row 1: everything but option 5 (E_EVENT_TYPE_ALL) is struck out.
                mFilterToggles.GetSelectable(KI_TOGGLE_SECOND_LEVEL)
                    ->mItemText.GetSelectable(EventPanel::E_EVENT_TYPE_RACE)->SetActive(false);
                mFilterToggles.GetSelectable(KI_TOGGLE_SECOND_LEVEL)
                    ->mItemText.GetSelectable(EventPanel::E_EVENT_TYPE_ROAD_RAGE)->SetActive(false);
                mFilterToggles.GetSelectable(KI_TOGGLE_SECOND_LEVEL)
                    ->mItemText.GetSelectable(EventPanel::E_EVENT_TYPE_SURVIVOR)->SetActive(false);
                mFilterToggles.GetSelectable(KI_TOGGLE_SECOND_LEVEL)
                    ->mItemText.GetSelectable(EventPanel::E_EVENT_TYPE_STUNT_ATTACK)->SetActive(false);
                mFilterToggles.GetSelectable(KI_TOGGLE_SECOND_LEVEL)
                    ->mItemText.GetSelectable(EventPanel::E_EVENT_TYPE_BURNING_ROUTE)->SetActive(false);
                mFilterToggles.GetSelectable(KI_TOGGLE_SECOND_LEVEL)
                    ->mItemText.GetSelectable(EventPanel::E_EVENT_TYPE_ALL)->SetSelectable(true);
                mFilterToggles.HighlightItem(KI_TOGGLE_SECOND_LEVEL, EventPanel::E_EVENT_TYPE_ALL);

                lpInEventRow->SetActive(true);
                lpInEventRow->SetHighlightable(true);
                lpInEventRow->SetSelectable(true);
                lpInEventRow->SetHighlighted(true);

                mFilterToggles.GetSelectable(KI_TOGGLE_TOP_LEVEL)->SetSelectable(false);
                mFilterToggles.GetSelectable(KI_TOGGLE_TOP_LEVEL)->SetHighlighted(false);
                mFilterToggles.GetSelectable(KI_TOGGLE_TOP_LEVEL)->SetActive(false);
                mFilterToggles.GetSelectable(KI_TOGGLE_SECOND_LEVEL)->SetSelectable(false);
                mFilterToggles.GetSelectable(KI_TOGGLE_SECOND_LEVEL)->SetHighlighted(false);
                mFilterToggles.GetSelectable(KI_TOGGLE_SECOND_LEVEL)->SetActive(false);

                mePanelType = E_PANEL_EVENT;   // stw 0 -> +0x90
            }
        }

        AddOutputAptViewState(macAnimationVarName,
                              mpacAnimationStrings[E_ANIMSTATE_TRANS_IN], false);
        ChangeVisiblePanelState(mePanelType, RivalMapPanel::E_RIVAL_TYPE_COUNT);
        mePrepareStage = E_PREPARESTAGE_DONE;   // stw 1 -> +0x8C
    }

    // @ 0x82440258 -- re-apply the saved triple over a freshly-built top-level row.
    void CrashNavPanel::RestoreSettings()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:190

        // The live road rule decides which of the two score types the panel comes back on:
        // rules 1/2 map to score type 0, rules 3/4 to score type 1. Any other value leaves
        // meSavedRRScoreType untouched (the console falls straight through).
        const s32 leActiveRoadRule = mpGuiCache->GetActiveRoadRule();   // +0xAC3C
        if (leActiveRoadRule == 1 || leActiveRoadRule == 2)
        {
            meSavedRRScoreType = static_cast<BrnStreetData::ScoreType>(0);
        }
        else if (leActiveRoadRule == 3 || leActiveRoadRule == 4)
        {
            meSavedRRScoreType = static_cast<BrnStreetData::ScoreType>(1);
        }

        // The console reads meSavedEventMode BEFORE it overwrites mePanelType; kept.
        const EventPanel::EEventType leSavedEventMode = meSavedEventMode;
        mePanelType = meSavedPanelType;

        mEventPanel.SetCurrentGameMode(leSavedEventMode);
        mRoadPanel.SetCurrentRule(static_cast<s32>(meSavedRRScoreType));

        if (mpGuiCache->IsOnlineStartInProgress() && mePanelType == E_PANEL_EVENT)
        {
            mePanelType = K_DEFAULT_PANELTYPE_ONLINE;
        }

        // Re-highlight the top-level option for the restored panel; dirty the row when the
        // highlight actually moved (`*(row + 0xC) |= 0x10` == Selectable::SetDirty).
        const PanelType leSelected = mePanelType;
        MenuToggle* const lpTopRow = mFilterToggles.GetSelectable(KI_TOGGLE_TOP_LEVEL);
        if (lpTopRow->mItemText.HighlightIndex(leSelected))
        {
            lpTopRow->SetDirty();
        }

        RefreshSecondLevelFilter(meSavedEventMode,
                                 static_cast<s32>(meSavedRRScoreType), true);
    }

    // @ 0x82418708
    void CrashNavPanel::StoreSettings(bool lbResetToDefaults)
    {
        if (lbResetToDefaults)
        {
            // MEASURED: with the flag TRUE nothing is captured -- the defaults are written
            // straight in (@0x82418724..0x82418734). CrashNavMap::Construct passes true.
            meSavedPanelType   = K_DEFAULT_PANELTYPE;
            meSavedEventMode   = K_DEFAULT_EVENTMODE;
            meSavedRRScoreType = K_DEFAULT_RR_SCORETYPE;
        }
        else if (mePrepareStage == E_PREPARESTAGE_DONE)
        {
            const PanelType lePanelType = mePanelType;
            meSavedPanelType = lePanelType;

            switch (lePanelType)
            {
            case E_PANEL_EVENT:
                meSavedEventMode = mEventPanel.meCurrentGameMode;   // +0x2EA8 + 0x8AC
                break;

            case E_PANEL_DRIVETHRU:
            case E_PANEL_RIVALS:
                // Neither panel carries a second-level selection worth saving.
                break;

            case E_PANEL_ROADSIGN:
                meSavedRRScoreType =
                    static_cast<BrnStreetData::ScoreType>(GetPanelActiveRoadRuleType());
                break;

            default:
                // Console: StrStream << "Unhandled panel type " << lePanelType << ".\n"
                // (BrnCrashNavPanel.cpp:173).
                CGS_ASSERT(false, "Unhandled panel type");
                break;
            }
        }
    }

    // @ 0x82418810 -- six instructions: a tail call through the toggle group's component
    // vtable slot 5 (SelectableGroup::Update).
    void CrashNavPanel::Update()
    {
        mFilterToggles.Update();
    }

    // @ 0x8243A548
    void CrashNavPanel::ChangeVisiblePanelState(PanelType leNextPanel,
                                                RivalMapPanel::ERivalType leRivalType)
    {
        // ---- transition the currently-visible panel OUT (skipped when it is already the
        //      requested one) ----------------------------------------------------------
        const PanelType leVisible = meVisiblePanel;
        if (leVisible != leNextPanel)
        {
            switch (leVisible)
            {
            case E_PANEL_EVENT:      mEventPanel.TransitionOut();     break;
            case E_PANEL_DRIVETHRU:
                // The console INLINES DriveThruMapPanel::TransitionOut here: the icon's
                // "transOut" state push plus `mbActive = false` (@0x8243A5C4 / +0x3A78).
                mDrivethruPanel.TransitionOut();
                break;
            case E_PANEL_ROADSIGN:   mRoadPanel.TransitionOut();      break;
            case E_PANEL_RIVALS:     mRivalPanel.TransitionOut();     break;
            case E_PANEL_GENERIC:    mGenericPanel.SetState(KAC_TRANSITION_OUT); break;
            case E_PANEL_COUNT:      break;   // nothing was showing yet
            default:
                // Console: "Unhandled next panel type " << leNextPanel << ".\n" (cpp:430).
                CGS_ASSERT(false, "Unhandled next panel type");
                break;
            }
        }

        // ---- transition the requested panel IN --------------------------------------
        // The early-out fires only when the panel is ALREADY visible and the caller asked
        // for no rival transition.
        if (meVisiblePanel == leNextPanel && leRivalType == RivalMapPanel::E_RIVAL_TYPE_COUNT)
        {
            meVisiblePanel = leNextPanel;
            return;
        }

        switch (leNextPanel)
        {
        case E_PANEL_EVENT:
            mEventPanel.TransitionIn();
            meVisiblePanel = leNextPanel;
            break;

        case E_PANEL_DRIVETHRU:
            // Inlined DriveThruMapPanel::TransitionIn ("transIn" + mbActive = true).
            mDrivethruPanel.TransitionIn();
            meVisiblePanel = leNextPanel;
            break;

        case E_PANEL_ROADSIGN:
            mRoadPanel.TransitionIn();
            meVisiblePanel = leNextPanel;
            break;

        case E_PANEL_RIVALS:
            if (leRivalType != RivalMapPanel::E_RIVAL_TYPE_COUNT)
            {
                mRivalPanel.TransitionIn(leRivalType);
                meVisiblePanel = leNextPanel;
                break;
            }
            // "No rival type given" -- re-show the OFFLINE RIVAL panel, unless it is
            // already up as that type. The console inlines this arm as three direct stores
            // into the rival panel (see the friend note in BrnRivalMapPanel.h):
            //   lbz  rival+0x7A1  (mbActive)   /  lwz rival+0x5D8 (meCurrentRivalType)
            //   sub_824E2B90(rival, "transInRival") ; stw 1 ; stb 1
            if (mRivalPanel.mbActive &&
                mRivalPanel.meCurrentRivalType == RivalMapPanel::E_RIVAL_TYPE_OFFLINE_RIVAL)
            {
                meVisiblePanel = leNextPanel;
                break;
            }
            mRivalPanel.SetState(KAC_TRANSITION_IN_RIVAL);
            mRivalPanel.meCurrentRivalType = RivalMapPanel::E_RIVAL_TYPE_OFFLINE_RIVAL;
            mRivalPanel.mbActive           = true;
            meVisiblePanel                 = leNextPanel;
            break;

        case E_PANEL_GENERIC:
            mGenericPanel.SetState(KAC_TRANSITION_IN);
            meVisiblePanel = leNextPanel;
            break;

        default:
            // Console: "Unhandled next panel type " << leNextPanel << ".\n" (cpp:481).
            // NOTE the console still commits the store on this arm (`goto LABEL_23`).
            CGS_ASSERT(false, "Unhandled next panel type");
            meVisiblePanel = leNextPanel;
            break;
        }
    }

    // @ 0x8243A820
    void CrashNavPanel::ShowBlank()
    {
        ChangeVisiblePanelState(E_PANEL_GENERIC, RivalMapPanel::E_RIVAL_TYPE_COUNT);

        mGenericPanelText1.ClearText();       // stb 0 -> +0x5110 (TextField::macText[0])
        mGenericPanelText1.OutputAptData();
        mGenericPanelText2.ClearText();       // stb 0 -> +0x5238
        mGenericPanelText2.OutputAptData();
    }

    // @ 0x8243A878
    void CrashNavPanel::SetEventPanelData(u32 luEventId, const ChallengedEventScore* lpScores,
                                          bool lbShowPanel)
    {
        CGS_ASSERT(mePanelType == E_PANEL_EVENT, "E_PANEL_EVENT == mePanelType");   // cpp:637

        ChangeVisiblePanelState(E_PANEL_EVENT, RivalMapPanel::E_RIVAL_TYPE_COUNT);
        mEventPanel.SetEventData(luEventId, lpScores, mpGuiCache, lbShowPanel);
    }

    // @ 0x8243A8F0
    void CrashNavPanel::SetDrivethruPanelData(CgsID lDriveThruId)
    {
        // X360 `cmplwi mePanelType, 2 ; bge` -- i.e. EVENT (0) or DRIVETHRU (1) pass.
        CGS_ASSERT(mePanelType < E_PANEL_ROADSIGN,
                   "(E_PANEL_DRIVETHRU == mePanelType) || (E_PANEL_EVENT == mePanelType)"); // cpp:657

        ChangeVisiblePanelState(E_PANEL_DRIVETHRU, RivalMapPanel::E_RIVAL_TYPE_COUNT);
        mDrivethruPanel.SetDriveThruData(lDriveThruId);
    }

    // @ 0x8243A978
    void CrashNavPanel::SetRoadPanelData(const char* lpacRoadName, RoadPanelData& lrData)
    {
        CGS_ASSERT(mePanelType == E_PANEL_ROADSIGN, "E_PANEL_ROADSIGN == mePanelType"); // cpp:678
        CGS_ASSERT(lpacRoadName != 0, "NULL != lpRoadName");                            // cpp:679

        // The console INLINES RoadPanel::SetRoadPanelData (DWARF BrnRoadPanel.cpp:203) here:
        //   RoadSignIcon::FindRoadFromName(mRoadPanel.mRoadSign, lpacRoadName)
        //   IconComponent::SetState(mRoadPanel.mRoadSign, <that road>, 0)
        //   memcpy(mRoadPanel.mRoadPanelData, lrData, 0x144)
        //   RoadPanel::UpdateVisibleScores(mRoadPanel)
        // -- all four on the embedded panel, so the reconstruction calls the method the
        // DWARF says owns them rather than reaching into RoadPanel's privates.
        mRoadPanel.SetRoadPanelData(lpacRoadName, lrData);

        ChangeVisiblePanelState(E_PANEL_ROADSIGN, RivalMapPanel::E_RIVAL_TYPE_COUNT);
    }

    // @ 0x8243AA38
    bool CrashNavPanel::ToggleRoadPanelScores()
    {
        // MEASURED (@0x8243AA4C..): the gate is on meVisiblePanel (+0x94), with mePanelType
        // (+0x90) only qualifying the two "panel is up but showing something else" cases.
        const PanelType leVisible = meVisiblePanel;

        const bool lbRivalsOverRoad =
            (leVisible == E_PANEL_RIVALS)  && (mePanelType == E_PANEL_ROADSIGN);
        const bool lbGenericOverRoad =
            (leVisible == E_PANEL_GENERIC) && (mePanelType == E_PANEL_ROADSIGN);

        if (leVisible == E_PANEL_ROADSIGN || lbRivalsOverRoad || lbGenericOverRoad)
        {
            mRoadPanel.SwitchScoreMode();
            return true;
        }
        return false;
    }

    // @ 0x8243AAC8 -- the no-argument overload: show the LOCAL player's own rival row.
    void CrashNavPanel::SetRivalPanelData()
    {
        if (!mpGuiCache->IsOnlineStartInProgress())
        {
            ChangeVisiblePanelState(E_PANEL_RIVALS, RivalMapPanel::E_RIVAL_TYPE_OFFLINE_PLAYER);
            mRivalPanel.SetPlayerData(mpGuiCache);
            return;
        }

        ChangeVisiblePanelState(E_PANEL_RIVALS, RivalMapPanel::E_RIVAL_TYPE_ONLINE_RIVAL);

        // The console localises the cached player name and wraps it in a stack PlayerName
        // before handing it to the panel (@0x8243AB14..0x8243AB4C).
        const char* const lpacPlayerNameKey = mpGuiCache->GetPlayerName();
        // X360: `lwz r3, 0x88(this)` (mpStateInterface) -> StateInterface::GetLanguageManager
        // -> LanguageManager::FindString (@0x8243AB20..0x8243AB2C), NOT the GuiComponent
        // convenience wrapper (which has no body anywhere in the tree).
        const char* const lpacLocalisedName = reinterpret_cast<const char*>(
            mpStateInterface->GetLanguageManager()->FindString(lpacPlayerNameKey));

        CgsNetwork::PlayerName lPlayerName;
        lPlayerName.Construct(lpacLocalisedName);

        mRivalPanel.SetRivalData(&lPlayerName, mpGuiCache->GetLocalPlayerCarId(), mpGuiCache);
    }

    // @ 0x8243AB68 (unnamed sub_8243AB68 in the export; identified by its assert literal at
    // BrnCrashNavPanel.cpp:760, the DWARF's own line for this overload).
    void CrashNavPanel::SetRivalPanelData(CgsID lRivalId)
    {
        CGS_ASSERT(mePanelType == E_PANEL_RIVALS, "E_PANEL_RIVALS == mePanelType");   // cpp:760

        // A null id is a no-op (the console skips the whole tail on `cmpldi r4, 0`).
        if (lRivalId != 0)
        {
            ChangeVisiblePanelState(E_PANEL_RIVALS, RivalMapPanel::E_RIVAL_TYPE_OFFLINE_RIVAL);
            mRivalPanel.SetRivalData(lRivalId);
        }
    }

    // @ 0x8243ABF0 (unnamed sub_8243ABF0; assert at BrnCrashNavPanel.cpp:787).
    void CrashNavPanel::SetRivalPanelData(const CgsNetwork::PlayerName* lpName, CgsID lRivalId)
    {
        CGS_ASSERT(mePanelType == E_PANEL_RIVALS, "E_PANEL_RIVALS == mePanelType");   // cpp:787

        ChangeVisiblePanelState(E_PANEL_RIVALS, RivalMapPanel::E_RIVAL_TYPE_ONLINE_RIVAL);
        mRivalPanel.SetRivalData(lpName, lRivalId, mpGuiCache);
    }

    // @ 0x82441F58 -- the panel's GUI-event sink.
    bool CrashNavPanel::RecEvent(const CgsModule::Event* lpEvent, s32 liEventId, s32 liEventSize)
    {
        // liEventSize is X360-real (the call site loads it, `lwz r6, var_DC(r1)`
        // @0x824DDA9C) but this build's body never reads it.
        (void)liEventSize;

        if (liEventId > 64)
        {
            if (liEventId == 436)   // GuiEventStatsResponse
            {
                mRivalPanel.StorePlayerInfo(lpEvent);
                return false;
            }
            if (liEventId != 438)   // GuiEventRankProgressResponse
            {
                return false;
            }

            const GuiEventRankProgressResponse* const lpRanks =
                reinterpret_cast<const GuiEventRankProgressResponse*>(lpEvent);

            s32 liPlayerRank;
            if (lpRanks->GetCurrentRankRaw() == KI_PLAYER_HAS_FINISHED_LAST_RANK)
            {
                // The player has dropped out of the last rank: fall back to the progression
                // table's own length (the console reads `*(progressionData + 20) - 1`).
                GuiCache* const lpGuiCache = mpStateInterface->GetAccessPointers()->GetGuiCache();
                CGS_ASSERT(lpGuiCache != 0, "lpGuiCache");                                    // cpp:563
                CGS_ASSERT(lpGuiCache->GetWorldDataController() != 0,
                           "lpGuiCache->GetWorldDataController()");                           // cpp:564
                CGS_ASSERT(lpGuiCache->GetWorldDataController()->GetProgressionData() != 0,
                           "lpGuiCache->GetWorldDataController()->GetProgressionData()");     // cpp:565

                liPlayerRank = static_cast<s32>(
                    lpGuiCache->GetWorldDataController()->GetProgressionData()
                        ->GetProgressionRankCount()) - 1;
            }
            else
            {
                liPlayerRank = lpRanks->GetPlayerRank();
            }

            mEventPanel.SetPlayerRank(liPlayerRank);
            mEventPanel.SetModeRanks(lpRanks->GetRaceRank(), lpRanks->GetRoadRageRank(),
                                     lpRanks->GetStuntAttackRank(), lpRanks->GetMarkedManRank());
            // The console inlines SetModeRankWins as four stores to +0x8C8..+0x8D4.
            mEventPanel.SetModeRankWins(lpRanks->GetRaceRankWins(), lpRanks->GetRoadRageRankWins(),
                                        lpRanks->GetStuntAttackRankWins(),
                                        lpRanks->GetMarkedManRankWins());
            return false;
        }

        if (liEventId != 64)   // 64 == the GuiCache bind
        {
            if (liEventId == 6)    // GuiEventControllerInputPressed
            {
                return HandleControllerInput(
                    reinterpret_cast<const CgsGui::GuiEventControllerInputPressed*>(lpEvent));
            }
            if (liEventId == 21)   // GuiEventAptTrigger
            {
                HandleAptEvents(
                    reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent));
                return false;
            }
            return false;
        }

        // ---- event 64: the GuiCache pointer arrives in the record's leading field ------
        if (mpGuiCache != 0)
        {
            return false;   // already bound; the console drops the repeat
        }
        mpGuiCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:532

        // ⛔ FLAG NOT REPRODUCED: the console follows the latch with a 16-byte outgoing
        // record posted straight onto the state interface's queue --
        //   `v17 = { 1, 437, 12 }; VariableEventQueue<65536,16>::AddEvent(si + 12, v17, 40, 16)`
        // (@0x82442008..0x82442024) -- i.e. GUI-out X360 event id 437 with a single payload
        // word of 1. It is the request whose answer arrives as the id-438 rank-progress
        // response handled above. NO committed event type in the tree carries X360 id 437,
        // and the DWARF id the `CgsGui::GuiEvent<N>` template would need is NOT derivable
        // from this call site (the X360 and DWARF ids differ throughout -- see the
        // "GuiEvent<450>; X360 id 455" precedent in BrnGuiEventTypeDefs.h). Inventing N
        // would put a wrong id on the wire, so the post is left unreconstructed and
        // recorded here. CONSEQUENCE on this build: the panel never asks for rank progress,
        // so the id-438 arm above stays dormant and the event panel shows no ranks.
        return false;
    }

    // @ 0x82418828
    void CrashNavPanel::HandleAptEvents(const CgsGui::GuiEventAptTriggerPayload* lpAptEvent)
    {
        CGS_ASSERT(lpAptEvent != 0, "lpAptEvent");   // cpp:807

        if (lpAptEvent->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD)
        {
            // `strstr(trigger->mpacComponentName, this + 0xBC)`; this+0xBC is
            // mFilterToggles.mGuiComponentBase.macName (group +0x18 + 0x04).
            if (std::strstr(lpAptEvent->mpacComponentName, mFilterToggles.GetName()) != 0)
            {
                // `*(this + 0xAC) |= 0x10` -- the group head IS a Selectable, and +0xAC is
                // mFilterToggles.muFlags.
                mFilterToggles.muFlags =
                    static_cast<u8>(mFilterToggles.muFlags | SelectableGroup::KU_FLAG_QUERIED);
            }
        }
    }

    // @ 0x824408E0
    bool CrashNavPanel::HandleControllerInput(
        const CgsGui::GuiEventControllerInputPressed* lpControllerEvent)
    {
        CGS_ASSERT(lpControllerEvent != 0, "lpControllerEvent");   // cpp:849

        if (mePrepareStage != E_PREPARESTAGE_DONE)
        {
            return false;
        }

        const s32 liAction = lpControllerEvent->miButtonId;

        switch (liAction)
        {
        case KI_ACTION_GUI_DPAD_UP:      // 37 -- group vtable slot 11
            if (!mFilterToggles.HighlightPrevious(false))
            {
                return false;
            }
            TriggerSound(liAction);
            // ⚠️ MEASURED: the console returns FALSE here even though it acted.
            return false;

        case KI_ACTION_GUI_DPAD_DOWN:    // 38 -- group vtable slot 10
            if (!mFilterToggles.HighlightNext(false))
            {
                return false;
            }
            TriggerSound(liAction);
            return false;                // as above -- console `result = 0`

        case KI_ACTION_GUI_DPAD_LEFT:    // 39 -- group vtable slot 14
            if (!mFilterToggles.HighlightPreviousItem())
            {
                return false;
            }
            break;

        case KI_ACTION_GUI_DPAD_RIGHT:   // 40 -- group vtable slot 13
            if (!mFilterToggles.HighlightNextItem())
            {
                return false;
            }
            break;

        default:
            return false;
        }

        // Shared tail of the two horizontal arms: which toggle row moved decides whether the
        // top-level filter has to be rebuilt or only the sub-panel refreshed.
        // (Assert line 894 on the LEFT arm, 920 on the RIGHT arm.)
        const s32 liCurrentlySelectedFilter = mFilterToggles.miHighlightedIndex;
        if (liCurrentlySelectedFilter == KI_FILTER_LEVEL_FIRST)
        {
            RefreshSecondLevelFilter(meSavedEventMode,
                                     static_cast<s32>(meSavedRRScoreType), false);
        }
        else
        {
            CGS_ASSERT(liCurrentlySelectedFilter == KI_FILTER_LEVEL_SECOND,
                       "E_FILTER_LEVEL_SECOND == liCurrentlySelectedFilter");
            UpdateDataPanel();
        }

        TriggerSound(liAction);
        return true;
    }

    // @ 0x8243AC60
    void CrashNavPanel::RefreshSecondLevelFilter(EventPanel::EEventType leEventMode,
                                                 s32 leRoadRuleScoreType, bool lbForce)
    {
        // The body runs only out of an event (or while an online start is in progress).
        const s32 leGameModeType = mpGuiCache->GetCurrentGameModeType();
        const bool lbRun = mpGuiCache->IsOnlineStartInProgress()
                        || (leGameModeType == KI_GAME_MODE_TYPE_NONE)
                        || (leGameModeType == KI_GAME_MODE_TYPE_FREEBURN);
        if (!lbRun)
        {
            return;
        }

        // The second-level row is highlightable again whatever happens next.
        mFilterToggles.GetSelectable(KI_TOGGLE_SECOND_LEVEL)->SetHighlightable(true);

        const s32 liSelectedFilter =
            mFilterToggles.GetSelectable(KI_TOGGLE_TOP_LEVEL)->mItemText.miHighlightedIndex;
        const PanelType leSelectedPanel = static_cast<PanelType>(liSelectedFilter);

        if (leSelectedPanel == mePanelType && !lbForce)
        {
            return;
        }

        ChangeVisiblePanelState(leSelectedPanel, RivalMapPanel::E_RIVAL_TYPE_COUNT);

        switch (leSelectedPanel)
        {
        case E_PANEL_EVENT:
            mFilterToggles.SetupToggle(KI_TOGGLE_SECOND_LEVEL, 6, true, KAPC_OPTION_HEADINGS[1],
                                       EventPanel::KAPC_EVENT_FILTER_OPTIONS, 0);
            mFilterToggles.HighlightItem(KI_TOGGLE_SECOND_LEVEL, leEventMode);
            break;

        case E_PANEL_DRIVETHRU:
            mFilterToggles.SetupToggle(KI_TOGGLE_SECOND_LEVEL, 1, true, KAPC_OPTION_HEADINGS[1],
                                       DriveThruMapPanel::KAPC_DRIVETHRU_FILTER_OPTIONS, 0);
            // The single drive-thru option is not a real choice, so the row is locked.
            mFilterToggles.GetSelectable(KI_TOGGLE_SECOND_LEVEL)->SetHighlightable(false);
            break;

        case E_PANEL_ROADSIGN:
            mFilterToggles.SetupToggle(KI_TOGGLE_SECOND_LEVEL, 2, true, KAPC_OPTION_HEADINGS[1],
                                       RoadPanel::KAPC_RR_FILTER_OPTIONS, 0);
            mFilterToggles.HighlightItem(KI_TOGGLE_SECOND_LEVEL, leRoadRuleScoreType);
            break;

        case E_PANEL_RIVALS:
            mFilterToggles.SetupToggle(KI_TOGGLE_SECOND_LEVEL, 0, true, 0, 0, 0);
            break;

        default:
            // Console: "No filters set up for panel view of type" << type << "\n" (cpp:1030).
            // NOTE the console still runs the common tail on this arm.
            CGS_ASSERT(false, "No filters set up for panel view of type");
            break;
        }

        mePanelType = leSelectedPanel;
        UpdateDataPanel();
    }

    // @ 0x8242D410
    void CrashNavPanel::UpdateDataPanel()
    {
        const s32 liSelectedOption =
            mFilterToggles.GetSelectable(KI_TOGGLE_SECOND_LEVEL)->mItemText.miHighlightedIndex;

        switch (mePanelType)
        {
        case E_PANEL_EVENT:
            mEventPanel.SetCurrentGameMode(static_cast<EventPanel::EEventType>(liSelectedOption));
            break;

        case E_PANEL_DRIVETHRU:
        case E_PANEL_RIVALS:
            // No second-level data on either panel.
            break;

        case E_PANEL_ROADSIGN:
            mRoadPanel.SetCurrentRule(liSelectedOption);
            break;

        default:
            // Console: "No filters set up for panel of type " << mePanelType << "\n" (cpp:1086).
            CGS_ASSERT(false, "No filters set up for panel of type ");
            break;
        }
    }

    // @ 0x8243AF30
    void CrashNavPanel::TriggerSound(s32 leAction)
    {
        const char* lpacLabel = 0;

        switch (leAction)
        {
        case KI_ACTION_GUI_DPAD_UP:      // 37
        case KI_ACTION_GUI_DPAD_DOWN:    // 38
        case KI_ACTION_GUI_UP:           // 41
        case KI_ACTION_GUI_DOWN:         // 42
            lpacLabel = KAC_SOUND_MENU_TOGGLE_DEFAULT;
            break;

        case KI_ACTION_GUI_DPAD_LEFT:    // 39
        case KI_ACTION_GUI_DPAD_RIGHT:   // 40
        case KI_ACTION_GUI_LEFT:         // 43
        case KI_ACTION_GUI_RIGHT:        // 44
            lpacLabel = KAC_SOUND_MENU_ITEM_TOGGLE_DEFAULT;
            break;

        default:
            CGS_ASSERT(false, "lpcLabel");   // cpp:1136
            break;
        }

        // The console builds the 100-byte GuiAudioTriggerEvent on its stack and queues it as
        // X360 id 457 onto `mpStateInterface + 12` (@0x8243AFC8..0x8243AFF4). The committed
        // OutputGuiEvent<GuiAudioTriggerEvent> instantiation (@0x82436890) IS that path.
        // FLAG: the committed record is `CgsGui::GuiEvent<201>` -- the DWARF id -- while the
        // X360 wire id is 457; that DWARF-vs-X360 id delta is the tree-wide convention (see
        // the "GuiEvent<450>; X360 id 455" note in BrnGuiEventTypeDefs.h), not a defect here.
        GuiAudioTriggerEvent lAudioEvent;
        lAudioEvent.Construct(KI_AUDIO_TRIGGER_ACTION, KAC_EMPTY_STRING, lpacLabel);
        mpStateInterface->OutputGuiEvent(lAudioEvent);
    }

    // @ 0x824188B0
    bool CrashNavPanel::IsRoadRuleFriendSelected() const
    {
        if (GetRoadPanelScoreMode() != RoadPanel::KI_ROAD_PANEL_MODE_ONLINE)
        {
            return false;
        }

        // Inlined strcmp against "-" (@0x824188EC..0x82418918): the placeholder row.
        return std::strcmp(mRoadPanel.GetSelectedFriendName(), "-") != 0;
    }

    // @ 0x82418938 -- the console INLINES RoadPanel::GetSelectedFriendName here, assert
    // included (the assert's file/line are BrnRoadPanel.h:231, which is what proves the
    // inline). Expressed as the call the DWARF says it is.
    const char* CrashNavPanel::GetRoadRuleFriendSelectedName() const
    {
        return mRoadPanel.GetSelectedFriendName();
    }

    // @ 0x824BAE58
    BrnProgression::RaceEventData::EModeType CrashNavPanel::GetPanelActiveGameModeType()
    {
        CGS_ASSERT(mePanelType == E_PANEL_EVENT,
                   "Cannot get active game mode type if not showing events");   // @0x824BAE58 (beq on +0x90==0)

        return mEventPanel.ConvertLocalEventDefToProgressionEventDef(mEventPanel.meCurrentGameMode);
    }

    // @ 0x824185C8
    s32 CrashNavPanel::GetPanelActiveRoadRuleType() const
    {
        CGS_ASSERT(mePanelType == E_PANEL_ROADSIGN,
                   "Cannot get active road rules type if not showing road rules");   // @0x824185C8 (beq on +0x90==2)

        // Inlined RoadPanel::GetCurrentRule() -- a bare `lwz` at mRoadPanel +0xD9C.
        return mRoadPanel.GetCurrentRule();
    }

    // @ 0x82418668
    s32 CrashNavPanel::GetRoadPanelScoreMode() const
    {
        CGS_ASSERT(mePanelType == E_PANEL_ROADSIGN,
                   "Cannot get active road rules scoring mode if not showing road rules");   // @0x82418668 (beq on +0x90==2)

        // Inlined RoadPanel::GetScoringMode() -- a bare `lwz` at mRoadPanel +0xDA0.
        return mRoadPanel.GetScoringMode();
    }
}
