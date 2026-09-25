#pragma once

// ===================================================================================
// BrnGui::GuiNetworkRouteInfo  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Screen/Components/BrnGuiNetworkRouteInfo.h
//
// The online "route info" panel: a small map of the round's route (the main map
// component, the icon manager's route icons and the county outline) beside nine
// heading/value rows describing the match options (mode, traffic, vehicle class, boost,
// rounds, ...). Composed by value inside the online screen states (OnlineGameOptions,
// OnlineGameOptionsSummary, OnlineLoading, OnlineGameRoomPlayerInfo).
//
// Members are real named sub-objects in declaration order; every access is by name.
// ===================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                               // Vector2
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"       // CgsGui::GuiComponent (base)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // SpecificGameModeEventInterface::Event (by value)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // BrnGui::GuiFlow
#include "GameSource/Gui/BrnGuiTextField.h"                               // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"  // BrnGui::AnimationComponent (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnCrashNavBorough.h"     // BrnGui::CrashNavBorough (by value)
#include "GameSource/Gui/SatNav/BrnMainMap.h"                             // BrnGui::MainMapComponent (by value)
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"                      // BrnGui::MapIconManager::OwnerId
#include "SharedClasses/World/BrnWorldRegion.h"                           // BrnWorld::ECounty

// The state in-queue Update drains (pointer-only; InputBuffer is the global event-queue
// namespace).
namespace InputBuffer { class GuiEventQueue; }
namespace CgsModule { struct Event; }

namespace BrnGui
{
    class  GuiCache;                    // pointer member
    struct GuiEventNetworkGameParams;   // SetInfo argument (home GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h)

    struct GuiNetworkRouteInfo : public CgsGui::GuiComponent
    {
        enum EState
        {
            E_STATE_VISIBLE   = 0,
            E_STATE_INVISIBLE = 1,
            E_STATE_COUNT     = 2,
        };

        enum EOptionComponent
        {
            E_OPTION_COMPONENT_GAMEMODE         = 0,
            E_OPTION_COMPONENT_TRAFFIC          = 1,
            E_OPTION_COMPONENT_VEHICLE_CLASS    = 2,
            E_OPTION_COMPONENT_INFINITE_BOOST   = 3,
            E_OPTION_COMPONENT_NUM_ROUNDS       = 4,
            E_OPTION_COMPONENT_TRAFFIC_CHECKING = 5,
            E_OPTION_COMPONENT_BOOST_TYPE       = 6,
            E_OPTION_COMPONENT_CRASH_LIMIT      = 7,
            E_OPTION_COMPONENT_TIME_LIMIT       = 8,
            E_OPTION_COMPONENT_COUNT            = 9,
        };

        // The loaded-count value SetInfo / Update / IsComponentLoaded gate on. The owning
        // screens store and compare it directly, so it stays reachable to them.
        static const s32 KI_NUM_COMPONENTS_TO_LOAD = 11;

        GuiNetworkRouteInfo();

        // The map type rides as its underlying integer (the composing screens pass the
        // literal); it is handed to the embedded map component as its GuiEventRenderMainMap
        // map type.
        void Construct(const char* lpacName, s32 leMapType,
                       CgsGui::StateInterface* lpStateInterface, const char* lpacParentName);
        void Destruct();
        void SetState(EState leState);
        void AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache);
        bool IsComponentLoaded() const { return miNumComponentsLoaded == KI_NUM_COMPONENTS_TO_LOAD; }
        void Update(InputBuffer::GuiEventQueue* lpInGuiEventQueue);
        void SetInfo(s32 liCurrentRound, const GuiEventNetworkGameParams* lpCreateMatchEvent);

    private:
        void  SetupMap();
        void  SetOptionText(EOptionComponent leOption, const s32* lpiMappingTable, const char* lpacText);
        void  UpdateIconManager();
        void  ShowAppropiateOptions(const s32* lpiMappingTable);
        void  SetActiveLandmarks();
        f32   CalculateZoomFactor();
        // The event-64 cache record (the GuiCache pointer the GUI module posts each frame).
        void  HandleGuiCacheEvent(const CgsModule::Event* lpEvent);

        // ---- option-row layout per game mode (row index per EOptionComponent; -1 == the
        //      option is not shown for that mode) -------------------------------------
        static const s32 KAI_RACE_OPTION_TO_COMPONENT_MAPPING[E_OPTION_COMPONENT_COUNT];
        static const s32 KAI_ROAD_RAGE_OPTION_TO_COMPONENT_MAPPING[E_OPTION_COMPONENT_COUNT];
        static const s32 KAI_BHR_OPTION_TO_COMPONENT_MAPPING[E_OPTION_COMPONENT_COUNT];

        // ---- apt component names ------------------------------------------------------
        static const char* const KPAC_OPTIONS_ANIMATOR_COMPONENT[E_OPTION_COMPONENT_COUNT];
        static const char* const KPAC_HEADING_COMPONENT[E_OPTION_COMPONENT_COUNT];
        static const char* const KPAC_VALUE_COMPONENT[E_OPTION_COMPONENT_COUNT];
        static const char KAC_APT_STATE[10];
        static const char* const KPAC_STATE_ID[E_STATE_COUNT];
        static const char* const KPAC_OPTION_STATE_ID[2];

        // ---- option text ids ----------------------------------------------------------
        static const char KAC_NUM_ROUNDS_STRING_ID[12];
        static const char KAC_NUM_ROUNDS_FORMAT_STRING_ID[27];
        static const char KAC_NUM_ROUNDS_RR_FORMAT_STRING_ID[30];
        static const char* const KPAC_HEADING_STRING_IDS[E_OPTION_COMPONENT_COUNT];
        static const char* const KPAC_VEHICLE_CLASS_STRING_IDS[10];
        static const char* const KPAC_RED_TEAM_INFINITE_BOOST_STRING_IDS[2];
        static const char* const KPAC_TRAFFIC_STRING_IDS[2];
        static const char* const KPAC_TRAFFIC_CHECKING_STRING_IDS[2];
        static const char* const KPAC_BOOST_TYPE_STRING_IDS[6];
        static const char* const KPAC_CRASH_LIMIT_STRING_IDS[6];
        static const char* const KPAC_TIME_LIMIT_STRING_IDS[3];

        // ---- the route map ------------------------------------------------------------
        static const Vector2 K_MAP_DISPLAY_RECT;
        static const char macSatNavIconBaseName[11];
        static const s32  KAC_ONLINEROUTEMAP_NUMICONS = 50;
        static const char macCrashNavBoroughName[11];

        // ---- data members -------------------------------------------------------------
        AnimationComponent        maOptionsAnimator[E_OPTION_COMPONENT_COUNT];
        TextField                 maHeading[E_OPTION_COMPONENT_COUNT];
        TextField                 maValue[E_OPTION_COMPONENT_COUNT];
        MapIconManager*           mpIconManager;
        MapIconManager::OwnerId   mIconManagerOwnerId;
        MainMapComponent          mMainMapComponent;
        Vector2                   mv2WorldCenterPoint;
        CrashNavBorough           mCrashNavBorough;
        BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface::Event mEvent;
        GuiCache*                 mpGuiCache;

    public:
        // The owning screens write the loaded count and read the visibility state directly.
        s32                       miNumComponentsLoaded;

    private:
        BrnWorld::ECounty         meSelectedCounty;

    public:
        EState                    meState;

    private:
        bool                      mbReceivedInfo;
        bool                      mbShowingMap;
    };
}
