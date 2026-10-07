// GameSource/Gui/Flow/Screen/States/BrnOnlineNews.cpp
//
// BrnGui::OnlineNews -- the online news / terms-of-service screen (ON_NEWS). The eleven
// bodies, read off the console asm:
//
//   OnEnter / OnLeave / Update          the lifecycle; OnEnter asks for the news download
//                                       and logs the visit (telemetry 42); the frame tick
//                                       (GUI 26) scrolls the text by the held stick
//   CheckForCompletedLoads              load ON_NEWS, then wait for its components
//   HandleGuiCacheEvent                 adopt the cache, register the two text fields
//   HandleNewsAndTOSEvent               a download finished or failed: refresh the text
//   HandleControllerInput(+SelectParams)  back leaves (the quiet way when this flow armed
//                                       the online start)
//   HandleControllerAxis                latch the stick outside the dead zone
//   ShowText                            the toggle + the text for its download state; the
//                                       first read of new news clears the unread flag
//
// Out-queue records are the console's own wire records, posted on channel 40 through
// GetOutputEventQueue()->AddEvent with host sizeof sizes.

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineNews.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase (unexpected-event log)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEventWrapper
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"               // CgsGui::GuiEventTimeInfo
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface / GuiEventNetworkSuspension
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // GuiEventNetworkNewsAndTOS / GuiTelemetryEvent / GuiAutosaveRequestEvent
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                      // OptionsDataProfile (the unread-news flag)
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include "GameSource/Input/GameInputActions.h"                            // EGameInputActions

namespace BrnGui
{
    // ================================================================================
    //  Class statics (values read from the image)
    // ================================================================================

    // The events the screen observes: 14, apt ONLOAD, controller press, gui cache, news /
    // TOS, disconnected, controller axis, the frame tick.
    const s32 OnlineNews::maiEventToObserve[8] = { 14, 21, 6, 64, 266, 44, 8, 26 };
    const s32 OnlineNews::miNumEventsObserved  = 8;

    // The one apt package the screen loads (ON_NEWS).
    const CgsGui::sResourceTuple OnlineNews::maResourceTuplesToLoad[] =
    {
        { 181, CgsGui::E_GUI_RESOURCETYPE_APT }
    };
    const s32 OnlineNews::miNumResourcesToLoad = 1;

    const f32 OnlineNews::KF_TIME_TO_SCROLL_ONE_LINE = 0.2f;
    const f32 OnlineNews::KF_AXIS_DEAD_ZONE          = 0.25f;

    const char OnlineNews::KAC_TOGGLE_TEXT_COMPONENT[11] = "ToggleText";
    const char OnlineNews::KAC_NEWS_TEXT_COMPONENT[9]    = "NewsText";

    const char* const OnlineNews::KPAC_TOGGLE_TEXT_STRING_ID[E_TOGGLE_ITEM_COUNT] =
    {
        "$ONLINE_NEWS_TOGGLE_NEWS", "$ONLINE_NEWS_TOGGLE_TOS"
    };

    const char* const OnlineNews::KPAC_NEWS_TEXT_STRING_ID[E_TOGGLE_ITEM_COUNT][E_DOWNLOAD_STATE_COUNT] =
    {
        { "$ONLINE_NEWS_DOWNLOADING_NEWS", "~NEWS_TEXT", "$ONLINE_NEWS_FAILED_DOWNLOAD_NEWS" },
        { "$ONLINE_NEWS_DOWNLOADING_TOS",  "~TOS_TEXT",  "$ONLINE_NEWS_FAILED_DOWNLOAD_TOS"  }
    };

    namespace
    {
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;   // mpInGuiEventQueue's real type

        const s32 KI_CHANNEL_GUI_OUT = 40;

        const s32 KI_EVENT_CONTROLLER_INPUT     = 6;
        const s32 KI_EVENT_CONTROLLER_AXIS      = 8;
        const s32 KI_EVENT_OBSERVED_NO_ARM_14   = 14;
        const s32 KI_EVENT_APT_ONLOAD           = 21;   // observed, no arm
        const s32 KI_EVENT_FRAME_TICK           = 26;
        const s32 KI_EVENT_NETWORK_DISCONNECTED = 44;
        const s32 KI_EVENT_GUI_CACHE            = 64;
        const s32 KI_EVENT_NEWS_AND_TOS         = 266;

        // The movie CheckForCompletedLoads plays: the screen's own package.
        const u32 KU_NEWS_MOVIE_RESOURCE   = 181;
        const s32 KI_APT_MOVIE_LEVEL       = 3;
        const char* const KPC_EMPTY_STRING = "";

