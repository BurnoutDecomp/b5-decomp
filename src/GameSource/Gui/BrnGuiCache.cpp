#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Gui/BrnGuiRaceCarInfoEvent.h"      // [H3b] GuiRaceCarInfoEvent (207)
#include "GameSource/Gui/BrnGuiShared.h"               // BrnGui::EGuiResourceId + gGuiResourceIdentifier (this TU defines the table)
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"   // BrnGui::OptionsDataProfile (types the opaque +0xB878 reservation)
#include "GameShared/GameClasses/Containers/CgsHash.h" // CgsContainers::CgsHash::CalculateHash (AppendExpectedAptComponent name entry)
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG] the satnav-diag one-shots
// (BrnGameStateSharedIO.h must NOT be included here: its real BrnGameState /
// BrnNetwork types clash with BrnGuiOptionsDataProfile.h's compile-only slices.
// GetNumEventStarts, which needs the real SetUpAllEventStartsInterface, lives in
// its own TU -- BrnGuiCache_GetNumEventStarts.cpp.)

#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptObjectController.h"
#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h"
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"   // CgsGui::GuiEventTimeInfo (the per-frame time latch)
// ([E1] GuiEventCurrentStatus(492) / GuiEventScoreUpdate(424) / GuiAttackScoreUpdate(428)
// live in BrnGuiEventTypeDefs.h, already reachable through BrnGuiCache.h. They were MOVED
// there out of BrnGuiDemangledEventTypes.h precisely because this TU cannot include that
// header: it and BrnGuiOptionsDataProfile.h both define BrnGui::GuiEventAudioTraxUpdate.)
// (GuiEventChangeDistrict now lives in BrnGuiEventTypeDefs.h, already included via BrnGuiCache.h)
#include "SharedClasses/World/BrnWorldRegion.h"        // BrnWorld::WorldRegion::DistrictToCounty (Construct's marker seed)

#include <cstring>   // std::memset (the ctor's zero-init of the unmodelled interior) / std::strlen

// Reconstructed from BURNOUT_X360_ARTIST.XEX. StateLoadingHelper tracks how many of
// its watched resources are pending an unload. Increment/Decrement adjust the count
// and then run a debug consistency check: the count must equal the number of resources
// actually in a pending-unload state (LOAD_CANCELLED / UNLOAD_REQUESTED / UNLOADING).
// The consistency loop feeds CGS_ASSERT, which is a no-op in this build (CgsAssert.h),
// matching the project convention for the X360 assert machinery.

namespace BrnGui
{
        // ARTIST off_82F278E0, read directly from BURNOUT_X360_ARTIST.XEX.i64.
        // StateLoadingHelper::Update indexes this 237-entry table by resource id.
        //
        // NOT a file-static: the X360 references off_82F278E0 from FOUR TUs -- this one
        // (the resource pump) plus PhotoBoothComponent::OnLoad @0x8243CD68,
        // LicenseComponent::OnLoad @0x82440AC0 and LicenseComponent::Update @0x8243C0B8,
        // each of which turns a resource id into the apt movie name it plays. It is
        // therefore a shared global with its declaration in BrnGuiShared.h
        // (DWARF name: gGuiResourceIdentifier, BrnGuiShared.h:473) and its single
        // definition here, in the TU that owns the resource pump.
        const char* const gGuiResourceIdentifier[E_GUI_RESOURCEID_NUM] =
        {
            "<LANG DATABASE>", "boostbarmask", "boostfirebody", "boostbarbackground",
            "boostbarbackgroundendcap", "boostfireover", "boostbarendcap", "boostbarendglow",
            "boostearnflame", "boostbarboosting", "boostgrowfireball", "boostbarmultiplier",
            "boostbarglow", "<SCREEN FSM>", "<HUD FSM>", "<OVERLAY FSM>",
            "BRNEVENTFSM", "WesternB5Header_70", "WesternB5Body_35", "WesternB5DotMat_35",
            "DFHEIC", "JAMA", "B5ComponentUnity", "B5NorthIndicatorComponent",
            "B5CompassComponent", "SatNavDistance", "SatNavStatic", "CountdownIcon",
            "ChevronIcon", "TextField", "ColourField", "B5MultiTextField",
            "Timer", "B5RoadRuleComponent", "B5MenuToggle", "B5MenuItemColourPicker",
            "B5MenuItem", "B5HudMessage", "B5CrashedHudMessages", "B5PreRaceMessageComponent",
            "B5CustomComponentTexture", "B5MapCursor", "B5HudSingleMetricComponent",
            "B5HudFractionMetricComponent", "B5ProgressBar", "B5MugShot", "RoadRuleShot", "Ticker",
            "CrashNavPanel", "CrashNavLegend", "CrashNavBorough", "B5RivalPanel",
            "B5RivalIcon", "B5RivalTable", "B5CarouselScrollBar", "B5ManufacturersIcon",
            "B5Triggers", "Toggle", "B5ScrollableSelection", "B5HelpItem",
            "BoostMessage", "B5ControllerButtons", "B5PositionIndicatorComponent", "B5HelperComponents",
            "DistrictIcon", "DistrictMarker", "B5PhotoLicenseComponent", "B5SpecialComponent",
            "B5DriversLicenseComponents", "B5ColourSelector", "B5CarsIcon", "B5MedalIcon",
            "B5GameModeLogos", "B5RaceEventInfo", "B5SatNavOverlay", "B5PositionTableComponent",
            "B5FriendList", "B5FriendListChangeIcon", "B5EATraxInGameComponent", "B5OnlineInviteComponent",
            "B5SaveIconComponent", "B5PaybackComponent", "PlayerStatsBar", "RoadIconComponent",
            "B5RoadSigns", "B5RoadRulerIcon", "B5EATraxMenuComponent", "B5ShowTimeBar",
            "B5ShowtimeComponents", "B5JunctionInfoComponent", "B5VersionTextComponent",
            "B5PhotoBoothComponent", "B5PhotoBoothComponentDMV", "B5PhotoBoothCptDMVUpgrade",
            "B5OnlineCarSelectComponents", "B5SkipCrashPrompt", "B5AchievementIcons",
            "B5LicenseRank0", "B5LicenseRank1", "B5LicenseRank2", "B5LicenseRank3",
            "B5LicenseRank4", "B5LicenseRank5", "B5LicenseElite", "B5LicenseEliteFinal",
            "DestN", "DestNW", "DestW", "DestSW", "DestS", "DestSE", "DestE", "DestNE",
            "LargeRoadRageIcon", "LargeFreestyleIcon", "LargeBurningRouteIcon", "LargeRaceIconPost",
            "LargeMarkedManIconPost", "LargeRoadRageIconPost", "LargeFreestyleIconPost",
            "LargeBurningRouteIconPost", "CAR_PUSCC01", "CAR_PUSCC01", "CAR_PUSCC01", "CAR_PUSCC01",
            "main", "BrnBootPreload", "SaveLoadComponent", "Title_Screen02", "EA_HD_Logo",
            "EA_Criterion_Logo", "CrashNavTitleBar", "BrnCrashNavMapMain", "BrnCrashNavMapEvent",
            "BrnCrashNavNews", "BrnCrashNavScrbdMenu", "BrnCrashNavDriversLicense", "BrnCrashNavStats",
            "BrnCrashNavRivals", "BrnCrashNavSettings", "BrnCrashNavProfile", "BrnCrashNavOptions",
            "BrnCrashNavAccountManagement", "BrnCrashNavColourCalibrate", "BrnCrashNavDriverDetails",
            "BrnCrashNavTrax", "BrnCrashNavAchievements", "BrnIntro", "BrnCarSelectUnlock",
            "BrnCarSelectMain", "BrnCarSelectLivery", "BrnCarSelectOnlineEnd", "BrnDriversLicenseScreens",
            "BrnGeneralPause", "Credits", "ReplaysClips", "ReplaysClipsOnline", "ReplaysOptions",
            "ReplaysIntro", "ReplaysMain", "ReplaysOutro", "ReplaysLoading", "ReplaysInfo",
            "ReplaysCredits", "ON_IMG_GAL", "ON_CONN", "ON_DISC", "ON_GR_PI", "ON_LOAD",
            "ON_PAUSE", "ON_POST", "ON_YOU_WIN", "ON_MAIN", "ON_QMCMCM", "ON_QWKM",
            "ON_CUSTM", "ON_CREA", "ON_CRSUM", "ON_ROUT", "ON_TEAMS", "ON_RIVAL",
            "ON_NEWS", "ON_SCORB", "ON_CAR", "ON_MARK_MAN", "ON_PRE_EVENT", "ON_STATS",
            "ON_CHAL", "ON_VWOPT", "ON_BLACK", "B5NetworkPlayerStats", "B5NetworkRouteInfo",
            "B5RaceHud", "B5CrashedHud", "B5CrashedStuntHud", "B5IdleHud", "FLAPTHUD", "Overlays",
            "B5AlwaysAvailableContainer", "SatNavMap", "B5SatNavComponent", "SatNavMask",
            "PreRaceBackgroundMask", "MainMapBackgroundMask", "Icons_EventIcon_NotAttempted_Anim",
            "Icons_EventIcon_Completed_Anim", "Icons_CrashNavIcon", "BrnPreRaceFlyByRace",
            "BrnPreRaceFlyByFaceOff", "BrnPreRaceFlyByOfflShowtime", "BrnPreRaceFlyByRoadRage",
            "BrnPreRaceFlyByPursuit", "BrnPreRaceFlyByBurningRoute", "BrnPreRaceFlyByEliminator",
            "BrnPreRaceFlyByStuntAttack", "BrnPreRaceFlyByMarkedMan", "BrnPreRaceFlyByTrafficAttack",
            "Results", "BrnUpgrade", "DriversLicense", "RivalryUpdate", "BrnCompletedGame",
            "goldCarUnlock", "platinumCarUnlock", "BrnRivalShutdown", "BrnTrophyCarUnlock",
            "OnlineResults", "OnlineScalpsAndAwards", "pfxhooks", "<COLOURCUBE1>",
            "<COLOURCUBE2>", "<COLOURCUBE3>", "<COLOURCUBE4>", "<COLOURCUBE5>",
            "<COLOURCUBE6>", "RoadSigns_0", "Headtif"
        };
        static_assert(sizeof(gGuiResourceIdentifier) / sizeof(gGuiResourceIdentifier[0]) == 237,
                      "ARTIST GUI resource-name table must contain 237 entries");

    namespace
    {
        u32 CountRealPendingUnloads(const StateLoadingHelper::ResourceInfo* lpResources, u32 luCount)
        {
            u32 luRealPending = 0;
            for (u32 i = 0; i < luCount; ++i)
            {
                StateLoadingHelper::EResourceState leState = lpResources[i].meState;
                if (leState == StateLoadingHelper::E_STATE_LOAD_CANCELLED
                    || leState == StateLoadingHelper::E_STATE_UNLOAD_REQUESTED
                    || leState == StateLoadingHelper::E_STATE_UNLOADING)
                {
                    ++luRealPending;
                }
            }
            return luRealPending;
        }
    }

    // @ 0x827E05B8 -- the cache constructor: a long per-field init list against the X360
    // layout (sentinels to -1, counters/times to 0). The PC model names only the members
    // the recovered accessors touch (the rest is explicit padding), so the faithful PC
    // form zero-fills the aggregate (the unmodelled fields' X360 init value is 0) and
    // applies the named non-zero inits: the mEvents array ctor sentinel at +40532 is the
    // one modelled member the X360 sets to -1 (the other -1 sentinels at +3800/+30560/
    // +40664/+40704/+41936/+42976 fall inside the reserved padding spans).
    GuiCache::GuiCache()
    {
        std::memset(this, 0, sizeof(GuiCache));
        mEventsCtorSentinel = -1;
        // NOTE: the X360 ctor's -1 store at +3800 lands on the (now modelled)
        // mStateLoadingHelper.maRequestDirtyList count word -- the Array's
        // pre-Construct sentinel. The zero-fill above leaves it 0 (constructed-empty)
        // instead: the member is private to the helper, and the dirty list is only
        // consumed after the helper's own lifecycle has run on the console flow, so
        // the observable behaviour (empty list at first use) is identical.
    }

    // @ 0x824FD978 (per-slot body inlined there) -- one watched slot to its reset state:
    // UNLOADED, no live resource, and the type back to the 23 wildcard ("no type
    // recorded yet" -- the value EnsureResourceIsLoaded/UnloadResource admit alongside
    // an exact match in their consistency asserts).
    void StateLoadingHelper::ResourceInfo::Construct()
    {
        meState    = E_STATE_UNLOADED;
        meType     = static_cast<CgsGui::ResourceRequestTypes>(23);
        mpResource = 0;
    }

