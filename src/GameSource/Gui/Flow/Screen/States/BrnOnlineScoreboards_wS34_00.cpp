// ===================================================================================
// BrnGui::OnlineScoreboards -- stub-wave partfile 00: the internal-state pump, the four
// scoreboard responses and the filter-mode input.
//   HandleTableData
//   HandleCategoryList
//   HandleIndexList
//   HandleVariationList
//   HandleControllerInputPressedUsingFilters
//   Update
// Reconstructed from the console image; the assembly arbitrates over the decompiler.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"

#include <cstring>                                                        // memcpy / strlen / strcpy
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface, GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include "GameSource/Input/GameInputActions.h"                            // E_GAMEINPUTACTIONS_GUI_*
#include "GameSource/Network/Managers/BrnNetworkScoreboard.h"             // BrnNetwork::Scoreboard

namespace BrnGui
{
    namespace
    {
        // The state in-queue concrete type (GuiEventQueue is its pointer-only face).
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // The apt movie the screen plays, and its level.
        const s32 KI_APT_MOVIE_LEVEL = 3;

        // The three filter rows of mFilterToggleGroup.
        const s32 KI_FILTER_CATEGORY  = 0;
        const s32 KI_FILTER_INDEX     = 1;
        const s32 KI_FILTER_VARIATION = 2;
        const s32 KI_NUM_FILTERS      = 3;

        // One leaderboard name in the response lists (31 chars including the terminator).
        const s32 KI_LIST_NAME_LENGTH = 31;

        // Controller event payload: the action id rides in the second word.
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miDevice;     // +0x00
            s32 miButtonId;   // +0x04
        };

        // Event payload views (the queue hands out header-stripped payloads).
        struct GuiEventScoreboardResponseCategoryPayload : public CgsModule::Event
        {
            s32  miNumCategories;                                                     // +0x000
            char maacCategories[OnlineScoreboards::KI_MAX_CATEGORIES][KI_LIST_NAME_LENGTH];  // +0x004
        };

        struct GuiEventScoreboardResponseIndexPayload : public CgsModule::Event
        {
            s32  miNumIndexes;                                                        // +0x000
            char maacIndexes[OnlineScoreboards::KI_MAX_INDEXES][KI_LIST_NAME_LENGTH];  // +0x004
        };

        struct GuiEventScoreboardResponseVariationPayload : public CgsModule::Event
        {
            s32  miNumVariations;                                                         // +0x000
            char maacVariations[OnlineScoreboards::KI_MAX_VARIATIONS][KI_LIST_NAME_LENGTH];  // +0x004
            bool mbPerRoadLeaderboard;                                                    // +0x802
            bool mbEventLeaderboard;                                                      // +0x803
        };

        bool IsDebugPrintOn()
        {
            return (CgsDev::Message::gxMessageFilterFlags & 1) != 0;
        }

        // The banner every response handler prints around its list dump.
        void PrintListBanner(const char* lpacRule, const char* lpacTitle)
        {
            if (IsDebugPrintOn()) *CgsDev::Log::gpDebugPrint << "\n\n\n";
            if (IsDebugPrintOn()) *CgsDev::Log::gpDebugPrint << lpacRule;
            if (IsDebugPrintOn()) *CgsDev::Log::gpDebugPrint << lpacTitle;
            if (IsDebugPrintOn()) *CgsDev::Log::gpDebugPrint << lpacRule;
        }

