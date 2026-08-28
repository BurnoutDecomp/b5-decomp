#include "GameSource/Gui/Flow/HUD/States/BrnFBurnMainHudState.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log (deferral gap log)
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // GuiEventDriveThruDiscovered / GuiEventChangeDistrict
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // CgsGui::GuiEventAptTriggerPayload (event 21, typed)
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
// The components whose TUs are reconstructed are driven for real: InGameMessages, the
// FULL DistrictMarker, JunctionInfo and Odometer (all three landed by the H1 wave,
// 2026-08-25), FriendsList + change icon, and the Flapt animators. The components whose
// TUs are NOT yet reconstructed -- the SatNav body, BoostMessageManager, and the
// RoadRule lifecycle -- are deferred: each call site keeps the X360 control flow (the
// gate bytes, the event routing) and logs the gap once instead of inventing a body.
namespace BrnGui
{
    namespace
    {
        // [H3b] the 12-byte id-213 show/hide PAYLOAD (the record body PostShowHide24
        // posts and SatNavComponent::RecvEvent(213) reads: word 1, f32 delay, show byte).
        struct SatNavShowHidePayload
        {
            s32 miOne;
            f32 mfDelay;
            u8  mu8Show;
            u8  mau8Pad[3];
        };
    }

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

        // (The apt movie mount/unmount goes through StateInterface::PlayAptMovie, not a
        // hand-rolled record: its GuiEventPlayAptMovie carries an 8-byte name pointer on
        // x64, so it must be posted at its true sizeof -- a hardcoded 20-byte post
        // truncates the trailing level field and the ViewModule reads a garbage level.)

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

        // [hud reveal gate 2026-08-25] THE FIX. This leaf used to `return 1;` -- hardcoding
        // E_ENGINE_ON -- on the premise that "with no live vehicle telemetry on the PC boot
        // path yet" the HUD should be shown rather than hidden. Both halves of that premise
        // are now false: the vehicle telemetry IS live (the world publishes
        // mePlayerEngineState every frame), and forcing ENGINE_ON is exactly the
        // user-reported defect -- the free-burn HUD stayed visible in the Junkyard and
        // through car select, because UpdateWFInit could never take the ENGINE_OFF arm that
        // parks the movie on its invisible transition frame.
        // ⚠️ The old comment's "X360 cache word +19220" was ALSO wrong: the asm at
        // @0x8247C7EC is `lwz r11, 0x4B20(r11)` == +19232. See BrnGuiCache.h's member note
        // for why Hex-Rays prints it as gapC[19220].
        s32 GuiCache_GetPlayerEngineState(const GuiCache* lpGuiCache)
        {
            return lpGuiCache->GetPlayerEngineState();
        }