        // The two stick axes the scroll follows.
        const s32 KI_SCROLL_AXIS_A = 1;
        const s32 KI_SCROLL_AXIS_B = 2;

        // The telemetry record OnEnter posts for a visit to the news page.
        const s32 KI_TELEMETRY_TYPE_NEWS = 42;

        const char KAC_GO_BACK_EVENT[]      = "GO_BACK";
        const char KAC_GO_BACK_EASY_EVENT[] = "GO_BACK_EASY";

        // ---- in-queue payload views (the queue hands out the header-stripped payload) ----
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (the input action id)
        };

        struct ControllerAxisPayload : public CgsModule::Event
        {
            s32 miAxis;    // +0x00
            f32 mfXAxis;   // +0x04
            f32 mfYAxis;   // +0x08
        };

        struct GuiCachePayload : public CgsModule::Event
        {
            GuiCache* mpCache;   // +0x00
        };

        // ---- out-queue wire records ------------------------------------------------------
        // { 4, 266, 12, the request }, channel 40, 16 bytes.
        typedef CgsGui::GuiEventWrapper<GuiEventNetworkNewsAndTOS, 40> GuiEventNetworkNewsAndTOSWire;
        // { 1, 356, 12, <the byte> }, channel 40, 16 bytes.
        typedef CgsGui::GuiEventWrapper<GuiAutosaveRequestEvent, 40> GuiAutosaveRequestEventWire;

