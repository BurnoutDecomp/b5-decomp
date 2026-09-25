// ===================================================================================
// BrnGui::GuiNetworkRouteInfo -- the online route-info panel.
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkRouteInfo.cpp
//
// Reconstructed from the console image; the assembly listing of each function
// arbitrates over the decompiler output throughout.
//
// The tables below are the image's static data, read back value for value, except
// K_MAP_DISPLAY_RECT, which the image fills in its dynamic initialiser ({591, 316}).
// The game-mode string table has eight rows in this build (modes 10..17); SetInfo's
// search is bounded by the table's end.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkRouteInfo.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface::GetLanguageManager
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // FormatAndAddText
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameSource/Gui/BrnGuiCache.h"                                   // GuiCache
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"           // GuiEventNetworkGameParams
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"                         // MapTransform
#include "SharedClasses/Traffic/BrnTrafficLightTrigger.h"                 // KU_LIGHT_TRIGGER_ID_OWNER_TAG

namespace BrnGui
{
    namespace
    {
        namespace GSM = BrnGameState::GameStateModuleIO;

        // The state in-queue concrete type (GuiEventQueue is its pointer-only face).
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // The GUI module's per-frame GuiCache post.
        const s32 KI_GUI_CACHE_EVENT_ID = 64;

        struct GuiEventCachePayload : public CgsModule::Event
        {
            GuiCache* mpGuiCache;
        };

        // The ARTIST build's online stunt co-op mode (one past the online-free-burn
        // showtime entry).
        const GSM::EGameModeType KE_MODE_ONLINE_STUNT_COOP = static_cast<GSM::EGameModeType>(17);

        // LightTriggerId::IsValid: neither the hull half (bits 8..23) nor the light-index
        // byte is all ones.
        const u32 KU_LIGHT_TRIGGER_HULL_MASK  = 0x00FFFF00u;
        const u32 KU_LIGHT_TRIGGER_INDEX_MASK = 0x000000FFu;

        bool IsLightTriggerIdValid(u32 luLightTriggerId)
        {
            return (luLightTriggerId & KU_LIGHT_TRIGGER_HULL_MASK) != KU_LIGHT_TRIGGER_HULL_MASK &&
                   (luLightTriggerId & KU_LIGHT_TRIGGER_INDEX_MASK) != KU_LIGHT_TRIGGER_INDEX_MASK;
        }

        // The route map's two layout rectangles (view + padding, normalised screen space).
        const f32 KF_VIEW_RECT_LEFT      = 0.0f;
        const f32 KF_VIEW_RECT_TOP       = 0.175f;
        const f32 KF_VIEW_RECT_RIGHT     = 1.0f;
        const f32 KF_VIEW_RECT_BOTTOM    = 0.84027779f;
        const f32 KF_PADDING_RECT_LEFT   = 0.4f;
        const f32 KF_PADDING_RECT_TOP    = 0.18055555f;
        const f32 KF_PADDING_RECT_RIGHT  = 0.95f;
        const f32 KF_PADDING_RECT_BOTTOM = 0.7f;

        // The zoom-fit aspect base handed to MapTransform::CalculateZoomFactor.
        const f32 KF_MAP_BASE_ASPECT_RATIO = 1.7777778f;

        // The owner name the icon set is parented under.
        const char KAC_ICON_PARENT_NAME[] = "RouteInfo";

        // The rounds text's formatter scratch.
        const s32 KI_FORMAT_BUFFER_COUNT  = 3;
        const s32 KI_FORMAT_BUFFER_LENGTH = 64;
    }

    // ---- the per-mode option string table ----------------------------------------------
    struct GameModeToString
    {
        GSM::EGameModeType meGameMode;
        const char*        mpcStringID;
    };

    const s32 KI_NUM_GAME_MODES = 8;