        // ⭐ [boost-bar gate 2026-08-25] THE FIX. This leaf used to be an EMPTY shim
        // labelled "the hud-ready byte (X360 cache+16496)". Both halves were wrong:
        // +16496 (0x4070) is mpHudMessageController (a pointer), and the store the asm
        // actually makes here is `stb r28, 0x407C(r11)` @0x8247CA58 -- cache+16508 ==
        // mbGameplayHudActive, with r28 == 1 on BOTH engine arms (`li r28,1` @0x8247C868 /
        // @0x8247C920). That byte heads the GetGameplayHudReady() trio the gameplay-HUD
        // components gate on, so the empty shim froze BoostBarRenderer::Update forever.
        // The console asserts the cache pointer first (@0x8247CA2C..CA50, "mpCache",
        // cpp:1584) and stores through it regardless.
        void GuiCache_SetHudReady(GuiCache* lpGuiCache)
        {
            CGS_ASSERT(lpGuiCache != 0, "mpCache");   // cpp:1584 (non-gating; the store follows)
            lpGuiCache->SetGameplayHudActive(true);   // stb 1 @cache+0x407C
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

        // [hud reveal gate 2026-08-25] ⭐ THIS IS THE SAME WORD AS GuiCache_GetPlayerEngineState
        // ABOVE, and the old "boost-bar config word" name was a mislabel. UpdateRunning
        // @0x8247B660 contains exactly ONE +0x4B20 read in its whole ~220-case switch --
        //     0x8247BD10  lwz  r11, 0x140(r31)      ; r31+0x140 == mpGuiCache
        //     0x8247BD14  lwz  r11, 0x4B20(r11)
        //     0x8247BD18  cmpwi cr6, r11, 1
        // -- and it sits inside jumptable CASE 215 (the boost-amount event), on the
        // boost-below-0.01 arm, gating the "visible" AddOutputAptViewState + the
        // OutputViewState<GuiEventShowHideBoostBar> at 0x8247BD4C. So the console is not
        // consulting a boost-bar *config* at all: it is asking "is the player's engine
        // running?" before it reveals the emptied boost bar -- the same reveal gate the
        // compose path uses. Kept as a separate named leaf only because the console reads it
        // at a separate seat; both now resolve to the one real member.
        s32 GuiCache_GetBoostBarConfig(const GuiCache* lpGuiCache)
        {
            return lpGuiCache->GetPlayerEngineState();
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
        // [H3b] X360 OnEnter @0x8247B0E8: SatNavComponent::Construct(&this->+0x160,
        // this->+0x1C iface, 0, mode 0) -- track-player mode, no parent name.
        mSatNavComponent.Construct(mpStateInterface, 0,
                                   SatNavComponent::E_SAT_NAV_MODE_TRACK_PLAYER);

        // [gateui r3] THE LAST RUNG OF THE gateui LADDER, landed. X360 OnEnter @0x8247B0E8:
        //     InGameMessagesComponent::Construct(&this->field_3E8, "hudMessages_mc",
        //                                        this->field_1C, 0);
        //     ... assert mpGuiCache (CgsGuiShared.h:201) ...
        //     InGameMessagesComponent::SetInGameMessagesQueue(&this->field_3E8,
        //                                                     guiCache + 16512);
        //     InGameMessagesComponent::Prepare(&this->field_3E8, "hudMessages_mc", lFile);
        // Round 2's park ("that component's Construct/Prepare TU is unreconstructed") was
        // wrong twice over: Prepare/AddMessage/Update/TerminateMessages were already bodied,
        // and the ONLY missing piece was this Construct call plus the component's five
        // private bodies -- all of which now exist (BrnInGameMessagesComponent.cpp).
        mInGameMessages.Construct("hudMessages_mc", mpStateInterface, 0);

        // ⭐ [gateui r4] CE-4: THE QUEUE HAND-OFF, LANDED. X360 OnEnter @0x8247B0E8 lines
        // 91-108, verbatim:
        //     v11 = this->field_1C;                                    // mpStateInterface
        //     if ( !*(v11 + 4) ) assert "mpAccessPointers != NULL"
        //                        (CgsGuiStateInterface.h:344)
        //     v12 = *(v11 + 4);                                        // the access pointers
        //     if ( !*(v12 + 16) ) assert "mpGuiCache" (CgsGuiShared.h:201)
        //     SetInGameMessagesQueue(&field_3E8, *(v12 + 16) + 16512);
        // 16512 == 0x4080 == the GuiCache's by-value InGameMessagesQueue, which round 4
        // homed as a named member (BrnGuiCache.h :: mInGameMessagesQueue) -- so the raw
        // console offset is replaced by the accessor, which is the only correct move on a
        // host where every pointer ahead of it in the cache has widened.
        {
            CgsGui::GuiAccessPointers* lpAccessPointers = mpStateInterface->GetAccessPointers();
            CGS_ASSERT(lpAccessPointers != 0, "mpAccessPointers != NULL");
            GuiCache* lpGuiCache = static_cast<GuiCache*>(lpAccessPointers->GetGuiCache());
            CGS_ASSERT(lpGuiCache != 0, "mpGuiCache");
            mInGameMessages.SetInGameMessagesQueue(lpGuiCache->GetInGameMessagesQueue());
        }

        mInGameMessages.Prepare("hudMessages_mc", lFile);

        // [H1] X360 OnEnter @0x8247B0E8: the full marker lifecycle (the old "only the
        // SetHideCountyIcon slice" deferral retired with the View ODR-fork).
        mDistrictMarker.Construct("marker_mc", mpStateInterface, 0);
        mDistrictMarker.Prepare("marker_mc", lFile);

        mbDistrictRefreshArmed = true;   // X360 +0x8AC := 1

        // X360 OnEnter @0x8247B3A8..C8: Construct("BoostManager", mpStateInterface, 0)
        // then Prepare("BoostManager", lFile) on the +0x8B0 member.
        mBoostMessages.Construct("BoostManager", mpStateInterface, 0);
        mBoostMessages.Prepare("BoostManager", lFile);

        // The "EventHud_Animator" pair + the road-rule/friends components are real.
        mEventHudAnimatorIcon.Construct("EventHud_Animator", mpStateInterface, 0);
        mEventHudAnimator.Construct(0, mpStateInterface, 0);
        mEventHudAnimator.Prepare("EventHud_Animator", lFile, 0);

        // [H2] X360 OnEnter @0x8247B0E8: Construct("RoadRule_mc", iface, 0, 1) --
        // apt layer index 1 (the B5RaceHud mount level; the component's sound events
        // carry it) -- then Prepare against the loaded file.
        mRoadRuleComponent.Construct("RoadRule_mc", mpStateInterface, 0, 1);
        mRoadRuleComponent.Prepare("RoadRule_mc", lFile);

        // FLAG deferred (Slice B): FriendsListComponent::Construct/Prepare
        // ("friendList") -- the committed FriendsList TU is the SetGuiCachePointer/
        // SetDirty slice only.
        LogDeferredComponent("FriendsListComponent");
        mFriendsListChangeIcon.Construct("FriendListChange_mc", mpStateInterface, 0);
        mFriendsListChangeIcon.Prepare("FriendListChange_mc", lFile);

        PostCommand16<94>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);

        // [H1] X360 OnEnter @0x8247B0E8 (asm h1_dump.txt): JunctionInfo then Odometer,
        // between the {1,94} post and the Ident animator. The X360 Odometer call carries
        // (name, iface, 0, -1); JunctionInfo's Hex-Rays view shows three args (its body
        // ignores the parent-name/layer pair) -- the same (0, -1) tail is passed.
        mJunctionInfoComponent.Construct("JunctionInfo_mc", mpStateInterface, 0, -1);
        mJunctionInfoComponent.Prepare("JunctionInfo_mc", lFile);
        mOdometer.Construct("Odometer_mc", mpStateInterface, 0, -1);
        mOdometer.Prepare("Odometer_mc", lFile);

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

        // Unmount the HUD apt: the empty-name record on the level the mount used (the
        // ViewModule's case-18 handler treats a <=1-char name as "clear this level").
        mpStateInterface->PlayAptMovie("", 1);

        mRaceMainHudClip.SetVisible(false);

        // [H1] "transout" across the animator set, in the console's own order (OnLeave
        // @0x82480B88): the marker's two container movies (the X360 vcalls slot 3 ==
        // FlaptIconComponent::SetState on this+0x840/+0x854 -- the direct member pokes the
        // marker header friend-grants), the junction panel, the EventHud animator, then the
        // odometer's active text.
        mDistrictMarker.mCountyContainerMovie.SetState("transout");
        mDistrictMarker.mDistrictContainerMovie.SetState("transout");
        mJunctionInfoComponent.Run("transout");
        mEventHudAnimator.Run("transout");
        mOdometer.TransOutActiveText();

        PostShowHide24(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1, 0.0f, 0);   // {12,213,flag0}

        if (mbSatNavEnabled)
        {
            // [H3b] X360 OnLeave: the hide mirror ({1, 0.0f, 0} payload) then Destruct.
            SatNavShowHidePayload lHide;
            lHide.miOne   = 1;
            lHide.mfDelay = 0.0f;
            lHide.mu8Show = 0;
            mSatNavComponent.RecvEvent(
                reinterpret_cast<const CgsModule::Event*>(&lHide), 213);
            mSatNavComponent.Destruct();
        }
        if (mbRoadRulesEnabled)
        {
            mRoadRuleComponent.EndTimers();   // [H2] X360 OnLeave @0x82480B88
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
    // Stage-transition log (the [BootLegal]/[BootProfile]-style diagnostic every boot/HUD
    // state carries).
    static void LogFBurnStage(s32 liFrom, s32 liTo)
    {
        char lac[64];
        std::snprintf(lac, sizeof(lac), "[FBurnMainHud] stage %d -> %d\n", liFrom, liTo);
        CgsDev::Log::WriteToLog(lac);
    }

    void FBurnMainHudState::Update()
    {
        const EInternalState lePrevState = meInternalState;
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
        if (meInternalState != lePrevState)
            LogFBurnStage(lePrevState, meInternalState);

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

        // [H3b] X360 UpdateSetupState @0x82480EA0: bind the cache into the component,
        // mirror the cache's sat-nav filter pair, and arm/disarm the events filter.
        mSatNavComponent.SetCachePointer(mpGuiCache);
        meSatNavEventFilter  = mpGuiCache->GetSatNavEventFilter();          // cache+0x8034
        mbEventFilterEnabled = mpGuiCache->GetSatNavEventFilterEnabled();   // cache+0x8038
        if (mbEventFilterEnabled)
            EnableSatNavEventsFilter();
        else
            DisableSatNavEventsFilter();

        PostCommand16<555>(mpStateInterface, KI_CHANNEL_GUI_OUT, 0);

        if (mbInGameMessagesEnabled)
        {
            // [gateui r3] X360 UpdateSetupState @0x82480EA0, verbatim:
            //     SetController (GuiCache::GetHudMessageController(cache));
            //     SetDirector   (GuiCache::GetHudMessageDirector  (cache));
            //     SetGameMode   (*(cache + 40536));
            // Both cache getters exist now. The controller one is GATED on the pointer
            // being present because GuiCache::GetHudMessageController asserts non-null and
            // this runs every frame until the state leaves SETUP -- the same bring-up gate
            // BrnGuiModule.cpp already uses at its own SetController site. The producer
            // (GameDataModule::PrepareHudMessages) landed this round; the one hand-off that
            // fills GuiCache::mpHudMessageController is a conductor edit in the gateui r3
            // report.
            if (mpGuiCache->HasHudMessageController())
                mInGameMessages.SetController(mpGuiCache->GetHudMessageController());

            mInGameMessages.SetDirector(mpGuiCache->GetHudMessageDirector());
            mInGameMessages.SetGameMode(
                static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
                    GuiCache_GetGameModeType(mpGuiCache)));
        }

        if (mbRoadRulesEnabled)
        {
            // [H2] X360 UpdateSetupState @0x82480EA0 tail: adopt the cache, then
            // adopt its active road rule.
            mRoadRuleComponent.SetCachePointer(mpGuiCache);
            mRoadRuleComponent.InitialiseMode();
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
            mSatNavComponent.LoadResources();   // [H3b] X360 UpdateLoading @0x8247C640
        }

        // Mount the HUD apt movie at level 1 (X360 off_82F27BE0[0] == "B5RaceHud"). Use
        // the StateInterface play-movie path so the record is the real GuiEventPlayAptMovie
        // posted at its true sizeof -- a hand-rolled record posted at the X360's 20-byte
        // size truncates the level field on x64 (8-byte name pointer) and the ViewModule
        // reads a garbage level number.
        mpStateInterface->PlayAptMovie("B5RaceHud", 1);

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

            // [H2] X360 UpdateWFInit @0x8247C710: replay any rule already live in the
            // cache (the by-type flags @cache+0xAC44) into the fresh panel.
            for (s32 leEnumIndex = 0; leEnumIndex < 2; ++leEnumIndex)
            {
                if (mpGuiCache->IsRoadRuleActive(leEnumIndex))
                {
                    mRoadRuleComponent.HandleRoadRuleBegin(
                        static_cast<BrnStreetData::ScoreType>(leEnumIndex));
                }
            }
        }

        // The player engine state (X360 cache word +0x4B20 == 19232; assert < 2). THE REVEAL
        // GATE: ENGINE_ON composes the HUD visible, ENGINE_OFF parks the master
        // "EventHud_Animator" on its invisible transition frame and leaves it there.
        const s32 liEngineState = GuiCache_GetPlayerEngineState(mpGuiCache);
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[hud-reveal] UpdateWFInit engineState=" << liEngineState
                << (liEngineState == 1 ? " -> compose VISIBLE\n" : " -> compose INVISIBLE\n");
        }
        CGS_ASSERT(liEngineState < 2,
                   "( GuiPlayerEngineEvent::E_ENGINE_OFF == mpCache->GetPlayerEngineState( )) || ( GuiPlayerEngineEvent::E_ENGINE_ON == mpCache->GetPlayerEngineState( ))");   // cpp:1536