        static_assert(sizeof(GuiEventNetworkNewsAndTOSWire) == 16, "news/TOS record is 16 bytes");
        static_assert(sizeof(GuiAutosaveRequestEventWire) == 16, "autosave record is 16 bytes");
        static_assert(sizeof(GuiTelemetryEvent) == 32, "telemetry record is 32 bytes");
        static_assert(sizeof(CgsGui::GuiEventNetworkSuspension) == 16, "suspension record is 16 bytes");
    }

    // ================================================================================
    //  OnEnter
    // ================================================================================
    void OnlineNews::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mToggleText.Construct(KAC_TOGGLE_TEXT_COMPONENT, mpStateInterface, 0);
        mNewsText.Construct(KAC_NEWS_TEXT_COMPONENT, mpStateInterface, 0);

        mfLastAxisValue     = 0.0f;
        mfScrollAmount      = 0.0f;
        meSubState          = E_SUBSTATE_LOADING_SCREEN;
        mpGuiCache          = 0;
        meCurrentToggleItem = E_TOGGLE_ITEM_NEWS;
        maeDownloadState[E_TOGGLE_ITEM_NEWS] = E_DOWNLOAD_STATE_DOWNLOADING;
        maeDownloadState[E_TOGGLE_ITEM_TOS]  = E_DOWNLOAD_STATE_DOWNLOADING;

        // Ask for the news.
        GuiEventNetworkNewsAndTOS lRequestNews;
        lRequestNews.meEventType = GuiEventNetworkNewsAndTOS::E_EVENT_TYPE_REQUEST_NEWS;
        const GuiEventNetworkNewsAndTOSWire lRequestNewsWire(lRequestNews);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequestNewsWire), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lRequestNewsWire)));

        // Log the visit (no parameters).
        GuiTelemetryEvent lTelemetry(KI_TELEMETRY_TYPE_NEWS);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lTelemetry), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lTelemetry)));
    }

    // ================================================================================
    //  OnLeave
    // ================================================================================
    void OnlineNews::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The console inlines StateInterface::PlayAptMovie here: the empty name at level 3
        // clears the level.
        mpStateInterface->PlayAptMovie(KPC_EMPTY_STRING, KI_APT_MOVIE_LEVEL);
    }

    // ================================================================================
    //  Update
    // ================================================================================
    void OnlineNews::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
            case KI_EVENT_CONTROLLER_INPUT:
                HandleControllerInput(lpEvent);
                break;

            case KI_EVENT_CONTROLLER_AXIS:
                HandleControllerAxis(lpEvent);
                break;

            case KI_EVENT_OBSERVED_NO_ARM_14:
            case KI_EVENT_APT_ONLOAD:
                break;

            case KI_EVENT_FRAME_TICK:
                // Integrate the held stick; each line's worth scrolls the text one line and
                // carries the remainder.
                if (mfLastAxisValue == 0.0f)
                {
                    mfScrollAmount = 0.0f;
                }
                else
                {
                    const CgsGui::GuiEventTimeInfo* lpTimeInfo =
                        reinterpret_cast<const CgsGui::GuiEventTimeInfo*>(lpEvent);
                    mfScrollAmount = lpTimeInfo->GetTimeStep() * mfLastAxisValue + mfScrollAmount;

                    if (!(mfScrollAmount > -KF_TIME_TO_SCROLL_ONE_LINE))
                    {
                        mNewsText.ScrollDown();
                        mNewsText.OutputAptData();
                        mfScrollAmount = mfScrollAmount + KF_TIME_TO_SCROLL_ONE_LINE;
                    }
                    else if (!(mfScrollAmount < KF_TIME_TO_SCROLL_ONE_LINE))
                    {
                        mNewsText.ScrollUp();
                        mNewsText.OutputAptData();
                        mfScrollAmount = mfScrollAmount - KF_TIME_TO_SCROLL_ONE_LINE;
                    }
                }
                break;

            case KI_EVENT_NETWORK_DISCONNECTED:
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
                mpGuiCache->SetDoDisconnectPopup(lpEvent);
                if (mpGuiCache->IsOnlineStartPending())
                {
                    // This flow armed the online start: lift the network suspension it put
                    // in place, then back out on the quiet path.
                    CgsGui::GuiEventNetworkSuspension lNetworkSuspension(false);
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lNetworkSuspension),
                        KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lNetworkSuspension)));

                    mpGuiCache->SetOnlineStartPending(false);
                    SendStateEvent(KAC_GO_BACK_EASY_EVENT);
                }
                else
                {
                    SendStateEvent(KAC_GO_BACK_EVENT);
                }
                break;

            case KI_EVENT_GUI_CACHE:
                HandleGuiCacheEvent(lpEvent);
                break;

            case KI_EVENT_NEWS_AND_TOS:
                HandleNewsAndTOSEvent(lpEvent);
                break;

            default:
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "Unexpected event received : " << liEventId
                        << " in "
                        << "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnOnlineNews.cpp"
                        << " at line " << 239 << "\n";
                }
                break;
            }
        }

        lpInQueue->Clear();

        CheckForCompletedLoads();
    }

    // ================================================================================
    //  CheckForCompletedLoads -- wait for the apt package, then for its components; then
    //  show the text.
    // ================================================================================
    void OnlineNews::CheckForCompletedLoads()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        if (meSubState == E_SUBSTATE_LOADING_SCREEN)
        {
            if (mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad,
                                                     static_cast<u32>(miNumResourcesToLoad)))
            {
                mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_NEWS_MOVIE_RESOURCE],
                                               KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_COMPONENTS;
            }
        }
        else if (meSubState == E_SUBSTATE_LOADING_COMPONENTS)
        {
            // The console re-tests the cache after the assert (the assert does not stop).
            if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                meSubState = E_SUBSTATE_SELECTING_PARAMS;
                ShowText();
            }
        }
    }

    // ================================================================================
    //  HandleControllerInput -- only the text page takes input.
    // ================================================================================
    void OnlineNews::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event sent to OnlineNews::HandleControllerInput");

        if (meSubState == E_SUBSTATE_SELECTING_PARAMS)
        {
            HandleControllerInputSelectParams(lpEvent);
        }
    }

    // ================================================================================
    //  HandleControllerInputSelectParams -- cancel goes back.
    // ================================================================================
    void OnlineNews::HandleControllerInputSelectParams(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineNews::HandleControllerInputSelectParams");

        const ControllerButtonPayload* lpInput =
            static_cast<const ControllerButtonPayload*>(lpEvent);
        if (lpInput->miButtonId != E_GAMEINPUTACTIONS_GUI_CANCEL)
        {
            return;
        }

        if (mpGuiCache->IsOnlineStartPending())
        {
            // This flow armed the online start: lift the network suspension it put in place,
            // then back out on the quiet path.
            CgsGui::GuiEventNetworkSuspension lNetworkSuspension(false);
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lNetworkSuspension),
                KI_CHANNEL_GUI_OUT, static_cast<s32>(sizeof(lNetworkSuspension)));

            mpGuiCache->SetOnlineStartPending(false);
            SendStateEvent(KAC_GO_BACK_EASY_EVENT);
            return;
        }

        SendStateEvent(KAC_GO_BACK_EVENT);
    }

    // ================================================================================
    //  HandleGuiCacheEvent -- adopt the first cache offered and register the text fields.
    // ================================================================================
    void OnlineNews::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        const GuiCachePayload* lpPayload = static_cast<const GuiCachePayload*>(lpEvent);

        CGS_ASSERT(lpPayload->mpCache != 0, "Invalid cache in OnlineNews::HandleGuiCacheEvent");

        if (mpGuiCache == 0)
        {
            mpGuiCache = lpPayload->mpCache;
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mToggleText.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mNewsText.GetName());
        }
    }

    // ================================================================================
    //  HandleNewsAndTOSEvent -- record a finished or failed download, then refresh.
    // ================================================================================
    void OnlineNews::HandleNewsAndTOSEvent(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event in OnlineNews::HandleNewsAndTOSEvent");

        const GuiEventNetworkNewsAndTOS* lpNewsAndTOS =
            reinterpret_cast<const GuiEventNetworkNewsAndTOS*>(lpEvent);
        switch (lpNewsAndTOS->meEventType)
        {
        case GuiEventNetworkNewsAndTOS::E_EVENT_TYPE_RETRIEVED_NEWS:
            maeDownloadState[E_TOGGLE_ITEM_NEWS] = E_DOWNLOAD_STATE_DONE;
            break;

        case GuiEventNetworkNewsAndTOS::E_EVENT_TYPE_RETRIEVED_TOS:
            maeDownloadState[E_TOGGLE_ITEM_TOS] = E_DOWNLOAD_STATE_DONE;
            break;

        case GuiEventNetworkNewsAndTOS::E_EVENT_TYPE_FAILED_NEWS:
            maeDownloadState[E_TOGGLE_ITEM_NEWS] = E_DOWNLOAD_STATE_FAILED;
            break;

        case GuiEventNetworkNewsAndTOS::E_EVENT_TYPE_FAILED_TOS:
            maeDownloadState[E_TOGGLE_ITEM_TOS] = E_DOWNLOAD_STATE_FAILED;
            break;

        default:
            break;
        }

        ShowText();
    }

    // ================================================================================
    //  HandleControllerAxis -- before the text page the stick is ignored; on it, either
    //  stick axis outside the dead zone is latched.
    // ================================================================================
    void OnlineNews::HandleControllerAxis(const CgsModule::Event* lpEvent)
    {
        // The console's text names SelectRoutes (the same pooled string).
        CGS_ASSERT(lpEvent != 0, "Invalid event in SelectRoutes::HandleControllerAxis");

        if (meSubState < E_SUBSTATE_SELECTING_PARAMS)
        {
            mfLastAxisValue = 0.0f;
            return;
        }

        const ControllerAxisPayload* lpAxis = static_cast<const ControllerAxisPayload*>(lpEvent);
        if (lpAxis->miAxis != KI_SCROLL_AXIS_A && lpAxis->miAxis != KI_SCROLL_AXIS_B)
        {
            return;
        }

        const f32 lfValue = lpAxis->mfYAxis;
        mfLastAxisValue = (lfValue < -KF_AXIS_DEAD_ZONE || lfValue > KF_AXIS_DEAD_ZONE)
                              ? lfValue
                              : 0.0f;
    }

    // ================================================================================
    //  ShowText -- the toggle label, the text for the toggled item's download state, and
    //  the first look at downloaded news clears the profile's unread flag (with a save).
    // ================================================================================
    void OnlineNews::ShowText()
    {
        mToggleText.SetText(KPAC_TOGGLE_TEXT_STRING_ID[meCurrentToggleItem]);
        mNewsText.SetText(
            KPAC_NEWS_TEXT_STRING_ID[meCurrentToggleItem][maeDownloadState[meCurrentToggleItem]]);
        mNewsText.ResetScroll();
        mNewsText.OutputAptData();

        if (mpGuiCache != 0
            && meCurrentToggleItem == E_TOGGLE_ITEM_NEWS
            && maeDownloadState[E_TOGGLE_ITEM_NEWS] == E_DOWNLOAD_STATE_DONE)
        {
            GuiAutosaveRequestEvent lAutosaveEvent;
            lAutosaveEvent.maData[0] = 0;

            OptionsDataProfile* lpOptions = mpGuiCache->GetOptionsDataProfile();
            if (lpOptions->IsThereUnreadNews())
            {
                lpOptions->SetUnreadNews(false);

                CGS_ASSERT(mpStateInterface != 0, "mpStateInterface");
                const GuiAutosaveRequestEventWire lAutosaveWire(lAutosaveEvent);
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lAutosaveWire), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lAutosaveWire)));
            }
        }
    }
}
