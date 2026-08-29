// ===================================================================================
// BrnGui::ShowtimeInstantResultsState  -- the post-event SHOWTIME instant-results screen
//   TU: GameSource/Gui/Flow/PostEvent/States/Showtime/BrnShowtimeInstantResults.cpp
//
// ⭐⭐ WHY THIS TU EXISTS. A finished showtime session already TERMINATES correctly (measured
// twice, on two builds, against a control that never terminates) -- and then it drew NOTHING,
// because the state the terminator hands over to had three logging stubs where its lifecycle
// should be. `ShowtimeInstantResultsState::OnEnter/OnLeave/Update` lived in
// BrnScreenStatesDataLinkStubs.cpp and printed a line. This TU replaces them with the real
// bodies, and the load-bearing one is Update's E_RESULTS_STATE_LOADING_RESOURCES arm:
//
//     mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_RESULTS_MOVIE_RESOURCE], 3);
//
// That single call is what puts the screen on the display. Everything else here fills it in.
//
// RECONSTRUCTED FROM THE X360 ASM (15 bodies; addresses are ARTIST):
//   OnEnter                  @0x824C5D28   OnLeave              @0x824C5FD8
//   Update                   @0x824DFB48   UpdateSubstate       @0x824DC3C8
//   HandleIncomingEvents     @0x824D5A78   HandleAptTriggers    @0x824B4A58
//   AppendExpectedComponents @0x824B4998   SetupComponents      @0x824B4B00
//   SetupTotalling           @0x824BB548   SetupSummary         @0x824B4BC0
//   UpdateEventResults       @0x824D6008   UpdateScoreTotalling @0x824C60B8
//   TickSubstateAndEndIfDone @0x824B4C40   TriggerExitResults   @0x824C6430
//   CalculateMultiplier      @0x824B4D60
// plus the three that were already here (GetNextSubstate @0x824B3B30, ResetStateTimer
// @0x824B3BD0, SetMultiplierText @0x824B3C38).
//
// ⛔ CORRECTION TO THE PREVIOUS REVISION OF THIS FILE -- ResetStateTimer was WRONG, and it
// was wrong in a way no gate could see. It read `mabSubStateFlags[0]` (the bool at +0xA7C)
// where the X360 reads `meActiveSubState` (the int at +0xA78):
//     0x824B3BD0  lwz  r11, 0xA78(r3)      <-- an lwz of a WORD at 0xA78, not an lbz at 0xA7C
//     0x824B3BD4  cmpwi cr6, r11, 0
// The consequence was not cosmetic. OnEnter leaves meActiveSubState == -1 and
// mabSubStateFlags[0] == false, so the old code took the "prime the timer" branch on entry
// and the new one takes the "zero it" branch -- and once UpdateSubstate advances to
// E_ACTIVE_SUBSTATE_EVENT_DONE (1) the old code would keep re-priming a timer the console
// zeroes. The X360's own inlined copy of ResetStateTimer at the tail of OnEnter
// (0x824C5F50, same lwz 0xA78) is the second, independent attestation.
// ===================================================================================
#include "GameSource/Gui/Flow/PostEvent/States/Showtime/BrnShowtimeInstantResults.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDCompress
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SnPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // PlayAptMovie / Register
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h"  // GuiEventAptTriggerPayload
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // ParameterFormatType
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"   // GuiEventShowHideSatNav / BoostBar / Hud
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"          // EPadButton
#include "GameSource/GameState/BrnGameStateSharedIO.h"  // EGameModeType (the mode asserts)

#include <cstring>   // strcmp (HandleAptTriggers' component-name matching)

namespace BrnGui
{
    namespace
    {
        // The state's inbound GUI queue. CgsGui::State only holds an INCOMPLETE
        // `InputBuffer::GuiEventQueue*`, so the concrete queue type has to be named here to
        // drain it; <18432,16> is the committed GUI queue shape and this is the house idiom
        // (identical typedef and cast in BrnOfflineInstantResults.cpp:1324).
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ---- observed event ids -----------------------------------------------------------
        // maiEventToObserve[7] @0x82065B74, READ OUT OF THE IMAGE with tools/re/x360rd.py (the
        // IDA export set is function-only, so no data symbol carries it):
        //     {21, 6, 44, 64, 322, 269, 81}
        // TWO INDEPENDENT CHECKS ON THE EXTENT: (a) the DWARF declares
        // `miNumEventsObserved = 7` (BrnShowtimeInstantResults.cpp:56) and both
        // RegisterForEvents (OnEnter) and UnRegisterForEvents (OnLeave) pass a literal 7;
        // (b) the three floats immediately BELOW the array at 0x82065B68/6C/70 are exactly
        // KF_TOTALLING/SUMMARY/TRANS_OUT_DURATION (10.0 / 3.0 / 1.0), i.e. the array's lower
        // bound is closed by known data too.
        // ⚠️ EVENT 109 IS HANDLED BUT NOT REGISTERED. HandleIncomingEvents has a real arm for
        // id 109 (the "everyone has finished" -> ADVANCE path) that is not in this list. That
        // is the console's own shape, not a transcription slip -- do not "fix" the table.
        const s32 KI_NUM_EVENTS_OBSERVED = 7;
        const s32 maiEventToObserve[KI_NUM_EVENTS_OBSERVED] =
        {
             21,    // CgsGui::GuiEventAptTrigger              -> HandleAptTriggers
              6,    // CgsGui::GuiEventControllerInputPressed  (observed; no arm in this build)
             44,    // network disconnect                      -> SetDoDisconnectPopup + DISCONNECT
             64,    // the GuiCache-is-ready event             -> latch mpGuiCache + copy results
            322,    // stop mode                               -> DISCONNECT or TriggerExitResults
            269,    // online-showtime switch                  -> re-post on the out queue
             81,    // car-select start (online)               -> TO_ON_CARSEL
        };

        // ---- GUI event ids used by name below ---------------------------------------------
        const s32 KI_EVENT_APT_TRIGGER          = 21;
        const s32 KI_EVENT_NETWORK_DISCONNECT   = 44;
        const s32 KI_EVENT_GUI_CACHE            = 64;
        const s32 KI_EVENT_CAR_SELECT_START     = 81;
        const s32 KI_EVENT_ALL_PLAYERS_FINISHED = 109;
        const s32 KI_EVENT_SHOWTIME_SWITCH      = 269;
        const s32 KI_EVENT_STOP_MODE            = 322;