    // @ 0x824FD978 -- reset the whole watcher: the 237 resource slots ({0, 23, NULL}
    // stores in the X360 loop), the control/pending counters, the dirty list, the two
    // load-request queues, and the three flow-layer expected-component blocks.
    void StateLoadingHelper::Construct()
    {
        muControlledComponentCount = 0;
        muPendingUnloadCount       = 0;

        for (u32 lu = 0; lu < KU_MAX_RESOURCES_TO_WATCH; ++lu)
            maResources[lu].Construct();

        maRequestDirtyList.Construct();
        miCurrentLoadRequestQueue = 0;
        for (s32 li = 0; li < KI_NUM_LOAD_REQUEST_QUEUES; ++li)
            mLoadRequestQueues[li].Construct();

        for (u32 luFlow = 0; luFlow < 3; ++luFlow)
        {
            ComponentsToWatch& lrWatch = maComponentsToWatch[luFlow];
            lrWatch.muNumberOfComponentsToWatch = 0;
            for (u32 lu = 0; lu < ComponentsToWatch::KU_MAX_COMPONENTS_TO_WATCH; ++lu)
            {
                lrWatch.mauComponentsToWatchIds[lu] = 0;
                lrWatch.mabComponentsLoaded[lu]     = false;
            }
        }
    }

    // @ 0x82505860 -- the cache Construct. The X360 form takes the tracker + system-user
    // -profile pointers (asserted non-null, stored at +16468/+16472) and inits the far
    // member block before running the embedded watcher's Construct. The tracker/profile
    // owners are un-reconstructed on PC, so this slice performs the watcher reset (the
    // part the boot path consumes -- it seeds the 23 wildcard types the resource state
    // machine's consistency asserts key on); the pointer stores land with their owners.
    void GuiCache::Construct()
    {
        mStateLoadingHelper.Construct();
        // X360 0x82505860 mid-body: BrnGui::OptionsDataProfile::Construct(this + 47224)
        // -- default the embedded player-options profile (brightness/contrast 50, the
        // volume/trax defaults) so its range-asserted getters hold before any save
        // overwrites it (ScreenLoading's ApplyOptionsDataProfileSettings reads it on
        // the world-load hand-off).
        GetOptionsDataProfile()->Construct();

        // X360 0x825060EC..0x82506110 (immediately after that call): seed the live DLC1
        // options block that sits directly behind the options profile at cache+76776 --
        // version 1, reserved 0, all eight flags clear. ReadProfileData @0x824FF298 copies
        // those four words into the stored image, so this is what lets a FIRST boot (no
        // save present) pass ProfileManager::ValidateProfiles' DLC1-options check instead
        // of logging "Options Data Profile version mismatch, expected 1, got 0".
        mOptionsDataProfileDLC1.Construct();

        // ⛔ FLAG GameState stand-in (2026-08-02, car-select wave).
        // CONSOLE CHAIN: meCarSelectType is latched by RecEvent's `case 77` (see the switch
        // below) from a 4-byte GAME-STATE event that carries the running car-select flavour;
        // the game-state module raises it when CarSelectManager::StartCarSelectState
        // @0x823872D0 enters E_STATE_CAR_SELECT.
        // WHY A STAND-IN: that action -> GUI-event bridge does not exist on this build
        // (BrnGameModule's GameState bridge is still a placeholder -- see the event-137 /
        // event-350 stand-ins in BrnGameModule.cpp), so the field stayed at
        // E_CAR_SELECT_TYPE_NONE (0) and the console's own
        // GetCurrentCarSelectType() assert -- "meCarSelectType >
        // GsmIO::E_CAR_SELECT_TYPE_NONE", inlined into CarSelectMain::OnEnter @0x824C8B34
        // and ExitCarSelection @0x824C8CF4 -- fired the moment BrnGui::CarSelectVehicle was
        // re-homed onto CarSelectMain and its OnEnter started running. A dev assert BLOCKS
        // the sim, so it would stall the very screen this wave brings up.
        // WHY JUNKYARD: E_CAR_SELECT_TYPE_JUNKYARD is the only value this build can reach --
        // the sibling E_CAR_SELECT_TYPE_ONLINE_EVENT_START needs an online lobby, and it is
        // the value ExitCarSelection's `== 1` branch tests for when it posts the junkyard
        // GuiEventActivateCarSelect. Replace this seed with the real event when the
        // GameState->Gui bridge lands (RecEvent's case 77 already consumes it).
        meCarSelectType = 1;   // BrnGameState::GameStateModuleIO::E_CAR_SELECT_TYPE_JUNKYARD

        // X360 0x82505860 mid-body (h1_dump2.txt): seed the district-marker source words --
        // district INVALID, county derived from it (== E_COUNTY_INVALID -> the "Anywhere"
        // icon), consumed byte clear. The clear consumed byte is what makes
        // FBurnMainHudState's first RUNNING frame run the marker refresh even before the
        // world posts its first region change.
        meChangeDistrictDistrict  = BrnWorld::E_DISTRICT_INVALID;
        meChangeDistrictCounty    =
            BrnWorld::WorldRegion::DistrictToCounty(BrnWorld::E_DISTRICT_INVALID);
        mu8ChangeDistrictConsumed = 0;
    }

