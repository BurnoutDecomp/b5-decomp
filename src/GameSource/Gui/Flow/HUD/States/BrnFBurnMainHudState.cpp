#include "GameSource/Gui/Flow/HUD/States/BrnFBurnMainHudState.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log (deferral gap log)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/Flapt/BrnFlaptManager.h"                         // BrnFlapt::FlaptManager
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                         // BrnFlapt::FileRef
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"               // BrnFlapt::MovieClipInstance::ResetTimeline

#include <cstdio>    // std::snprintf (the one-shot deferral log)
#include <cstring>   // std::strcmp (the apt transition-name dispatch)

// BrnGui::FBurnMainHudState -- reconstructed from BURNOUT_X360_ARTIST.XEX (per-function
// addresses in the header). FBURN_MAIN: the free-drive main HUD. The state captures the
// GuiCache, loads the 42-entry HUD resource list (the B5RaceHud apt + its aux component
// imports), shows the FLAPT persistent HUD's "RaceMainHUD_mc" clip, mounts the
// "B5RaceHud" apt movie on the view channel, then runs the per-frame HUD event dispatch.
//
// COMPONENT DEFERRALS. The X360 state is a ~0x5350-byte aggregate of HUD components.
// The components whose TUs are reconstructed are driven for real (RoadRule,
// FriendsList + change icon, the Flapt animators, the DistrictMarker/InGameMessages
// slices). The components whose TUs are NOT yet reconstructed -- the SatNav body,
// OdometerComponent, JunctionInfoComponent, BoostMessageManager, and the
// DistrictMarker/InGameMessages Construct/Prepare halves -- are deferred: each call
// site keeps the X360 control flow (the gate bytes, the event routing) and logs the
// gap once instead of inventing a body. Reconstructing those TUs (Slice B in
// scratch/hud_fburn_plan.md) drops straight into these call sites.
namespace BrnGui
{
    namespace
    {
        const s32 KI_CHANNEL_GUI_OUT        = 40;  // GuiEventOut
        const s32 KI_CHANNEL_VIEW_STATE     = 41;  // GuiOutViewState
        const s32 KI_CHANNEL_INTERNAL_STATE = 42;  // the internal-state mirror channel

        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ARTIST event-64 payload (the per-frame cache event).
        struct GuiEventCache : public CgsModule::Event
        {
            GuiCache* mpGuiCache;
        };

        // 16-byte GuiEvent<N> command { 1, N, 12, flag } (the shared state-channel record).
        template <s32 N>
        struct GuiCommandEvent16 : public CgsGui::GuiEvent<N>
        {
            u8 mu8Flag;
            u8 maPad[3];
            GuiCommandEvent16(u8 lu8Flag = 0) : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag)
            { maPad[0] = maPad[1] = maPad[2] = 0; }
        };