        // 292 (0x124) is the post-event TEARDOWN, the same id the offline results screen
        // posts on its way out. GuiCache::RecEvent case 292 clears mOfflinePostEventData.
        const s32 KI_EVENT_POST_EVENT_TEARDOWN  = 292;
        // The state-output channel every OutputGuiEvent<T> body passes (`li r5, 0x28`).
        const s32 KI_CHANNEL_GUI_OUT            = 40;

        // The FSM this screen hands the HUD flow back to when the session was OFFLINE
        // showtime. Spelled exactly as the image's literal at the CgsIDCompress call site
        // (off_820654E8); CgsIDCompress is case-folding, so it agrees with the "BrnFBFsm"
        // spelling BridgeGameToGui uses for the same id.
        const char KAC_FREEBURN_FSM_ID[] = "BRNFBFSM";

        // The apt movie this screen plays. 217 is "Results" -- the same movie the OFFLINE
        // results screen plays, and also maResourcesToLoad[0]. Update's asm reads it as
        // off_82F27C44, which is gGuiResourceIdentifier (off_82F278E0) + 0x364 bytes ==
        // + 217 entries; the resource table itself says 217 a second time.
        const u32 KU_RESULTS_MOVIE_RESOURCE = 217;

        // ---- component names (X360 string literals at the Construct call sites) -----------
        // ⚠️ Only the names an OnEnter Construct actually passes are defined here. The DWARF
        // also declares KAC_MAIN_BG_ICON_NAME (char[11]), KAC_SECOND_RESULTS_ICON_NAME
        // (char[18]) and KAC_TOTAL_SCORE_ICON_NAME -- nothing in the ARTIST build's emitted
        // code references them, so their contents are NOT recoverable and are not invented.
        // The DWARF array lengths corroborate every name below (KAC_RESULTS_ICON_NAME is
        // char[6] == "Medal", KAC_FINISHED_TEXT_NAME char[9] == "Finished",
        // KAC_MULTIPLIER_SYMBOL_NAME char[11] == "ShowtimeEx"; the two char[13] slots are
        // told apart by WHICH member each Construct targets, not by length).
        const char KAC_RESULTS_ICON_NAME[]      = "Medal";
        const char KAC_FINISHED_TEXT_NAME[]     = "Finished";
        const char KAC_TOTAL_SCORE_TEXT_NAME[]  = "TargetResult";
        const char KAC_MULTIPLIER_TEXT_NAME[]   = "ShowtimeMult";
        const char KAC_MULTIPLIER_SYMBOL_NAME[] = "ShowtimeEx";

        // off_82F26BD4 .. unk_82F26BE0 -- three const char* read out of the image
        // (0x82065268 / 0x8206525C / 0x82065250). The loop bound IS the end address, so the
        // extent is the data's own, not a guess: (0x82F26BE0 - 0x82F26BD4) / 4 == 3.
        const char* const KAC_HELPITEM_NAMES[ShowtimeInstantResultsState::KI_HELPITEMS] =
        {
            "HelpItem1", "HelpItem2", "HelpItem3",
        };

        // ---- localisation ids (BrnShowtimeInstantResults.cpp:88..93) ----------------------
        // ⭐ Every one of these constants holds its OWN identifier as its value, and the DWARF
        // proves it independently: it declares KAC_SHOWTIME_OVER as char[18] and the literal
        // "KAC_SHOWTIME_OVER" is 18 bytes with its NUL; char[19] / "KAC_SHOWTIME_SCORE";
        // char[32] / "KAC_SHOWTIME_DISTANCE_IN_METRES"; char[39] /
        // "KAC_SHOWTIME_DAMAGE_WITHOUT_MULTIPLIER". Six for six.
        const char KAC_SHOWTIME_OVER[]                     = "KAC_SHOWTIME_OVER";
        const char KAC_SHOWTIME_SCORE[]                    = "KAC_SHOWTIME_SCORE";
        const char KAC_SHOWTIME_DAMAGE_WITHOUT_MULTIPLIER[] =
            "KAC_SHOWTIME_DAMAGE_WITHOUT_MULTIPLIER";
        const char KAC_SHOWTIME_DISTANCE_IN_METRES[]       = "KAC_SHOWTIME_DISTANCE_IN_METRES";

        // ---- apt icon states the count-up drives (X360 string literals) --------------------
        const char KAC_ICON_STATE_SCORING[]        = "scoring";
        const char KAC_ICON_STATE_SHOWTIME[]       = "showtime";
        const char KAC_ICON_STATE_SHOWTIME_OUT[]   = "showtimeOut";
        const char KAC_ICON_STATE_INVISIBLE[]      = "Invisible";
        const char KAC_ICON_STATE_DIST_TO_DOLLARS[] = "DistToDollars";
        const char KAC_ICON_STATE_BANK_DIST[]      = "BankDist";
        const char KAC_ICON_STATE_BANK_ALL[]       = "BankAll";
        const char KAC_ICON_STATE_PULSE_MULT[]     = "PulseMultiplier";

        // ---- audio triggers ----------------------------------------------------------------
        // Action 7 is the same action byte every GuiAudioTriggerEvent::Construct site in the
        // tree passes; the component name is the empty string (&unk_820046A7 -- the image byte
        // there is 0, read not assumed).
        const s32  KI_AUDIO_ACTION                 = 7;
        const char KAC_SOUND_SHOWTIME_COUNT_START[] = "ShowtimeCountStart";
        const char KAC_SOUND_SHOWTIME_COUNT_END[]   = "ShowtimeCountEnd";

        // ---- .rdata tuning constants (BrnShowtimeInstantResults.cpp:37..43) -----------------
        // Read with tools/re/x360rd.py at the exact addresses the asm loads from; the DWARF
        // names the run and the six values sit consecutively, which is the corroboration.
        const s32 KI_NUM_CRASH_EXTENSIONS_ALLOWED  = 3;      // dword_82F26BA0
        const f32 KF_CRASH_EXTENSION               = 2.0f;   // flt_82F26BA4
        const f32 KF_TIME_FOR_DISTANCE_DOLLARS     = 8.0f;   // flt_82F26BA8
        const f32 KF_TIME_FOR_DISTANCE_BANKING     = 6.0f;   // flt_82F26BAC
        const f32 KF_TIME_FOR_MULTIPLIER_BANKING   = 3.0f;   // flt_82F26BB0
        const s32 KI_SCORE_MULTIPLIER_BANKING_SPEED = 120;   // dword_82F26BB4 (0x78)

