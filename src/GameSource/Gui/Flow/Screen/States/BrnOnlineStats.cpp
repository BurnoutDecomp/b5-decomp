// GameSource/Gui/Flow/Screen/States/BrnOnlineStats.cpp
//
// BrnGui::OnlineStats -- the online-stats screen (ON_STATS). The eleven bodies, read off the
// console asm:
//
//   OnEnter / OnLeave / Update          the lifecycle and the internal-state chain
//   UpdateGetCache                      adopt the cache from the in-queue
//   UpdateWFInit / ClearExpectedComponent
//   UpdateSetupComponents               post the stats request (GUI 241)
//   UpdateRunning                       the screen answers its own request with a fixed set
//                                       of totals (GUI 242), then prints whatever 242 says
//   UpdatePermanent                     disconnected (GUI 44) goes back
//   HandleControllerInputPressed / HandleStatsData
//
// Out-queue records are the console's own wire records, posted on channel 40 through
// GetOutputEventQueue()->AddEvent with host sizeof sizes.

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineStats.h"

#include <cstddef>                                                        // offsetof (wire records)
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> / GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include "GameSource/Gui/Events/BrnGuiEventOnlineStatsResponse.h"         // GuiEventOnlineStatsResponse
#include "GameSource/Input/GameInputActions.h"                            // EGameInputActions

namespace BrnGui
{
    // ================================================================================
    //  Class statics (values read from the image)
    // ================================================================================

    // The events the screen observes: 14, apt ONLOAD, controller press, disconnected, gui
    // cache, the stats response, the stats request.
    const s32 OnlineStats::maiEventToObserve[7] = { 14, 21, 6, 44, 64, 242, 241 };
    const s32 OnlineStats::miNumEventsObserved  = 7;

    // The one apt package the screen loads (ON_STATS).
    const CgsGui::sResourceTuple OnlineStats::maResourcesToLoad[] =
    {
        { 186, CgsGui::E_GUI_RESOURCETYPE_APT }
    };
    const u32 OnlineStats::muNumResourcesToLoad = 1;

    const char OnlineStats::KAC_TEXTFIELD_NAME_TOTAL_GAMES[14]     = "TotalGames_mc";
    const char OnlineStats::KAC_TEXTFIELD_NAME_WIN_RATE[11]        = "WinRate_mc";
    const char OnlineStats::KAC_TEXTFIELD_NAME_TAKEDOWNS[13]       = "Takedowns_mc";
    const char OnlineStats::KAC_TEXTFIELD_NAME_RIVALS[10]          = "Rivals_mc";
    const char OnlineStats::KAC_TEXTFIELD_NAME_MUGSHOTS[12]        = "Mugshots_mc";
    const char OnlineStats::KAC_TEXTFIELD_NAME_DISCONNECT_RATE[18] = "DisconnectRate_mc";

    namespace
    {
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;   // mpInGuiEventQueue's real type

        const s32 KI_CHANNEL_GUI_OUT = 40;

        const s32 KI_EVENT_CONTROLLER_INPUT     = 6;
        const s32 KI_EVENT_NETWORK_DISCONNECTED = 44;
        const s32 KI_EVENT_GUI_CACHE            = 64;
        const s32 KI_EVENT_STATS_REQUEST        = 241;
        const s32 KI_EVENT_STATS_RESPONSE       = 242;

        // The movie the PLAYSWF step plays: the screen's own package.
        const u32 KU_STATS_MOVIE_RESOURCE  = 186;
        const s32 KI_APT_MOVIE_LEVEL       = 3;
        const char* const KPC_EMPTY_STRING = "";

        const char KAC_GO_BACK_EVENT[] = "GO_BACK";

        // The totals the screen hands itself in answer to its own request.
        const s32 KI_CANNED_TOTAL_GAMES     = 37;
        const s32 KI_CANNED_WIN_RATE        = 42;
        const s32 KI_CANNED_TAKEDOWNS       = 67;
        const s32 KI_CANNED_RIVALS          = 41;
        const s32 KI_CANNED_MUGSHOTS        = 15;
        const s32 KI_CANNED_DISCONNECT_RATE = 7;

        // HandleStatsData's number buffer.
        const u32 KU_STAT_TEXT_LENGTH = 32;

        // ---- in-queue payload views (the queue hands out the header-stripped payload) ----
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (the input action id)
        };

        // The gui-cache event (the assert text names its field mpCachePointer).
        struct GuiEventCachePayload : public CgsModule::Event
        {
            GuiCache* mpCachePointer;   // +0x00
        };

