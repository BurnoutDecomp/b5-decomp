// ===================================================================================
// BrnGui::OnlineStats -- out-of-line bodies for the online-stats screen state.
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   HandleControllerInputPressed @0x82487670, HandleStatsData @0x82487728,
//   OnEnter @0x82487470, OnLeave @0x824A18E8, UpdatePermanent @0x82492908,
//   UpdateWFInit @0x82487608.
// (ClearExpectedComponent @0x82487590 / UpdateGetCache / UpdateRunning are SKIPPED --
//  declared-only.)
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineStats.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsModule::Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue<18432,16>
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache

namespace BrnGui
{
    // ---- static out-of-line definitions -------------------------------------------
    const s32 OnlineStats::maiEventToObserve[] = { 0, 0, 0, 0, 0, 0, 0 };   // @0x8205F9FC (7 entries; values not recovered)
    const s32 OnlineStats::miNumEventsObserved = 7;

    const CgsGui::sResourceTuple OnlineStats::maResourcesToLoad[] =
    {
        { 0u, CgsGui::E_GUI_RESOURCETYPE_APT },   // FLAG: id unattested in scope
    };
    const u32 OnlineStats::muNumResourcesToLoad = 1;

    const char* const OnlineStats::KAC_TEXTFIELD_NAME_TOTAL_GAMES     = "TotalGames_mc";
    const char* const OnlineStats::KAC_TEXTFIELD_NAME_WIN_RATE        = "WinRate_mc";
    const char* const OnlineStats::KAC_TEXTFIELD_NAME_TAKEDOWNS       = "Takedowns_mc";
    const char* const OnlineStats::KAC_TEXTFIELD_NAME_RIVALS          = "Rivals_mc";
    const char* const OnlineStats::KAC_TEXTFIELD_NAME_MUGSHOTS        = "Mugshots_mc";
    const char* const OnlineStats::KAC_TEXTFIELD_NAME_DISCONNECT_RATE = "DisconnectRate_mc";

    // The state in-queue is an 18KB variable event queue (X360 VariableEventQueue<18432,16>),
    // reached through the base State's mpInGuiEventQueue (the committed BrnCredits idiom).
    typedef CgsModule::VariableEventQueue<18432, 16> OnlineStatsInQueue;

    // ---- OnEnter @ 0x82487470 -----------------------------------------------------
    void OnlineStats::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        meInternalState = E_INTERNALSTATE_GETCACHE;   // X360 +0x3C = 0
        mpGuiCache      = NULL;                        // X360 +0x38 = 0

        mTotalGames.Construct(KAC_TEXTFIELD_NAME_TOTAL_GAMES, mpStateInterface, NULL);
        mWinRate.Construct(KAC_TEXTFIELD_NAME_WIN_RATE, mpStateInterface, NULL);
        mTakedowns.Construct(KAC_TEXTFIELD_NAME_TAKEDOWNS, mpStateInterface, NULL);
        mRivals.Construct(KAC_TEXTFIELD_NAME_RIVALS, mpStateInterface, NULL);
        mMugshots.Construct(KAC_TEXTFIELD_NAME_MUGSHOTS, mpStateInterface, NULL);
        mDisconnectRate.Construct(KAC_TEXTFIELD_NAME_DISCONNECT_RATE, mpStateInterface, NULL);
    }

    // ---- OnLeave @ 0x824A18E8 -----------------------------------------------------
    void OnlineStats::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
        mpStateInterface->PlayAptMovie("", 3);
        ClearExpectedComponent();
    }

    // ---- HandleControllerInputPressed @ 0x82487670 --------------------------------
    void OnlineStats::HandleControllerInputPressed(const CgsGui::GuiEventControllerInputPressed* lpEvent)
    {
        CGS_ASSERT(lpEvent != NULL,
                   "Invalid event sent to OnlineStats::HandleControllerInput");   // cpp:442

        const s32 liAction =
            *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 4);
        if (liAction == KI_ACTION_START)
            SendStateEvent("GO_BACK");
    }

    // ---- HandleStatsData @ 0x82487728 ---------------------------------------------
    void OnlineStats::HandleStatsData(const GuiEventOnlineStatsResponse* lpEvent)
    {
        const s32* lpaiStats = reinterpret_cast<const s32*>(lpEvent);

        char lacBuffer[32];

        CgsCore::SPrintf(lacBuffer, 32, "%d", lpaiStats[0]); lacBuffer[31] = 0; mTotalGames.SetText(lacBuffer);
        CgsCore::SPrintf(lacBuffer, 32, "%d", lpaiStats[1]); lacBuffer[31] = 0; mWinRate.SetText(lacBuffer);
        CgsCore::SPrintf(lacBuffer, 32, "%d", lpaiStats[2]); lacBuffer[31] = 0; mTakedowns.SetText(lacBuffer);
        CgsCore::SPrintf(lacBuffer, 32, "%d", lpaiStats[3]); lacBuffer[31] = 0; mRivals.SetText(lacBuffer);
        CgsCore::SPrintf(lacBuffer, 32, "%d", lpaiStats[4]); lacBuffer[31] = 0; mMugshots.SetText(lacBuffer);
        CgsCore::SPrintf(lacBuffer, 32, "%d", lpaiStats[5]); lacBuffer[31] = 0; mDisconnectRate.SetText(lacBuffer);
    }

    // ---- UpdateWFInit @ 0x82487608 ------------------------------------------------
    bool OnlineStats::UpdateWFInit()
    {
        // X360: leFlow arg is literal 0 (li r4,0), same idiom as OnlineRivals.
        if (!mpGuiCache->AreAllAptComponentsInitialised(static_cast<GuiFlow>(0)))
            return false;

        ClearExpectedComponent();
        return true;
    }

    // ---- UpdatePermanent @ 0x82492908 ---------------------------------------------
    void OnlineStats::UpdatePermanent()
    {
        OnlineStatsInQueue* lpInQueue = reinterpret_cast<OnlineStatsInQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = NULL;
        s32 liSize = 0;
        s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent != NULL)
        {
            if (liEventId == KI_EVENT_GO_BACK)
                SendStateEvent("GO_BACK");

            liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
    }
}
