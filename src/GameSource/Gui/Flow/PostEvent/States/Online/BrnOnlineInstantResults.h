#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/Events/BrnGuiEventOnlinePostEvent.h"
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnTable.h"
#include "GameSource/Gui/BrnGuiTextField.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"

// ARTIST POST_ON_IR owner. Member names follow DecFIGS where the target agrees;
// the third animator, team fields and points heading are ARTIST additions, named
// from OnEnter's actual component bindings. Console offsets are evidence only.
namespace CgsDev { struct Debug2DImmediateRender; }

namespace BrnGui
{
    class GuiCache;

    struct OnlineInstantResultsState : public CgsGui::State
    {
        // BrnOnlineInstantResults.h (the screen's internal sub-states).
        enum EInternalState
        {
            E_ONLINE_INSTANT_RESULTS_LOADING_SCREEN          = 0,
            E_ONLINE_INSTANT_RESULTS_LOADING_COMPONENTS      = 1,
            E_ONLINE_INSTANT_RESULTS_UPDATING                = 2,
            E_ONLINE_INSTANT_RESULTS_SHOWING                 = 3,
            E_ONLINE_INSTANT_RESULTS_STANDINGSUPDATE         = 4,
            E_ONLINE_INSTANT_RESULTS_CURRENT_STANDINGSUPDATE = 5,
            E_ONLINE_INSTANT_RESULTS_FINAL_STANDINGSUPDATE   = 6,
            E_ONLINE_INSTANT_RESULTS_FINAL_STANDINGS         = 7,
            E_ONLINE_INSTANT_RESULTS_LOADING_STANDINGS       = 8,
            E_ONLINE_INSTANT_RESULTS_DONE                    = 9,
        };

        // BrnOnlineInstantResults.h -- the ticker text of one online award: up to six
        // interchangeable string ids for a plural award value and six for a value of exactly
        // one, and how many message parameters (the player's name, then the value) each takes.
        struct AwardData
        {
            static const s32 KI_MAX_AWARD_STRINGS = 6;

            s8          miMultiParamCount;                          // +0x00
            s8          miSingleParamCount;                         // +0x01
            const char* mapcMultiString[KI_MAX_AWARD_STRINGS];      // +0x04
            const char* mapcSingleString[KI_MAX_AWARD_STRINGS];     // +0x1C

            //  /, header inline: the number of string ids before the first null.
            s32 CountStringsSingle() const { return CountStrings(mapcSingleString); }
            s32 CountStringsPlural() const { return CountStrings(mapcMultiString); }

        private:
            static s32 CountStrings(const char* const* lapcStrings)
            {
                s32 liNumStrings = 0;
                while (liNumStrings < KI_MAX_AWARD_STRINGS && lapcStrings[liNumStrings] != 0)
                {
                    ++liNumStrings;
                }
                return liNumStrings;
            }
        };

        // BrnOnlineInstantResults.cpp -- one entry per BrnGameState::EOnlineAwardID
        // (read from the image).
        static const s32       KI_NUM_ONLINE_AWARD_TYPES = 11;
        static const AwardData KA_AWARD_OVERALL[KI_NUM_ONLINE_AWARD_TYPES];

        OnlineInstantResultsState();
        virtual void OnEnter() ;
        virtual void OnLeave() ;
        virtual void Update() ;

        // Hands the instant-results screen's resource list out.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourceTuplesToLoad;
            *lpuNumberOfResources = static_cast<u32>(miNumResourcesToLoad);
        }

        // Direct non-virtual callee from GuiDebugComponent::RenderHUD 0x824FD2B0.
        void RenderDebug(CgsDev::Debug2DImmediateRender* lpDisplay);

    private:
        void CheckForCompletedLoads();
        void PopulateNameAndTimeTableCells(const GuiEventOnlinePostEvent* lpEvent);
        void UpdateDirtyFlags(const GuiEventOnlinePostEvent* lpEvent);
        void ReorderResultsTable();
        s32 GetOnlinePlayerIndexFromTablePosition(s32 liTablePosition);
        GuiEventOnlinePostEvent::Record* GetOnlinePlayerFromTablePosition(s32 liTablePosition);
        GuiEventOnlinePostEvent::Record* GetOnlinePlayerFromSortedTablePosition(s32 liTablePosition);
        // ARTIST r6/r7/r8 are three independent bools; older DecFIGS has only r6.
        void SetAptData(s32 liRow, const GuiEventOnlinePostEvent::Record* lpPlayer,
                        bool lbResultsForThisRoundOnly, bool lbHideRepeatedTeam,
                        bool lbTeamResults);
        void FillOutTicker();
        void HandleControllerInputPressed(const CgsGui::GuiEventControllerInputPressed* lpEvent);
        void HandleControllerInputFinalStandingsSubState(
            const CgsGui::GuiEventControllerInputPressed* lpEvent);
        void HandleDisconnectedEvent(const CgsGui::GuiEventNetworkDisconnected* lpEvent);
        void SetResultColumnHeading();
        void SetRoundText();                         // ARTIST-only emitted helper
        void UpdateCurrentScreenDisplay();            // ARTIST-only emitted helper
        void UpdateStuntRunColoursAndScores();        // ARTIST-only emitted helper
        void LeaveScreen();

        static const CgsGui::sResourceTuple maResourceTuplesToLoad[];
        static const s32 miNumResourcesToLoad;

        EInternalState meCurrentState;
        Table mTable;
        TextField maTextFields[40];
        IconComponent maIcons[8];
        AnimationComponent mAnimationComponent;
        AnimationComponent mButtonAnimationComponent;
        AnimationComponent mGamerTagColourAnimationComponent;
        CgsGui::GuiComponent* maapTableCellComponentPtrs[16][16];
        TableDataSet mTableData;
        TableRowDataSet maTableRowDataSets[16];
        u32 miItemsLoaded;
        u32 muNumTextFilesToLoad;
        CgsGui::GuiEventNetworkDisconnected mDisconnectedEvent;
        GuiEventOnlinePostEvent mOnlinePostEvent;
        GuiCache* mpGuiCache;
        GuiEventOnlineEventFinishingOrder mOnlineFinishingOrder;
        s32 maiOnlinePlayersIndexFromTablePosition[8];
        f32 mfUpdateTimer;
        f32 mfTableHadDataDuration;
        BrnGameState::GameStateModuleIO::EGameModeType meCurrentGameMode;
        bool mabDirty[8];
        TextField mTitleTextfield;
        TextField mWinnerGamertag;
        TextField mCurrentRound;
        TextField mYourTeamScoreHeading;
        TextField mRivalTeamScoreHeading;
        TextField mYourTeamScore;
        TextField mRivalTeamScore;
        TextField mYourTeamIs;
        TextField mResultColumnHeading;
        TextField mRevengeColumnHeading;
        TextField mPointsColumnHeading;
        bool mbPlayerCanLeave;
        bool mbAbortDueToDisconnects;
        bool mbLeaveNextUpdate;
        bool mbCachedDisconnectEvent;
        bool mbReceivedFinishedModeResults;
    };
}