    const GameModeToString KA_GAMEMODE_TO_STRING[KI_NUM_GAME_MODES] =
    {
        { GSM::E_MODE_ONLINE_RACE,             "$ONLINE_GAME_OPTION_MODE_RACE" },
        { GSM::E_MODE_ONLINE_ROAD_RAGE,        "$ONLINE_GAME_OPTION_MODE_ROAD_RAGE" },
        { GSM::E_MODE_ONLINE_FUGITIVE,         "$ONLINE_GAME_OPTION_MODE_STUNT" },
        { GSM::E_MODE_ONLINE_BURNING_HOME_RUN, "$ONLINE_GAME_OPTION_MODE_BURNING_HOME_RUN" },
        { GSM::E_MODE_ONLINE_FREE_BURN,        "$ONLINE_GAME_OPTION_MODE_STUNT_FREE_FOR_ALL" },
        { GSM::E_MODE_ONLINE_FREE_BURN_LOBBY,  "$ONLINE_GAME_OPTION_MODE_FREE_BURN" },
        { GSM::E_MODE_ONLINE_SHOWTIME,         "$ONLINE_GAME_OPTION_MODE_SHOWTIME" },
        { KE_MODE_ONLINE_STUNT_COOP,           "$ONLINE_GAME_OPTION_MODE_STUNT_COOP" },
    };

    // ---- class statics --------------------------------------------------------------------
    const char* const GuiNetworkRouteInfo::KPAC_OPTIONS_ANIMATOR_COMPONENT[E_OPTION_COMPONENT_COUNT] =
    {
        "GameOptions_1_anim", "GameOptions_2_anim", "GameOptions_3_anim",
        "GameOptions_4_anim", "GameOptions_5_anim", "GameOptions_6_anim",
        "GameOptions_7_anim", "GameOptions_8_anim", "GameOptions_9_anim",
    };

    const char* const GuiNetworkRouteInfo::KPAC_HEADING_COMPONENT[E_OPTION_COMPONENT_COUNT] =
    {
        "Heading1", "Heading2", "Heading3", "Heading4", "Heading5",
        "Heading6", "Heading7", "Heading8", "Heading9",
    };

    const char* const GuiNetworkRouteInfo::KPAC_VALUE_COMPONENT[E_OPTION_COMPONENT_COUNT] =
    {
        "Value1", "Value2", "Value3", "Value4", "Value5",
        "Value6", "Value7", "Value8", "Value9",
    };

    const s32 GuiNetworkRouteInfo::KAI_RACE_OPTION_TO_COMPONENT_MAPPING[E_OPTION_COMPONENT_COUNT] =
    {
        0, 3, 2, -1, 1, -1, -1, -1, -1,
    };

    const s32 GuiNetworkRouteInfo::KAI_ROAD_RAGE_OPTION_TO_COMPONENT_MAPPING[E_OPTION_COMPONENT_COUNT] =
    {
        0, 3, 2, 4, 1, -1, -1, -1, -1,
    };

    const s32 GuiNetworkRouteInfo::KAI_BHR_OPTION_TO_COMPONENT_MAPPING[E_OPTION_COMPONENT_COUNT] =
    {
        0, 3, 4, 3, 1, -1, -1, 5, 6,
    };

    const char GuiNetworkRouteInfo::KAC_APT_STATE[10] = "apt_state";

    const char GuiNetworkRouteInfo::KAC_NUM_ROUNDS_STRING_ID[12]           = "$NUM_ROUNDS";
    const char GuiNetworkRouteInfo::KAC_NUM_ROUNDS_FORMAT_STRING_ID[27]    = "ONLINE_ROUTE_CURRENT_ROUND";
    const char GuiNetworkRouteInfo::KAC_NUM_ROUNDS_RR_FORMAT_STRING_ID[30] = "ONLINE_ROUTE_CURRENT_ROUND_RR";

    const char* const GuiNetworkRouteInfo::KPAC_HEADING_STRING_IDS[E_OPTION_COMPONENT_COUNT] =
    {
        "$ONLINE_GAME_OPTION_MODE",
        "$ONLINE_GAME_OPTION_TRAFFIC",
        "$ONLINE_GAME_OPTION_VEHICLE_CLASS",
        "$ONLINE_GAME_OPTION_RED_TEAM",
        "$ONLINE_GAME_OPTION_ROUNDS",
        "$ONLINE_GAME_OPTION_TRAF_CHK",
        "$ONLINE_GAME_OPTION_BOOST",
        "$ONLINE_GAME_OPTION_CR_LIMIT",
        "$ONLINE_GAME_OPTION_TIME_LIMIT",
    };