        // flt_820DB5A8 -- metres to yards. The image word is 0x3F8BFB85 == 1.0936133f, which
        // is the standard conversion to five places, so the identification is arithmetic
        // rather than a reading of intent.
        const f32 KF_METRES_TO_YARDS = 1.0936133f;

        // The SnPrintf scratch SetupSummary formats the base score into (X360 sp+0x50, 64
        // bytes, with index 63 explicitly cleared).
        const u32 KU_SCORE_TEXT_LEN = 64;
    }

    // ---- static resource list ---------------------------------------------------------------
    // maResourcesToLoad @0x82F26BB8 / muNumResourcesToLoad @0x82F26BD0 are DEFINED in
    // BrnScreenStatesDataLinkStubs.cpp beside the other screens' tables; they are declared in
    // this class's header and not re-defined here.

    // X360 .rdata durations (loaded by ResetStateTimer for the TOTALLING/SUMMARY/LEAVING cases).
    const f32 ShowtimeInstantResultsState::KF_TOTALLING_DURATION = 10.0f;   // cpp:33, flt_82065B68
    const f32 ShowtimeInstantResultsState::KF_SUMMARY_DURATION   =  3.0f;   // cpp:34, flt_82065654
    const f32 ShowtimeInstantResultsState::KF_TRANS_OUT_DURATION =  1.0f;   // cpp:35, flt_82001C98

    // =======================================================================================
    //  Lifecycle
    // =======================================================================================