        if (liEngineState == 1)
        {
            PostShowHide24(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1, 0.0f, 1);      // {12,213,flag1}
            PostShowHide24(mpStateInterface, KI_CHANNEL_INTERNAL_STATE, 1, 0.0f, 1);  // ch42 mirror
            // X360 @0x8247C710: BOTH halves of the "EventHud_Animator" pair -- the
            // apt-view-state write the movie's AS polls (field_A40) AND the FLAPT
            // goto-and-play (field_ACC). The apt write is what makes the HUD's master
            // transition play (the movie starts on its invisible frame).
            mEventHudAnimatorIcon.AddOutputAptViewState("apt_Transition", "visible", false);
            mEventHudAnimator.Run("visible");
            PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1);
        }
        else
        {
            mEventHudAnimatorIcon.AddOutputAptViewState("apt_Transition", "invisible", false);
            mEventHudAnimator.Run("invisible");
            PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 0);
            PostShowHide24(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1, 0.0f, 0);      // {12,213,flag0}
            PostShowHide24(mpStateInterface, KI_CHANNEL_INTERNAL_STATE, 1, 0.0f, 0);
        }

        // [H3b] X360 UpdateWFInit: mirror the show record into the component.
        // ⭐ [hud reveal gate 2026-08-25] FIXED: mu8Show was HARDCODED to 1 here, so the
        // sat-nav icon set was told "visible" even on the ENGINE_OFF arm -- which is why the
        // yellow player chevron kept drawing over the junkyard with no minimap under it.
        // The console does NOT pass a constant. Its single RecvEvent(213) call site
        // @0x8247CA28 takes `addi r4, r1, var_60` -- the SAME 24-byte stack record each arm
        // has just filled -- and the two arms write DIFFERENT show bytes into it:
        //     ENGINE_OFF arm @0x8247C898  stb r27, var_5C+4      with `li r27, 0` @0x8247C720
        //     ENGINE_ON  arm @0x8247C934  stb r28, var_5C+4      with `li r28, 1` @0x8247C920
        // (two distinct registers for the one field is itself the tell -- a shared constant
        // would have reused one). So the mirror carries THE ARM'S OWN FLAG, exactly like the
        // {12,213,flag} pair each arm posts on channels 0x29/0x2A just above.
        if (mbSatNavEnabled)
        {
            SatNavShowHidePayload lShow;
            lShow.miOne   = 1;
            lShow.mfDelay = 0.0f;
            lShow.mu8Show = static_cast<u8>(liEngineState == 1 ? 1 : 0);
            mSatNavComponent.RecvEvent(
                reinterpret_cast<const CgsModule::Event*>(&lShow), 213);
        }

        GuiCache_SetHudReady(mpGuiCache);   // X360 stb 1 @cache+0x407C (mbGameplayHudActive; @0x8247CA58)

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
            mOdometer.TransIn();   // [H1] X360 UpdateWFInit tail
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
            // [H3b] X360 UpdateRunning @0x8247B660 head: the per-frame pre-pass. The
            // "+656/+948" words are INSIDE the component -- its player-info binding
            // (+0x130) and its icon manager's used count (through +0x254) -- reset
            // before the event pump repopulates them (friend grants on both classes).
            mSatNavComponent.mpPlayerInfo = 0;
            if (mSatNavComponent.mpIconManager != 0)
                mSatNavComponent.mpIconManager->miNumUsedIcons = 0;
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
                // [gateui r3] X360 @0x8247B660 case 154, verbatim:
                //     if (*(a1 + 333)) InGameMessagesComponent::AddMessage(a1 + 1000, v7);
                // v7 is the queued record's payload -- the whole GuiHudMessage the
                // HudMessageDirector published as event 154 and the model/view layer fanned
                // out to this state. THIS is the rung the `[UI-gate] hud message sent` line
                // in BrnGuiHudMessageDirector_gUI_01.cpp hands off to.
                // [gateui r4] The round-3 `HasMessageQueue()` bring-up gate is GONE: OnEnter
                // above now always runs SetInGameMessagesQueue, exactly as the console does,
                // so the pointer is never null here.
                if (mbInGameMessagesEnabled)
                {
                    mInGameMessages.AddMessage(lpEvent);
                }
                break;
            case 156:
                // [gateui r3] X360 case 156: TerminateMessages(a1 + 1000) under the same gate.
                if (mbInGameMessagesEnabled)
                {
                    mInGameMessages.TerminateMessages();
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
                if (mbSatNavEnabled)   // [H3b] the X360 mirror of the same payload
                {
                    SatNavShowHidePayload lMirror;
                    lMirror.miOne   = 1;
                    lMirror.mfDelay = 0.0f;
                    lMirror.mu8Show = static_cast<u8>(lpiPayload[0] != 0);
                    mSatNavComponent.RecvEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lMirror), 213);
                }
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
                        mEventHudAnimatorIcon.AddOutputAptViewState("apt_Transition", "invisible", false);
                        mEventHudAnimator.Run("invisible");
                        PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 0);
                        PostShowHide24(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1, 0.0f, 0);
                        PostShowHide24(mpStateInterface, KI_CHANNEL_INTERNAL_STATE, 1, 0.0f, 0);
                        if (mbSatNavEnabled)   // [H3b] the X360 LABEL_46 mirror
                        {
                            SatNavShowHidePayload lMirror;
                            lMirror.miOne   = 1;
                            lMirror.mfDelay = 0.0f;
                            lMirror.mu8Show = 0;
                            mSatNavComponent.RecvEvent(
                                reinterpret_cast<const CgsModule::Event*>(&lMirror), 213);
                        }
                    }
                    else if (GuiCache_GetBoostBarConfig(mpGuiCache) == 1)
                    {
                        mEventHudAnimatorIcon.AddOutputAptViewState("apt_Transition", "visible", false);
                        mEventHudAnimator.Run("visible");
                        PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1);
                        PostShowHide24(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1, 0.0f, 1);
                        PostShowHide24(mpStateInterface, KI_CHANNEL_INTERNAL_STATE, 1, 0.0f, 1);
                        if (mbSatNavEnabled)   // [H3b] the X360 LABEL_46 mirror
                        {
                            SatNavShowHidePayload lMirror;
                            lMirror.miOne   = 1;
                            lMirror.mfDelay = 0.0f;
                            lMirror.mu8Show = 1;
                            mSatNavComponent.RecvEvent(
                                reinterpret_cast<const CgsModule::Event*>(&lMirror), 213);
                        }
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
                    // [H1] X360 @0x8247C258 (asm h1_dump2.txt): r4 = the queue payload
                    // (Hex-Rays DROPS this argument -- the documented trap), r5 = the whole
                    // CgsID `ld cache+0x4AF8` (the ORIGINAL car id, RecEvent case 415's
                    // twin store).
                    mJunctionInfoComponent.HandleJunctionChange(
                        reinterpret_cast<const GuiEventJunctionInfo*>(lpEvent),
                        mpGuiCache->GetLocalPlayerOriginalCarId());
                    mDistrictMarker.SetHideCountyIcon(mbCountyIconHidden);
                }
                if (mbOdometerEnabled)
                {
                    mOdometer.HandleJunctionChange(
                        reinterpret_cast<const GuiEventJunctionInfo*>(lpEvent));
                }
                break;
            case 314:
                if (mbOdometerEnabled)
                {
                    mOdometer.HandleDriveThruDiscovered(
                        reinterpret_cast<const GuiEventDriveThruDiscovered*>(lpEvent));
                }
                break;
            // [H2] the road-rule event family (X360 UpdateRunning @0x8247C058..0x8247C164,
            // decomp+asm in scratch h2_dump7.txt). Every arm is gated on the enable byte.
            case 333:   // GuiEventRoadRuleEnter
                if (mbRoadRulesEnabled)
                    mRoadRuleComponent.HandleEnterRoadEvent(
                        reinterpret_cast<const GuiEventRoadRuleEnter*>(lpEvent));
                break;
            case 335:   // road-rule begin { ScoreType }
                if (mbRoadRulesEnabled)
                    mRoadRuleComponent.HandleRoadRuleBegin(
                        static_cast<BrnStreetData::ScoreType>(lpiPayload[0]));
                break;
            case 336:   // GuiEventRoadRuleEnd
                if (mbRoadRulesEnabled)
                    mRoadRuleComponent.HandleRoadRuleEnd(
                        reinterpret_cast<const GuiEventRoadRuleEnd*>(lpEvent));
                break;
            case 338:   // rule-time update { f32 time, .., f32 crashTarget, s32 multiplier }
                if (mbRoadRulesEnabled)
                {
                    const f32* lpfPayload = reinterpret_cast<const f32*>(lpEvent);
                    mRoadRuleComponent.UpdateCurrentTime(lpfPayload[0]);
                    // X360 @0x8247C0F0..: the crash-target pair rides the same record
                    // (payload f32[3] / s32[4]); a changed multiplier nudges the target
                    // by +0.01 so the eased readout re-renders (friend-granted pokes).
                    const s32 liNewMultiplier = lpiPayload[4];
                    mRoadRuleComponent.mfTargetCrashScore = lpfPayload[3];
                    if (mRoadRuleComponent.miCrashMultiplier != liNewMultiplier)
                    {
                        mRoadRuleComponent.miCrashMultiplier  = liNewMultiplier;
                        mRoadRuleComponent.mfTargetCrashScore = lpfPayload[3] + 0.0099999998f;
                    }
                }
                break;
            case 339:   // GuiEventRoadRuleUpdateTargetScores
                if (mbRoadRulesEnabled)
                {
                    CGS_ASSERT(lpEvent != 0, "lpRRTargetUpdate");   // cpp:669 (non-gating)
                    mRoadRuleComponent.HandleRoadRuleTargetUpdate(
                        reinterpret_cast<const GuiEventRoadRuleUpdateTargetScores*>(lpEvent));
                }
                break;
            case 340:   // road-rule leave { CgsID }
                if (mbRoadRulesEnabled)
                    mRoadRuleComponent.HandleLeaveRoadEvent(
                        *reinterpret_cast<const CgsID*>(lpEvent));
                break;
            case 341:   // GuiEventRoadRuleUpcomingRoads
                if (mbRoadRulesEnabled)
                    mRoadRuleComponent.HandleUpcomingRoadEvent(
                        reinterpret_cast<const GuiEventRoadRuleUpcomingRoads*>(lpEvent));
                break;
            case 343:   // road-rule mode change { EActiveRoadRule }
                if (mbRoadRulesEnabled)
                    mRoadRuleComponent.SwitchModes(
                        static_cast<BrnGameState::EActiveRoadRule>(lpiPayload[0]));
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
                    // X360 UpdateRunning @0x8247C03C: RecvEvent(+0x8B0, lpEvent, id,
                    // mpGuiCache) for every id in this family.
                    mBoostMessages.RecvEvent(lpEvent, liEventId, mpGuiCache);
                }
                break;
            case 379:   // engine state changed
                CGS_ASSERT(lpiPayload[0] < 2,
                           "( GuiPlayerEngineEvent::E_ENGINE_OFF == lpEngineChange->meNewEngineState ) || ( GuiPlayerEngineEvent::E_ENGINE_ON == lpEngineChange->meNewEngineState )");   // cpp:772
                if (lpiPayload[0] == 1)
                {
                    mEventHudAnimatorIcon.AddOutputAptViewState("apt_Transition", "transin", false);
                    mEventHudAnimator.Run("transin");
                    PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1);
                }
                else
                {
                    mEventHudAnimatorIcon.AddOutputAptViewState("apt_Transition", "transout", false);
                    mEventHudAnimator.Run("transout");
                    PostCommand16<214>(mpStateInterface, KI_CHANNEL_VIEW_STATE, 0);
                }
                PostShowHide24(mpStateInterface, KI_CHANNEL_VIEW_STATE, 1, 0.0f,
                               static_cast<u8>(lpiPayload[0] == 1));
                PostShowHide24(mpStateInterface, KI_CHANNEL_INTERNAL_STATE, 1, 0.0f,
                               static_cast<u8>(lpiPayload[0] == 1));
                if (mbSatNavEnabled)   // [H3b] the X360 mirror
                {
                    SatNavShowHidePayload lMirror;
                    lMirror.miOne   = 1;
                    lMirror.mfDelay = 0.0f;
                    lMirror.mu8Show = static_cast<u8>(lpiPayload[0] == 1);
                    mSatNavComponent.RecvEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lMirror), 213);
                }
                break;
            default:
                break;
            }
        }

        // ---- the per-frame component ticks --------------------------------------
        if (mbDistrictMarkerEnabled)
        {
            // [H1] X360 @0x8247B660 post-loop (h1_dump.txt): the marker's per-frame member
            // handler (ICF-folded EMPTY on the console -- see DistrictMarker.cpp's Update
            // note), then the county/district refresh off the cache's latest
            // GuiEventChangeDistrict record: consume it when it is fresh, OR re-run it when
            // this state's own refresh is armed (the OnEnter +0x8AC latch) and the district
            // is valid; hand the record back through RecEvent(169) with the consumed byte
            // set, and disarm.
            mDistrictMarker.Update();

            GuiEventChangeDistrict lRecord;
            lRecord.meCounty     = mpGuiCache->GetChangeDistrictCounty();
            lRecord.meDistrict   = mpGuiCache->GetChangeDistrictDistrict();
            lRecord.mu8Consumed  = mpGuiCache->IsChangeDistrictConsumed() ? 1 : 0;
            lRecord.maPad[0] = lRecord.maPad[1] = lRecord.maPad[2] = 0;
            if (!lRecord.mu8Consumed ||
                (mbDistrictRefreshArmed && lRecord.meDistrict != BrnWorld::E_DISTRICT_INVALID))
            {
                mDistrictMarker.SetCounty(static_cast<BrnWorld::ECounty>(lRecord.meCounty));
                mDistrictMarker.SetDistrict(static_cast<BrnWorld::EDistrict>(lRecord.meDistrict));
                lRecord.mu8Consumed = 1;
                mpGuiCache->RecEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord), 169);
                mbDistrictRefreshArmed = false;
            }
        }
        if (mbSatNavEnabled)
        {
            mSatNavComponent.Update();   // [H3b] X360 UpdateRunning tail
        }
        if (mbBoostMessagesEnabled)
        {
            // X360 UpdateRunning @0x8247C4AC: Update(+0x8B0, cache->mfTimeStep,
            // mode == CRASH(2) || SHOWTIME(16)). In the showtime modes the manager runs
            // ONLY its showtime ticker.
            const s32 liGameMode = mpGuiCache->GetGameMode();
            mBoostMessages.Update(mpGuiCache->GetTimeStep(),
                                  (liGameMode == 2 || liGameMode == 16));
        }
        if (mbInGameMessagesEnabled)
        {
            // [gateui r6] X360 UpdateRunning @0x8247B660, verbatim:
            //     if (*(a1 + 333)) BrnGui::InGameMessagesComponent::Update(a1 + 1000);
            // This is the tick that RETIRES a shown message: Update ends a VISIBLE slot
            // once the base timer passes the end time EndTransition latched from the
            // message's mfDuration, which runs the "transout" animation and frees the
            // slot. Without it the popup was correct-but-immortal -- it slid in and stayed
            // on screen for the rest of the drive. The whole component (Update /
            // EndTransition / EndMessage / SendGameMessage) landed in round 3, so the
            // round-2 "Slice B" deferral had nothing left to defer.
            mInGameMessages.Update();
        }
        if (mbFriendsListEnabled)
        {
            /* FLAG deferred (Slice B): FriendsList Update */;
        }
        if (mbRoadRulesEnabled)
        {
            const f32 lfTimeNow = mpGuiCache->GetTime();
            CGS_ASSERT(lfTimeNow != -3.4028235e38f, "mfTimeNow!=-FLT_MAX");   // CgsGuiEventTypeDefs.h:250
            mRoadRuleComponent.Update(lfTimeNow);   // [H2] the per-frame crash-score / leader tick
        }
        if (mbOdometerEnabled)
        {
            mOdometer.Update();   // [H1] the per-frame mileage/flash tick
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
            // ⚠️ THE PAYLOAD WIDTH IS PER-ARM, AND IT IS TAKEN FROM THE ASM.
            // UpdatePermenant @0x824810F0 reads each record with the load its record
            // size demands, and two of the three are BYTE loads:
            //   case 0x135 (309) @0x824811B4/@0x824811C0: `lbz r11,0(r30)` cmplwi 1 +
            //                    `lbz r11,1(r30)` cmplwi 0   -- TWO u8s
            //   case 0x94  (148) @0x82481154:             `lbz r11,0(r30)`  -- ONE u8
            //   case 0x179 (377) @0x824811DC:             `lwz r11,0(r30)`  -- an s32
            // and the record sizes agree at the producer: GuiEventGameCompleted is
            // `u8 maData[2]` (id 309 size 2) and GuiEventShowHideHud is `u8 maData[1]`
            // (id 148 size 1) -- BrnGuiDemangledEventTypes.h:159/246. Reading either as
            // an s32 pair walked 8 bytes off a 2-byte record and 4 off a 1-byte one, so
            // the tests could only pass on whatever the queue happened to hold next.
            const u8* lpu8Payload = reinterpret_cast<const u8*>(lpEvent);
            switch (liEventId)
            {
            case 309:
                if (lpu8Payload[0] == 1u && lpu8Payload[1] == 0u)
                    SendStateEvent("PAUSE");
                break;
            case 21:
                ProcessAptEvents(lpiPayload);
                break;
            case 148:
                if (lpu8Payload[0] != 0u)
                {
                    // [H3b] the sat-nav events-filter re-arm (X360: pick by the
                    // mirrored enable byte).
                    if (mbEventFilterEnabled)
                        EnableSatNavEventsFilter();
                    else
                        DisableSatNavEventsFilter();
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
                // [crash-hud] delivery witness. NOT X360. One-shot; proves the producer's event
                // actually REACHES this consumer and which arm it takes.
                {
                    static bool sbSeen377 = false;
                    if (!sbSeen377 && CgsDev::Log::gpDebugPrint != 0)
                    {
                        sbSeen377 = true;
                        *CgsDev::Log::gpDebugPrint
                            << "[crash-hud] FBurnMainHudState received GUI 377, payload="
                            << lpiPayload[0] << "\n";
                    }
                }
                if (lpiPayload[0] == 0 || lpiPayload[0] == 2)
                {
                    if (mbFriendsListEnabled)
                        /* FLAG deferred (Slice B): FriendsList SaveCurrentState */;
                    if (CgsDev::Log::gpDebugPrint != 0)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "[crash-hud] SendStateEvent(\"START_CRASH\")\n";
                    }
                    SendStateEvent("START_CRASH");
                }
                break;
            default:
                break;
            }
        }

        if (mbRoadRulesEnabled && mpGuiCache != 0)
        {
            // [H2] X360 @0x82481248: `lvx128 v1, cache, 0x4AE0` -- the world-camera
            // vector (the operand Hex-Rays drops; taken from the asm) into
            // UpdateRoadSignDistances.
            const Vector4& lv4Camera = mpGuiCache->GetWorldCameraPosition();
            Vector3 lv3Camera;
            lv3Camera.x = lv4Camera.x;
            lv3Camera.y = lv4Camera.y;
            lv3Camera.z = lv4Camera.z;
            lv3Camera.w = lv4Camera.w;
            mRoadRuleComponent.UpdateRoadSignDistances(lv3Camera);
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
                // [H3b] X360: forward the apt trigger record itself (id 21).
                mSatNavComponent.RecvEvent(
                    reinterpret_cast<const CgsModule::Event*>(lpEvent), 21);
            }
            if (mbDistrictMarkerEnabled)
            {
                mDistrictMarker.Update();   // [H1] ICF-folded empty on X360 (see the TU)
            }
            if (mbJunctionInfoEnabled)
            {
                // [H1] X360 type-1 arm: Refresh(the payload's component-name pointer).
                mJunctionInfoComponent.Refresh(
                    reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent)
                        ->mpacComponentName);
            }
        }
        else if (lpEvent[0] == 4)
        {
            // [H1] The event-21 record on this host IS the native-width
            // CgsGui::GuiEventAptTriggerPayload (CgsAptCommunicator.h -- the same typed
            // read GuiCache::RecEvent case 21 already does). The X360 reads the name as
            // "payload word 2" because that is where the 32-bit record's pointer lands;
            // this TU's old same-slot arithmetic read ([2] of 8-byte slots == +16) landed
            // on muComponentNameHash on x64 -- a garbage pointer had any type-4 trigger
            // ever fired. By-name is both the house rule and the fix.
            const CgsGui::GuiEventAptTriggerPayload* lpTrigger =
                reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent);
            const char* lpacClipName = lpTrigger->mpacComponentName;

            if (mbRoadRulesEnabled && lpacClipName != 0 &&
                std::strcmp(lpacClipName, "RoadRule_mc") == 0)
            {
                // [H2] X360 @0x82475048 type-4 RoadRule arm: TransitionComplete(payload
                // word 1 == miUniqueId -- the frame-trigger label id).
                mRoadRuleComponent.TransitionComplete(lpTrigger->miUniqueId);
            }
            else if (mbInGameMessagesEnabled && lpacClipName != 0 &&
                     std::strcmp(lpacClipName, "hudMessages_mc") == 0)
            {
                // [H1] The round-3 InGameMessages TU carries EndTransition -- the old
                // "Slice B" deferral here was stale.
                mInGameMessages.EndTransition();
            }
            else if (mbDistrictMarkerEnabled)
            {
                // [H1] X360 else-arm: county first (no name), then district (name).
                mDistrictMarker.ProcessCountyTransitionComplete(0);
                mDistrictMarker.ProcessDistrictTransitionComplete(lpacClipName);
            }
        }

        if (mbBoostMessagesEnabled)
        {
            CGS_ASSERT(mpGuiCache != 0, "mpCache != NULL");   // cpp:1807
            // X360 ProcessAptEvents @0x8247525C: RecvEvent(+0x8B0, lpEvent, 21,
            // mpGuiCache). Id 21 is the apt-trigger record the manager's switch
            // deliberately ignores (its jump table starts at 206) -- the call is made
            // and falls through to default, exactly as shipped.
            mBoostMessages.RecvEvent(reinterpret_cast<const CgsModule::Event*>(lpEvent),
                                     21, mpGuiCache);
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
            // X360 ProcessBoostInfo @0x8247502C: RecvEvent(+0x8B0, lpEvent, 206,
            // mpGuiCache) -- the boost-type latch that tints every posted message.
            mBoostMessages.RecvEvent(static_cast<const CgsModule::Event*>(lpEvent),
                                     206, mpGuiCache);
        }
    }

    // =======================================================================
    //  UpdateSatNav  @ 0x82475268
    // =======================================================================
    void FBurnMainHudState::UpdateSatNav(const void* lpEvent, s32 liEventId)
    {
        CGS_ASSERT(lpEvent != 0, " invalid event passed ");   // cpp:1828
        if (mbSatNavEnabled)
        {
            mSatNavComponent.RecvEvent(
                reinterpret_cast<const CgsModule::Event*>(lpEvent), liEventId);
        }
    }

    // =======================================================================
    //  EnableSatNavEventsFilter  @ 0x8247CAE8 / DisableSatNavEventsFilter @ 0x8247CB80
    // =======================================================================
    // The id-204 GuiEventEnableSatNavIcons records: enable posts {displayType 0,
    // modeFilter = meSatNavEventFilter, show 1} on the view-state AND internal-state
    // channels (41 + 40); disable posts {displayType 5 (COUNT == none), modeFilter 6,
    // show 0} on the view-state channel only. 24-byte records: {12, 204, 12} head +
    // the 12-byte payload.
    void FBurnMainHudState::EnableSatNavEventsFilter()
    {
        struct GuiEvent204 : public CgsGui::GuiEvent<204>
        {
            s32 miDisplayType;
            s32 miModeFilter;
            u8  mu8Show;
            u8  mau8Pad[3];
            GuiEvent204(s32 liDisplayType, s32 liModeFilter, u8 lu8Show)
                : CgsGui::GuiEvent<204>(12, 12)
                , miDisplayType(liDisplayType)
                , miModeFilter(liModeFilter)
                , mu8Show(lu8Show)
            {
                mau8Pad[0] = mau8Pad[1] = mau8Pad[2] = 0;
            }
        };

        GuiEvent204 lRecord(0, meSatNavEventFilter, 1);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRecord), KI_CHANNEL_VIEW_STATE, 24);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRecord), KI_CHANNEL_INTERNAL_STATE, 24);
    }

    void FBurnMainHudState::DisableSatNavEventsFilter()
    {
        struct GuiEvent204 : public CgsGui::GuiEvent<204>
        {
            s32 miDisplayType;
            s32 miModeFilter;
            u8  mu8Show;
            u8  mau8Pad[3];
            GuiEvent204(s32 liDisplayType, s32 liModeFilter, u8 lu8Show)
                : CgsGui::GuiEvent<204>(12, 12)
                , miDisplayType(liDisplayType)
                , miModeFilter(liModeFilter)
                , mu8Show(lu8Show)
            {
                mau8Pad[0] = mau8Pad[1] = mau8Pad[2] = 0;
            }
        };

        GuiEvent204 lRecord(5, 6, 0);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRecord), KI_CHANNEL_VIEW_STATE, 24);
    }
}