    const char* const GuiNetworkRouteInfo::KPAC_VEHICLE_CLASS_STRING_IDS[10] =
    {
        "$ONLINE_GAME_OPTION_VEHICLE_CLASS_1",
        "$ONLINE_GAME_OPTION_VEHICLE_CLASS_5",
        "$ONLINE_GAME_OPTION_VEHICLE_CLASS_4",
        "$ONLINE_GAME_OPTION_VEHICLE_CLASS_3",
        "$ONLINE_GAME_OPTION_VEHICLE_CLASS_2",
        "$ONLINE_GAME_OPTION_VEHICLE_CLASS_6",
        "$ONLINE_GAME_OPTION_VEHICLE_CLASS_7",
        "$ONLINE_GAME_OPTION_VEHICLE_CLASS_8",
        "$ONLINE_GAME_OPTION_VEHICLE_CLASS_9",
        "$ONLINE_GAME_OPTION_VEHICLE_CLASS_10",
    };

    const char* const GuiNetworkRouteInfo::KPAC_RED_TEAM_INFINITE_BOOST_STRING_IDS[2] =
    {
        "$ONLINE_GAME_OPTION_INFINITE_BOOST",
        "$ONLINE_GAME_OPTION_NORMAL_BOOST",
    };

    const char* const GuiNetworkRouteInfo::KPAC_TRAFFIC_STRING_IDS[2] =
    {
        "$GENERAL_OPTION_ON",
        "$GENERAL_OPTION_OFF",
    };

    const char* const GuiNetworkRouteInfo::KPAC_TRAFFIC_CHECKING_STRING_IDS[2] =
    {
        "$GENERAL_OPTION_ON",
        "$GENERAL_OPTION_OFF",
    };

    const char* const GuiNetworkRouteInfo::KPAC_BOOST_TYPE_STRING_IDS[6] =
    {
        "$ONLINE_GAME_OPTION_BOOST_B1",
        "$ONLINE_GAME_OPTION_BOOST_B2",
        "$ONLINE_GAME_OPTION_BOOST_B3",
        "$ONLINE_GAME_OPTION_BOOST_B4",
        "$ONLINE_GAME_OPTION_BOOST_B5",
        "$ONLINE_GAME_OPTION_BOOST_I",
    };

    const char* const GuiNetworkRouteInfo::KPAC_CRASH_LIMIT_STRING_IDS[6] =
    {
        "$ONLINE_GAME_OPTION_CR_LIMIT_1",
        "$ONLINE_GAME_OPTION_CR_LIMIT_2",
        "$ONLINE_GAME_OPTION_CR_LIMIT_3",
        "$ONLINE_GAME_OPTION_CR_LIMIT_4",
        "$ONLINE_GAME_OPTION_CR_LIMIT_5",
        "$ONLINE_GAME_OPTION_CR_LIMIT_N",
    };

    const char* const GuiNetworkRouteInfo::KPAC_TIME_LIMIT_STRING_IDS[3] =
    {
        "$ONLINE_GAME_OPTION_TIME_LIMIT_10",
        "$ONLINE_GAME_OPTION_TIME_LIMIT_20",
        "$ONLINE_GAME_OPTION_TIME_LIMIT_30",
    };

    const char* const GuiNetworkRouteInfo::KPAC_STATE_ID[E_STATE_COUNT]  = { "visible", "invisible" };
    const char* const GuiNetworkRouteInfo::KPAC_OPTION_STATE_ID[2]       = { "visible", "invisible" };

    const char GuiNetworkRouteInfo::macCrashNavBoroughName[11] = "Borough_mc";
    const char GuiNetworkRouteInfo::macSatNavIconBaseName[11]  = "SatNavIcon";

    const Vector2 GuiNetworkRouteInfo::K_MAP_DISPLAY_RECT = { 591.0f, 316.0f, 0.0f, 0.0f };

