#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"   // BrnGui::OptionsDataProfile (types the opaque +0xB878 reservation)
#include "GameShared/GameClasses/Containers/CgsHash.h" // CgsContainers::CgsHash::CalculateHash (AppendExpectedAptComponent name entry)
#include "GameShared/GameClasses/Core/CgsAssert.h"
// (BrnGameStateSharedIO.h must NOT be included here: its real BrnGameState /
// BrnNetwork types clash with BrnGuiOptionsDataProfile.h's compile-only slices.
// GetNumEventStarts, which needs the real SetUpAllEventStartsInterface, lives in
// its own TU -- BrnGuiCache_GetNumEventStarts.cpp.)

#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptObjectController.h"
#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h"

#include <cstring>   // std::memset (the ctor's zero-init of the unmodelled interior) / std::strlen

// Reconstructed from BURNOUT_X360_ARTIST.XEX. StateLoadingHelper tracks how many of
// its watched resources are pending an unload. Increment/Decrement adjust the count
// and then run a debug consistency check: the count must equal the number of resources
// actually in a pending-unload state (LOAD_CANCELLED / UNLOAD_REQUESTED / UNLOADING).
// The consistency loop feeds CGS_ASSERT, which is a no-op in this build (CgsAssert.h),
// matching the project convention for the X360 assert machinery.

namespace BrnGui
{
    namespace
    {
        // ARTIST off_82F278E0, read directly from BURNOUT_X360_ARTIST.XEX.i64.
        // StateLoadingHelper::Update indexes this 237-entry table by resource id.
        const char* const kapcGuiResourceNames[237] =
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
        static_assert(sizeof(kapcGuiResourceNames) / sizeof(kapcGuiResourceNames[0]) == 237,
                      "ARTIST GUI resource-name table must contain 237 entries");

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

            const char* lpacResourceName = kapcGuiResourceNames[luResourceId];

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

    void GuiCache::UnloadResources(const CgsGui::sResourceTuple* lpResources, u32 luCount)
    {
        for (u32 luResource = 0; luResource < luCount; ++luResource)
            mStateLoadingHelper.UnloadResource(lpResources[luResource]);
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

    // @ 0x8240F168
    const FreeburnChallengeManager* GuiCache::GetFreeburnChallengeManager() const
    {
        CGS_ASSERT(mpChallengeManager != nullptr, "mpChallengeManager");
        return mpChallengeManager;
    }

    // @ 0x82472D00
    const HudMessageController* GuiCache::GetHudMessageController() const
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

    // @ (far member +40536) -- the active game-mode the GUI reads to pick mode-specific
    // apt key-frames. Plain read, no assert in the X360 path.
    s32 GuiCache::GetCurrentGameModeType() const
    {
        return meGameModeType;
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