    // @0x8250DC30 -- publish the queue selected on the previous frame, clear it,
    // rotate the two-queue index, then turn each dirty cache slot into the exact
    // resource request described by its state, indexing ARTIST's off_82F278E0 table.
    void StateLoadingHelper::Update(CgsGui::ModelIO::InputBuffer* lpInputBuffer)
    {
        CGS_ASSERT(lpInputBuffer != 0, "Invalid ModelIO input buffer");

        const s32 liQueue = miCurrentLoadRequestQueue;
        CgsGui::GuiEventQueueSmall& lrQueue = mLoadRequestQueues[liQueue];
        lpInputBuffer->GetLoadRequests()->Append(lrQueue);
        lrQueue.Clear();
        miCurrentLoadRequestQueue = (liQueue + 1) % KI_NUM_LOAD_REQUEST_QUEUES;

        const u32 luDirtyCount = maRequestDirtyList.GetLength();
        for (u32 luDirty = 0; luDirty < luDirtyCount; ++luDirty)
        {
            const u32 luResourceId = maRequestDirtyList.GetItem(luDirty);
            ResourceInfo& lrInfo = maResources[luResourceId];

            const char* lpacResourceName = gGuiResourceIdentifier[luResourceId];

            CgsGui::GuiEventLoadRequest lRequest;
            if (lrInfo.meState == E_STATE_LOAD_REQUESTED)
            {
                lRequest.Construct(lrInfo.meType, CgsGui::E_GUI_RESOURCEREQUEST_LOAD,
                                   lpacResourceName, luResourceId);
                lrInfo.meState = E_STATE_LOADING;
            }
            else if (lrInfo.meState == E_STATE_UNLOAD_REQUESTED)
            {
                lRequest.Construct(lrInfo.meType, CgsGui::E_GUI_RESOURCEREQUEST_UNLOAD,
                                   lpacResourceName, luResourceId);
                lrInfo.meState = E_STATE_UNLOADING;
            }
            else
            {
                continue;
            }

            lrQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRequest), 39,
                             static_cast<s32>(sizeof(lRequest)));
        }
        maRequestDirtyList.Clear();
    }

    // @0x8250DD80 -- the observable resource portion of GuiCache::Update.
    void GuiCache::Update(CgsGui::ModelIO::InputBuffer* lpInputBuffer)
    {
        mStateLoadingHelper.Update(lpInputBuffer);
    }

    // @0x824FE3D0 -- direct resource-module loads that bypassed the cache are only
    // warned about in ARTIST (state UNLOADED). Cached LOADING transitions to LOADED;
    // a load which completed after cancellation is immediately scheduled to unload.
    void StateLoadingHelper::OnLoadNotification(
        const CgsGui::GuiEventLoadNotification* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid load event sent to StateLoadingHelper::OnLoadNotification");
        if (lpEvent == 0)
            return;

        const u32 luResourceId = lpEvent->muLoadRequestId;
        CGS_ASSERT(luResourceId < KU_MAX_RESOURCES_TO_WATCH,
                   "Invalid resource id to mark as loaded in StateLoadingHelper::OnLoadNotification");
        if (luResourceId >= KU_MAX_RESOURCES_TO_WATCH)
            return;

        ResourceInfo& lrInfo = maResources[luResourceId];
        switch (lrInfo.meState)
        {
        case E_STATE_UNLOADED:
            break; // A direct StateInterface request bypassed the cache.

        case E_STATE_LOAD_REQUESTED:
        case E_STATE_UNLOAD_REQUESTED:
        case E_STATE_UNLOADING:
        case E_STATE_UNLOAD_CANCELLED:
            CGS_ASSERT(false, "GuiCache: bad state transition");
            break;

        case E_STATE_LOADING:
        case E_STATE_LOAD_CANCELLED:
        {
            void* lpResource = 0;
            if (lpEvent->mResourceHandle.mpResourceMemory != 0)
            {
                lpResource = *reinterpret_cast<void* const*>(
                    lpEvent->mResourceHandle.mpResourceMemory);
            }
            // FLAG PC-platform guard: the console always has the resource, so this asserts
            // non-null. On PC a MISSING (un-converted) GUI bundle -- e.g. the freeburn
            // HUD's sat-nav map/mask textures -- completes the load with a null handle
            // (see GuiResourceModule::ParseResource's matching PC guard). The watcher
            // still advances the slot to LOADED so the state's EnsureResourcesAreLoaded
            // count converges; the null resource is simply never rendered.
            CGS_ASSERT(lpResource != 0 || lpEvent->mResourceHandle.mpResourceMemory == 0,
                       "Invalid resource pointer");

            const bool lbWasCancelled = lrInfo.meState == E_STATE_LOAD_CANCELLED;
            lrInfo.meState = E_STATE_LOADED;
            lrInfo.mpResource = lpResource;
            if (lbWasCancelled)
            {
                const CgsGui::sResourceTuple lTuple = { luResourceId, lrInfo.meType };
                DecrementUnloadPending();
                UnloadResource(lTuple);
            }
            break;
        }

        case E_STATE_LOADED:
            CGS_ASSERT(false,
                       "GuiCache: StateInterface::Request() called directly on a resource already in the GuiCache");
            break;

        default:
            CGS_ASSERT(false, "StateLoadingHelper::OnLoadNotification: unknown state");
            break;
        }
    }

    // @0x824FE7E0 -- mirror completion for unloads. A cancelled unload returns to
    // UNLOADED, decrements the pending count, then re-requests the resource.
    void StateLoadingHelper::OnUnloadNotification(
        const CgsGui::GuiEventUnloadNotification* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid unload event sent to StateLoadingHelper::OnUnloadNotification");
        if (lpEvent == 0)
            return;

        const u32 luResourceId = lpEvent->muLoadRequestId;
        CGS_ASSERT(luResourceId < KU_MAX_RESOURCES_TO_WATCH,
                   "Invalid resource id to mark as loaded in StateLoadingHelper::OnUnloadNotification");
        if (luResourceId >= KU_MAX_RESOURCES_TO_WATCH)
            return;

        ResourceInfo& lrInfo = maResources[luResourceId];
        switch (lrInfo.meState)
        {
        case E_STATE_UNLOADED:
            break; // A direct StateInterface unload bypassed the cache.

        case E_STATE_LOAD_REQUESTED:
        case E_STATE_LOADING:
        case E_STATE_LOAD_CANCELLED:
        case E_STATE_UNLOAD_REQUESTED:
            CGS_ASSERT(false, "GuiCache: bad state transition");
            break;

        case E_STATE_LOADED:
            CGS_ASSERT(false,
                       "GuiCache: StateInterface::UnloadResource() called directly on a resource already in the GuiCache");
            break;

        case E_STATE_UNLOADING:
            lrInfo.meState = E_STATE_UNLOADED;
            lrInfo.mpResource = 0;
            DecrementUnloadPending();
            break;

        case E_STATE_UNLOAD_CANCELLED:
        {
            const CgsGui::sResourceTuple lTuple = { luResourceId, lrInfo.meType };
            lrInfo.meState = E_STATE_UNLOADED;
            lrInfo.mpResource = 0;
            DecrementUnloadPending();
            EnsureResourceIsLoaded(lTuple);
            break;
        }

        default:
            CGS_ASSERT(false, "StateLoadingHelper::OnUnloadNotification: unknown state");
            break;
        }
    }

    // @ 0x824FDA28 -- step one watched resource's state machine towards LOADED.
    // UNLOADED requests the load (after a null-resource check and a type-consistency
    // check against the recorded type -- 23 is the wildcard "unset" type); the three
    // pending-unload states step back to their load-side counterparts; the transit
    // states (LOAD_REQUESTED / LOADING / LOADED / UNLOAD_CANCELLED) are left alone;
    // anything else is the streamed unknown-state assert (folded static). Any state
    // change re-appends the resource id to the dirty list (erase-then-append keeps
    // it unique). Returns whether the resource is now LOADED. (The console's assert
    // streams the resource's name from the GUI resource-name table; the streamed
    // decoration is dropped per the project assert rule.)
    bool StateLoadingHelper::EnsureResourceIsLoaded(const CgsGui::sResourceTuple& lResource)
    {
        CGS_ASSERT(lResource.muId < KU_MAX_RESOURCES_TO_WATCH,
                   "lResourceTuple.muId out of bounds in StateLoadingHelper::EnsureResourceIsLoaded");

        ResourceInfo&        lrResourceInfo = maResources[lResource.muId];
        const EResourceState leOldState     = lrResourceInfo.meState;

        switch (leOldState)
        {
        case E_STATE_UNLOADED:
            CGS_ASSERT(lrResourceInfo.mpResource == 0, "lResourceInfo.mpResource==NULL");
            // 23 is the wildcard "no type recorded yet" value the X360 admits alongside
            // an exact match ("GuiCache: inconsistent type used for resource Id=" fold).
            CGS_ASSERT(lrResourceInfo.meType == 23 || lrResourceInfo.meType == lResource.meType,
                       "GuiCache: inconsistent type used for resource Id=");
            lrResourceInfo.meState = E_STATE_LOAD_REQUESTED;
            lrResourceInfo.meType  = lResource.meType;
            break;

        case E_STATE_LOAD_REQUESTED:
        case E_STATE_LOADING:
        case E_STATE_LOADED:
        case E_STATE_UNLOAD_CANCELLED:
            break;

        case E_STATE_LOAD_CANCELLED:
            lrResourceInfo.meState = E_STATE_LOADING;
            break;

        case E_STATE_UNLOAD_REQUESTED:
            lrResourceInfo.meState = E_STATE_LOADED;
            break;

        case E_STATE_UNLOADING:
            lrResourceInfo.meState = E_STATE_UNLOAD_CANCELLED;
            break;

        default:
            // cpp:258 -- streamed "StateLoadingHelper::EnsureResourceIsLoaded: unknown
            // state! Resource <name> is in state <state>"; folded static.
            CGS_ASSERT(false, "StateLoadingHelper::EnsureResourceIsLoaded: unknown state! Resource ");
            break;
        }

        if (lrResourceInfo.meState != leOldState)
        {
            maRequestDirtyList.EraseInstancesOf(lResource.muId);
            maRequestDirtyList.Append(lResource.muId);
        }

        return lrResourceInfo.meState == E_STATE_LOADED;
    }

    // @ 0x824FDD20 -- recount the really-pending unloads (LOAD_CANCELLED /
    // UNLOAD_REQUESTED / UNLOADING), assert the latched count agreed (streamed
    // expected/actual dropped per the assert rule), and re-latch it. While ANY
    // unload is pending nothing loads (returns false); otherwise step every tuple
    // and report whether all of them reached LOADED.
    bool StateLoadingHelper::EnsureResourcesAreLoaded(const CgsGui::sResourceTuple* lpResources,
                                                      u32 luCount)
    {
        CGS_ASSERT(muPendingUnloadCount <= KU_MAX_RESOURCES_TO_WATCH,
                   "muPendingUnloadCount <= KU_MAX_RESOURCES_TO_WATCH");

        const u32 luRealPending = CountRealPendingUnloads(maResources, KU_MAX_RESOURCES_TO_WATCH);
        CGS_ASSERT(muPendingUnloadCount == luRealPending,
                   "Pending Unload count does not equal real pending unload count. "
                   "This is skipable but might crash with out of memory");
        muPendingUnloadCount = luRealPending;

        if (luRealPending != 0)
        {
            return false;
        }

        bool lbAllLoaded = true;
        for (u32 luResource = 0; luResource < luCount; ++luResource)
        {
            if (!EnsureResourceIsLoaded(lpResources[luResource]))
            {
                lbAllLoaded = false;
            }
        }
        return lbAllLoaded;
    }

    // @ 0x824FDF58 -- step one watched resource's state machine towards UNLOADED
    // (the mirror of EnsureResourceIsLoaded): LOAD_REQUESTED backs out to UNLOADED,
    // LOADING becomes LOAD_CANCELLED (+pending), LOADED becomes UNLOAD_REQUESTED
    // (+pending, after the live-resource and type-consistency asserts),
    // UNLOAD_CANCELLED resumes UNLOADING; the already-unloading states are left
    // alone. Any change re-appends the id to the dirty list. (The console's asserts
    // stream the resource's name from the GUI resource-name table; streamed
    // decoration dropped per the project assert rule.)
    void StateLoadingHelper::UnloadResource(const CgsGui::sResourceTuple& lResource)
    {
        ResourceInfo&        lrResourceInfo = maResources[lResource.muId];
        const EResourceState leOldState     = lrResourceInfo.meState;

        switch (leOldState)
        {
        case E_STATE_UNLOADED:
        case E_STATE_LOAD_CANCELLED:
        case E_STATE_UNLOAD_REQUESTED:
        case E_STATE_UNLOADING:
            break;

        case E_STATE_LOAD_REQUESTED:
            lrResourceInfo.meState = E_STATE_UNLOADED;
            break;

        case E_STATE_LOADING:
            lrResourceInfo.meState = E_STATE_LOAD_CANCELLED;
            IncrementUnloadPending();
            break;

        case E_STATE_LOADED:
            CGS_ASSERT(lrResourceInfo.mpResource != 0, "lResourceInfo.mpResource!=NULL");
            CGS_ASSERT(lrResourceInfo.meType == 23 || lrResourceInfo.meType == lResource.meType,
                       "GuiCache: inconsistent type used for resource Id=");
            lrResourceInfo.meState = E_STATE_UNLOAD_REQUESTED;
            lrResourceInfo.meType  = lResource.meType;
            IncrementUnloadPending();
            break;

        case E_STATE_UNLOAD_CANCELLED:
            lrResourceInfo.meState = E_STATE_UNLOADING;
            break;

        default:
            // cpp:429 -- streamed "StateLoadingHelper::UnloadResource: unknown state!
            // Resource <name> is in state <state>"; folded static.
            CGS_ASSERT(false, "StateLoadingHelper::UnloadResource: unknown state! Resource ");
            break;
        }

        if (lrResourceInfo.meState != leOldState)
        {
            maRequestDirtyList.EraseInstancesOf(lResource.muId);
            maRequestDirtyList.Append(lResource.muId);
        }
    }

    // @ 0x824FE258 -- the unload-side mirror of EnsureResourceIsLoaded. The X360 body is
    // the bounds assert (BrnGuiCache.cpp:505, same message text as the load side but with
    // the Unloaded verb), then: if the slot is not already UNLOADED, step it with
    // UnloadResource and report NOT-yet-unloaded; otherwise report unloaded.
    bool StateLoadingHelper::EnsureResourceIsUnloaded(const CgsGui::sResourceTuple& lResource)
    {
        CGS_ASSERT(lResource.muId < KU_MAX_RESOURCES_TO_WATCH,
                   "lResourceTuple.muId out of bounds in StateLoadingHelper::EnsureResourceIsUnloaded");

        if (maResources[lResource.muId].meState != E_STATE_UNLOADED)
        {
            UnloadResource(lResource);
            return false;
        }
        return true;
    }

    // @ 0x824FE330 -- the unload-side mirror of EnsureResourcesAreLoaded. The X360 walks
    // ALL KU_MAX_RESOURCES_TO_WATCH slots first and bails out `false` the moment any of
    // them is mid-LOAD (LOAD_REQUESTED / LOADING) or has a cancelled unload
    // (UNLOAD_CANCELLED) -- i.e. nothing unloads while a load is in flight, the exact
    // counterpart of the load side's pending-unload gate. Only then does it step every
    // tuple and report whether ALL of them reached UNLOADED. (Note the X360 tests the
    // three states directly here; there is no latched counter on this side.)
    bool StateLoadingHelper::EnsureResourcesAreUnloaded(const CgsGui::sResourceTuple* lpResources,
                                                        u32 luCount)
    {
        for (u32 luWatched = 0; luWatched < KU_MAX_RESOURCES_TO_WATCH; ++luWatched)
        {
            const EResourceState leState = maResources[luWatched].meState;
            if (leState == E_STATE_LOADING || leState == E_STATE_UNLOAD_CANCELLED
                || leState == E_STATE_LOAD_REQUESTED)
            {
                return false;
            }
        }

        bool lbAllUnloaded = true;
        for (u32 luResource = 0; luResource < luCount; ++luResource)
        {
            if (!EnsureResourceIsUnloaded(lpResources[luResource]))
            {
                lbAllUnloaded = false;
            }
        }
        return lbAllUnloaded;
    }

    // @ 0x824FE1F8 -- request the unload of every watched slot of the given type
    // that is not already sitting UNLOADED.
    void StateLoadingHelper::UnloadAllResources(CgsGui::ResourceRequestTypes leType)
    {
        for (u32 luResource = 0; luResource < KU_MAX_RESOURCES_TO_WATCH; ++luResource)
        {
            const ResourceInfo& lrResourceInfo = maResources[luResource];
            if (lrResourceInfo.meState != E_STATE_UNLOADED && lrResourceInfo.meType == leType)
            {
                CgsGui::sResourceTuple lTuple;
                lTuple.muId   = luResource;
                lTuple.meType = lrResourceInfo.meType;
                UnloadResource(lTuple);
            }
        }
    }

    // @ 0x824EDC20 -- true once every expected apt component registered on the
    // flow layer has been marked initialised. The flow-range assert (cpp:895)
    // streams the offending value on the console; folded static.
    bool StateLoadingHelper::AreAllAptComponentsInitialised(GuiFlow leFlow) const
    {
        CGS_ASSERT(static_cast<u32>(leFlow) <= 2, "Invalid GuiFlow of ");   // cpp:895

        const ComponentsToWatch& lrWatch = maComponentsToWatch[leFlow];
        for (u32 luComponent = 0; luComponent < lrWatch.muNumberOfComponentsToWatch; ++luComponent)
        {
            if (!lrWatch.mabComponentsLoaded[luComponent])
            {
                return false;
            }
        }
        return true;
    }

    // @ 0x824EE058 -- zero the flow layer's per-component loaded flags and its
    // expected count (the id list is left to be overwritten by the next register).
    // Flow-range assert as above (cpp:1039).
    void StateLoadingHelper::ClearComponentInitialised(GuiFlow leFlow)
    {
        CGS_ASSERT(static_cast<u32>(leFlow) <= 2, "Invalid GuiFlow of ");   // cpp:1039

        ComponentsToWatch& lrWatch = maComponentsToWatch[leFlow];
        for (u32 luComponent = 0; luComponent < ComponentsToWatch::KU_MAX_COMPONENTS_TO_WATCH;
             ++luComponent)
        {
            lrWatch.mabComponentsLoaded[luComponent] = false;
        }
        lrWatch.muNumberOfComponentsToWatch = 0;
    }

    // @ 0x824F85D8 -- register one component name hash as "expected" on the flow
    // layer. The three X360 asserts are non-gating (the append always runs): the
    // flow-range stream (cpp:784, folded static as above), the capacity check
    // (cpp:789) and the duplicate check (cpp:790, computed through the scan below
    // exactly as the X360 does).
    void StateLoadingHelper::AppendExpectedAptComponent(GuiFlow leFlow, u32 luComponentNameHash)
    {
        CGS_ASSERT(static_cast<u32>(leFlow) <= 2, "Invalid GuiFlow of ");   // cpp:784

        ComponentsToWatch& lrWatch = maComponentsToWatch[leFlow];
        CGS_ASSERT(lrWatch.muNumberOfComponentsToWatch
                       < ComponentsToWatch::KU_MAX_COMPONENTS_TO_WATCH,
                   "Component list is full. Consider increasing the size, or are we "
                   "doing something silly?");   // cpp:789

        const bool lbAlreadyWaiting = IsWaitingAptComponent(leFlow, luComponentNameHash);
        CGS_ASSERT(!lbAlreadyWaiting,
                   "Appending a component to the list that already exists");   // cpp:790
        (void)lbAlreadyWaiting;

        lrWatch.mauComponentsToWatchIds[lrWatch.muNumberOfComponentsToWatch] = luComponentNameHash;
        lrWatch.mabComponentsLoaded[lrWatch.muNumberOfComponentsToWatch]     = false;
        ++lrWatch.muNumberOfComponentsToWatch;
    }

    // @ 0x824EDB08 -- linear-scan the flow layer's expected component ids for the
    // hash. Flow-range assert as above (cpp:867).
    bool StateLoadingHelper::IsWaitingAptComponent(GuiFlow leFlow, u32 luComponentNameHash) const
    {
        CGS_ASSERT(static_cast<u32>(leFlow) <= 2, "Invalid GuiFlow of ");   // cpp:867

        const ComponentsToWatch& lrWatch = maComponentsToWatch[leFlow];
        for (u32 luComponent = 0; luComponent < lrWatch.muNumberOfComponentsToWatch;
             ++luComponent)
        {
            if (lrWatch.mauComponentsToWatchIds[luComponent] == luComponentNameHash)
            {
                return true;
            }
        }
        return false;
    }

    // @ 0x824EDEC8 -- consume an Apt ONLOAD trigger. ARTIST scans all three
    // flow watcher blocks and marks every matching component hash. It then binds
    // the Apt reference to a matching controlled component and removes that
    // pending entry by swapping the final entry down into the vacated slot.
    void StateLoadingHelper::MarkAptComponentInitialised(
        const CgsGui::GuiEventAptTriggerPayload* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid apt trigger event sent to StateLoadingHelper::MarkAptComponentInitialised");
        if (lpEvent == 0)
            return;

        for (u32 luFlow = 0; luFlow < 3; ++luFlow)
        {
            ComponentsToWatch& lrWatch = maComponentsToWatch[luFlow];
            for (u32 luComponent = 0;
                 luComponent < lrWatch.muNumberOfComponentsToWatch;
                 ++luComponent)
            {
                if (lrWatch.mauComponentsToWatchIds[luComponent] ==
                    lpEvent->muComponentNameHash)
                {
                    lrWatch.mabComponentsLoaded[luComponent] = true;
                }
            }
        }

        for (u32 luControlled = 0; luControlled < muControlledComponentCount; ++luControlled)
        {
            if (muControlledComponentNameHash[luControlled] != lpEvent->muComponentNameHash)
                continue;

            mpaControlledComponents[luControlled]->AttachController(lpEvent->mpComponentRef);
            const u32 luLast = muControlledComponentCount - 1;
            if (luControlled != luLast)
            {
                mpaControlledComponents[luControlled] = mpaControlledComponents[luLast];
                muControlledComponentNameHash[luControlled] =
                    muControlledComponentNameHash[luLast];
            }
            --muControlledComponentCount;
            break;
        }
    }

    // @ 0x824FEB58 / @ 0x824FEB50 / @ 0x824FEBB0 / @ 0x824EE7A8 / @ 0x824EE528 --
    // the GuiCache faces of the helpers above (X360: `addi r3,r3,8` + tail-branch
    // into the embedded watcher at +0x8).
    bool GuiCache::EnsureResourcesAreLoaded(const CgsGui::sResourceTuple* lpResources, u32 luCount)
    {
        return mStateLoadingHelper.EnsureResourcesAreLoaded(lpResources, luCount);
    }

    bool GuiCache::EnsureResourceIsLoaded(const CgsGui::sResourceTuple& lResource)
    {
        return mStateLoadingHelper.EnsureResourceIsLoaded(lResource);
    }

    // @ 0x824FEBB8 / @ 0x824FEBC0 -- both are literally `addi r3,r3,8` + tail-branch into
    // the embedded watcher, i.e. the same two-instruction face as the load pair above.
    bool GuiCache::EnsureResourceIsUnloaded(const CgsGui::sResourceTuple& lResource)
    {
        return mStateLoadingHelper.EnsureResourceIsUnloaded(lResource);
    }

    bool GuiCache::EnsureResourcesAreUnloaded(const CgsGui::sResourceTuple* lpResources, u32 luCount)
    {
        return mStateLoadingHelper.EnsureResourcesAreUnloaded(lpResources, luCount);
    }

    // @ 0x824FEB60 -- the single-tuple GuiCache face, and the same two-instruction shape as
    // the four above: `addi r3, r3, 8` + `b BrnGui__StateLoadingHelper__UnloadResource`.
    // The tail-branch leaves r4 (the sResourceTuple reference) untouched, which is why the
    // IDA pseudocode shows only one parameter -- the DWARF signature (BrnGuiCache.h:638,
    // `void UnloadResource(const sResourceTuple&)`) is the attested one, and the +8 is
    // mStateLoadingHelper. PreRaceFlyByState::OnLeave is the caller
    // (BrnPreRaceFlyBy_wJ_02.cpp:324 / :334).
    void GuiCache::UnloadResource(const CgsGui::sResourceTuple& lResource)
    {
        mStateLoadingHelper.UnloadResource(lResource);
    }

    void GuiCache::UnloadResources(const CgsGui::sResourceTuple* lpResources, u32 luCount)
    {
        for (u32 luResource = 0; luResource < luCount; ++luResource)
            mStateLoadingHelper.UnloadResource(lpResources[luResource]);
    }

    // @ 0x824ED6C0 -- StateLoadingHelper::GetLoadedResource. The id -> loaded-asset lookup:
    //     if (luId >= 0xED) assert;                       ; 0xED == 237 == KU_MAX_RESOURCES_TO_WATCH
    //     v4 = 12 * luId + this;                          ; ResourceInfo stride 12 {state,type,ptr}
    //     if (!*(v4 + 8)) assert("maResources[luId].mpResource!=NULL");
    //     return *(v4 + 8);
    // Both asserts are the console's own (BrnGuiCache.cpp:345 / :346).
    //
    // ⭐ This is the whole of the "resource id -> asset" binding. The id is an index into
    // maResources, and maResources[id].mpResource is filled by the resource module's LOADED
    // notification -- so an id only resolves if something ASKED for it first
    // (EnsureResourceIsLoaded on a matching sResourceTuple). It was declared and never
    // defined, which is why BrnNetworkPlayerImageRenderer could not link.
    const void* StateLoadingHelper::GetLoadedResource(u32 luId) const
    {
        CGS_ASSERT(luId < KU_MAX_RESOURCES_TO_WATCH,
                   "(luId >= 0) && (luId < KU_MAX_RESOURCES_TO_WATCH)");
        if (luId >= KU_MAX_RESOURCES_TO_WATCH)
            return 0;

        CGS_ASSERT(maResources[luId].mpResource != 0, "maResources[luId].mpResource!=NULL");
        return maResources[luId].mpResource;
    }

    // @ 0x824EE520 -- the GuiCache face: `return StateLoadingHelper::GetLoadedResource(this + 8);`
    const void* GuiCache::GetLoadedResource(u32 luId) const
    {
        return mStateLoadingHelper.GetLoadedResource(luId);
    }

    void GuiCache::UnloadAllResources(CgsGui::ResourceRequestTypes leType)
    {
        mStateLoadingHelper.UnloadAllResources(leType);
    }

    bool GuiCache::AreAllAptComponentsInitialised(GuiFlow leFlow) const
    {
        return mStateLoadingHelper.AreAllAptComponentsInitialised(leFlow);
    }

    void GuiCache::ClearExpectedAptComponentList(GuiFlow leFlow)
    {
        mStateLoadingHelper.ClearComponentInitialised(leFlow);
    }

    // The one @0xAC74 far member behind both header accessors (see the header note).
    u32 GuiCache::GetFriendsListCachedField() const
    {
        return muNumActivePlayers;
    }

    s32 GuiCache::GetNumActivePlayers() const
    {
        return static_cast<s32>(muNumActivePlayers);
    }

    // Replace the flow layer's expected-component list wholesale (the header's
    // ADDITIVE-GROW declaration; ImageGalleryState @0x82484720 and FBurnMainHudState
    // @0x82475328 drive it): clear the layer, then append each caller hash.
    void GuiCache::SetExpectedAptComponentList(GuiFlow leFlow,
                                               const u32* lpauComponentNameHashes,
                                               u32 luCount)
    {
        mStateLoadingHelper.ClearComponentInitialised(leFlow);
        for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
            mStateLoadingHelper.AppendExpectedAptComponent(leFlow, lpauComponentNameHashes[luIndex]);
    }

    // @ 0x8250DDF0 -- the three event families used by the reconstructed module:
    // resource load/unload completions and Apt ONLOAD component triggers.
    void GuiCache::RecEvent(const CgsModule::Event* lpEvent, s32 liEventId)
    {
        if (lpEvent == 0)
            return;

        switch (liEventId)
        {
        case 14:
            mStateLoadingHelper.OnLoadNotification(
                reinterpret_cast<const CgsGui::GuiEventLoadNotification*>(lpEvent));
            break;
        case 16:
            mStateLoadingHelper.OnUnloadNotification(
                reinterpret_cast<const CgsGui::GuiEventUnloadNotification*>(lpEvent));
            break;
        case 21:
        {
            const CgsGui::GuiEventAptTriggerPayload* lpTrigger =
                reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent);
            if (lpTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD)
                mStateLoadingHelper.MarkAptComponentInitialised(lpTrigger);
            break;
        }
        case 132:
            // ⭐ [boost-bar gate 2026-08-25] X360 case 132 @0x8250F58C: the game-flow-state
            // change. Latch the new state word (+0x4B30), DROP the gameplay-HUD-active gate
            // (+0x407C -- the flow states' UpdateWFInit/UpdateSetupState re-raise it, see the
            // header's SetGameplayHudActive note), clear the +0x4B34 byte when the new state
            // is 1 or 3, and reset the +0x9FD4/+0x9FD8 last-score pair to -1.
            {
                const s32 liNewFlowState = *reinterpret_cast<const s32*>(lpEvent);
                miGameFlowState     = liNewFlowState;             // stw +0x4B30
                mbGameplayHudActive = false;                      // stb 0 +0x407C
                if (liNewFlowState == 1 || liNewFlowState == 3)
                    mu8GameFlowByte_4B34 = 0;                     // stb 0 +0x4B34
                miLastStuntScore          = -1;                   // stwx -1 +0x9FD4
                miGameFlowResetWord_9FD8  = -1;                   // stwx -1 +0x9FD8
            }
            break;
        case 147:
            // [hud H3b tracking slice 2026-08-25] X360 case 147 @0x8250DDF0 (h1_dump.txt):
            // the three HUD words {speed, rpm, gear} -> +19208/+19212/+19216. The producer
            // is the bridge's GuiEventUpdateHud post; the satnav component's GuiPlayerInfo
            // view reads miPlayerSpeedMph (@+0x4B08) for its view-distance/zoom math.
            {
                const BrnGui::GuiEventUpdateHud* lpHudEvent =
                    reinterpret_cast<const BrnGui::GuiEventUpdateHud*>(lpEvent);
                miPlayerSpeedMph = lpHudEvent->miSpeedMph;   // +19208
                miPlayerRPM      = lpHudEvent->miRPM;        // +19212
                miPlayerGear     = lpHudEvent->mi8Gear;      // +19216 (word store of the byte)
            }
            break;

        case 199:
            // [hud H3b tracking slice 2026-08-25] X360 case 199 @0x8250DDF0: the per-frame
            // GuiEventUpdateSatNav icon array (count @+0x900, clamped to 48). The PLAYER
            // arm (icon type 0) is THE producer of the mv4WorldCameraPosition block the
            // whole sat-nav view chain reads -- gated on the case-376 pair being live and
            // the case-207 used byte (IsActiveRaceCarIndexUsed).
            // [FLAG deferred] the DRIVE-THRU arm (icon types 7/9/10/11/12: the dedupe-by-id
            // append into maDriveThroughInfo + the 15-entry canned-position override table
            // @0x8206F868) -- no producer on this build emits those icon types (the world
            // route-information bridge that does is itself FLAG-deferred in
            // GameBridgeWorldToGui.cpp); the arm lands with that producer.
            {
                const BrnGui::GuiEventUpdateSatNav* lpSatNavEvent =
                    reinterpret_cast<const BrnGui::GuiEventUpdateSatNav*>(lpEvent);
                s32 liNumIcons = lpSatNavEvent->miNumIcons;              // @+0x900
                if (liNumIcons >= BrnGui::GuiEventUpdateSatNav::KI_MAX_SAT_NAV_ICONS)
                    liNumIcons = BrnGui::GuiEventUpdateSatNav::KI_MAX_SAT_NAV_ICONS;

                for (s32 liIcon = 0; liIcon < liNumIcons; ++liIcon)
                {
                    const BrnGui::GuiEventUpdateSatNav::SatNavIconInfo& lrIcon =
                        lpSatNavEvent->maIconInfo[liIcon];

                    switch (lrIcon.GetIconTypeByte())
                    {
                    case BrnGui::GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_PLAYER_CAR:
                        // [DIAG] NOT IN THE X360 BINARY -- [satnav-diag] the gate, once.
                        {
                            static bool sbGateLogged = false;
                            if (!sbGateLogged && CgsDev::Log::gpDebugPrint != 0)
                            {
                                sbGateLogged = true;
                                *CgsDev::Log::gpDebugPrint
                                    << "[satnav-diag] cache 199 gate: activeIdx="
                                    << mePlayerActiveRaceCarIndex << " used="
                                    << static_cast<s32>(
                                           mePlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID
                                               ? (IsActiveRaceCarIndexUsed(static_cast<EActiveRaceCarIndex>(
                                                     mePlayerActiveRaceCarIndex)) ? 1 : 0)
                                               : -1) << "\n";
                            }
                        }
                        if (mePlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID &&
                            IsActiveRaceCarIndexUsed(static_cast<EActiveRaceCarIndex>(
                                mePlayerActiveRaceCarIndex)))
                        {
                            // [DIAG] NOT IN THE X360 BINARY -- [satnav-diag] first store.
                            {
                                static bool sbLogged = false;
                                if (!sbLogged && CgsDev::Log::gpDebugPrint != 0)
                                {
                                    sbLogged = true;
                                    *CgsDev::Log::gpDebugPrint
                                        << "[satnav-diag] cache 199 player store: pos=("
                                        << lrIcon.GetPositionLane().x << ","
                                        << lrIcon.GetPositionLane().z << ") rot="
                                        << lrIcon.GetRotation() << "\n";
                                }
                            }
                            mv4WorldCameraPosition = lrIcon.GetPositionLane();   // stvx +19168
                            mfPlayerOrientation    = lrIcon.GetRotation();       // +19220 (lfs icon+24)
                            mePlayerCounty         = static_cast<s32>(lrIcon.GetCounty());   // +19224
                            mePlayerDistrict       = static_cast<s32>(lrIcon.GetDistrict()); // +19228
                        }
                        break;
                    default:
                        // (rival/network icons are read straight off the record by the
                        // renderer's icon pass; the cache stores nothing for them here.)
                        break;
                    }
                }
            }
            break;

        case 207:
            // [hud H3b tracking slice 2026-08-25] X360 case 207: memcpy(this+0xA020,
            // payload, 240) -- the whole GuiRaceCarInfoEvent over the mRaceCarInfo SoA.
            // Reproduced MEMBER-WISE (the x64 layouts match field for field; a raw byte
            // copy would silently couple the two layouts). This is THE producer of
            // maRaceCarUsed/maRaceCarPositions -- IsActiveRaceCarIndexUsed's backing.
            {
                const BrnGui::GuiRaceCarInfoEvent* lpInfoEvent =
                    reinterpret_cast<const BrnGui::GuiRaceCarInfoEvent*>(lpEvent);
                for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
                {
                    maRaceCarPositions[liSlot]    = lpInfoEvent->GetPosition(liSlot);     // +0xA020
                    maRaceCarIdentities[liSlot]   = lpInfoEvent->GetIdentity(liSlot);     // +0xA0A0
                    maRaceCarUsed[liSlot]         = lpInfoEvent->GetUsedFlag(liSlot);     // +0xA0E4
                    maRaceCarConnecting[liSlot]   = lpInfoEvent->GetConnectingFlag(liSlot);   // +0xA0EC
                    maRaceCarDisconnected[liSlot] = lpInfoEvent->GetDisconnectedFlag(liSlot); // +0xA0F4
                    maRaceCarInRange[liSlot]      = lpInfoEvent->GetInRangeFlag(liSlot);  // +0xA0FC
                    maRaceCarCrashing[liSlot]     = lpInfoEvent->GetCrashingFlag(liSlot); // +0xA104
                }
                miNumRaceCarsInInfo = lpInfoEvent->GetNumEntries();                        // +0xA0E0
            }
            break;

        case 203:
            // ⭐⭐ [event-starts producer wave 2026-08-27] THE EVENT-START TABLE, consumer half.
            // X360 case 203 @0x8250DDF0: `memcpy(this + 22160, payload, 8416)` -- 22160 == 0x5690,
            // the base of this cache's embedded mSetUpAllEventStartsInterface (maEventStarts[175]
            // + miEventStartsCount @+0x7760, BrnGuiCache.h), and 8416 == the whole interface.
            // The payload IS a BrnGameState::GameStateModuleIO::SetUpAllEventStartsInterface with
            // no GuiEvent header in front of it (see GuiEventUpdateEventStarts in
            // BrnGuiEventTypeDefs.h for the asm that settles that).
            //
            // ⭐ THIS ARM IS WHAT SILENCES "Unable to find event start with event id: ". Before it
            // landed, NOTHING anywhere wrote maEventStarts: the whole producer chain
            // (SendSetUpAllEventStartsMessage -> the output interface -> the bridge -> here) was
            // missing at its first link, so miEventStartsCount stayed 0 and BOTH display-info
            // lookups walked a zero-length array and fell through to their console asserts on
            // every sat-nav refresh. The assert was never wrong; it was reporting this gap.
            //
            // ⚠️ REPRODUCED MEMBER-WISE, not as the console's byte blit -- the same call the
            // case-207 arm below makes, and for the same reason. The two ends are two names for
            // ONE console record (BrnGameState's EventStart / BrnGui's SatNavEventDisplayInfo)
            // and their host layouts agree field for field, but a raw memcpy would couple them
            // silently. Each side's own _AssertLayout/static_assert pins the 0x30 stride, so a
            // future drift on either side becomes a compile error here instead of a wrong icon.
            // The count is CLAMPED to the cache's capacity: the source array cannot exceed 175
            // (AddEventStart asserts it) but the clamp costs nothing and keeps a corrupt length
            // out of a 175-element array.
            {
                const BrnGui::GuiEventUpdateEventStarts* lpEventStarts =
                    reinterpret_cast<const BrnGui::GuiEventUpdateEventStarts*>(lpEvent);

                s32 liCount = static_cast<s32>(lpEventStarts->mEventStarts.GetNumEventStarts());
                if (liCount > 175)
                    liCount = 175;

                for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
                {
                    const BrnGameState::GameStateModuleIO::SetUpAllEventStartsInterface::EventStart&
                        lrSource = lpEventStarts->mEventStarts.GetEventStart(
                                       static_cast<u32>(liIndex));

                    SatNavEventDisplayInfo& lrDest = maEventStarts[liIndex];
                    lrDest.mv3Position       = lrSource.GetPosition();              // +0x00
                    lrDest.muLightTriggerId  = lrSource.GetLightTriggerId();         // +0x10
                    lrDest.muJunctionId      = static_cast<u32>(lrSource.GetEventIndex()); // +0x14
                    lrDest.muEventInstanceId = static_cast<u32>(lrSource.GetEventID());    // +0x18
                    lrDest.muCounty          = static_cast<u32>(lrSource.GetCounty());     // +0x1C
                    lrDest.mi16AISectionIndex = lrSource.GetAISectionIndex();              // +0x20
                }
                miEventStartsCount = liCount;                                        // +0x7760
            }
            break;

        case 204:
            // [hud H3b tracking slice 2026-08-25] X360 case 204: the sat-nav event-filter
            // pair -- `+32824 (mbSatNavEventFilterEnabled) = payload[8];
            // +32820 (meSatNavEventFilter) = payload word @+4`. The producer is the FBurn
            // HUD state's Enable/DisableSatNavEventsFilter (the record rides channel 40
            // into the module input; its channel-41 twin feeds the custom-renderer path).
            {
                const u8* lpu8Payload = reinterpret_cast<const u8*>(lpEvent);
                mbSatNavEventFilterEnabled = lpu8Payload[8] != 0;                          // +32824
                meSatNavEventFilter =
                    *reinterpret_cast<const s32*>(lpu8Payload + 4);                        // +32820
            }
            break;

        case 376:
            // [hud H3b tracking slice 2026-08-25] X360 case 376: the player race-car index
            // pair, with the console's four range asserts (BrnGuiCache.cpp:1904-1907).
            // This pair GATES the case-199 player store above.
            {
                const BrnGui::GuiPlayerRaceCarIdEvent* lpRaceCarIdEvent =
                    reinterpret_cast<const BrnGui::GuiPlayerRaceCarIdEvent*>(lpEvent);
                CGS_ASSERT(lpRaceCarIdEvent->mePlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID,
                           "lpRaceCarIdEvent->mePlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID"); // :1904
                CGS_ASSERT(lpRaceCarIdEvent->mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                           "lpRaceCarIdEvent->mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");   // :1905
                CGS_ASSERT(lpRaceCarIdEvent->mePlayerGlobalRaceCarIndex > E_GLOBAL_RACE_CAR_INDEX_INVALID,
                           "lpRaceCarIdEvent->mePlayerGlobalRaceCarIndex > E_GLOBAL_RACE_CAR_INDEX_INVALID"); // :1906
                CGS_ASSERT(lpRaceCarIdEvent->mePlayerGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                           "lpRaceCarIdEvent->mePlayerGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");   // :1907
                mePlayerActiveRaceCarIndex = lpRaceCarIdEvent->mePlayerActiveRaceCarIndex; // +19200
                mePlayerGlobalRaceCarIndex = lpRaceCarIdEvent->mePlayerGlobalRaceCarIndex; // +19204
            }
            break;

        case 379:
            // [hud reveal gate 2026-08-25] X360 case 379 -- the IGNITION latch, and the
            // whole free-burn HUD reveal gate. The console arm is one store, and it is the
            // ONLY store to +0x4B20 in this entire ~180-case switch:
            //     0x8251017C  stw  r11, 0x4B20(r31)
            //     (Hex-Rays: `else if ( a3 == 379 ) *(a1 + 19232) = *a2;`)
            // The payload is GuiPlayerEngineEvent's single word -- 0 == E_ENGINE_OFF,
            // 1 == E_ENGINE_ON (BrnGuiDemangledEventTypes.h:267, id 379 size 4). The three
            // readers of the word are FBurnMainHudState::UpdateWFInit (compose visible vs
            // stay on the invisible transition frame), FBurnMainHudState::UpdateRunning
            // (the case-215 boost-bar arm) and RaceMainHudState::RevealHud.
            //
            // Note the console does NOT range-assert here -- it asserts at the CONSUMER
            // (UpdateWFInit's `< 2`, BrnFBurnMainHudState.cpp:1536). Reproduced as-is: adding
            // a producer-side assert would be an invented arm.
            mePlayerEngineState = *reinterpret_cast<const s32*>(lpEvent);              // +19232
            break;

        case 350:
            // ADDITIVE (HUD H1 wave, 2026-08-25 -- landed as the fix for the odometer's
            // mpProfile assert storm: the odometer TU reads the cache profile every frame,
            // and the PC cache never consumed the event that carries it). X360 case 350
            // @0x8250DDF0 (h1_dump.txt), the load-bearing store:
            //     assert lpProfileEvent (cpp:2977)
            //     mpProfile = payload->mpProfile;               // +16476 <- *(payload+0)
            //     *(byte*)(this+80794) = payload->flag;         // the road-rules byte
            //     [gated] DetermineCarUnlockPending(mpProfile);
            //     [gated] word@44096 refinement off Profile+42517
            // Reproduced: the profile store. FLAG'd deferrals: the +80794 byte (un-homed
            // member; not fabricated), the DetermineCarUnlockPending call (bodied in
            // BrnGuiCache_wB_10.cpp but its two gate bytes +19318/+19316 are un-homed
            // here), and the +44096 refinement (un-homed). The PC producer is the
            // event-350 stand-in in BrnGameModule.cpp, which posts a REAL Profile*.
            {
                const BrnGui::GuiEventProgressionProfileData* lpProfileEvent =
                    reinterpret_cast<const BrnGui::GuiEventProgressionProfileData*>(lpEvent);
                CGS_ASSERT(lpProfileEvent != 0, "lpProfileEvent");   // cpp:2977
                mpProfile = lpProfileEvent->mpProfile;
            }
            break;
        case 169:
            // ADDITIVE (HUD H1 wave, 2026-08-25). X360 case 169 @0x8250DDF0 (h1_dump.txt):
            // three word copies of the GuiEventChangeDistrict record into the marker source
            // words. BOTH producers route here -- the game bridge's fresh region change
            // (consumed byte 0) and FBurnMainHudState's own consumed-marking write-back
            // (the state calls RecEvent directly with the byte set).
            {
                const BrnGui::GuiEventChangeDistrict* lpChangeDistrict =
                    reinterpret_cast<const BrnGui::GuiEventChangeDistrict*>(lpEvent);
                meChangeDistrictCounty    = lpChangeDistrict->meCounty;
                meChangeDistrictDistrict  = lpChangeDistrict->meDistrict;
                mu8ChangeDistrictConsumed = lpChangeDistrict->mu8Consumed;
            }
            break;
        // ================================================================================
        // ⭐⭐ [E1 event-status wave 2026-08-26] THE EVENT SCORE / TIMER FEED, consumer half.
        // Three arms of the console's third RecEvent sub-switch (`addi r11, r5, -0x17C` +
        // jpt_825101AC @0x8251018C, so jump-table case N == GUI event id 380 + N). Producer
        // for all three: BrnGameModule::BridgeGameStateToGui @0x823EE880, landed as the
        // stunt slice in GameSource/Game/GameBridgeGameStateToX_EventStatusGuiEvents.cpp.
        // Until this wave NOTHING wrote mfEventTime / miScoreCurrent / miScoreTarget /
        // miScoreCombo / miComboMultiplier, so every event readout rendered 0 / x1 / 0m00s.
        // ================================================================================

        case 492:
            // X360 jpt_825101AC case 112 @0x82510540..0x8251060C -- GuiEventCurrentStatus.
            // Store-for-store: latch the remaining-checkpoint count (+0x4F9C), then the
            // distance-driven float (+0x13B94) and the 8-lane player-team table (+0xB808).
            //
            // ⛔ FLAG DEFERRED -- the landmark-TRACKER tail (@0x82510568..0x825105A0 and
            // @0x825105D4..0x82510600). The console, when the count is > 0, maps each
            // checkpoint index through the active-landmark u16 table at cache+0x5288 into a
            // u16 scratch list at cache+0x4B9C, and then -- ONLY when meGameModeType ==
            // E_MODE_ONLINE_BURNING_HOME_RUN (13) AND the count actually changed -- calls
            // GuiCache::UpdateTrackerInfo(this, cache+0x4B9C, count). THREE things are
            // missing here: cache+0x4B9C (unmodelled, inside mPad_4B77), the +0x5288 u16
            // array (unmodelled, inside mPad_5287) and UpdateTrackerInfo itself (no body
            // anywhere in src). All of it is dead outside mode 13, which is the ONLINE
            // Burning Home Run -- unreachable from this wave's offline stunt-run target --
            // so it is named rather than faked. Landing it is a header carve plus one
            // function, and the arm below is where it plugs in.
            {
                const BrnGui::GuiEventCurrentStatus* lpStatus =
                    reinterpret_cast<const BrnGui::GuiEventCurrentStatus*>(lpEvent);

                miNumRemainingCheckpoints = lpStatus->miNumRemainingCheckpoints;   // stw +0x4F9C
                // (the +0x4B9C landmark mapping loop would run here -- see the FLAG above)
                mfDistanceDriven = lpStatus->mfDistanceDrivenInCurrentCar;         // stfsx +0x13B94
                for (s32 liCar = 0; liCar < 8; ++liCar)                            // the 8-word ctr loop
                    maCurrentPlayerTeam[liCar] = lpStatus->maePlayerTeam[liCar];   // -> +0xB808
                // (the meGameModeType == 13 UpdateTrackerInfo call would run here)
            }
            break;

        case 424:
            // X360 jpt_825101AC case 44 @0x82510780..0x82510884 -- GuiEventScoreUpdate.
            // THE EVENT TIMER. The two time words are gated on the record's mbTimerActive
            // byte (`lbz r11, 0x10(r30) ; cmplwi 0 ; beq` @0x82510804), so a stopped event
            // timer FREEZES the displayed value rather than zeroing it -- console behaviour,
            // not an oversight. The distance word carries a sentinel: FLT_MAX means "no
            // checkpoint distance this frame", and the console then only lifts a NEGATIVE
            // cached distance back to zero.
            {
                const BrnGui::GuiEventScoreUpdate* lpScoreUpdate =
                    reinterpret_cast<const BrnGui::GuiEventScoreUpdate*>(lpEvent);

                // @0x8251078C `cmpwi r11, 0x12 ; blt` -- the console streams
                // "Mode is " << meGameModeType << "\n" into the message before firing.
                // ⚠️ The literal is the ASM's (< 18). BrnGameStateSharedIO.h's committed
                // EGameModeType spells E_MODE_COUNT == 17 (aliased onto
                // E_MODE_ONLINE_MODE_END); the console's is 18, or this assert would fire
                // on its own last mode. Written as the asm's bound, not the enum's.
                CGS_ASSERT(meGameModeType < 18,
                           "BrnGameState::GameStateModuleIO::E_MODE_COUNT > meGameModeType"); // cpp:2227

                meCurrentMedalTarget = lpScoreUpdate->meCurrentMedalTarget;        // stwx +0x9F28
                if (lpScoreUpdate->mbTimerActive)
                {
                    mfEventTime   = lpScoreUpdate->mfModeTime;                     // stfsx +0x9F2C
                    mfTargetTime  = lpScoreUpdate->mfCurrentTargetModeTime;        // stfsx +0x9F30
                }

                // flt_82F27EFC, read from the image rodata at VA 0x82F27EFC: 7F 7F FF FF
                // == FLT_MAX. The console compares the payload float against it for EQUALITY
                // (`fcmpu ; beq`), so it is a sentinel, not a clamp.
                const f32 KF_NO_CHECKPOINT_DISTANCE = 3.4028234663852886e+38f;
                const f32 lfDistance = lpScoreUpdate->mfDistanceToNextCheckpoint;
                if (lfDistance != KF_NO_CHECKPOINT_DISTANCE)
                {
                    mfDistanceInEvent = lfDistance;                                // stfsx +0x9F48
                }
                else if (mfDistanceInEvent < 0.0f)   // flt_82001CC0 == 0.0f (image.bin @0x82001CC0)
                {
                    mfDistanceInEvent = 0.0f;
                }
            }
            break;

        case 428:
            // X360 jpt_825101AC case 48 @0x825108E8..0x82510A3C -- GuiAttackScoreUpdate.
            // THE STUNT-RUN SCORE READOUT: current / target / banked combo / multiplier,
            // the stunt-count pair, the combo-warning timer and its two flag bytes, plus
            // the single "stunt to display" record. The tail latch (@0x82510A0C) mirrors the
            // live combo pair into the last-stunt pair ONLY while a combo is banked -- the
            // same +0x9FD4/+0x9FD8 words the case-132 flow-state change resets to -1.
            {
                const BrnGui::GuiAttackScoreUpdate* lpAttack =
                    reinterpret_cast<const BrnGui::GuiAttackScoreUpdate*>(lpEvent);

                // @0x825108F4..0x8251093C -- the console accepts E_MODE_TRAFFIC_ATTACK (9),
                // E_MODE_STUNT_ATTACK (7), the three IsOnlineStuntRun modes
                // E_MODE_ONLINE_FUGITIVE (12) / E_MODE_ONLINE_FREE_BURN (14) /
                // E_MODE_ONLINE_MODE_END (17), E_MODE_ONLINE_FREE_BURN_LOBBY (15) and
                // E_MODE_NONE (-1); anything else fires. Values are spelled as literals
                // because this TU cannot include BrnGameStateSharedIO.h (see the header
                // note at the top of this file) -- each one is read off the asm's `cmpwi`.
                CGS_ASSERT(meGameModeType == 9  || meGameModeType == 7  ||
                           meGameModeType == 12 || meGameModeType == 14 ||
                           meGameModeType == 17 || meGameModeType == 15 ||
                           meGameModeType == -1,
                           "BrnGameState::GameStateModuleIO::E_MODE_TRAFFIC_ATTACK == meGameModeType"
                           " || BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK == meGameModeType"
                           " || GsmIO::IsOnlineStuntRun( meGameModeType )"
                           " || BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY == meGameModeType"
                           " || BrnGameState::GameStateModuleIO::E_MODE_NONE == meGameModeType"); // cpp:2271

                miScoreCurrent    = lpAttack->miCurrentScore;             // stwx +0x9FC4
                miScoreTarget     = lpAttack->miTargetScore;              // stwx +0x9FC8
                miScoreCombo      = lpAttack->miComboScore;               // stw  +0x9FCC
                miComboMultiplier = lpAttack->miComboMultiplier;          // stw  +0x9FD0

                muCurrentStunts          = lpAttack->muCurrentStunts;          // stwx  +0xAC64
                muAllStunts              = lpAttack->muAllStunts;              // stwx  +0xAC68
                mfComboWarningTimeActive = lpAttack->mfComboWarningTimeActive; // stfsx +0xAC6C
                mbComboWarningActive     = lpAttack->mbComboWarningActive;     // stbx  +0xAC70
                mbComboInProgress        = lpAttack->mbComboInProgress;        // stbx  +0xAC71

                // the merged 8-byte ScoringOutputInterface::maStunts[0] pair
                maStuntToDisplay[0].miStuntId  = lpAttack->meStuntToDisplayType;   // stw +0xAC5C
                maStuntToDisplay[0].miField_04 = lpAttack->miStuntToDisplayScore;  // stw +0xAC60

                // @0x82510A0C: both words are RELOADED from the cache, not reused from the
                // payload -- reproduced as written.
                if (miScoreCombo != 0)
                {
                    miLastStuntScore         = miScoreCombo;        // stwx +0x9FD4
                    miGameFlowResetWord_9FD8 = miComboMultiplier;   // stwx +0x9FD8
                }
            }
            break;

        // ================================================================================
        // ⭐⭐⭐ [A9 mode-type arm 2026-08-27] THE MODE-START SEED -- and THE writer of
        // meGameModeType (+0x9E58).
        //
        // X360 GuiCache::RecEvent @0x8250DDF0, `jumptable 8250DE3C case 89` @0x8250E7E0
        // (the first sub-switch rebases by -4, so jump-table case N == GUI event id N + 4;
        // 89 + 4 == 93 == GuiEventPrepareForModeStart). Producer:
        // BrnGame::TranslateEventFlowGameActionToGuiEvent's case-23 arm
        // (GameBridgeGameStateToX_EventFlowGuiEvents.cpp:509), mounted and called.
        //
        // ⛔ WHY THIS ARM IS THE #1 BLOCKER IT IS. meGameModeType had NO writer anywhere in
        // src before this. It is the switch variable of EventInfoComponent::Update
        // @0x82435430 (`if (lpCache->GetGameMode() == meCurrentEventType) switch (...)`), the
        // gate RaceMainHudState::SetupEventInfo @0x82474A60 seeds the component's event type
        // from, and the mode word RecEvent's OWN case-424 and case-428 arms assert against.
        // With it stuck at 0 the stunt readout could never reach its case 7 -- the whole
        // score / timer / multiplier column rendered blank no matter what the producers did.
        // ================================================================================
        case 93:
        {
            const BrnGui::GuiEventPrepareForModeStart* lpPrepare =
                reinterpret_cast<const BrnGui::GuiEventPrepareForModeStart*>(lpEvent);

            // ---- the mode identity (@0x8250E7E4..0x8250E8F0) --------------------------
            meGameModeType = lpPrepare->meGameModeType;              // stw  +0x9E58
            muEventID      = lpPrepare->muEventJunctionID;           // stw  +0x9E5C
            muJunctionID   = lpPrepare->muJunctionID;                // stw  +0x9E60

            // `li r8, 3` @0x8250E7E8 -- the medal target is seeded to the console's literal
            // 3 (E_CURRENT_MEDAL_TARGET_TIME_NONE); RecEvent's case-424 arm overwrites it
            // every frame from ScoringOutputInterface::meCurrentMedalTarget.
            meCurrentMedalTarget = 3;                                // stw  +0x9F28

            // `lbz 0x90(payload) ; stb 0x4B4C` @0x8250E8FC/0x8250E904. The SAME byte this arm
            // gates its own online tail on, five statements down.
            mbOnlineStartInProgress = (lpPrepare->mbIsOnline != 0);  // stb  +0x4B4C

            // ---- the timers + the medal score targets --------------------------------
            // ⚠️ mfEventTime and mfTargetTime take the SAME payload word (+0x84,
            // GameModeParams::mfModeTimeLimit): @0x8250E90C `lfs f13, 0x84(r11)` -> +0x9F2C
            // and @0x8250E91C `lfs f13, 0x84(r30)` -> +0x9F30. Not a transcription slip --
            // at mode start the elapsed clock IS the full limit, and case 424 then drives
            // the two apart every frame.
            mfEventTime  = lpPrepare->mfModeTimeLimit;               // stfs +0x9F2C
            mfTargetTime = lpPrepare->mfModeTimeLimit;               // stfs +0x9F30
            // Highest medal first: [0] gold (+0x80), [1] silver (+0x7C), [2] bronze (+0x78).
            mafTargetScores[0] = lpPrepare->mfNeedForGold;           // stfs +0x9F34
            mafTargetScores[1] = lpPrepare->mfNeedForSilver;         // stfs +0x9F38
            mafTargetScores[2] = lpPrepare->mfNeedForBronze;         // stfs +0x9F3C
            // (mafTargetScores[3] @+0x9F40 is NOT written by this arm.)

            // ---- the "nothing scored yet" reset run (r29 == -1 throughout) ------------
            miPursuitRivalDamageLeft_9FE8 = -1;                      // stwx +0x9FE8
            miTakedownsCurrent            = -1;                      // stwx +0x9FBC
            // flt_820037C8, read from the image rodata at VA 0x820037C8: BF 80 00 00 == -1.0f.
            // The NEGATIVE sentinel is load-bearing: RecEvent's case-424 arm only lifts the
            // cached distance back to 0.0f when it has gone negative.
            mfDistanceInEvent             = -1.0f;                   // stfsx +0x9F48
            miScoreCurrent                = -1;                      // stwx +0x9FC4
            miScoreTarget                 = -1;                      // stwx +0x9FC8
            miScoreCombo                  = -1;                      // stwx +0x9FCC
            miComboMultiplier             = -1;                      // stwx +0x9FD0

            // ---- the per-mode scalars carried straight off the wire -------------------
            miPursuitRivalTotalDamage = lpPrepare->miPursuitRivalTotalDamage;  // stwx +0x9FEC
            mPursuedCarID             = lpPrepare->mPursuedCarId;              // stdx +0x9FE0 (8B)
            miCheckpointReached       = 0;                                     // stwx +0x9FB4
            // BYTE store on the console (`lbz 0x8C ; stbx 0x9FB8`) into a u8 member -- see
            // the width correction on muCheckpointsInEvent in BrnGuiCache.h.
            muCheckpointsInEvent      = lpPrepare->mu8CheckpointCount;         // stbx +0x9FB8
            // ⚠️ SIGN-EXTENDED, not zero-extended: `lbz r11, 0x8F(r30) ; extsb r11, r11 ;
            // stwx r11, r31, r19` @0x8250E97C..0x8250E984. A road-rage threshold of 0xFF on
            // the wire means -1 ("no target"), which a zero-extending read would turn into
            // 255 takedowns.
            miTakedownTarget          = lpPrepare->mi8RoadRageThreshold;       // extsb + stwx +0x9FC0
            miOpponentsInEvent        = static_cast<s8>(lpPrepare->mu8CarCount); // stbx +0x9F44

            // [FLAG deferred] @0x8250E990 `bl BrnGui::GuiEventOnlinePostEvent::Clear` on
            // `this + 43524` (== cache +0xAA04) -- the cache's embedded online-post-event
            // record, reset at every mode start. NOT called here because the record has no
            // named member on this class yet AND the committed carve of that region is in
            // conflict with it: BrnGuiEventOnlinePostEvent.h pins Clear's 8-record loop at
            // `r3 + 0x24` (stride 0x38), which from +0xAA04 puts record[0] at +0xAA28, while
            // BrnGuiCache.h carries `PerRacerPair_AA30 maPerRacerData_AA30[8]` (the SAME
            // 8 x 0x38 shape, ctor-inferred) at +0xAA30 -- eight bytes apart. Two models of
            // one array; arbitrating them is a header carve of its own and nothing on the
            // offline stunt path reads either. Naming it rather than faking a member.
            // DELETE-WHEN the +0xAA04 GuiEventOnlinePostEvent embed is arbitrated.

            mbOnlineTimeoutPending      = false;                     // stbx +0x13B5C (r24 == 0)
            mbEventPreparedForModeStart = true;                      // stbx +0xA014 (r20 == 1)
            // @0x8250E9B8..0x8250E9C8: `addi r11, r31, 0x4B7C ; mtctr 8 ; stw ; addi 4 ; bdnz`.
            for (s32 liLane = 0; liLane < 8; ++liLane)
                maPerRaceCarWord_4B7C[liLane] = 0;

            // ---- the checkpoint tables (@0x8250E9CC..0x8250EA98) ----------------------
            // Copy the wire's live entries, then fill the remainder of BOTH tables up to
            // KI_MAX_LANDMARKS_IN_MODE. Nothing is written at all when the count is zero --
            // the console guards the whole block on it.
            if (lpPrepare->mu8CheckpointCount != 0)
            {
                const s32 KI_MAX_LANDMARKS_IN_MODE = 16;   // `cmplwi r11, 0x10` @0x8250E9E4
                CGS_ASSERT(static_cast<s32>(lpPrepare->mu8CheckpointCount) <= KI_MAX_LANDMARKS_IN_MODE,
                           "lpGuiEventPrepareForModeStart->muCheckpointsInEvent <= KI_MAX_LANDMARKS_IN_MODE"); // cpp:2094

                s32 liCheckpoint = 0;
                for (; liCheckpoint < static_cast<s32>(lpPrepare->mu8CheckpointCount); ++liCheckpoint)
                {
                    maCheckpointLandmarks[liCheckpoint] = lpPrepare->mau16CheckpointLandmark[liCheckpoint]; // sth +0x9F54
                    maCheckpointDistricts[liCheckpoint] = lpPrepare->maiCheckpointDistrict[liCheckpoint];   // stw +0x9F74
                }
                for (; liCheckpoint < KI_MAX_LANDMARKS_IN_MODE; ++liCheckpoint)
                {
                    maCheckpointLandmarks[liCheckpoint] = 0;    // `sth 0`
                    maCheckpointDistricts[liCheckpoint] = 18;   // `stw 18` == BrnWorld::E_DISTRICT_INVALID
                }
            }

            if (mbOnlineStartInProgress)
            {
                // ---- the ONLINE tail (@0x8250EAA8..) ---------------------------------
                miOnlineRoundIndex = lpPrepare->miCurrentRound;      // stw +0xA7FC
                CGS_ASSERT(miOnlineRoundIndex >= 0 && static_cast<u32>(miOnlineRoundIndex) < 10u,
                           "miRoundIndex>=0 && uint32_t(miRoundIndex)<BrnGameState::GameStateModuleIO::"
                           "KU_MAX_ONLINE_ROUNDS_IN_MODE");        // cpp:2115 (`cmplwi r11, 0xA`)

                // [FLAG deferred -- ONLINE ONLY, unreachable from this wave's offline stunt run]
                // Three console legs of this tail are named rather than faked:
                //   (a) `sub_82507070(this, &maOnlineGameModeOptions[round])` -- the GuiTracker
                //       refresh: it walks the round's SpecificGameModeEventInterface events,
                //       resolves each through GuiCache::GetLandmarkInfoFromIndex (declared-only,
                //       BrnGuiCache.h:465, no body in src) and posts the 3088-byte record to
                //       GuiTracker::RecEvent @0x82501D28. Both callees are un-bodied here.
                //   (b) the meGameModeType 10/11 arm: mEventDestinationLandmarkIndex <- the
                //       round's first event index, then mEventDestinationDistrict <-
                //       WorldDataController::GetLandmarkInfoFromIndex(...)+50, behind the
                //       console's own `mpWorldDataController` (cpp:2126) and `lpLandmark`
                //       (cpp:2129) asserts. The ELSE half of that arm IS reproduced below.
                //   (c) the free-burn-lobby reset (meGameModeType 15/16 AND the wire's
                //       mbOnlineLobbyTransition byte clear): zeroes cache+47180 (8 words) and
                //       cache+47196[0..7], both inside unmodelled padding, then falls out of
                //       the arm early.
                // DELETE-WHEN GetLandmarkInfoFromIndex / UpdateTrackerInfo land and the
                // +47180 lobby run is carved.
                mEventDestinationDistrict      = 18;                     // stw  +0x9F50 (E_DISTRICT_INVALID)
                // word_82F27F00, read from the image rodata at VA 0x82F27F00: FF FF == the
                // invalid LandmarkIndex sentinel.
                mEventDestinationLandmarkIndex = 0xFFFFu;                // sth  +0x9F4C
            }
            else
            {
                // ---- the OFFLINE tail (@0x8250EBxx) ----------------------------------
                // The event's destination is checkpoint 0 when the mode carries checkpoints,
                // and the invalid pair otherwise.
                if (lpPrepare->mu8CheckpointCount != 0)
                {
                    mEventDestinationLandmarkIndex = lpPrepare->mau16CheckpointLandmark[0]; // sth +0x9F4C
                    mEventDestinationDistrict      = lpPrepare->maiCheckpointDistrict[0];   // stw +0x9F50
                }
                else
                {
                    mEventDestinationDistrict      = 18;      // stw +0x9F50 (E_DISTRICT_INVALID)
                    mEventDestinationLandmarkIndex = 0xFFFFu; // sth +0x9F4C (word_82F27F00)
                }
                // [FLAG deferred] `GuiCache::UpdateTrackerInfo(this, payload + 24, count)` --
                // the SAME un-bodied function the case-492 arm above already defers (there is
                // no UpdateTrackerInfo body anywhere in src). It feeds the landmark TRACKER
                // panel, not the event-info readout. DELETE-WHEN UpdateTrackerInfo lands;
                // this is its second call site.
            }

            miSatNavZoomLevel = 0;   // stw +0x803C -- both tails converge on this
            break;
        }

        case 77:
            // ADDITIVE (car-select wave 2026-08-02). The X360 switch (rebased by -4) reaches
            // `jumptable 8250DE3C case 77` at 0x8250EE20 and does exactly this: one
            // `lwz r11, 0(payload)` / `stw r11, 0x4B70(this)`. +0x4B70 is meCarSelectType,
            // which CarSelectMain::OnEnter and CarSelectVehicle read through
            // GetCurrentCarSelectType(). No producer for this event exists on PC yet -- see
            // the FLAG stand-in in Construct() above -- but wiring the consumer now means the
            // real producer needs no further change here.
            meCarSelectType = *reinterpret_cast<const s32*>(lpEvent);
            break;
        default:
            break;
        }
    }

    // @ 0x824F87B8 -- the hash-taking face.
    void GuiCache::AppendExpectedAptComponent(GuiFlow leFlow, u32 luComponentNameHash)
    {
        mStateLoadingHelper.AppendExpectedAptComponent(leFlow, luComponentNameHash);
    }

    // @ 0x824F87C0 -- the name-taking entry: walk the name to its NUL (the asm's
    // `while (*p++);` length measure), hash the bytes with the container CRC-32,
    // then register the hash. CalculateHash takes a mutable char*; the component
    // names are read-only ids so the cast is harmless (the hash never writes).
    void GuiCache::AppendExpectedAptComponent(GuiFlow leFlow, const char* lpacComponentName)
    {
        const u32 luComponentNameHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(lpacComponentName),
            static_cast<int>(std::strlen(lpacComponentName)));
        mStateLoadingHelper.AppendExpectedAptComponent(leFlow, luComponentNameHash);
    }

    // GetTimeStep -- no standalone X360 symbol (header-inline; the cache's leading
    // GuiEventTimeInfo delta word). Declared out-of-line in the committed header, so
    // the body lives here.
    f32 GuiCache::GetTimeStep() const
    {
        return mfTimeStep;
    }

    // The per-frame time latch (see the header note). Copies GUI event 26's two words into
    // the cache's leading GuiEventTimeInfo pair.
    void GuiCache::RecTimeInfo(const CgsGui::GuiEventTimeInfo* lpTimeInfo)
    {
        CGS_ASSERT(lpTimeInfo != 0, "lpTimeInfo");
        if (lpTimeInfo == 0)
            return;

        mfTimeStep = lpTimeInfo->GetTimeStep();
        mfTimeNow  = lpTimeInfo->GetTimeNow();
    }

    // @ X360 far member +0xB878 -- hand out the embedded player-options profile
    // block (the X360 callers inline the address computation; the accessor is the
    // named PC face of that far member). The block is reserved opaque in the
    // header (see the layout note there); it is typed here, in the one TU that
    // can safely see the profile header.
    OptionsDataProfile* GuiCache::GetOptionsDataProfile()
    {
        static_assert(sizeof(OptionsDataProfile) <= sizeof(mOptionsDataProfileStorage),
                      "OptionsDataProfile outgrew the cache's opaque reservation");
        return reinterpret_cast<OptionsDataProfile*>(mOptionsDataProfileStorage);
    }

    // @ 0x824EC008
    void StateLoadingHelper::IncrementUnloadPending()
    {
        ++muPendingUnloadCount;
        CGS_ASSERT(muPendingUnloadCount <= KU_MAX_RESOURCES_TO_WATCH,
                   "muPendingUnloadCount <= KU_MAX_RESOURCES_TO_WATCH");

        const u32 luRealPending = CountRealPendingUnloads(maResources, KU_MAX_RESOURCES_TO_WATCH);
        CGS_ASSERT(muPendingUnloadCount == luRealPending,
                   "Pending Unload count does not equal real pending unload count. "
                   "This is skipable but might crash with out of memory");
        (void)luRealPending;
    }

    // @ 0x824EC1E8
    void StateLoadingHelper::DecrementUnloadPending()
    {
        --muPendingUnloadCount;
        CGS_ASSERT(muPendingUnloadCount <= KU_MAX_RESOURCES_TO_WATCH,
                   "muPendingUnloadCount <= KU_MAX_RESOURCES_TO_WATCH");

        const u32 luRealPending = CountRealPendingUnloads(maResources, KU_MAX_RESOURCES_TO_WATCH);
        CGS_ASSERT(muPendingUnloadCount == luRealPending,
                   "Pending Unload count does not equal real pending unload count. "
                   "This is skipable but might crash with out of memory");
        (void)luRealPending;
    }

    // =====================================================================
    //  GuiCache scalar / pointer snapshot accessors.
    //
    //  Each is a thin read of one cached member, guarded by the game's debug
    //  assert (CGS_ASSERT is a no-op in this build, matching the X360 release
    //  assert machinery). Offsets / branch senses are taken straight from the
    //  X360 ARTIST asm; members are accessed BY NAME against the recovered
    //  GuiCache layout in BrnGuiCache.h.
    // =====================================================================

    // @ 0x8240F2C8
    f32 GuiCache::GetCurrentTimeInEvent() const
    {
        CGS_ASSERT(0.0f <= mfEventTime, "0.0f <= mfEventTime");
        return mfEventTime;
    }

    // @ 0x8240F330
    f32 GuiCache::GetTargetTimeInEvent() const
    {
        CGS_ASSERT(0.0f <= mfTargetTime, "0.0f <= mfTargetTime");
        return mfTargetTime;
    }

    // @ 0x8240F450
    s32 GuiCache::GetOpponentsInEvent() const
    {
        CGS_ASSERT(-1 < miOpponentsInEvent, "-1 < miOpponentsInEvent");
        return miOpponentsInEvent;
    }

    // @ 0x82472DB0 -- valid only in the race-style game modes; then asserts the
    // district was actually resolved (!= E_DISTRICT_INVALID, value 18).
    s32 GuiCache::GetEventDestinationDistrict() const
    {
        // asm @0x82472DD8..E08 skips the assert (beq) for meGameModeType in {0,1,10,6,8,5}.
        CGS_ASSERT(
            (meGameModeType == 0) || (meGameModeType == 1) || (meGameModeType == 10)
                || (meGameModeType == 6) || (meGameModeType == 8) || (meGameModeType == 5),
            "race-style game mode required for GetEventDestinationDistrict");
        // BrnWorld::E_DISTRICT_INVALID == 18 (asm immediate).
        CGS_ASSERT(mEventDestinationDistrict != 18,
                   "mEventDestinationDistrict != BrnWorld::E_DISTRICT_INVALID");
        return mEventDestinationDistrict;
    }

    // @ 0x824EC468
    s32 GuiCache::GetCheckpointReached() const
    {
        CGS_ASSERT(0 <= miCheckpointReached, "0 <= miCheckpointReached");
        return miCheckpointReached;
    }

    // @ 0x8240F4B0 -- road-rage only.
    s32 GuiCache::GetCurrentTakedownsInEvent() const
    {
        CGS_ASSERT(meGameModeType == 3, "BrnGameState::GsmIO::E_MODE_ROAD_RAGE == meGameModeType");
        CGS_ASSERT(-1 < miTakedownsCurrent, "-1 < miTakedownsCurrent");
        return miTakedownsCurrent;
    }

    // @ 0x8240F550 -- road-rage only.
    s32 GuiCache::GetTargetTakedownsInEvent() const
    {
        CGS_ASSERT(meGameModeType == 3, "BrnGameState::GsmIO::E_MODE_ROAD_RAGE == meGameModeType");
        CGS_ASSERT(-1 < miTakedownTarget, "-1 < miTakedownTarget");
        return miTakedownTarget;
    }

    // @ 0x8240F5F0
    s32 GuiCache::GetCurrentScoreInEvent() const
    {
        CGS_ASSERT(-1 < miScoreCurrent, "-1 < miScoreCurrent");
        return miScoreCurrent;
    }

    // @ 0x8240F650
    s32 GuiCache::GetTargetScoreInEvent() const
    {
        CGS_ASSERT(-1 < miScoreTarget, "-1 < miScoreTarget");
        return miScoreTarget;
    }

    // @ 0x8240F6B0
    s32 GuiCache::GetCurrentComboInEvent() const
    {
        CGS_ASSERT(-1 < miScoreCombo, "-1 < miScoreCombo");
        return miScoreCombo;
    }

    // @ 0x8240F710
    s32 GuiCache::GetMultiplierInEvent() const
    {
        CGS_ASSERT(-1 < miComboMultiplier, "-1 < miComboMultiplier");
        return miComboMultiplier;
    }

    // @ 0x8240F7F0 -- pursuit mode; the X360 tests the high word of the 8-byte id
    // for kCGSID_NULL (id != 0).
    CgsID GuiCache::GetPursuitCarID() const
    {
        CGS_ASSERT(meGameModeType == 4, "BrnGameState::GsmIO::E_MODE_PURSUIT == meGameModeType");
        CGS_ASSERT(mPursuedCarID != static_cast<CgsID>(0), "kCGSID_NULL != mPursuedCarID");
        return mPursuedCarID;
    }

    // @ 0x824B3060
    CgsID GuiCache::GetShutdownCarID() const
    {
        CGS_ASSERT(mShutdownCarID != static_cast<CgsID>(0), "kCGSID_NULL != mShutdownCarID");
        return mShutdownCarID;
    }

    // @ 0x824B30C0 -- E_UNLOCKTYPE_NONE is 0.
    s32 GuiCache::GetTrophyCarUnlockType() const
    {
        CGS_ASSERT(meTrophyCarUnlockType != 0,
                   "BrnProgression::TrophyUnlockData::E_UNLOCKTYPE_NONE != meTrophyCarUnlockType");
        return meTrophyCarUnlockType;
    }

    // @ 0x8240FC28 -- E_ROAD_PANEL_MODE_COUNT is 2.
    s32 GuiCache::GetActiveRoadRuleScoringMode() const
    {
        CGS_ASSERT(2 != meRoadRuleScoreMode,
                   "GuiEventSetRoadRuleScoreMode::E_ROAD_PANEL_MODE_COUNT != meRoadRuleScoreMode");
        return meRoadRuleScoreMode;
    }

    // @ 0x82472E78 -- the per-score-type road-rule live flags (@0xAC44). [H2 wave
    // 2026-08-25: body landed with the FBurnMainHudState WFInit sweep -- the H2 link
    // round caught the declaration-only state.]
    // (IsRoadRuleActive: this TU's assert-less copy retired 2026-08-27 -- the faithful body
    // with the console's two "Invalid score type" asserts lives in BrnGuiCache_wB_08.cpp,
    // which mounted this wave for ZoomSatNavOut; two definitions were LNK2005.)

    // The sat-nav renderer's world-camera lane (@0x4AE0; header note). [H2 wave
    // 2026-08-25: same link round -- the header promised "body links from the GuiCache
    // TU" but none had landed.]
    const Vector4& GuiCache::GetWorldCameraPosition() const
    {
        return mv4WorldCameraPosition;
    }

    // @ 0x8240F168
    const FreeburnChallengeManager* GuiCache::GetFreeburnChallengeManager() const
    {
        CGS_ASSERT(mpChallengeManager != nullptr, "mpChallengeManager");
        return mpChallengeManager;
    }

    // @ 0x82472D00
    const BrnResource::HudMessageController* GuiCache::GetHudMessageController() const
    {
        CGS_ASSERT(mpHudMessageController != nullptr, "mpHudMessageController");
        return mpHudMessageController;
    }

    // @ 0x82472D58
    const HudMessageDirector* GuiCache::GetHudMessageDirector() const
    {
        CGS_ASSERT(mpHudMessageDirector != nullptr, "mpHudMessageDirector");
        return mpHudMessageDirector;
    }

    // @ 0x8240F0B0 -- mTimeInfo.mfTimeNow (member at +4); guards the uninitialised sentinel.
    f32 GuiCache::GetTime() const
    {
        CGS_ASSERT(mfTimeNow != -3.4028235e38f, "mfTimeNow!=-FLT_MAX");
        return mfTimeNow;
    }

    // @ 0x8240F110
    WorldDataController* GuiCache::GetWorldDataController() const
    {
        CGS_ASSERT(mpWorldDataController != nullptr, "mpWorldDataController");
        // The X360 stores the controller as a non-const pointer the GUI mutates through;
        // the cache member is declared const, so cast away for the (non-const) accessor.
        return const_cast<WorldDataController*>(mpWorldDataController);
    }

    // X360-INLINED at BrnGuiCache.h:2310 -- BrnGui::GuiModule::Construct @0x82518028 fires
    // the "lpController" assert then stores the module's own WorldDataController into the
    // cache (`*(gm + 1021860) = gm + 307836`, i.e. cache+0x4064).
    void GuiCache::SetWorldDataController(WorldDataController* lpController)
    {
        CGS_ASSERT(lpController != nullptr, "lpController");   // BrnGuiCache.h:2310
        mpWorldDataController = lpController;
    }

    // @ (far member +40536) -- the active game-mode the GUI reads to pick mode-specific
    // apt key-frames. Plain read, no assert in the X360 path.
    s32 GuiCache::GetCurrentGameModeType() const
    {
        return meGameModeType;
    }

    // @ (far member +0x13B58 / 80728) -- the latched BrnGui::GuiEventCamStatus word.
    // Every X360 reader inlines a bare `lwzx` of this slot and tests it against zero; there
    // is no out-of-line accessor in the image, so this one exists purely to keep the
    // component TUs off a raw offset. Plain read, no assert (none of the fifteen X360 read
    // sites guards it).
    s32 GuiCache::GetCamStatus() const
    {
        return miCamStatus;
    }

    // [gateui] @ (far member +0x405C / 16476) -- the player profile the cache latches.
    // Same situation as GetCamStatus: there is NO out-of-line X360 accessor (every reader
    // inlines the `lwz mpGuiCache+0x405C`, e.g. OdometerComponent::Construct @0x82415088
    // and HudMessageAnalyzer::Update's 100%-viewed gate @0x825275xx), so this exists purely
    // to keep those TUs off a raw offset. Plain read, no assert -- none of the X360 read
    // sites guards it. The header has declared it since the odometer wave; the body was
    // never landed, which left HudMessageAnalyzer::Update unlinkable.
    const BrnProgression::Profile* GuiCache::GetProfile() const
    {
        return mpProfile;
    }

    // [H1] @ (far member +0x13B94 / 80788) -- the odometer's offline-distance readout
    // source (OdometerComponent::Update @0x82424160 is the attested reader; the header
    // member note carries the same cite). Same no-out-of-line-accessor situation as
    // GetProfile above: the header has declared it since the odometer wave, the body was
    // never landed, which left the odometer TU unlinkable the moment it was mounted.
    f32 GuiCache::GetDistanceDriven() const
    {
        return mfDistanceDriven;
    }

    // ---- the player-name string ids -------------------------------------------------
    // Namespace-scope .data const char* pointers on X360 (off_82F278AC / off_82F278B0),
    // shared with BrnGuiModule.cpp's UpdatePlayerName -- which is the writer side: it
    // AddString()s the live gamertag into the language database under exactly these two
    // ids (and falls back to the "DEFAULTPLAYERNAME" / "DEFAULTPLAYERNAMEQUOTED" database
    // entries, off_82F278B4 / off_82F278B8, when XUserGetName fails). Literals read from
    // BURNOUT_X360_ARTIST.XEX. They live here because this TU owns the two accessors; the
    // GuiModule TU picks them up by declaration when it grows UpdatePlayerName.
    const char* const KAPC_PLAYER_NAME_STRING_ID          = "PLAYER_NAME_STRING_ID";    // @0x8206E7DC
    const char* const KAPC_PLAYER_NAME_QUOTED_STRING_ID   = "PLAYER_NAME_STRING_ID_Q";  // @0x8206E7C4

    // @ 0x824EE7B0 -- `lwz r3, off_82F278AC; blr`. No `this` access, no assert.
    const char* GuiCache::GetPlayerName() const
    {
        return KAPC_PLAYER_NAME_STRING_ID;
    }

    // @ 0x824EE7C0 -- `lwz r3, off_82F278B0; blr`. No `this` access, no assert.
    const char* GuiCache::GetPlayerNameInQuotes() const
    {
        return KAPC_PLAYER_NAME_QUOTED_STRING_ID;
    }

    // The GuiCache layout pin (GuiCache::_AssertLayout) now lives ONCE, inline in
    // BrnGuiCache.h, as the comprehensive GC_FAR block that pins every asm-attested far member
    // relative to mv4WorldCameraPosition (plus the pointer-invariant prefix). The former,
    // narrower copy that lived here was a redefinition of that inline (C2084) and pinned a
    // strict subset of the same offsets, so it has been removed -- the header version is
    // canonical and supersedes it.

    // GetNumEventStarts @0x824F8830 is homed in BrnGuiCache_GetNumEventStarts.cpp --
    // it needs the REAL BrnGameStateSharedIO.h types this TU cannot include (see the
    // include-clash note at the top).
}