    // ------------------------------------------------------------ ctor
    // Member construction only (the nine animators and text-field pairs, the map component
    // with its map manager, the county outline).
    GuiNetworkRouteInfo::GuiNetworkRouteInfo()
    {
    }

    // ------------------------------------------------------------ Construct
    void GuiNetworkRouteInfo::Construct(const char* lpacName, s32 leMapType,
                                        CgsGui::StateInterface* lpStateInterface,
                                        const char* lpacParentName)
    {
        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

        for (s32 liComponent = 0; liComponent < E_OPTION_COMPONENT_COUNT; ++liComponent)
        {
            maOptionsAnimator[liComponent].Construct(KPAC_OPTIONS_ANIMATOR_COMPONENT[liComponent],
                                                     lpStateInterface, GetName());
            maHeading[liComponent].Construct(KPAC_HEADING_COMPONENT[liComponent],
                                             lpStateInterface, GetName());
            maValue[liComponent].Construct(KPAC_VALUE_COMPONENT[liComponent],
                                           lpStateInterface, GetName());
            maHeading[liComponent].SetAutoSize(true);
            maValue[liComponent].SetAutoSize(true);
        }

        MainMapComponent::MainMapParameterBundle lParameters;
        lParameters.mv4ViewRect.x    = KF_VIEW_RECT_LEFT;
        lParameters.mv4ViewRect.y    = KF_VIEW_RECT_TOP;
        lParameters.mv4ViewRect.z    = KF_VIEW_RECT_RIGHT;
        lParameters.mv4ViewRect.w    = KF_VIEW_RECT_BOTTOM;
        lParameters.mv4PaddingRect.x = KF_PADDING_RECT_LEFT;
        lParameters.mv4PaddingRect.y = KF_PADDING_RECT_TOP;
        lParameters.mv4PaddingRect.z = KF_PADDING_RECT_RIGHT;
        lParameters.mv4PaddingRect.w = KF_PADDING_RECT_BOTTOM;
        lParameters.meMapType        = static_cast<GuiEventRenderMainMap::EMapType>(leMapType);

        mMainMapComponent.Construct(mpStateInterface, &lParameters);
        mMainMapComponent.Prepare();
        mMainMapComponent.SetStickMapToScreenEdges(false, false, false, false);

        mpIconManager = 0;
        mv2WorldCenterPoint.x = 0.0f;
        mv2WorldCenterPoint.y = 0.0f;
        mv2WorldCenterPoint.z = 0.0f;
        mv2WorldCenterPoint.w = 0.0f;

        mCrashNavBorough.Construct(macCrashNavBoroughName, mpStateInterface, 0);

        meState               = E_STATE_INVISIBLE;
        mpGuiCache            = 0;
        miNumComponentsLoaded = 0;
        mbReceivedInfo        = false;
        mbShowingMap          = false;

        mEvent.Construct(0, BrnTraffic::KU_LIGHT_TRIGGER_ID_OWNER_TAG, 0, 0);

        mIconManagerOwnerId = MapIconManager::E_CRASHNAV_MAP_ONLINE;
    }