    // ---------------------------------------------------------------------------------------
    // OnEnter  @0x824C5D28  (cpp:112)
    // Register for the seven observed events, seed the whole scalar tail, take the sat-nav /
    // boost bar / HUD off the display, then construct the six screen components. The tail is
    // an INLINED ResetStateTimer (0x824C5F50 onward is character-for-character the standalone
    // body at 0x824B3BD0), preceded by the one store the inline does not cover,
    // miCurrentMultiplier = 0.
    // ---------------------------------------------------------------------------------------
    void ShowtimeInstantResultsState::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, KI_NUM_EVENTS_OBSERVED);

        mpGuiCache       = 0;
        meCurrentState   = E_RESULTS_STATE_UNLOADED;
        meActiveSubState = E_ACTIVE_SUBSTATE_EVENT_NONE;

        for (s32 liSubState = 0; liSubState < E_ACTIVE_SUBSTATE_EVENT_COUNT; ++liSubState)
            mabSubStateFlags[liSubState] = false;

        // DONE is always reachable; RESULTS is raised by Update once the components report in.
        mabSubStateFlags[E_ACTIVE_SUBSTATE_EVENT_DONE] = true;

        meSubStateState  = E_SUBSTATE_INVALID;
        meTotallingStage = E_TOTALLING_SUBSTATE_INVALID;

        // ---- three state-interface events, posted before any component exists ---------------
        // Identical shapes to the offline results screen's, and decoded the same way:
        //   {12, 213, 12} + 3 words on channels 41 AND 42 -> GuiEventShowHideSatNav, i.e.
        //        OutputViewState then OutputInternalState of the same object. Payload words
        //        are (1, 0.0f, 0) == (E_MAPTYPE_GPS, no fade, DO NOT SHOW).
        //   {1, 214, 12} + 1 byte on channel 41 -> GuiEventShowHideBoostBar, byte 0 == hide.
        //   {1, 148, 12} + 1 byte on channel 40 -> GuiEventShowHideHud, byte 0 == hide.
        // ⭐ The third one is why this screen was invisible in a different sense than the
        // wave brief assumed: the state hides the in-game HUD on entry, so if it then draws
        // nothing the frame is genuinely emptier than the control's, not merely unchanged.
        GuiEventShowHideSatNav lHideSatNav;
        lHideSatNav.Construct(GuiEventShowHideSatNav::E_MAPTYPE_GPS, false, 0.0f);
        mpStateInterface->OutputViewState(lHideSatNav);
        mpStateInterface->OutputInternalState(lHideSatNav);

        GuiEventShowHideBoostBar lHideBoostBar;
        lHideBoostBar.maData[0] = 0;
        mpStateInterface->OutputViewState(lHideBoostBar);

        GuiEventShowHideHud lHideHud;
        lHideHud.maData[0] = 0;
        mpStateInterface->OutputGuiEvent(lHideHud);

        // ---- the components -----------------------------------------------------------------
        for (s32 liHelpItem = 0; liHelpItem < KI_HELPITEMS; ++liHelpItem)
        {
            mHelpItems[liHelpItem].Construct(KAC_HELPITEM_NAMES[liHelpItem], mpStateInterface, 0);
        }

        // The icon FIRST: its composed name is the parent of all four text fields (the X360
        // caches `r31 + 0x9E0` == mResultsIcon.GetName() and passes it to each Construct).
        mResultsIcon.Construct(KAC_RESULTS_ICON_NAME, mpStateInterface, 0, 0);

        mFinishedText.Construct(KAC_FINISHED_TEXT_NAME, mpStateInterface,
                                mResultsIcon.GetName());
        mTotalScoreText.Construct(KAC_TOTAL_SCORE_TEXT_NAME, mpStateInterface,
                                  mResultsIcon.GetName());
        mMultiplierText.Construct(KAC_MULTIPLIER_TEXT_NAME, mpStateInterface,
                                  mResultsIcon.GetName());
        mMultSymbolText.Construct(KAC_MULTIPLIER_SYMBOL_NAME, mpStateInterface,
                                  mResultsIcon.GetName());

        miCurrentMultiplier = 0;
        ResetStateTimer();
    }

    // ---------------------------------------------------------------------------------------
    // OnLeave  @0x824C5FD8  (cpp:183)
    // Hand the HUD flow back to the FreeBurn FSM (offline sessions only), drop the cache,
    // clear the apt movie, and stop observing.
    // ---------------------------------------------------------------------------------------
    void ShowtimeInstantResultsState::OnLeave()
    {
        // The two ONLINE modes keep their own flow, so only an offline showtime session
        // re-runs BRNFBFSM here. The X360 tests 15 and 16, which are
        // E_MODE_ONLINE_FREE_BURN_LOBBY and E_MODE_ONLINE_SHOWTIME.
        const bool lbWasOnlineSession =
            mResults.meFinishedGameModeType
                == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY
            || mResults.meFinishedGameModeType
                == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME;

        if (!lbWasOnlineSession)
        {
            GuiEventRunFsm lRunFsm;
            lRunFsm.mFsmId          = CgsIDCompress(KAC_FREEBURN_FSM_ID);
            lRunFsm.mInitialStateId = static_cast<CgsID>(0);
            lRunFsm.meFsmToRun      = E_GUI_HUD_FREEBURN;
            lRunFsm.meFlowToUse     = E_GUIFLOW_HUD;
            mpStateInterface->OutputGuiEvent(lRunFsm);
        }

        mpGuiCache = 0;

        // Play the EMPTY apt movie at level 3 -- the level Update played the results movie
        // into, so this clears it. The name pointer is &unk_820046A7 and the image byte there
        // is 0, i.e. the empty string (read, not assumed). Unlike the offline sibling there is
        // no second movie to clear: this screen only ever plays one.
        mpStateInterface->PlayAptMovie("", 3);

        mpStateInterface->UnRegisterForEvents(maiEventToObserve, KI_NUM_EVENTS_OBSERVED);
        meCurrentState = E_RESULTS_STATE_INVALID;
    }

    // ---------------------------------------------------------------------------------------
    // Update  @0x824DFB48  (cpp:223)
    // The load ladder. ⭐ Case E_RESULTS_STATE_LOADING_RESOURCES is the line the whole
    // "showtime has no pixels" problem came down to.
    // ---------------------------------------------------------------------------------------
    void ShowtimeInstantResultsState::Update()
    {
        switch (meCurrentState)
        {
        case E_RESULTS_STATE_INVALID:
            CGS_ASSERT(false, "Invalid state");                        // cpp:281
            break;

        case E_RESULTS_STATE_UNLOADED:
            // Nothing to prepare on this screen -- unlike the offline sibling there is no
            // licence, photo booth or per-mode icon to resolve first, so the arrival of the
            // cache is the whole precondition.
            if (mpGuiCache != 0)
                meCurrentState = E_RESULTS_STATE_LOADING_RESOURCES;
            break;

        case E_RESULTS_STATE_LOADING_RESOURCES:
            if (mpGuiCache != 0
                && mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
            {
                // ⭐⭐ THIS IS THE LINE THAT PUTS THE SHOWTIME RESULTS SCREEN ON THE DISPLAY.
                mpStateInterface->PlayAptMovie(
                    gGuiResourceIdentifier[KU_RESULTS_MOVIE_RESOURCE], 3);

                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                AppendExpectedComponents();

                meCurrentState = E_RESULTS_STATE_LOADING_COMPONENTS;
            }
            break;

        case E_RESULTS_STATE_LOADING_COMPONENTS:
            if (mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                // Only now is the RESULTS sub-state reachable: GetNextSubstate scans
                // mabSubStateFlags, and this is the store that raises index 0.
                mabSubStateFlags[E_ACTIVE_SUBSTATE_EVENT_RESULTS] = true;
                meCurrentState = E_RESULTS_STATE_ACTIVE;

                GuiEventShowHideHud lHideHud;
                lHideHud.maData[0] = 0;
                mpStateInterface->OutputGuiEvent(lHideHud);
            }
            break;

        case E_RESULTS_STATE_ACTIVE:
            UpdateSubstate();
            break;

        default:
            break;
        }

        HandleIncomingEvents();
        reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue)->Clear();
    }

    // =======================================================================================
    //  Components
    // =======================================================================================

    // ---------------------------------------------------------------------------------------
    // AppendExpectedComponents  @0x824B4998  (cpp:303)
    // Register every component whose apt counterpart must report in before the screen counts
    // as initialised. The X360 passes each component's OWN name buffer (component + 4 ==
    // macName), i.e. the composed "<parent>_<name>", not the raw literal -- which is why the
    // asm's offsets sit four bytes above the members'. The icon is registered FIRST, before
    // the four fields parented to it.
    // ---------------------------------------------------------------------------------------
    void ShowtimeInstantResultsState::AppendExpectedComponents()
    {
        CGS_ASSERT(mpGuiCache, "mpGuiCache");                          // cpp:305

        for (s32 liHelpItem = 0; liHelpItem < KI_HELPITEMS; ++liHelpItem)
        {
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   mHelpItems[liHelpItem].GetName());
        }

        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mResultsIcon.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mFinishedText.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTotalScoreText.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMultiplierText.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMultSymbolText.GetName());
    }

    // ---------------------------------------------------------------------------------------
    // SetupComponents  @0x824B4B00  (cpp:519)
    // Fill the three help items and kick the icon into its "scoring" artwork.
    // ---------------------------------------------------------------------------------------
    void ShowtimeInstantResultsState::SetupComponents()
    {
        mHelpItems[0].SetItem("$GENERAL_OPTION_RETRY",
                              ButtonIconComponent::E_PADBUTTON_OPTION0,
                              ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        mHelpItems[1].SetItem("$GENERAL_OPTION_CONTINUE",
                              ButtonIconComponent::E_PADBUTTON_SELECT,
                              ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        // The third help item's caption is &unk_820046A7 -- the image byte there is 0, i.e.
        // the EMPTY STRING. Same constant, same reading, as the offline sibling.
        mHelpItems[2].SetItem("",
                              ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                              ButtonIconComponent::E_PADBUTTON_INVISIBLE);

        CGS_ASSERT(mResults.meFinishedGameModeType
                       == BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME
                   || mResults.meFinishedGameModeType
                       == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME,
                   "mResults.meFinishedGameModeType == BrnGameState::GameStateModuleIO::"
                   "E_MODE_OFFLINE_SHOWTIME || mResults.meFinishedGameModeType == "
                   "BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME");   // cpp:527

        mResultsIcon.SetState(KAC_ICON_STATE_SCORING);
        SetMultiplierText();
    }

    // ---------------------------------------------------------------------------------------
    // SetupTotalling  @0x824BB548  (cpp:543)
    // Turn the raw session record into the four running counters the count-up ladder drains.
    // The distance is converted metres -> yards and scaled by 100 to become "dollars"; the
    // multiplier bonus is (base + distance) * (multiplier - 1).
    // ---------------------------------------------------------------------------------------
    void ShowtimeInstantResultsState::SetupTotalling()
    {
        const s32 liMultiplierBonus = mResults.miScoreMultiplier - 1;

        miBankingDollarTotal = mResults.miBaseScore;

        // fmuls / fctiwz: the truncation to int happens BEFORE the x100, not after.
        miDistanceDollars =
            100 * static_cast<s32>(mResults.mfDistanceTravelled * KF_METRES_TO_YARDS);

        miScoreMultiplierDollars =
            (mResults.miBaseScore + miDistanceDollars) * liMultiplierBonus;

        miBankingDistanceDollars = miDistanceDollars;
        miBankingScoreMultiplier = miScoreMultiplierDollars;

        if (mResults.miBaseScore <= 0)
        {
            miCurrentMultiplier = 1;
        }
        else
        {
            // The console's rounded divide: (multiplier + divisor/2) / divisor + 1, with the
            // divisor's halving done by `srawi 1` + `addze` (round toward zero).
            const s32 liDivisor = miDistanceDollars + mResults.miBaseScore;
            miCurrentMultiplier =
                (miBankingScoreMultiplier + liDivisor / 2) / liDivisor + 1;
        }

        meTotallingStage = E_TOTALLING_SUBSTATE_SHOWING_DISTANCE_METRES;
    }

    // ---------------------------------------------------------------------------------------
    // SetupSummary  @0x824B4BC0  (cpp:569)
    // The end-of-count-up caption pair: "SHOWTIME OVER" plus the final score.
    // ---------------------------------------------------------------------------------------
    void ShowtimeInstantResultsState::SetupSummary()
    {
        mFinishedText.SetLocalisedText(KAC_SHOWTIME_OVER,
                                       CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);

        // The X360 prints the score with a bare "%d" into a 64-byte stack buffer and clears
        // index 63 before handing the buffer to the ONE-POSITIONAL-PARAMETER form.
        // ⚠️ THE SLOT IS miModeScore (+0x1C), NOT miBaseScore (+0x24). `lwz r6, 0xAA4(r31)`
        // @0x824B4BF0, and mResults is based at +0xA88, so 0xAA4 - 0xA88 == 0x1C. Every OTHER
        // read in this TU -- SetupTotalling, CalculateMultiplier -- uses 0xAAC == +0x24 ==
        // miBaseScore, which is exactly why this one is easy to get wrong: the summary shows
        // the MODE score, while the count-up ladder works off the base damage score.
        char lacScoreText[KU_SCORE_TEXT_LEN];
        CgsCore::SnPrintf(lacScoreText, KU_SCORE_TEXT_LEN, "%d", mResults.miModeScore);
        lacScoreText[KU_SCORE_TEXT_LEN - 1] = 0;

        mTotalScoreText.SetLocalisedText(KAC_SHOWTIME_SCORE,
                                         CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                                         1, lacScoreText,
                                         CgsLanguage::LanguageManager::E_FORMAT_MONEY);
    }

    // =======================================================================================
    //  Sub-state machine
    // =======================================================================================

    // ---------------------------------------------------------------------------------------
    // UpdateSubstate  @0x824DC3C8  (cpp:597)
    // ---------------------------------------------------------------------------------------
    void ShowtimeInstantResultsState::UpdateSubstate()
    {
        CGS_ASSERT(meCurrentState == E_RESULTS_STATE_ACTIVE, "Invalid state");   // cpp:599

        switch (meActiveSubState)
        {
        case E_ACTIVE_SUBSTATE_EVENT_NONE:
            meActiveSubState = GetNextSubstate();
            ResetStateTimer();
            meSubStateState = E_SUBSTATE_SET_UP_COMPONENTS;
            break;

        case E_ACTIVE_SUBSTATE_EVENT_RESULTS:
            UpdateEventResults();
            break;

        case E_ACTIVE_SUBSTATE_EVENT_DONE:
            TriggerExitResults();
            break;

        default:
            CGS_ASSERT(false, "Invalid substate");                     // cpp:627
            break;
        }
    }

    // ---------------------------------------------------------------------------------------
    // UpdateEventResults  @0x824D6008  (cpp:642)
    // The four-phase presentation: set up, count up, summarise, transition out.
    // ---------------------------------------------------------------------------------------
    void ShowtimeInstantResultsState::UpdateEventResults()
    {
        CGS_ASSERT(meActiveSubState == E_ACTIVE_SUBSTATE_EVENT_RESULTS,
                   "E_ACTIVE_SUBSTATE_EVENT_RESULTS == meActiveSubState");        // cpp:644

        switch (meSubStateState)
        {
        case E_SUBSTATE_SET_UP_COMPONENTS:
            SetupTotalling();
            SetupComponents();
            meSubStateState = E_SUBSTATE_TOTALLING;
            ResetStateTimer();
            break;

        case E_SUBSTATE_TOTALLING:
            UpdateScoreTotalling();
            if (TickSubstateAndEndIfDone())
            {
                SetupSummary();
                meSubStateState = E_SUBSTATE_SUMMARY;
                ResetStateTimer();
                mResultsIcon.SetState(KAC_ICON_STATE_SHOWTIME);
            }
            break;

        case E_SUBSTATE_SUMMARY:
            if (TickSubstateAndEndIfDone())
            {
                mResultsIcon.SetState(KAC_ICON_STATE_SHOWTIME_OUT);
                meActiveSubState = GetNextSubstate();
                meSubStateState  = E_SUBSTATE_LEAVING;
                ResetStateTimer();
            }
            break;

        case E_SUBSTATE_LEAVING:
            if (TickSubstateAndEndIfDone())
            {
                mResultsIcon.SetState(KAC_ICON_STATE_INVISIBLE);
                meActiveSubState = GetNextSubstate();
                meSubStateState  = E_SUBSTATE_SET_UP_COMPONENTS;
                ResetStateTimer();
            }
            break;

        default:
            // The console streams the offending value into the message buffer. The literal is
            // the console's own, copy-and-paste artefact and all ("car unlock" on the showtime
            // screen) -- transcribed, not corrected.
            CGS_ASSERT(false,
                       "Should not be updating car unlock when substate is in state ");  // cpp:717
            break;
        }
    }

    // ---------------------------------------------------------------------------------------
    // UpdateScoreTotalling  @0x824C60B8  (cpp:732)
    // The count-up ladder itself. Each rung runs until mfTimeRemaining drops below its own
    // threshold, then hands over to the next. The tail line runs on EVERY rung: the damage
    // total is re-pushed to the caption each frame.
    // ---------------------------------------------------------------------------------------
    void ShowtimeInstantResultsState::UpdateScoreTotalling()
    {
        switch (meTotallingStage)
        {
        case E_TOTALLING_SUBSTATE_SHOWING_DISTANCE_METRES:
            if (mfTimeRemaining > KF_TIME_FOR_DISTANCE_DOLLARS)
            {
                mTotalScoreText.SetLocalisedText(
                    KAC_SHOWTIME_DISTANCE_IN_METRES,
                    CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                    mResults.mfDistanceTravelled,
                    CgsLanguage::LanguageManager::E_FORMAT_SMALL_DISTANCE);
            }
            else
            {
                mResultsIcon.SetState(KAC_ICON_STATE_DIST_TO_DOLLARS);
                meTotallingStage = E_TOTALLING_SUBSTATE_SHOWING_DISTANCE_DOLLARS;
            }
            break;

        case E_TOTALLING_SUBSTATE_SHOWING_DISTANCE_DOLLARS:
            if (mfTimeRemaining > KF_TIME_FOR_DISTANCE_BANKING)
            {
                mTotalScoreText.SetLocalisedText(
                    miBankingDistanceDollars,
                    CgsLanguage::LanguageManager::E_FORMAT_MONEY);
            }
            else
            {
                meTotallingStage = E_TOTALLING_SUBSTATE_BANKING_DISTANCE;
            }
            break;

        case E_TOTALLING_SUBSTATE_BANKING_DISTANCE:
            if (mfTimeRemaining > KF_TIME_FOR_MULTIPLIER_BANKING)
            {
                // The distance pot is banked in ONE go, not drained.
                if (miBankingDistanceDollars > 0)
                {
                    miBankingDollarTotal    += miBankingDistanceDollars;
                    miBankingDistanceDollars = 0;
                    mResultsIcon.SetState(KAC_ICON_STATE_BANK_DIST);
                }
                CGS_ASSERT(miBankingDistanceDollars >= 0,
                           "miBankingDistanceDollars >= 0");            // cpp:812
            }
            else
            {
                GuiAudioTriggerEvent lCountStart;
                lCountStart.Construct(KI_AUDIO_ACTION, "", KAC_SOUND_SHOWTIME_COUNT_START);
                mpStateInterface->OutputGuiEvent(lCountStart);
                meTotallingStage = E_TOTALLING_SUBSTATE_SHOWING_MULTIPLICATION;
            }
            break;

        case E_TOTALLING_SUBSTATE_SHOWING_MULTIPLICATION:
            CGS_ASSERT(mResults.miScoreMultiplier > 0,
                       "mResults.miScoreMultiplier > 0");                // cpp:819

            if (miBankingScoreMultiplier > 0)
            {
                // Drain the multiplier pot at a fixed rate, but never past what is left:
                // the console's `v12 & ~((v9-v12)>>31) | ((v9-v12)>>31) & v9` is a branchless
                // min() of the per-frame step and the remaining balance.
                const s32 liStep = miScoreMultiplierDollars / KI_SCORE_MULTIPLIER_BANKING_SPEED;
                const s32 liBanked =
                    (liStep < miBankingScoreMultiplier) ? liStep : miBankingScoreMultiplier;

                if (liBanked > 0)
                {
                    miBankingScoreMultiplier -= liBanked;
                    miBankingDollarTotal     += liBanked;
                }

                CGS_ASSERT(miBankingScoreMultiplier >= 0,
                           "miBankingScoreMultiplier >= 0");             // cpp:843

                const s32 liMultiplier = CalculateMultiplier();
                if (liMultiplier != miCurrentMultiplier)
                {
                    miCurrentMultiplier = liMultiplier;
                    SetMultiplierText();
                    mResultsIcon.SetState(KAC_ICON_STATE_PULSE_MULT);
                }
            }
            else
            {
                mResultsIcon.SetState(KAC_ICON_STATE_BANK_ALL);

                GuiAudioTriggerEvent lCountEnd;
                lCountEnd.Construct(KI_AUDIO_ACTION, "", KAC_SOUND_SHOWTIME_COUNT_END);
                mpStateInterface->OutputGuiEvent(lCountEnd);

                meTotallingStage = E_TOTALLING_SUBSTATE_BANKING_SCORE;
            }
            break;

        case E_TOTALLING_SUBSTATE_BANKING_SCORE:
            break;

        default:
            CGS_ASSERT(false, "Invalid totalling substate - ");          // cpp:870
            break;
        }

        // Runs on every rung including the default one -- the X360 falls through the whole
        // switch into this single tail call.
        mFinishedText.SetLocalisedText(KAC_SHOWTIME_DAMAGE_WITHOUT_MULTIPLIER,
                                       CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                                       miBankingDollarTotal,
                                       CgsLanguage::LanguageManager::E_FORMAT_MONEY);
    }

    // ---------------------------------------------------------------------------------------
    // TickSubstateAndEndIfDone  @0x824B4C40  (cpp:891)
    // Run the sub-state clock down by one frame, and -- this is the interesting part -- give
    // the player MORE time whenever another car has crashed since the last tick, up to
    // KI_NUM_CRASH_EXTENSIONS_ALLOWED times. That is what makes a big pile-up hold the
    // count-up open.
    // ---------------------------------------------------------------------------------------
    bool ShowtimeInstantResultsState::TickSubstateAndEndIfDone()
    {
        CGS_ASSERT(meActiveSubState > E_ACTIVE_SUBSTATE_EVENT_NONE
                       && meActiveSubState < E_ACTIVE_SUBSTATE_EVENT_COUNT,
                   "(E_ACTIVE_SUBSTATE_EVENT_NONE < meActiveSubState) && "
                   "(E_ACTIVE_SUBSTATE_EVENT_COUNT > meActiveSubState)");     // cpp:894
        CGS_ASSERT(meSubStateState == E_SUBSTATE_TOTALLING
                       || meSubStateState == E_SUBSTATE_SUMMARY,
                   "E_SUBSTATE_TOTALLING == meSubStateState || "
                   "E_SUBSTATE_SUMMARY == meSubStateState");                  // cpp:896
        CGS_ASSERT(mpGuiCache, "mpGuiCache");                                 // cpp:900

        mfTimeRemaining -= mpGuiCache->GetTimeStep();

        const s32 liCarsCrashed = mpGuiCache->GetShowTimeCarsCrashed();
        if (miLastCrashedCars != liCarsCrashed)
        {
            miLastCrashedCars = liCarsCrashed;
            if (miCrashExtensionsRemaining > 0)
            {
                --miCrashExtensionsRemaining;
                mfTimeRemaining += KF_CRASH_EXTENSION;
            }
        }

        return mfTimeRemaining <= 0.0f;
    }

    // ---------------------------------------------------------------------------------------
    // TriggerExitResults  @0x824C6430  (cpp:933)
    // Leave: advance the flow FSM and post the post-event teardown. Simpler than the offline
    // sibling's -- there are no credits and no music-stream suppression on this screen.
    // ---------------------------------------------------------------------------------------
    void ShowtimeInstantResultsState::TriggerExitResults()
    {
        SendStateEvent("ADVANCE");

        // {1, 292, 12} on channel 40. The one payload word at +12 is never written by the
        // console either -- it is the sizeof of an empty event struct, not a value. Zeroed
        // here rather than left indeterminate; no consumer reads it.
        struct PostEventTeardownRecord
        {
            s32 miOutEventSize;
            s32 miOutEventType;
            s32 miOutEventOffset;
            s32 miPayload;
        } lTeardown;
        lTeardown.miOutEventSize   = 1;
        lTeardown.miOutEventType   = KI_EVENT_POST_EVENT_TEARDOWN;   // 0x124
        lTeardown.miOutEventOffset = 12;
        lTeardown.miPayload        = 0;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lTeardown), KI_CHANNEL_GUI_OUT, 16);
    }

    // ---------------------------------------------------------------------------------------
    // CalculateMultiplier  @0x824B4D60  (cpp:951)
    // The multiplier the count-up is currently showing: how many times the base-plus-distance
    // pot the still-unbanked multiplier balance represents, rounded, plus one.
    // ---------------------------------------------------------------------------------------
    s32 ShowtimeInstantResultsState::CalculateMultiplier()
    {
        if (mResults.miBaseScore <= 0)
            return 1;

        const s32 liDivisor = miDistanceDollars + mResults.miBaseScore;
        return (miBankingScoreMultiplier + liDivisor / 2) / liDivisor + 1;
    }

    // =======================================================================================
    //  Events
    // =======================================================================================

    // ---------------------------------------------------------------------------------------
    // HandleIncomingEvents  @0x824D5A78  (cpp:330)
    // ---------------------------------------------------------------------------------------
    void ShowtimeInstantResultsState::HandleIncomingEvents()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;

        for (s32 liEventType = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventType = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            // The X360 re-reads meCurrentState at the top of every iteration and stops
            // dispatching once the state has been invalidated (OnLeave ran).
            if (meCurrentState == E_RESULTS_STATE_INVALID)
                break;

            switch (liEventType)
            {
            case KI_EVENT_APT_TRIGGER:
                HandleAptTriggers(
                    reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent));
                break;

            case KI_EVENT_NETWORK_DISCONNECT:
                CGS_ASSERT(mpGuiCache, "mpGuiCache");                         // cpp:390
                mpGuiCache->SetDoDisconnectPopup(lpEvent);
                SendStateEvent("DISCONNECT");
                if ((CgsDev::Message::gxMessageFilterFlags
                     & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "\n\nONLINE SHOWTIME INSTANT RESULTS: NETWORK DISCONNECT RECEIVED\n\n";
                }
                break;

            case KI_EVENT_GUI_CACHE:
                // Latched ONCE: the X360 guards the whole arm on `if (!mpGuiCache)`.
                if (mpGuiCache == 0)
                {
                    CGS_ASSERT(*reinterpret_cast<GuiCache* const*>(lpEvent),
                               "Invalid cache in "
                               "ShowtimeInstantResultsState::UpdateLoadingScreen");   // cpp:349

                    mpGuiCache = *reinterpret_cast<GuiCache* const*>(lpEvent);

                    // X360: memcpy(&mResults, (u8*)cache + 40552, 192). Expressed as the
                    // struct copy it is, so no offset arithmetic survives into the C++.
                    mResults = mpGuiCache->GetOfflinePostEventData();

                    // Seed the crash-extension clock from the session's live crash count, so
                    // the first TickSubstateAndEndIfDone only extends on a NEW crash.
                    miLastCrashedCars          = mpGuiCache->GetShowTimeCarsCrashed();
                    miCrashExtensionsRemaining = KI_NUM_CRASH_EXTENSIONS_ALLOWED;
                }
                break;

            case KI_EVENT_CAR_SELECT_START:
            {
                // Re-post the HUD hide, this time on the INTERNAL-STATE channel (42) rather
                // than the OutputGuiEvent channel (40) OnEnter used, then hand the flow to
                // the online car-select screen.
                // ⚠️ The payload byte is 0 -- the same value OnEnter posts, i.e. HIDE. An
                // earlier draft of this arm called it "show the HUD again"; the console
                // stores `li r11, 0 / stb r11, sp+0x8C`, so it does not.
                GuiEventShowHideHud lHideHud;
                lHideHud.maData[0] = 0;
                mpStateInterface->OutputInternalState(lHideHud);

                CGS_ASSERT(*reinterpret_cast<const s32*>(lpEvent) == 2,
                           "static_cast<const GuiCarSelectStartEvent*>( lpEvent )"
                           "->meCarSelectType == GsmIO::E_CAR_SELECT_TYPE_ONLINE_EVENT_START");
                                                                              // cpp:438
                SendStateEvent("TO_ON_CARSEL");
                break;
            }

            case KI_EVENT_ALL_PLAYERS_FINISHED:
            {
                // The online "everyone is done" path. The X360 walks
                // mpStateInterface->GetAccessPointers()->mpGuiCache by hand (four asserts on
                // the way) rather than using its own mpGuiCache, because this arm can run
                // before the cache event has been latched.
                CGS_ASSERT(mpStateInterface, "mpStateInterface");             // cpp:450
                CGS_ASSERT(mpStateInterface->GetAccessPointers(),
                           "mpStateInterface->GetAccessPointers()");          // cpp:451
                GuiCache* lpGuiCache = mpStateInterface->GetAccessPointers()->GetGuiCache();
                CGS_ASSERT(lpGuiCache, "lpGuiCache");                         // cpp:453

                if (lpGuiCache->GetGameMode()
                        != BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME)
                    break;

                SendStateEvent("ADVANCE");

                struct ShowtimeFinishedRecord
                {
                    s32 miOutEventSize;
                    s32 miOutEventType;
                    s32 miOutEventOffset;
                    s32 miPayload;
                } lFinished;
                lFinished.miOutEventSize   = 1;
                lFinished.miOutEventType   = KI_EVENT_POST_EVENT_TEARDOWN;
                lFinished.miOutEventOffset = 12;
                lFinished.miPayload        = 0;
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lFinished),
                    KI_CHANNEL_GUI_OUT, 16);
                break;
            }

            case KI_EVENT_SHOWTIME_SWITCH:
                // Re-post the switch on the OUT queue so the mode manager sees it, but only
                // for the one sub-type the screen forwards.
                if (*reinterpret_cast<const s32*>(lpEvent) == 1)
                {
                    struct ShowtimeSwitchRecord
                    {
                        s32 miOutEventSize;
                        s32 miOutEventType;
                        s32 miOutEventOffset;
                        s32 miPayload;
                    } lSwitch;
                    lSwitch.miOutEventSize   = 4;
                    lSwitch.miOutEventType   = KI_EVENT_SHOWTIME_SWITCH;
                    lSwitch.miOutEventOffset = 12;
                    lSwitch.miPayload        = 2;
                    mpStateInterface->GetOutputEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lSwitch),
                        KI_CHANNEL_GUI_OUT, 16);
                }
                break;

            case KI_EVENT_STOP_MODE:
                // Two BYTES in the record distinguish a disconnect-driven stop from a normal
                // one. ⚠️ THE WIDTH IS ASM-PINNED, NOT READ OFF THE PSEUDOCODE: Hex-Rays
                // renders these as `*(v3 + 10) || *(v3 + 9)` over an `int v3`, which reads as
                // word indices 10 and 9 (byte offsets 40 and 36). The X360 is
                //     0x824D5E08  lbz  r11, 0xA(r27)
                //     0x824D5E14  lbz  r11, 9(r27)
                // -- lbz, i.e. the BYTES at +10 and +9. The offline sibling's own event-322
                // arm reads the same record as `const u8*`, which is the second attestation.
                if (reinterpret_cast<const u8*>(lpEvent)[10] != 0
                    || reinterpret_cast<const u8*>(lpEvent)[9] != 0)
                {
                    if ((CgsDev::Message::gxMessageFilterFlags
                         & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "\n\nONLINE SHOWTIME INSTANT RESULTS: STOP MODE "
                               "(DUE TO DISCONNECT) RECEIVED\n\n";
                    }
                    SendStateEvent("DISCONNECT");
                }
                else
                {
                    if ((CgsDev::Message::gxMessageFilterFlags
                         & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "\n\nONLINE SHOWTIME INSTANT RESULTS: STOP MODE "
                               "(NORMAL) RECEIVED\n\n";
                    }
                    TriggerExitResults();
                }
                break;

            default:
                break;
            }
        }
    }

    // ---------------------------------------------------------------------------------------
    // HandleAptTriggers  @0x824B4A58  (cpp:482)
    // When the "Finished" caption's apt clip finishes loading, re-push the text the field is
    // already holding -- otherwise the clip comes up with the previous frame's string.
    // ---------------------------------------------------------------------------------------
    void ShowtimeInstantResultsState::HandleAptTriggers(
        const CgsGui::GuiEventAptTriggerPayload* lpEvent)
    {
        CGS_ASSERT(lpEvent, "lpEvent");                                       // cpp:484

        if (lpEvent->meEventType != CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD)
            return;

        if (std::strcmp(mFinishedText.GetName(), lpEvent->mpacComponentName) == 0)
            mFinishedText.SetText(mFinishedText.GetText());
    }

    // =======================================================================================
    //  Small helpers (these three predate this wave; ResetStateTimer is CORRECTED -- see the
    //  banner).
    // =======================================================================================

    // ---------------------------------------------------------------------------------------
    // GetNextSubstate  @0x824B3B30  (DWARF h:271)
    // Scan mabSubStateFlags forward from just past meActiveSubState for the first raised flag
    // and return its index; if none is raised, return E_ACTIVE_SUBSTATE_EVENT_DONE.
    // ---------------------------------------------------------------------------------------
    ShowtimeInstantResultsState::EResultsActiveSubStates
    ShowtimeInstantResultsState::GetNextSubstate()
    {
        CGS_ASSERT(meActiveSubState < E_ACTIVE_SUBSTATE_EVENT_COUNT,
                   "meActiveSubState < E_ACTIVE_SUBSTATE_EVENT_COUNT");       // h:273

        for (s32 liIndex = meActiveSubState + 1;
             liIndex < E_ACTIVE_SUBSTATE_EVENT_COUNT; ++liIndex)
        {
            if (mabSubStateFlags[liIndex])
                return static_cast<EResultsActiveSubStates>(liIndex);
        }
        return E_ACTIVE_SUBSTATE_EVENT_DONE;
    }

    // ---------------------------------------------------------------------------------------
    // ResetStateTimer  @0x824B3BD0  (DWARF h:303)
    // Reload mfTimeRemaining with the duration for the current sub-state -- but only while the
    // RESULTS sub-state is the active one; every other sub-state gets a zero clock.
    // ⛔ THE GATE IS meActiveSubState (`lwz r11, 0xA78`), NOT mabSubStateFlags[0]. The previous
    // revision of this file had the wrong member; see the file banner.
    // ---------------------------------------------------------------------------------------
    void ShowtimeInstantResultsState::ResetStateTimer()
    {
        if (meActiveSubState != E_ACTIVE_SUBSTATE_EVENT_RESULTS)
        {
            mfTimeRemaining = 0.0f;
            return;
        }

        switch (meSubStateState)
        {
        case E_SUBSTATE_TOTALLING:
            mfTimeRemaining = KF_TOTALLING_DURATION;
            break;
        case E_SUBSTATE_SUMMARY:
            mfTimeRemaining = KF_SUMMARY_DURATION;
            break;
        case E_SUBSTATE_LEAVING:
            mfTimeRemaining = KF_TRANS_OUT_DURATION;
            break;
        default:
            mfTimeRemaining = 0.0f;
            break;
        }
    }

    // ---------------------------------------------------------------------------------------
    // SetMultiplierText  @0x824B3C38  (DWARF h:354)
    // A multiplier of 1 or less means "no multiplier": both the value field and the 'X' symbol
    // are cleared. Otherwise the value is formatted into the multiplier field and the symbol
    // field shows "X".
    // ---------------------------------------------------------------------------------------
    void ShowtimeInstantResultsState::SetMultiplierText()
    {
        if (miCurrentMultiplier <= 1)
        {
            mMultiplierText.SetText("");
            mMultiplierText.OutputAptData();
            mMultSymbolText.SetText("");
            mMultSymbolText.OutputAptData();
        }
        else
        {
            mMultiplierText.SetLocalisedText(
                miCurrentMultiplier, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
            mMultSymbolText.SetText("X");
        }
    }
}