        template <s32 N>
        void PostCommand16(CgsGui::StateInterface* lpInterface, s32 liChannel, u8 lu8Flag = 0)
        {
            GuiCommandEvent16<N> lEvent(lu8Flag);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), liChannel, 16);
        }

        // 20-byte GuiEvent<25> "set option" record { 8, 25, 12, reserved, 1.0f } (the
        // view-option record every boot/HUD state posts on entry; X360 OnEnter posts
        // reserved=1).
        struct GuiOptionEvent20 : public CgsGui::GuiEvent<25>
        {
            s32 miReserved;   // +0x0C
            f32 mfValue;      // +0x10 (1.0f)
            explicit GuiOptionEvent20(s32 liReserved)
                : CgsGui::GuiEvent<25>(8, 12), miReserved(liReserved), mfValue(1.0f) {}
        };

        // 20-byte GuiEvent<18> "apt name + level" record { 8, 18, 12, name, level } --
        // the view-channel mount/unmount post (same record BrnBootProfile/BootLegal use).
        struct GuiAptNameFlagEvent20 : public CgsGui::GuiEvent<18>
        {
            const char* mpacAptName;   // +0x0C
            s32         miLevel;       // +0x10
            GuiAptNameFlagEvent20(const char* lpacAptName, s32 liLevel)
                : CgsGui::GuiEvent<18>(8, 12), mpacAptName(lpacAptName), miLevel(liLevel) {}
        };

        // 24-byte GuiEvent<213> show/hide record { 12, 213, 12, show, f32, flag } (the
        // X360 UpdateWFInit/UpdateRunning "ShowHideSatNav/BoostBar" family posts; the
        // typed OutputViewState<T> instantiations reduce to these records).
        struct GuiShowHideEvent24 : public CgsGui::GuiEvent<213>
        {
            s32 miShow;      // +0x0C
            f32 mfValue;     // +0x10
            u8  mu8Flag;     // +0x14
            u8  maPad[3];
            GuiShowHideEvent24(s32 liShow, f32 lfValue, u8 lu8Flag)
                : CgsGui::GuiEvent<213>(12, 12), miShow(liShow), mfValue(lfValue), mu8Flag(lu8Flag)
            { maPad[0] = maPad[1] = maPad[2] = 0; }
        };

        void PostShowHide24(CgsGui::StateInterface* lpInterface, s32 liChannel,
                            s32 liShow, f32 lfValue, u8 lu8Flag)
        {
            GuiShowHideEvent24 lEvent(liShow, lfValue, lu8Flag);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), liChannel, 24);
        }

        // ---- GuiCache boundary (un-named cache far fields the X360 pokes) ----------
        // The same idiom as BrnBootProfile.cpp's cache boundary: each helper stands in
        // for one X360 cache field that BrnGuiCache.h has not yet named. The offsets
        // are recorded so the eventual cache-member naming lands here.

        // FLAG PC-platform leaf: the live game-mode word (X360 cache+19256; -1 == no
        // mode). Un-named in the cache recon; report a live mode so the free-drive HUD
        // shows instead of bouncing to PAUSE on the PC boot path (the console value is
        // game-side state the PC world does not populate yet).
        s32 GuiCache_GetGameModeWord(const GuiCache* /*lpGuiCache*/)
        {
            return 0;
        }

        // FLAG PC-platform leaf: the pause-override word (X360 cache+80784); un-named.
        bool GuiCache_GetPauseOverride(const GuiCache* /*lpGuiCache*/)
        {
            return false;
        }

        // FLAG PC-platform leaf: the player engine state (X360 cache word +19220;
        // 0 == E_ENGINE_OFF, 1 == E_ENGINE_ON). Un-named; the PC boot path has no
        // vehicle yet, so the engine reads OFF (the HUD takes the "invisible" arm).
        s32 GuiCache_GetPlayerEngineState(const GuiCache* /*lpGuiCache*/)
        {
            return 0;
        }

        // FLAG PC-platform leaf: the hud-ready byte (X360 cache+16496 := 1); un-named.
        void GuiCache_SetHudReady(GuiCache* /*lpGuiCache*/)
        {
        }

        // FLAG PC-platform leaf: the friends-list overlay-active word (X360
        // cache+47212); un-named. No live overlay on the PC boot path.
        bool GuiCache_FriendsListOverlayActive(const GuiCache* /*lpGuiCache*/)
        {
            return false;
        }

        // FLAG PC-platform leaf: the friends-list input-block byte (X360 cache+19287);
        // un-named. Input is never blocked on the PC boot path.
        bool GuiCache_FriendsListInputBlocked(const GuiCache* /*lpGuiCache*/)
        {
            return false;
        }

        // FLAG PC-platform leaf: the friends-list change-pending byte (X360
        // cache+46870); un-named.
        bool GuiCache_FriendsListChangePending(const GuiCache* /*lpGuiCache*/)
        {
            return false;
        }

        // FLAG PC-platform leaf: the boost-bar config word (X360 cache+19232; 1 ==
        // boost bar enabled); un-named. Report enabled -- the free-drive default.
        s32 GuiCache_GetBoostBarConfig(const GuiCache* /*lpGuiCache*/)
        {
            return 1;
        }

        // FLAG PC-platform leaf: the game-mode-type word (X360 cache+40536; -1 ==
        // offline/none); un-named.
        s32 GuiCache_GetGameModeType(const GuiCache* /*lpGuiCache*/)
        {
            return -1;
        }

        // One-shot deferral log: the un-reconstructed component TUs this state drives.
        // Slice-B reconstruction replaces each call site; until then the gap stays
        // visible in the log instead of silently vanishing.
        void LogDeferredComponent(const char* lpacComponent)
        {
            static bool sbLogged[8];
            static const char* sapcNames[8];
            for (s32 li = 0; li < 8; ++li)
            {
                if (sapcNames[li] == lpacComponent)
                    return;
                if (sapcNames[li] == 0)
                {
                    sapcNames[li] = lpacComponent;
                    sbLogged[li]  = true;
                    char lac[128];
                    std::snprintf(lac, sizeof(lac),
                                  "[FBurnMainHud] %s -- component TU deferred (Slice B).\n",
                                  lpacComponent);
                    CgsDev::Log::WriteToLog(lac);
                    return;
                }
            }
            (void)sbLogged;
        }
    }

    // =======================================================================
    //  The static .rdata tables (values read from the XEX image; see
    //  scratch/fburn_rdata*.txt for the raw dump + name resolution)
    // =======================================================================

    // @ 0x82F26230 (count @ 0x82F2622C == 42): the B5RaceHud apt movie plus its aux
    // component imports (type 7 == GUIAPT aux import; type 11 == texture). The name
    // comments are off_82F278E0[id] from the image.
    const CgsGui::sResourceTuple FBurnMainHudState::maResourcesToLoad[] =
    {
        { 192u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5RaceHud
        {  29u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // TextField
        {  32u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // Timer
        {  23u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5NorthIndicatorComponent
        {  37u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5HudMessage
        {  27u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // CountdownIcon
        { 200u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5SatNavComponent
        {  25u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // SatNavDistance
        {  26u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // SatNavStatic
        { 199u, CgsGui::E_GUI_RESOURCETYPE_LOCALISED_TEXT },  // SatNavMap
        { 201u, CgsGui::E_GUI_RESOURCETYPE_LOCALISED_TEXT },  // SatNavMask
        {  60u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // BoostMessage
        {  56u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5Triggers
        {  62u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5PositionIndicatorComponent
        {  64u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // DistrictIcon
        {  65u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // DistrictMarker
        {  33u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5RoadRuleComponent
        {  40u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5CustomComponentTexture
        {  22u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5ComponentUnity
        {  38u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5CrashedHudMessages
        {  39u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5PreRaceMessageComponent
        {  36u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5MenuItem
        {  58u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5ScrollableSelection
        {  59u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5HelpItem
        {  41u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5MapCursor
        {  44u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5ProgressBar
        {  45u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5MugShot
        {  47u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // Ticker
        {  34u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5MenuToggle
        {  57u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // Toggle
        {  61u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5ControllerButtons
        {  63u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5HelperComponents
        {  69u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5ColourSelector
        {  73u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5RaceEventInfo
        {  74u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5SatNavOverlay
        {  75u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5PositionTableComponent
        {  76u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5FriendList
        {  81u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5PaybackComponent
        {  84u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5RoadSigns
        {  87u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5ShowTimeBar
        {  89u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5JunctionInfoComponent
        {  90u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },   // B5VersionTextComponent
    };
    const u32 FBurnMainHudState::muNumResourcesToLoad = 42;

    // @ 0x8205AED8: the 56 event ids OnEnter registers (values from the image).
    const s32 FBurnMainHudState::maiEventToObserve[] =
    {
          6,  21, 199, 200,  79, 154, 156, 224, 205, 148, 158, 226, 227,  64,
        206, 377, 379, 218, 367, 382, 368, 383, 384, 385, 386, 387, 388, 389,
        390, 391, 365, 364, 394, 401, 400, 333, 335, 336, 338, 339, 340, 341,
        343, 101, 102, 103, 104,  94,  95, 106, 221, 222, 311, 314, 309, 350,
    };
    const s32 FBurnMainHudState::miNumEventsObserved = 56;

    // =======================================================================
    //  OnEnter  @ 0x8247B0E8
    // =======================================================================
    // Reset the phase machine, register the 56 observed events, post the view-option
    // record, resolve + show the FLAPT persistent HUD's "RaceMainHUD_mc" clip, then
    // construct/prepare the HUD components against the FLAPT file and post the entry
    // records ({1,94 ch40}, {1,215 flag1 ch41}, {1,96 flag1 ch40}, {1,308 ch40}).
    void FBurnMainHudState::OnEnter()
    {
        meInternalState        = E_INTERNALSTATE_SETUP;
        mpGuiCache             = 0;
        mbFriendsListEnabled   = false;

        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        GuiOptionEvent20 lOption(1);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lOption), KI_CHANNEL_VIEW_STATE, 20);

        // The FLAPT persistent HUD file: access pointers -> FlaptManager -> file 0 ->
        // root movie clip -> the RaceMainHUD_mc child, shown + timeline-reset.
        CgsGui::GuiAccessPointers* lpAccessPointers = mpStateInterface->GetAccessPointers();
        CGS_ASSERT(lpAccessPointers != 0, "mpAccessPointers != NULL");             // h:344
        BrnFlapt::FlaptManager* lpFlaptManager = lpAccessPointers->GetFlaptManager();
        CGS_ASSERT(lpFlaptManager != 0, "NULL != mpFlaptManager");                 // CgsGuiShared.h:194

        BrnFlapt::FileRef lFile;
        lpFlaptManager->GetFile(&lFile, 0);
        BrnFlapt::MovieClipRef lRootClip;
        lFile.GetRootMovieClip(&lRootClip);
        lRootClip.FindChildMovieClip(&mRaceMainHudClip, "RaceMainHUD_mc");
        mRaceMainHudClip.SetVisible(true);
        CGS_ASSERT(mRaceMainHudClip.IsValid(), "mpMovieClipInst");                 // BrnFlaptMovieClipRef.h:272
        mRaceMainHudClip.mpMovieClipInst->ResetTimeline();

        // X360: GuiModuleSerialiser::GetStaticLayout(serialiser)->EndMessage() -- close
        // any static-layout message left open by the previous state.
        // FLAG deferred (Slice B): the BrnReplays::GuiModuleSerialiser recon does not
        // yet surface GetStaticLayout/EndMessage; the layout stream is host-side inert.

        // ---- component construction against the FLAPT file ----------------------
        // FLAG deferred (Slice B): SatNavComponent::Construct(+0x160, iface, 0, 0) --
        // the SatNav body TU is not reconstructed.
        LogDeferredComponent("SatNavComponent");

        // FLAG deferred (Slice B): InGameMessagesComponent::Construct("hudMessages_mc")
        // + SetInGameMessagesQueue(cache+16512) + Prepare -- the ctor/prepare half of
        // that TU is not reconstructed (the setters are; driven in UpdateSetupState).
        LogDeferredComponent("InGameMessagesComponent");

        // FLAG deferred (Slice B): DistrictMarkerComponent::Construct/Prepare
        // ("marker_mc") -- only the SetHideCountyIcon slice is reconstructed.
        LogDeferredComponent("DistrictMarkerComponent");

        mbDistrictRefreshArmed = true;   // X360 +0x8AC := 1

        // FLAG deferred (Slice B): BoostMessageManager::Construct/Prepare
        // ("BoostManager") -- TU not reconstructed.
        LogDeferredComponent("BoostMessageManager");

        // The "EventHud_Animator" pair + the road-rule/friends components are real.
        mEventHudAnimatorIcon.Construct("EventHud_Animator", mpStateInterface, 0);
        mEventHudAnimator.Construct(0, mpStateInterface, 0);
        mEventHudAnimator.Prepare("EventHud_Animator", lFile, 0);

        // FLAG deferred (Slice B): RoadRuleComponent::Construct/Prepare
        // ("RoadRule_mc") -- the committed road-rule TU carries the sign-state/
        // selection slice only (no lifecycle bodies yet).
        LogDeferredComponent("RoadRuleComponent");

        // FLAG deferred (Slice B): FriendsListComponent::Construct/Prepare
        // ("friendList") -- the committed FriendsList TU is the SetGuiCachePointer/
        // SetDirty slice only.
        LogDeferredComponent("FriendsListComponent");
        mFriendsListChangeIcon.Construct("FriendListChange_mc", mpStateInterface, 0);
        mFriendsListChangeIcon.Prepare("FriendListChange_mc", lFile);

        PostCommand16<94>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);

        // FLAG deferred (Slice B): JunctionInfoComponent::Construct/Prepare
        // ("JunctionInfo_mc", -1) and OdometerComponent::Construct/Prepare
        // ("Odometer_mc", -1) -- TUs not reconstructed.
        LogDeferredComponent("JunctionInfoComponent");
        LogDeferredComponent("OdometerComponent");

        mIdentAnimator.Construct("Ident_Animator", mpStateInterface, 0);
        mIdentAnimator.Prepare("Ident_Animator", lFile, 0);

        mbPpToggleActive    = false;   // +0x53C8 := 0
        miPpToggleRunCount  = 0;       // +0x53D0 := 0
        mfPpToggleNextTime  = 0.0f;    // +0x53CC := 0.0
        mfBoostAmountPrev   = 0.0f;    // +0x53D4 := 0.0

        PostCommand16<215>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1);
        PostCommand16<96>(mpStateInterface, KI_CHANNEL_GUI_OUT, 1);
        PostCommand16<308>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);

        mbTrophyUnlockScanned = false;   // +0x53D8 := 0
    }

    // =======================================================================
    //  OnLeave  @ 0x82480B88
    // =======================================================================
    void FBurnMainHudState::OnLeave()
    {
        meInternalState = E_INTERNALSTATE_LEAVING;   // X360 +0x38 := 4

        if (mbFriendsListEnabled)
        {
            CgsGui::GuiAccessPointers* lpAccessPointers = mpStateInterface->GetAccessPointers();
            CGS_ASSERT(lpAccessPointers != 0, "mpAccessPointers != NULL");
            GuiCache* lpGuiCache = static_cast<GuiCache*>(lpAccessPointers->GetGuiCache());
            CGS_ASSERT(lpGuiCache != 0, "mpGuiCache");
            if (GuiCache_FriendsListOverlayActive(lpGuiCache))
            {
                /* FLAG deferred (Slice B): FriendsList Close */;
            }
        }

        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // Unmount the HUD apt: the empty-name level-1 record on the view channel.
        GuiAptNameFlagEvent20 lUnmount("", 1);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lUnmount), KI_CHANNEL_VIEW_STATE, 20);

        mRaceMainHudClip.SetVisible(false);

        // "transout" across the animator set. The two DistrictMarker sub-animators
        // (X360 +0x840/+0x854 vtbl slot +12) ride the deferred marker TU.
        mEventHudAnimator.Run("transout");
        // FLAG deferred (Slice B): JunctionInfoComponent::Run("transout") +
        // OdometerComponent::TransOutActiveText -- TUs not reconstructed.

        PostShowHide24(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1, 0.0f, 0);   // {12,213,flag0}

        if (mbSatNavEnabled)
        {
            // FLAG deferred (Slice B): SatNav RecvEvent(213) + Destruct.
        }
        if (mbRoadRulesEnabled)
        {
            /* FLAG deferred (Slice B): RoadRule EndTimers */;
        }

        PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 0);
        PostCommand16<215>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 0);
        PostCommand16<94>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);

        // { 2, 536, 12, hi256 } -- the X360 posts the 16-byte type-2 record id 536
        // with the 0x0100 half-word flag.
        {
            struct GuiEvent536 : public CgsGui::GuiEvent<536>
            {
                u16 mu16Pad;
                u16 mu16Flag;
                GuiEvent536() : CgsGui::GuiEvent<536>(2, 12), mu16Pad(0), mu16Flag(256) {}
            } lEvent;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), KI_CHANNEL_GUI_OUT, 16);
        }

        // { 12, 204, 12, 5, 6, ... } -- the 24-byte id-204 record with the 5/6 pair.
        {
            struct GuiEvent204 : public CgsGui::GuiEvent<204>
            {
                s32 miValueA;   // 5
                s32 miValueB;   // 6
                s32 miPad;
                GuiEvent204() : CgsGui::GuiEvent<204>(12, 12), miValueA(5), miValueB(6), miPad(0) {}
            } lEvent;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), KI_CHANNEL_VIEW_STATE, 24);
        }
    }

    // =======================================================================
    //  Update -- the phase dispatch (header-inline on X360, not exported; composed
    //  from the phase bodies' advance returns; UpdatePermenant runs every frame)
    // =======================================================================
    void FBurnMainHudState::Update()
    {
        switch (meInternalState)
        {
        case E_INTERNALSTATE_SETUP:
            if (UpdateSetupState())
                meInternalState = E_INTERNALSTATE_LOADING;
            break;
        case E_INTERNALSTATE_LOADING:
            if (UpdateLoading())
                meInternalState = E_INTERNALSTATE_WFINIT;
            break;
        case E_INTERNALSTATE_WFINIT:
            if (UpdateWFInit())
                meInternalState = E_INTERNALSTATE_RUNNING;
            break;
        case E_INTERNALSTATE_RUNNING:
            UpdateRunning();
            break;
        default:
            break;
        }

        UpdatePermenant();

        // The pumps read without consuming; the state clears its in-queue at frame end
        // (the shared boot/HUD-state idiom).
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue != 0)
            lpInQueue->Clear();
    }

    // =======================================================================
    //  UpdateSetupState  @ 0x82480EA0
    // =======================================================================
    bool FBurnMainHudState::UpdateSetupState()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)
            return false;

        mpGuiCache = 0;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == 64)
            {
                GuiCache* lpCache = reinterpret_cast<const GuiEventCache*>(lpEvent)->mpGuiCache;
                CGS_ASSERT(lpCache != 0, "Invalid cache in FBurnMainHudState::OnEnter");   // cpp:1322
                mpGuiCache = lpCache;
            }
        }

        if (mpGuiCache == 0)
            return false;

        // The pause gate: no live game mode and no override -> hand the FSM "PAUSE"
        // (X360 cache words +19256 / +80784).
        if (GuiCache_GetGameModeWord(mpGuiCache) == -1 && !GuiCache_GetPauseOverride(mpGuiCache))
        {
            SendStateEvent("PAUSE");
            return false;
        }

        mbInGameMessagesEnabled = true;   // +333
        mbEnable14E             = true;   // +334
        mbBoostMessagesEnabled  = true;   // +335
        mbRoadRulesEnabled      = true;   // +336
        mbFriendsListEnabled    = true;   // +337
        mbJunctionInfoEnabled   = true;   // +338
        mbOdometerEnabled       = true;   // +339
        mbPpToggleEnabled       = true;   // +340
        mbDistrictMarkerEnabled = true;   // +341
        mbSatNavEnabled         = true;   // +332

        // FLAG deferred (Slice B): SatNavComponent::SetCachePointer + the satnav
        // show/filter words from cache+32820/+32824 and the Enable/Disable events
        // filter swap -- the SatNav body TU is not reconstructed.
        miSatNavShowState    = 0;
        miSatNavEventsFilter = 0;

        PostCommand16<555>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);

        if (mbInGameMessagesEnabled)
        {
            // FLAG deferred (Slice B): InGameMessages SetController/SetDirector wiring
            // needs GuiCache::GetHudMessageController/GetHudMessageDirector, not yet
            // surfaced by the cache recon; SetGameMode likewise reads cache+40536.
        }

        if (mbRoadRulesEnabled)
        {
            // FLAG deferred (Slice B): RoadRuleComponent::SetCachePointer +
            // InitialiseMode -- the component member itself is deferred (see the
            // header's absent-member note).
        }

        return true;
    }

    // =======================================================================
    //  UpdateLoading  @ 0x8247C640
    // =======================================================================
    bool FBurnMainHudState::UpdateLoading()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache != NULL");
        if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
            return false;

        if (mbSatNavEnabled)
        {
            // FLAG deferred (Slice B): SatNavComponent::LoadResources.
        }

        // Mount the HUD apt movie: { 8, 18, 12, "B5RaceHud", 1 } on the view channel
        // (X360 off_82F27BE0[0] == "B5RaceHud").
        GuiAptNameFlagEvent20 lMount("B5RaceHud", 1);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lMount), KI_CHANNEL_VIEW_STATE, 20);

        SetExpectedAptComponentList();
        return true;
    }

    // =======================================================================
    //  SetExpectedAptComponentList  @ 0x82475328
    // =======================================================================
    // Clear the flow-1 watcher and install an EMPTY 64-slot list (count 0) -- the
    // freeburn HUD has no per-component init handshake.
    void FBurnMainHudState::SetExpectedAptComponentList()
    {
        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_HUD);
        static const u32 sauEmptyHashes[64] = { 0 };
        mpGuiCache->SetExpectedAptComponentList(E_GUIFLOW_HUD, sauEmptyHashes, 0);
    }

    // =======================================================================
    //  UpdateWFInit  @ 0x8247C710
    // =======================================================================
    bool FBurnMainHudState::UpdateWFInit()
    {
        if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_HUD))
            return false;

        if (mbRoadRulesEnabled)
        {
            // { 8, 327, 16 } on the gui-out channel, then the road-rule begin sweep.
            struct GuiEvent327 : public CgsGui::GuiEvent<327>
            {
                s32 miPadA;
                s32 miPadB;
                s32 miPadC;
                GuiEvent327() : CgsGui::GuiEvent<327>(8, 16), miPadA(0), miPadB(0), miPadC(0) {}
            } lEvent;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), KI_CHANNEL_GUI_OUT, 24);

            for (s32 leEnumIndex = 0; leEnumIndex < 2; ++leEnumIndex)
            {
                // FLAG deferred (Slice B): GuiCache::IsRoadRuleActive(index) +
                // RoadRuleComponent::HandleRoadRuleBegin -- neither surfaced yet.
            }
        }

        // The player engine state (X360 cache word +19220; assert < 2).
        const s32 liEngineState = GuiCache_GetPlayerEngineState(mpGuiCache);
        CGS_ASSERT(liEngineState < 2,
                   "( GuiPlayerEngineEvent::E_ENGINE_OFF == mpCache->GetPlayerEngineState( )) || ( GuiPlayerEngineEvent::E_ENGINE_ON == mpCache->GetPlayerEngineState( ))");   // cpp:1536

        if (liEngineState == 1)
        {
            PostShowHide24(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1, 0.0f, 1);      // {12,213,flag1}
            PostShowHide24(mpStateInterface, KI_CHANNEL_INTERNAL_STATE, 1, 0.0f, 1);  // ch42 mirror
            mEventHudAnimator.Run("visible");
            PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1);
        }
        else
        {
            mEventHudAnimator.Run("invisible");
            PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 0);
            PostShowHide24(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1, 0.0f, 0);      // {12,213,flag0}
            PostShowHide24(mpStateInterface, KI_CHANNEL_INTERNAL_STATE, 1, 0.0f, 0);
        }

        // FLAG deferred (Slice B): SatNav RecvEvent(213) mirror of the show record.

        GuiCache_SetHudReady(mpGuiCache);   // X360 cache byte +16496 := 1

        if (mbFriendsListEnabled)
        {
            mFriendsList.SetGuiCachePointer(mpGuiCache);
            if (GuiCache_FriendsListChangePending(mpGuiCache))
                mFriendsListChangeIcon.ShowNow();
            /* FLAG deferred (Slice B): FriendsList AttemptStateRestore */;
        }

        // X360 byte +0x155-gated mirror of cache byte +19264 into +0x89A rides the
        // deferred marker/messages slice.

        if (mbOdometerEnabled)
        {
            // FLAG deferred (Slice B): OdometerComponent::TransIn.
        }

        return true;
    }

    // =======================================================================
    //  UpdateRunning  @ 0x8247B660 -- the RUNNING per-frame event dispatch
    // =======================================================================
    void FBurnMainHudState::UpdateRunning()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)
            return;

        if (mbSatNavEnabled)
        {
            // FLAG deferred (Slice B): the per-frame satnav pre-pass (word +656 := 0 +
            // the satnav sub-object's +2448 clear).
        }

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            const s32* lpiPayload = reinterpret_cast<const s32*>(lpEvent);
            switch (liEventId)
            {
            case 6:      // controller input
                if (mbFriendsListEnabled && !GuiCache_FriendsListInputBlocked(mpGuiCache))
                    /* FLAG deferred (Slice B): FriendsList HandleControllerInput */;
                break;
            case 79:
                if (mbFriendsListEnabled && GuiCache_FriendsListOverlayActive(mpGuiCache))
                    /* FLAG deferred (Slice B): FriendsList Close */;
                break;
            case 94:
                if (mbFriendsListEnabled)
                    mFriendsListChangeIcon.Hide();
                break;
            case 95:
                if (mbFriendsListEnabled)
                    /* FLAG deferred (Slice B): FriendsList EndWait */;
                break;
            case 101:
                if (mbFriendsListEnabled)
                    /* FLAG deferred (Slice B): FriendsList SetTotalFriends */;
                break;
            case 102:
                if (mbFriendsListEnabled)
                    /* FLAG deferred (Slice B): FriendsList ProcessNewEntryData */;
                break;
            case 103:
                if (mbFriendsListEnabled)
                    /* FLAG deferred (Slice B): FriendsList RequestRefreshedData */;
                break;
            case 104:
                if (mbFriendsListEnabled && !GuiCache_FriendsListInputBlocked(mpGuiCache))
                    /* FLAG deferred (Slice B): FriendsList ReshowShortcuts */;
                break;
            case 106:
                if (mbFriendsListEnabled && !GuiCache_FriendsListOverlayActive(mpGuiCache))
                    mFriendsListChangeIcon.AnimateIn();
                break;
            case 154:
                if (mbInGameMessagesEnabled)
                {
                    // FLAG deferred (Slice B): InGameMessagesComponent::AddMessage.
                }
                break;
            case 156:
                if (mbInGameMessagesEnabled)
                {
                    // FLAG deferred (Slice B): InGameMessagesComponent::TerminateMessages.
                }
                break;
            case 199:
            case 200:
                UpdateSatNav(lpEvent, liEventId);
                break;
            case 205:
                // Show/hide satnav passthrough: view record + the satnav mirror.
                PostShowHide24(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1, 0.0f,
                               static_cast<u8>(lpiPayload[0] != 0));
                // FLAG deferred (Slice B): SatNav RecvEvent(213).
                break;
            case 206:
                ProcessBoostInfo(lpEvent);
                break;
            case 221:   // boost amount changed (f32)
            {
                const f32 lfBoost = *reinterpret_cast<const f32*>(lpiPayload);
                if (mfBoostAmountPrev != lfBoost)
                {
                    if (lfBoost >= 0.0099999998f)
                    {
                        mEventHudAnimator.Run("invisible");
                        PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 0);
                        PostShowHide24(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1, 0.0f, 0);
                        PostShowHide24(mpStateInterface, KI_CHANNEL_INTERNAL_STATE, 1, 0.0f, 0);
                        // FLAG deferred (Slice B): SatNav RecvEvent(213).
                    }
                    else if (GuiCache_GetBoostBarConfig(mpGuiCache) == 1)
                    {
                        mEventHudAnimator.Run("visible");
                        PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1);
                        PostShowHide24(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1, 0.0f, 1);
                        PostShowHide24(mpStateInterface, KI_CHANNEL_INTERNAL_STATE, 1, 0.0f, 1);
                        // FLAG deferred (Slice B): SatNav RecvEvent(213).
                    }
                    mfBoostAmountPrev = lfBoost;
                }
                break;
            }
            case 222:   // the PP-toggle (X360 assert "lpPPToggle" cpp:909)
                if (mbPpToggleEnabled)
                {
                    CGS_ASSERT(lpEvent != 0, "lpPPToggle");
                    if (lpiPayload[0] == 1)
                    {
                        mIdentAnimator.Run("transIn");
                        mbPpToggleActive   = true;
                        miPpToggleRunCount = 1;
                        mfPpToggleNextTime = mpGuiCache->GetTime() + 32.0f;
                    }
                    else
                    {
                        mIdentAnimator.Run("invisible");
                        mfPpToggleNextTime = 0.0f;
                        mbPpToggleActive   = false;
                        miPpToggleRunCount = 0;
                    }
                }
                break;
            case 226:
                PostCommand16<60>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 0);
                break;
            case 227:
                PostCommand16<61>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 0);
                break;
            case 311:
                if (mbJunctionInfoEnabled)
                {
                    // FLAG deferred (Slice B): JunctionInfo HandleJunctionChange
                    // (cache words +19192/+19196) + DistrictMarker SetHideCountyIcon.
                    mDistrictMarker.SetHideCountyIcon(mbCountyIconHidden);
                }
                if (mbOdometerEnabled)
                {
                    // FLAG deferred (Slice B): Odometer HandleJunctionChange.
                }
                break;
            case 314:
                if (mbOdometerEnabled)
                {
                    // FLAG deferred (Slice B): Odometer HandleDriveThruDiscovered.
                }
                break;
            case 333:
            case 335:
            case 336:
            case 338:
            case 339:
            case 340:
            case 341:
            case 343:
                if (mbRoadRulesEnabled)
                {
                    // FLAG deferred (Slice B): the RoadRule event handlers
                    // (HandleEnterRoadEvent / HandleRoadRuleBegin / HandleRoadRuleEnd /
                    // UpdateCurrentTime / HandleRoadRuleTargetUpdate /
                    // HandleLeaveRoadEvent / HandleUpcomingRoadEvent / SwitchModes) --
                    // not in the committed road-rule slice yet; the lap words
                    // (+0x102C/+0x1034) ride with UpdateCurrentTime.
                    LogDeferredComponent("RoadRuleComponent-handlers");
                }
                break;
            case 350:   // progression loaded { Profile*, ProgressionData* }
                // FLAG deferred (Slice B): the trophy-unlock scan
                // (BrnProgression::ProgressionData::GetTrophyUnlock / Profile::FindCar /
                // GetSeenTrophyUnlockSequence -> GuiEventTrophyCarUnlock) -- the
                // progression accessors are not reconstructed. The one-shot latch is
                // kept so the scan runs once when it lands.
                mbTrophyUnlockScanned = true;
                break;
            case 218:
            case 364: case 365: case 367: case 368:
            case 382: case 383: case 384: case 385: case 386: case 387:
            case 388: case 389: case 390: case 391:
            case 394: case 400: case 401:
                CGS_ASSERT(mpGuiCache != 0, "mpCache != NULL");   // cpp:595
                if (mbBoostMessagesEnabled)
                {
                    // FLAG deferred (Slice B): BoostMessageManager::RecvEvent(id).
                }
                break;
            case 379:   // engine state changed
                CGS_ASSERT(lpiPayload[0] < 2,
                           "( GuiPlayerEngineEvent::E_ENGINE_OFF == lpEngineChange->meNewEngineState ) || ( GuiPlayerEngineEvent::E_ENGINE_ON == lpEngineChange->meNewEngineState )");   // cpp:772
                if (lpiPayload[0] == 1)
                {
                    mEventHudAnimator.Run("transin");
                    PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1);
                }
                else
                {
                    mEventHudAnimator.Run("transout");
                    PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 0);
                }
                PostShowHide24(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1, 0.0f,
                               static_cast<u8>(lpiPayload[0] == 1));
                PostShowHide24(mpStateInterface, KI_CHANNEL_INTERNAL_STATE, 1, 0.0f,
                               static_cast<u8>(lpiPayload[0] == 1));
                // FLAG deferred (Slice B): SatNav RecvEvent(213).
                break;
            default:
                break;
            }
        }

        // ---- the per-frame component ticks --------------------------------------
        if (mbDistrictMarkerEnabled)
        {
            // FLAG deferred (Slice B): the marker frame handler + the county/district
            // refresh from cache words +20384/+20388/+20392 (SetCounty/SetDistrict +
            // GuiCache::RecEvent(169)) -- the marker TU carries only the
            // SetHideCountyIcon slice.
        }
        if (mbSatNavEnabled)
        {
            // FLAG deferred (Slice B): SatNavComponent::Update.
        }
        if (mbBoostMessagesEnabled)
        {
            // FLAG deferred (Slice B): BoostMessageManager::Update(cache time).
        }
        if (mbInGameMessagesEnabled)
        {
            // FLAG deferred (Slice B): InGameMessagesComponent::Update.
        }
        if (mbFriendsListEnabled)
        {
            /* FLAG deferred (Slice B): FriendsList Update */;
        }
        if (mbRoadRulesEnabled)
        {
            const f32 lfTimeNow = mpGuiCache->GetTime();
            CGS_ASSERT(lfTimeNow != -3.4028235e38f, "mfTimeNow!=-FLT_MAX");   // CgsGuiEventTypeDefs.h:250
            (void)lfTimeNow;   /* FLAG deferred (Slice B): RoadRule Update(timeNow) */
        }
        if (mbOdometerEnabled)
        {
            // FLAG deferred (Slice B): OdometerComponent::Update.
        }
        if (mbPpToggleEnabled && mbPpToggleActive)
        {
            const f32 lfTimeNow = mpGuiCache->GetTime();
            CGS_ASSERT(lfTimeNow != -3.4028235e38f, "mfTimeNow!=-FLT_MAX");
            if (lfTimeNow > mfPpToggleNextTime)
            {
                mIdentAnimator.Run("invisible");
                mIdentAnimator.Run("transIn");
                ++miPpToggleRunCount;
                // The X360 fsel: max(0, runCount*20 - 300) extra delay + 12s base.
                f32 lfExtra = miPpToggleRunCount * 20.0f - 300.0f;
                if (lfExtra < 0.0f)
                    lfExtra = 0.0f;
                mfPpToggleNextTime = lfTimeNow + lfExtra + 12.0f;
            }
        }
    }

    // =======================================================================
    //  UpdatePermenant  @ 0x824810F0 -- every frame, all phases
    // =======================================================================
    void FBurnMainHudState::UpdatePermenant()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)
            return;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            const s32* lpiPayload = reinterpret_cast<const s32*>(lpEvent);
            switch (liEventId)
            {
            case 309:
                if (lpiPayload[0] == 1 && lpiPayload[1] == 0)
                    SendStateEvent("PAUSE");
                break;
            case 21:
                ProcessAptEvents(lpiPayload);
                break;
            case 148:
                if (lpiPayload[0] != 0)
                {
                    // FLAG deferred (Slice B): the satnav events-filter re-arm
                    // (Enable/DisableSatNavEventsFilter per word +996).
                }
                else
                {
                    // X360: OutputGuiEvent<GuiAudioEvent>{2,0,-1,-1} then PAUSE.
                    // FLAG deferred (Slice B): the GuiAudioEvent record type is not
                    // yet homed; the pause transition is the load-bearing effect.
                    SendStateEvent("PAUSE");
                }
                break;
            case 377:
                if (lpiPayload[0] == 0 || lpiPayload[0] == 2)
                {
                    if (mbFriendsListEnabled)
                        /* FLAG deferred (Slice B): FriendsList SaveCurrentState */;
                    SendStateEvent("START_CRASH");
                }
                break;
            default:
                break;
            }
        }

        if (mbRoadRulesEnabled && mpGuiCache != 0)
        {
            // FLAG deferred (Slice B): RoadRule UpdateRoadSignDistances with the
            // player-position vector (X360 lvx128 from cache+19168 -- the pseudocode
            // drops the operand; take it from the asm when the handler TU lands).
        }
        if (mbFriendsListEnabled && mpGuiCache != 0)
        {
            mbFriendsListOffline = GuiCache_GetGameModeType(mpGuiCache) == -1;
            /* FLAG deferred (Slice B): FriendsList UpdateAptVariables */;
        }
    }

    // =======================================================================
    //  ProcessAptEvents  @ 0x82475048
    // =======================================================================
    void FBurnMainHudState::ProcessAptEvents(const s32* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event passed to FBurnMainHudState::ProcessAptEvents");   // cpp:1659

        if (lpEvent[0] == 1)
        {
            if (mbSatNavEnabled)
            {
                // FLAG deferred (Slice B): SatNav RecvEvent(21).
            }
            if (mbDistrictMarkerEnabled)
            {
                // FLAG deferred (Slice B): the marker per-frame member handler (the
                // ICF-folded "BaseCollisionGenerator::Destruct" body).
            }
            if (mbJunctionInfoEnabled)
            {
                // FLAG deferred (Slice B): JunctionInfo Refresh(name).
            }
        }
        else if (lpEvent[0] == 4)
        {
            const char* lpacClipName = reinterpret_cast<const char*>(
                static_cast<uintptr_t>(static_cast<u32>(lpEvent[2])));
            // The X360 payload carries the transition-complete clip-name pointer at
            // word 2; on the host the record layout widens, so resolve it through the
            // same-slot read the queue delivered.
            lpacClipName = reinterpret_cast<const char* const*>(lpEvent)[2] != 0
                               ? reinterpret_cast<const char* const*>(lpEvent)[2]
                               : lpacClipName;

            if (mbRoadRulesEnabled && lpacClipName != 0 &&
                std::strcmp(lpacClipName, "RoadRule_mc") == 0)
            {
                // FLAG deferred (Slice B): RoadRule TransitionComplete(word1).
            }
            else if (mbInGameMessagesEnabled && lpacClipName != 0 &&
                     std::strcmp(lpacClipName, "hudMessages_mc") == 0)
            {
                // FLAG deferred (Slice B): InGameMessages EndTransition.
            }
            else if (mbDistrictMarkerEnabled)
            {
                // FLAG deferred (Slice B): DistrictMarker ProcessCountyTransitionComplete
                // + ProcessDistrictTransitionComplete(name).
            }
        }

        if (mbBoostMessagesEnabled)
        {
            CGS_ASSERT(mpGuiCache != 0, "mpCache != NULL");   // cpp:1807
            // FLAG deferred (Slice B): BoostMessageManager RecvEvent(21).
        }
    }

    // =======================================================================
    //  ProcessBoostInfo  @ 0x82474F60
    // =======================================================================
    void FBurnMainHudState::ProcessBoostInfo(const void* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event");   // cpp:1631
        if (mbBoostMessagesEnabled)
        {
            CGS_ASSERT(mpGuiCache != 0, "mpCache != NULL");   // cpp:1639
            // FLAG deferred (Slice B): BoostMessageManager RecvEvent(206).
        }
    }

    // =======================================================================
    //  UpdateSatNav  @ 0x82475268
    // =======================================================================
    void FBurnMainHudState::UpdateSatNav(const void* lpEvent, s32 /*liEventId*/)
    {
        CGS_ASSERT(lpEvent != 0, " invalid event passed ");   // cpp:1828
        if (mbSatNavEnabled)
        {
            // FLAG deferred (Slice B): SatNav RecvEvent(id).
        }
    }
}