    // ------------------------------------------------------------ Destruct
    void GuiNetworkRouteInfo::Destruct()
    {
        miNumComponentsLoaded = 0;
        meState               = E_STATE_INVISIBLE;
        mpGuiCache            = 0;
        mbReceivedInfo        = false;

        if (mpIconManager != 0)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "MAPICONMANAGER: GuiNetworkRouteInfo is calling ReleaseResources.\n";
            }
            mpIconManager->ReleaseResources(mpStateInterface, mIconManagerOwnerId);
        }
    }

    // ------------------------------------------------------------ Update
    void GuiNetworkRouteInfo::Update(InputBuffer::GuiEventQueue* lpInGuiEventQueue)
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(lpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_GUI_CACHE_EVENT_ID)
            {
                HandleGuiCacheEvent(lpEvent);
            }
            mMainMapComponent.RecvEvent(lpEvent, liEventId);
        }

        if (miNumComponentsLoaded == KI_NUM_COMPONENTS_TO_LOAD && mbReceivedInfo && mbShowingMap)
        {
            mv2WorldCenterPoint = mMainMapComponent.Update(mv2WorldCenterPoint);
            mCrashNavBorough.SetCurrentBorough(meSelectedCounty);
            UpdateIconManager();

            if (mMainMapComponent.IsZooming())
            {
                mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_CUSTOM, CalculateZoomFactor(), false);
            }
        }
    }

    // ------------------------------------------------------------ HandleGuiCacheEvent
    void GuiNetworkRouteInfo::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        const GuiEventCachePayload* lpCacheEvent = reinterpret_cast<const GuiEventCachePayload*>(lpEvent);

        CGS_ASSERT(lpCacheEvent->mpGuiCache != 0, "Invalid cache in HandleGuiCacheEvent::Update");

        if (mpGuiCache == 0)
        {
            mpGuiCache = lpCacheEvent->mpGuiCache;
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

            mpIconManager = mpGuiCache->GetMapIconManager();
            CGS_ASSERT(mpIconManager != 0, "mpIconManager");
        }
    }

    // ------------------------------------------------------------ SetState
    void GuiNetworkRouteInfo::SetState(EState leState)
    {
        const char* lpacState;

        switch (leState)
        {
        case E_STATE_VISIBLE:
            meState  = E_STATE_VISIBLE;
            lpacState = KPAC_STATE_ID[E_STATE_VISIBLE];
            break;

        case E_STATE_INVISIBLE:
            meState  = E_STATE_INVISIBLE;
            lpacState = KPAC_STATE_ID[E_STATE_INVISIBLE];
            break;

        default:
            CGS_ASSERT(false, "Invalid state");
            return;
        }

        AddOutputAptViewState(KAC_APT_STATE, lpacState, false);
    }

    // ------------------------------------------------------------ SetInfo
    void GuiNetworkRouteInfo::SetInfo(s32 liCurrentRound, const GuiEventNetworkGameParams* lpCreateMatchEvent)
    {
        if (miNumComponentsLoaded != KI_NUM_COMPONENTS_TO_LOAD)
        {
            return;
        }

        CGS_ASSERT(liCurrentRound >= 0, "liCurrentRound >= 0");
        CGS_ASSERT(liCurrentRound < static_cast<s32>(GSM::KU_MAX_ONLINE_ROUNDS_IN_MODE),
                   "liCurrentRound < (int32_t) BrnGameState::GameStateModuleIO::KU_MAX_ONLINE_ROUNDS_IN_MODE");

        mEvent = lpCreateMatchEvent->maEvents[liCurrentRound];

        const s32 leGameMode = lpCreateMatchEvent->meGameMode;

        s32 liModeIndex = 0;
        for (; liModeIndex < KI_NUM_GAME_MODES; ++liModeIndex)
        {
            if (KA_GAMEMODE_TO_STRING[liModeIndex].meGameMode == leGameMode)
            {
                break;
            }
        }
        CGS_ASSERT(liModeIndex < KI_NUM_GAME_MODES, "Can't find game mode string");

        const s32* lpiMappingTable;

        switch (leGameMode)
        {
        case GSM::E_MODE_ONLINE_RACE:
            lpiMappingTable = KAI_RACE_OPTION_TO_COMPONENT_MAPPING;
            break;

        case GSM::E_MODE_ONLINE_ROAD_RAGE:
        case GSM::E_MODE_ONLINE_FUGITIVE:
        case GSM::E_MODE_ONLINE_FREE_BURN:
        case KE_MODE_ONLINE_STUNT_COOP:
            lpiMappingTable = KAI_ROAD_RAGE_OPTION_TO_COMPONENT_MAPPING;
            break;

        case GSM::E_MODE_ONLINE_BURNING_HOME_RUN:
            lpiMappingTable = KAI_BHR_OPTION_TO_COMPONENT_MAPPING;
            break;

        case GSM::E_MODE_ONLINE_FREE_BURN_LOBBY:
            return;

        default:
            CGS_ASSERT(false, "Invalid game mode!\n");
            lpiMappingTable = KAI_RACE_OPTION_TO_COMPONENT_MAPPING;
            break;
        }

        ShowAppropiateOptions(lpiMappingTable);

        // ---- the "round N of M" text ---------------------------------------------------
        char laacFormatBuffer[KI_FORMAT_BUFFER_COUNT][KI_FORMAT_BUFFER_LENGTH];

        if (leGameMode != GSM::E_MODE_ONLINE_ROAD_RAGE ||
            mpGuiCache->GetGameMode() == GSM::E_MODE_ONLINE_ROAD_RAGE)
        {
            CgsCore::SPrintf(laacFormatBuffer[0], KI_FORMAT_BUFFER_LENGTH, "%d", liCurrentRound + 1);
            CgsCore::SPrintf(laacFormatBuffer[1], KI_FORMAT_BUFFER_LENGTH, "%d", lpCreateMatchEvent->miNumRounds);

            mpStateInterface->GetLanguageManager()->FormatAndAddText(
                KAC_NUM_ROUNDS_STRING_ID + 1, KAC_NUM_ROUNDS_FORMAT_STRING_ID,
                CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 2,
                laacFormatBuffer[0], CgsLanguage::LanguageManager::E_FORMAT_INTEGER,
                laacFormatBuffer[1], CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
        }
        else
        {
            // The road-rage form also carries the following round's number.
            CgsCore::SPrintf(laacFormatBuffer[0], KI_FORMAT_BUFFER_LENGTH, "%d", liCurrentRound + 1);
            CgsCore::SPrintf(laacFormatBuffer[1], KI_FORMAT_BUFFER_LENGTH, "%d", liCurrentRound + 2);
            CgsCore::SPrintf(laacFormatBuffer[2], KI_FORMAT_BUFFER_LENGTH, "%d", lpCreateMatchEvent->miNumRounds);

            mpStateInterface->GetLanguageManager()->FormatAndAddText(
                KAC_NUM_ROUNDS_STRING_ID + 1, KAC_NUM_ROUNDS_RR_FORMAT_STRING_ID,
                CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 3,
                laacFormatBuffer[0], CgsLanguage::LanguageManager::E_FORMAT_INTEGER,
                laacFormatBuffer[1], CgsLanguage::LanguageManager::E_FORMAT_INTEGER,
                laacFormatBuffer[2], CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
        }

        // ---- the option rows -------------------------------------------------------------
        SetOptionText(E_OPTION_COMPONENT_VEHICLE_CLASS, lpiMappingTable,
                      KPAC_VEHICLE_CLASS_STRING_IDS[lpCreateMatchEvent->miVehicleClass]);

        SetOptionText(E_OPTION_COMPONENT_BOOST_TYPE, lpiMappingTable,
                      KPAC_BOOST_TYPE_STRING_IDS[lpCreateMatchEvent->meBoostType]);

        if (leGameMode == GSM::E_MODE_ONLINE_BURNING_HOME_RUN || leGameMode == GSM::E_MODE_ONLINE_ROAD_RAGE)
        {
            SetOptionText(E_OPTION_COMPONENT_INFINITE_BOOST, lpiMappingTable,
                          lpCreateMatchEvent->mbInfiniteBoost ? KPAC_RED_TEAM_INFINITE_BOOST_STRING_IDS[0]
                                                              : KPAC_RED_TEAM_INFINITE_BOOST_STRING_IDS[1]);
        }

        SetOptionText(E_OPTION_COMPONENT_TRAFFIC, lpiMappingTable,
                      lpCreateMatchEvent->mbTrafficOn ? KPAC_TRAFFIC_STRING_IDS[0] : KPAC_TRAFFIC_STRING_IDS[1]);

        // The console reads mbTrafficCheckingOn here but selects the "off" string either way.
        SetOptionText(E_OPTION_COMPONENT_TRAFFIC_CHECKING, lpiMappingTable, KPAC_TRAFFIC_CHECKING_STRING_IDS[1]);

        if (leGameMode == GSM::E_MODE_ONLINE_BURNING_HOME_RUN)
        {
            SetOptionText(E_OPTION_COMPONENT_CRASH_LIMIT, lpiMappingTable,
                          KPAC_CRASH_LIMIT_STRING_IDS[lpCreateMatchEvent->miNumRunnerCrashes - 1]);
        }

        if (leGameMode == GSM::E_MODE_ONLINE_BURNING_HOME_RUN)
        {
            SetOptionText(E_OPTION_COMPONENT_TIME_LIMIT, lpiMappingTable,
                          KPAC_TIME_LIMIT_STRING_IDS[lpCreateMatchEvent->miTimeLimit]);
        }

        SetOptionText(E_OPTION_COMPONENT_GAMEMODE, lpiMappingTable, KA_GAMEMODE_TO_STRING[liModeIndex].mpcStringID);
        SetOptionText(E_OPTION_COMPONENT_NUM_ROUNDS, lpiMappingTable, KAC_NUM_ROUNDS_STRING_ID);

        mbReceivedInfo = true;

        // ---- the route map: only for a real event with a start junction ------------------
        if (mpGuiCache != 0 &&
            !GSM::IsOnlineFreeBurnLobby(static_cast<GSM::EGameModeType>(leGameMode)) &&
            IsLightTriggerIdValid(mEvent.GetTrafficLightTriggerId()))
        {
            mbShowingMap = true;
            meSelectedCounty = static_cast<BrnWorld::ECounty>(
                mpGuiCache->GetPresetEventDisplayInfo(mEvent.GetTrafficLightTriggerId())->muCounty);
            SetupMap();
        }
        else
        {
            mbShowingMap = false;
        }
    }

    // ------------------------------------------------------------ SetOptionText
    void GuiNetworkRouteInfo::SetOptionText(EOptionComponent leOption, const s32* lpiMappingTable,
                                            const char* lpacText)
    {
        if (lpiMappingTable[leOption] > -1)
        {
            maHeading[lpiMappingTable[leOption]].SetText(KPAC_HEADING_STRING_IDS[leOption]);
            maValue[lpiMappingTable[leOption]].SetText(lpacText);
        }
    }

    // ------------------------------------------------------------ AppendExpectedAptComponent
    void GuiNetworkRouteInfo::AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache)
    {
        lpGuiCache->AppendExpectedAptComponent(leFlow, GetName());

        for (s32 liComponent = 0; liComponent < E_OPTION_COMPONENT_COUNT; ++liComponent)
        {
            lpGuiCache->AppendExpectedAptComponent(leFlow, maHeading[liComponent].GetName());
            lpGuiCache->AppendExpectedAptComponent(leFlow, maValue[liComponent].GetName());
            lpGuiCache->AppendExpectedAptComponent(leFlow, maOptionsAnimator[liComponent].GetName());
        }
    }

    // ------------------------------------------------------------ SetupMap
    void GuiNetworkRouteInfo::SetupMap()
    {
        CGS_ASSERT(mpIconManager != 0, "mpIconManager");

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "MAPICONMANAGER: GuiNetworkRouteInfo is calling SetOwnerParameters with OwnerID "
                                       << static_cast<s32>(mIconManagerOwnerId) << ".\n";
        }

        mIconManagerOwnerId = mpIconManager->SetOwnerParameters(
            mpStateInterface, macSatNavIconBaseName, KAC_ONLINEROUTEMAP_NUMICONS, mIconManagerOwnerId,
            false, false, false, GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT, KAC_ICON_PARENT_NAME);

        mpIconManager->mbRotateSatNav       = false;
        mpIconManager->meIconSizeMode       = MapIconManager::E_ICONSIZE_LARGE;
        mpIconManager->mbShowingOnlineRoute = true;
        mpIconManager->SetIconsVisible(true);

        mCrashNavBorough.SetCurrentBorough(meSelectedCounty);
        SetActiveLandmarks();
        mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_CUSTOM, CalculateZoomFactor(), false);
    }

    // ------------------------------------------------------------ UpdateIconManager
    void GuiNetworkRouteInfo::UpdateIconManager()
    {
        if (mpGuiCache != 0 && mpIconManager != 0 && mbReceivedInfo && mbShowingMap)
        {
            mpIconManager->meIconFilterMode        = MapIconManager::E_ICONFILTER_ALL;
            mpIconManager->mbIsDisplayingEventInfo = false;
            mpIconManager->miSelectedCheckpoint    = 0;
            mpIconManager->mSelectedLightTriggerID = mEvent.GetTrafficLightTriggerId();
            mpIconManager->miNumUsedIcons          = 0;
            mpIconManager->Update();
        }
    }

    // ------------------------------------------------------------ CalculateZoomFactor
    // Fit the round's start junction and every landmark of the event into the map display
    // rectangle, and aim the map at the middle of that box.
    f32 GuiNetworkRouteInfo::CalculateZoomFactor()
    {
        const SatNavEventDisplayInfo* const lpEventStart =
            mpGuiCache->GetPresetEventDisplayInfo(mEvent.GetTrafficLightTriggerId());
        CGS_ASSERT(lpEventStart != 0, "lpEventStart");

        Vector2 lv2Min = MapTransform::Flatten(lpEventStart->mv3Position);
        Vector2 lv2Max = lv2Min;

        for (s32 liLandmark = 0; liLandmark < mEvent.GetNumLandmarks(); ++liLandmark)
        {
            GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
            mpGuiCache->GetLandmarkInfoFromIndex(mEvent.GetLandmark(liLandmark), &lLandmarkInfo);

            const Vector4& lv4Position = lLandmarkInfo.GetPositionLane();
            const Vector2 lv2Point = { lv4Position.x, lv4Position.z, 0.0f, 0.0f };

            lv2Min.x = (lv2Min.x < lv2Point.x) ? lv2Min.x : lv2Point.x;   // vminfp
            lv2Min.y = (lv2Min.y < lv2Point.y) ? lv2Min.y : lv2Point.y;
            lv2Max.x = (lv2Max.x > lv2Point.x) ? lv2Max.x : lv2Point.x;   // vmaxfp
            lv2Max.y = (lv2Max.y > lv2Point.y) ? lv2Max.y : lv2Point.y;
        }

        const Vector2 lv2Centre = { (lv2Max.x + lv2Min.x) / 2.0f,
                                    (lv2Max.y + lv2Min.y) / 2.0f,
                                    (lv2Max.z + lv2Min.z) / 2.0f,
                                    (lv2Max.w + lv2Min.w) / 2.0f };
        mMainMapComponent.SetDesiredWorldCentre(lv2Centre);

        return MapTransform::CalculateZoomFactor(lv2Min, lv2Max, K_MAP_DISPLAY_RECT, KF_MAP_BASE_ASPECT_RATIO);
    }

    // ------------------------------------------------------------ SetActiveLandmarks
    void GuiNetworkRouteInfo::SetActiveLandmarks()
    {
        GuiEventSetActiveLandmarks lActiveLandmarks;
        lActiveLandmarks.muNumLandmarks = static_cast<u32>(mEvent.GetNumLandmarks());

        for (s32 liLandmark = 0; liLandmark < mEvent.GetNumLandmarks(); ++liLandmark)
        {
            lActiveLandmarks.maLandmarkIndices[liLandmark] = mEvent.GetLandmark(liLandmark);
        }

        mpGuiCache->HandleSetActiveLandmarksEvent(&lActiveLandmarks);
    }

    // ------------------------------------------------------------ ShowAppropiateOptions
    // Show each option row the mode's mapping table places, hide the rest.
    void GuiNetworkRouteInfo::ShowAppropiateOptions(const s32* lpiMappingTable)
    {
        CGS_ASSERT(lpiMappingTable != 0, "lpiMappingTable");

        for (s32 liComponent = 0; liComponent < E_OPTION_COMPONENT_COUNT; ++liComponent)
        {
            s32 liOption = 0;
            for (; liOption < E_OPTION_COMPONENT_COUNT; ++liOption)
            {
                if (lpiMappingTable[liOption] == liComponent)
                {
                    break;
                }
            }

            maOptionsAnimator[liComponent].Run(
                (liOption == E_OPTION_COMPONENT_COUNT) ? KPAC_OPTION_STATE_ID[1] : KPAC_OPTION_STATE_ID[0]);
        }
    }
}