        void PrintNameList(const char* lpacRule, const char* lpacEntryLabel,
                           const char (*lpaacNames)[KI_LIST_NAME_LENGTH], s32 liCount)
        {
            if (IsDebugPrintOn())
                *CgsDev::Log::gpDebugPrint << "Number in list : " << liCount << "\n";
            for (s32 liEntry = 0; liEntry < liCount; ++liEntry)
            {
                if (IsDebugPrintOn())
                {
                    *CgsDev::Log::gpDebugPrint << lpacEntryLabel << liEntry << "] : "
                                               << lpaacNames[liEntry] << "\n";
                }
            }
            if (IsDebugPrintOn()) *CgsDev::Log::gpDebugPrint << lpacRule;
            if (IsDebugPrintOn()) *CgsDev::Log::gpDebugPrint << "\n\n\n";
        }
    }

    // ================================================================================
    //  HandleTableData
    //
    //  The requested leaderboard table arrived: show it, drop the loading icon, flag an
    //  empty table and refresh the button prompts.
    // ================================================================================
    void OnlineScoreboards::HandleTableData(const CgsModule::Event* lpEvent)
    {
        const BrnNetwork::Scoreboard* lpScoreboard =
            reinterpret_cast<const BrnNetwork::Scoreboard*>(lpEvent);

        mTable.SetupScoreboard(lpScoreboard);

        if (mbCurrentlyLoading)
        {
            mbCurrentlyLoading = false;
            mLoadingIcon.SetState("invisible");
        }

        if (lpScoreboard->GetNumberOfRows() == 0)
            ShowTableEmpty(true);

        SetupButtons();

        const char* const lpacRule = "-------------------\n";
        PrintListBanner(lpacRule, "TABLE DATA RECEIVED\n");
        if (IsDebugPrintOn())
            *CgsDev::Log::gpDebugPrint << "Name : " << lpScoreboard->GetTitle() << "\n";
        if (IsDebugPrintOn())
            *CgsDev::Log::gpDebugPrint << "Titles :";
        for (s32 liColumn = 0; liColumn < lpScoreboard->GetNumberOfColumns(); ++liColumn)
        {
            if (IsDebugPrintOn())
                *CgsDev::Log::gpDebugPrint << "\t\t" << lpScoreboard->GetColumn(liColumn)->GetTitle();
        }
        if (IsDebugPrintOn())
            *CgsDev::Log::gpDebugPrint << "\n";

        for (s32 liRow = 0; liRow < lpScoreboard->GetNumberOfRows(); ++liRow)
        {
            if (IsDebugPrintOn())
                *CgsDev::Log::gpDebugPrint << "Row[" << liRow << "] :";
            const BrnNetwork::ScoreboardRow* lpRow = lpScoreboard->GetRow(liRow);
            for (s32 liColumn = 0; liColumn < lpScoreboard->GetNumberOfColumns(); ++liColumn)
            {
                if (IsDebugPrintOn())
                    *CgsDev::Log::gpDebugPrint << "\t\t" << lpRow->GetData(liColumn);
            }
            if (IsDebugPrintOn())
                *CgsDev::Log::gpDebugPrint << "\n";
        }

        if (IsDebugPrintOn()) *CgsDev::Log::gpDebugPrint << lpacRule;
        if (IsDebugPrintOn()) *CgsDev::Log::gpDebugPrint << "\n\n\n";
    }

    // ================================================================================
    //  HandleCategoryList
    // ================================================================================
    void OnlineScoreboards::HandleCategoryList(const CgsModule::Event* lpEvent)
    {
        const GuiEventScoreboardResponseCategoryPayload* lpList =
            static_cast<const GuiEventScoreboardResponseCategoryPayload*>(lpEvent);

        miMaxCategories = lpList->miNumCategories;
        std::memcpy(maacCategories, lpList->maacCategories,
                    static_cast<size_t>(KI_LIST_NAME_LENGTH * lpList->miNumCategories));

        const char* const lpacRule = "----------------------\n";
        PrintListBanner(lpacRule, "CATEGORY LIST RECEIVED\n");
        PrintNameList(lpacRule, "Category [", maacCategories, miMaxCategories);

        SetupCategories();
    }

    // ================================================================================
    //  HandleIndexList
    // ================================================================================
    void OnlineScoreboards::HandleIndexList(const CgsModule::Event* lpEvent)
    {
        const GuiEventScoreboardResponseIndexPayload* lpList =
            static_cast<const GuiEventScoreboardResponseIndexPayload*>(lpEvent);

        miMaxIndexes = lpList->miNumIndexes;
        std::memcpy(maacIndexes, lpList->maacIndexes,
                    static_cast<size_t>(KI_LIST_NAME_LENGTH * lpList->miNumIndexes));
        if (miCurrentIndex >= miMaxIndexes)
            miCurrentIndex = 0;

        const char* const lpacRule = "-------------------\n";
        PrintListBanner(lpacRule, "INDEX LIST RECEIVED\n");
        PrintNameList(lpacRule, "Index [", maacIndexes, miMaxIndexes);

        SetupIndexes();
    }

    // ================================================================================
    //  HandleVariationList
    //
    //  A per-road leaderboard's variations are the roads: they are copied in the
    //  alphabetical order BuildAlphabeticalRoadIndexes worked out.
    // ================================================================================
    void OnlineScoreboards::HandleVariationList(const CgsModule::Event* lpEvent)
    {
        const GuiEventScoreboardResponseVariationPayload* lpList =
            static_cast<const GuiEventScoreboardResponseVariationPayload*>(lpEvent);

        miMaxVariations = lpList->miNumVariations;
        if (lpList->mbPerRoadLeaderboard)
        {
            for (s32 liRoadIndex = 0; liRoadIndex < miMaxVariations; ++liRoadIndex)
            {
                CGS_ASSERT(maiAlphabeticalRoadIndex[liRoadIndex] >= 0,
                           "maiAlphabeticalRoadIndex[ liRoadIndex ] >= 0");
                CGS_ASSERT(maiAlphabeticalRoadIndex[liRoadIndex] < KI_MAX_VARIATIONS,
                           "maiAlphabeticalRoadIndex[ liRoadIndex ] < GuiEventScoreboardResponseVariationEvent::KI_MAX_VARIATIONS");

                const char* lpacRoad = lpList->maacVariations[maiAlphabeticalRoadIndex[liRoadIndex]];
                // The console's checked string copy: assert the name fits, then copy it.
                CGS_ASSERT(std::strlen(lpacRoad) < static_cast<size_t>(KI_LIST_NAME_LENGTH),
                           "String is too long");
                std::strcpy(maacVariations[liRoadIndex], lpacRoad);
            }
        }
        else
        {
            std::memcpy(maacVariations, lpList->maacVariations,
                        static_cast<size_t>(KI_LIST_NAME_LENGTH * lpList->miNumVariations));
        }

        mbPerRoadLeaderboard = lpList->mbPerRoadLeaderboard;
        mbEventLeaderboard   = lpList->mbEventLeaderboard;
        if (miCurrentVariation >= miMaxVariations)
            miCurrentVariation = 0;

        const char* const lpacRule = "-----------------------\n";
        PrintListBanner(lpacRule, "VARIATION LIST RECEIVED\n");
        PrintNameList(lpacRule, "Variation [", maacVariations, miMaxVariations);

        SetupVariations();
    }

    // ================================================================================
    //  HandleControllerInputPressedUsingFilters
    //
    //  Filter mode: up/down move between the three filter rows, left/right change the
    //  highlighted filter (and request the next rung of the leaderboard ladder), select
    //  drops into the table, cancel leaves, and the two option buttons page the table.
    // ================================================================================
    void OnlineScoreboards::HandleControllerInputPressedUsingFilters(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event sent to OnlineScoreboards::HandleControllerInput");

        const s32 leAction = static_cast<const ControllerButtonPayload*>(lpEvent)->miButtonId;

        switch (leAction)
        {
            case E_GAMEINPUTACTIONS_GUI_UP:
                if (mFilterToggleGroup.HighlightPrevious(false))
                    TriggerSound(leAction);
                miCurrentFilterHighlighted = mFilterToggleGroup.GetHighlightedIndex();
                break;

            case E_GAMEINPUTACTIONS_GUI_DOWN:
                if (mFilterToggleGroup.HighlightNext(false))
                    TriggerSound(leAction);
                miCurrentFilterHighlighted = mFilterToggleGroup.GetHighlightedIndex();
                break;

            case E_GAMEINPUTACTIONS_GUI_LEFT:
            case E_GAMEINPUTACTIONS_GUI_RIGHT:
            {
                const bool lbChanged = (leAction == E_GAMEINPUTACTIONS_GUI_LEFT)
                                           ? mFilterToggleGroup.HighlightPreviousItem()
                                           : mFilterToggleGroup.HighlightNextItem();
                if (!lbChanged)
                    break;

                TriggerSound(leAction);
                const s32 liFilter = mFilterToggleGroup.GetHighlightedIndex();
                if (liFilter == KI_FILTER_CATEGORY)
                {
                    meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_GET_INDEX;
                    miCurrentCategory =
                        mFilterToggleGroup.GetSelectable(KI_FILTER_CATEGORY)->mItemText.GetHighlightedIndex();
                }
                else if (liFilter == KI_FILTER_INDEX)
                {
                    meLeaderboardRequestState = E_LEADERBOARD_REQUEST_STATE_GET_VARIATIONS;
                    miCurrentIndex =
                        mFilterToggleGroup.GetSelectable(KI_FILTER_INDEX)->mItemText.GetHighlightedIndex();
                }
                else if (static_cast<u32>(liFilter) < static_cast<u32>(KI_NUM_FILTERS))
                {
                    // A new road: wait a moment before asking for its table.
                    mfCurrentTableWaitTime = KF_DELAY_TO_REQUEST_TABLE;
                    mTable.SetupScoreboard(0);
                    if (!mbCurrentlyLoading)
                    {
                        mbCurrentlyLoading = true;
                        mLoadingIcon.SetState("loading");
                    }
                    ShowTableEmpty(false);
                    miCurrentVariation =
                        mFilterToggleGroup.GetSelectable(liFilter)->mItemText.GetHighlightedIndex();
                }
                else
                {
                    // The console streams the filter index after this text.
                    CGS_ASSERT(false, "Invalid filter selected : ");
                }

                // A changed filter forgets the challenge target.
                mCurrentTargetScorePlayerName.macName[0] = 0;
                mbWaitingForTargetScoreResponse = false;
                break;
            }

            case E_GAMEINPUTACTIONS_GUI_SELECT:
                // Into the table, when it has rows.
                if (mTable.GetRowsUsed() > 0)
                {
                    mbUsingFilters = false;
                    mTable.SetHighlight(0);
                    mTable.DrawScoreboard();
                    mFilterToggleGroup.HighlightIndex(-1);
                    SetupButtons();
                }
                break;

            case E_GAMEINPUTACTIONS_GUI_CANCEL:
                // Leaving while an online start is pending also suspends the network.
                if (mpGuiCache->IsOnlineStartPending())
                {
                    CgsGui::GuiEventNetworkSuspension lSuspend(false);
                    mpStateInterface->OutputGuiEvent(lSuspend);
                    mpGuiCache->SetOnlineStartPending(false);
                    SendStateEvent("GO_BACK_EASY");
                }
                else
                {
                    SendStateEvent("GO_BACK");
                }
                break;

            case E_GAMEINPUTACTIONS_GUI_OPTION0:
                PageDown(false);
                break;

            case E_GAMEINPUTACTIONS_GUI_OPTION1:
                PageUp(false);
                break;

            default:
                break;
        }
    }

    // ================================================================================
    //  Update
    //
    //  The internal-state machine. Each state falls into the next once its work is done,
    //  so a frame can run several of them; the permanent update and the queue clear run
    //  every frame.
    // ================================================================================
    void OnlineScoreboards::Update()
    {
        switch (meInternalState)
        {
            case E_INTERNALSTATE_GETCACHE:
                UpdateGetCache();
                // fall through
            case E_INTERNALSTATE_LOADRESOURCES:
                meInternalState = E_INTERNALSTATE_LOADRESOURCES;
                if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
                    break;
                // fall through
            case E_INTERNALSTATE_PLAYSWF:
                meInternalState = E_INTERNALSTATE_PLAYSWF;
                ClearExpectedComponent();
                mFilterToggleGroup.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache, false);
                mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[maResourcesToLoad[0].muId],
                                               KI_APT_MOVIE_LEVEL);
                // fall through
            case E_INTERNALSTATE_WFINIT:
                meInternalState = E_INTERNALSTATE_WFINIT;
                if (!UpdateWFInit())
                    break;
                // fall through
            case E_INTERNALSTATE_WFDATA:
                meInternalState = E_INTERNALSTATE_WFDATA;
                if (meLeaderboardRequestState != E_LEADERBOARD_REQUEST_STATE_DONE)
                    break;
                // fall through
            case E_INTERNALSTATE_SETUPCOMPONENTS:
                UpdateSetupComponents();
                // fall through
            case E_INTERNALSTATE_RUNNING:
                meInternalState = E_INTERNALSTATE_RUNNING;
                UpdateRunning();
                break;

            case E_INTERNALSTATE_LEFT:
                break;

            default:
                // The console streams the state value after this text.
                CGS_ASSERT(false, "Invalid internal state : ");
                break;
        }

        UpdatePermanent();
        reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue)->Clear();
    }
}