        // ---- out-queue wire records ------------------------------------------------------
        // { 1, 241, 12, <one byte> }, channel 40, 16 bytes: the stats request. The console
        // never writes the payload byte.
        struct OnlineStatsRequestWire : public CgsGui::GuiEvent<241>
        {
            u8 muUnwrittenPayload;   // +0x0C

            OnlineStatsRequestWire()
                : CgsGui::GuiEvent<241>(
                      static_cast<u32>(sizeof(u8)),
                      static_cast<u32>(offsetof(OnlineStatsRequestWire, muUnwrittenPayload)))
                , muUnwrittenPayload(0)
            {
            }
        };

        // { 24, 242, 12, the six totals }, channel 40, 36 bytes.
        typedef CgsGui::GuiEventWrapper<GuiEventOnlineStatsResponse, 40> OnlineStatsResponseWire;

        static_assert(sizeof(OnlineStatsRequestWire) == 16, "stats request record is 16 bytes");
        static_assert(sizeof(OnlineStatsResponseWire) == 36, "stats response record is 36 bytes");
    }

    // ================================================================================
    //  OnEnter
    // ================================================================================
    void OnlineStats::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        meInternalState = E_INTERNALSTATE_GETCACHE;
        mpGuiCache      = 0;

        mTotalGames.Construct(KAC_TEXTFIELD_NAME_TOTAL_GAMES, mpStateInterface, 0);
        mWinRate.Construct(KAC_TEXTFIELD_NAME_WIN_RATE, mpStateInterface, 0);
        mTakedowns.Construct(KAC_TEXTFIELD_NAME_TAKEDOWNS, mpStateInterface, 0);
        mRivals.Construct(KAC_TEXTFIELD_NAME_RIVALS, mpStateInterface, 0);
        mMugshots.Construct(KAC_TEXTFIELD_NAME_MUGSHOTS, mpStateInterface, 0);
        mDisconnectRate.Construct(KAC_TEXTFIELD_NAME_DISCONNECT_RATE, mpStateInterface, 0);
    }

    // ================================================================================
    //  OnLeave
    // ================================================================================
    void OnlineStats::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The console inlines StateInterface::PlayAptMovie here: the empty name at level 3
        // clears the level.
        mpStateInterface->PlayAptMovie(KPC_EMPTY_STRING, KI_APT_MOVIE_LEVEL);

        ClearExpectedComponent();
    }

    // ================================================================================
    //  Update -- each step stores its state on entry and drops into the next one once it
    //  completes; the permanent handlers then run and the in-queue is cleared.
    // ================================================================================
    void OnlineStats::Update()
    {
        switch (meInternalState)
        {
        case E_INTERNALSTATE_GETCACHE:
            UpdateGetCache();
            // fall through

        case E_INTERNALSTATE_LOADRESOURCES:
            meInternalState = E_INTERNALSTATE_LOADRESOURCES;
            if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
            {
                break;
            }
            // fall through

        case E_INTERNALSTATE_PLAYSWF:
            meInternalState = E_INTERNALSTATE_PLAYSWF;
            ClearExpectedComponent();
            mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_STATS_MOVIE_RESOURCE],
                                           KI_APT_MOVIE_LEVEL);
            // fall through

        case E_INTERNALSTATE_WFINIT:
            meInternalState = E_INTERNALSTATE_WFINIT;
            if (!UpdateWFInit())
            {
                break;
            }
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
            CGS_ASSERT(false, "Invalid internal state : ");
            break;
        }

        UpdatePermanent();

        reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue)->Clear();
    }

    // ================================================================================
    //  UpdateGetCache -- take the cache from the first gui-cache event in the in-queue.
    // ================================================================================
    void OnlineStats::UpdateGetCache()
    {
        CGS_ASSERT(0 == mpGuiCache, "NULL == mpGuiCache");

        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_GUI_CACHE)
            {
                const GuiEventCachePayload* lpCacheEvent =
                    static_cast<const GuiEventCachePayload*>(lpEvent);
                CGS_ASSERT(0 != lpCacheEvent->mpCachePointer,
                           "NULL != lpCacheEvent->mpCachePointer");
                mpGuiCache = lpCacheEvent->mpCachePointer;
                break;
            }
        }

        CGS_ASSERT(0 != mpGuiCache, "NULL != mpGuiCache");
    }

    // ================================================================================
    //  UpdateWFInit -- wait for every expected apt component.
    // ================================================================================
    bool OnlineStats::UpdateWFInit()
    {
        if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            return false;
        }

        ClearExpectedComponent();
        return true;
    }

    // ================================================================================
    //  UpdateSetupComponents -- ask for the stats.
    // ================================================================================
    void OnlineStats::UpdateSetupComponents()
    {
        const OnlineStatsRequestWire lRequest;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lRequest)));
    }

    // ================================================================================
    //  UpdateRunning
    // ================================================================================
    void OnlineStats::UpdateRunning()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_CONTROLLER_INPUT)
            {
                HandleControllerInputPressed(lpEvent);
            }
            else if (liEventId == KI_EVENT_STATS_REQUEST)
            {
                // The request is answered here, with fixed totals.
                GuiEventOnlineStatsResponse lResponse;
                lResponse.miTotalGames     = KI_CANNED_TOTAL_GAMES;
                lResponse.miWinRate        = KI_CANNED_WIN_RATE;
                lResponse.miTakedowns      = KI_CANNED_TAKEDOWNS;
                lResponse.miRivals         = KI_CANNED_RIVALS;
                lResponse.miMugshots       = KI_CANNED_MUGSHOTS;
                lResponse.miDisconnectRate = KI_CANNED_DISCONNECT_RATE;

                const OnlineStatsResponseWire lResponseWire(lResponse);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lResponseWire), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lResponseWire)));
            }
            else if (liEventId == KI_EVENT_STATS_RESPONSE)
            {
                HandleStatsData(reinterpret_cast<const GuiEventOnlineStatsResponse*>(lpEvent));
            }
        }
    }

    // ================================================================================
    //  UpdatePermanent -- disconnected goes back.
    // ================================================================================
    void OnlineStats::UpdatePermanent()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_NETWORK_DISCONNECTED)
            {
                SendStateEvent(KAC_GO_BACK_EVENT);
            }
        }
    }

    // ================================================================================
    //  ClearExpectedComponent -- drop the cache's expected list and the local copy.
    // ================================================================================
    void OnlineStats::ClearExpectedComponent()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

        for (u32 luIndex = 0; luIndex < KU_MAX_INIT_COMPONENTS_NUM; ++luIndex)
        {
            mauExpectedComponentIds[luIndex] = 0;
        }
        muNumExpectedComponents = 0;
    }

    // ================================================================================
    //  HandleControllerInputPressed -- cancel goes back.
    // ================================================================================
    void OnlineStats::HandleControllerInputPressed(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event sent to OnlineStats::HandleControllerInput");

        const ControllerButtonPayload* lpInput =
            static_cast<const ControllerButtonPayload*>(lpEvent);
        if (lpInput->miButtonId == E_GAMEINPUTACTIONS_GUI_CANCEL)
        {
            SendStateEvent(KAC_GO_BACK_EVENT);
        }
    }

    // ================================================================================
    //  HandleStatsData -- print the six totals.
    // ================================================================================
    void OnlineStats::HandleStatsData(const GuiEventOnlineStatsResponse* lpStats)
    {
        char lacText[KU_STAT_TEXT_LENGTH];

        CgsCore::SPrintf(lacText, KU_STAT_TEXT_LENGTH, "%d", lpStats->miTotalGames);
        lacText[KU_STAT_TEXT_LENGTH - 1] = 0;
        mTotalGames.SetText(lacText);

        CgsCore::SPrintf(lacText, KU_STAT_TEXT_LENGTH, "%d", lpStats->miWinRate);
        lacText[KU_STAT_TEXT_LENGTH - 1] = 0;
        mWinRate.SetText(lacText);

        CgsCore::SPrintf(lacText, KU_STAT_TEXT_LENGTH, "%d", lpStats->miTakedowns);
        lacText[KU_STAT_TEXT_LENGTH - 1] = 0;
        mTakedowns.SetText(lacText);

        CgsCore::SPrintf(lacText, KU_STAT_TEXT_LENGTH, "%d", lpStats->miRivals);
        lacText[KU_STAT_TEXT_LENGTH - 1] = 0;
        mRivals.SetText(lacText);

        CgsCore::SPrintf(lacText, KU_STAT_TEXT_LENGTH, "%d", lpStats->miMugshots);
        lacText[KU_STAT_TEXT_LENGTH - 1] = 0;
        mMugshots.SetText(lacText);

        CgsCore::SPrintf(lacText, KU_STAT_TEXT_LENGTH, "%d", lpStats->miDisconnectRate);
        lacText[KU_STAT_TEXT_LENGTH - 1] = 0;
        mDisconnectRate.SetText(lacText);
    }
}
