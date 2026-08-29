// ===================================================================================
// BrnGui::InstantResultsState  -- the offline post-event instant-results presentation state
//   class:BrnGui::InstantResultsState
//
// ⭐⭐ WHY THIS FILE GREW: FINISHING AN OFFLINE EVENT PRODUCED NO PIXELS, AND THIS STATE WAS
// THE REASON. The chain in front of it is whole end to end -- ModeManager::FinishCurrentMode
// -> ShowModeResults posts game action 37 -> the bridge translates it to GUI 291 ->
// InGame::Update case 291 -> SendStateEvent("TO_OFF_POST") -> the screen flow ENTERS THIS
// STATE. It then hit `OnEnter() { LogUnreconstructedState(...); }` in
// BrnScreenStatesDataLinkStubs.cpp and returned. Four run logs on disk end with exactly that
// one line after the whole chain succeeds.
//
// ⛔ THE LEDGER CALLED ALL 32 OF THIS CLASS'S FUNCTIONS `reviewed` WHILE 28 HAD NO BODY. The
// link did not catch it because three logging stubs satisfied it. `reviewed` is not evidence
// a body exists; only the file is.
//
// FUNCTIONS BODIED HERE (X360 addresses; instruction counts from the exports):
//   InstantResultsState (ctor)     @0x825006D8 ( 56)   OnEnter        @0x824C3398 (357)
//   OnLeave                        @0x824C3930 (208)   Update         @0x824DF760 (244)
//   HandleIncomingEvents           @0x824DBAD8 (427, PARTIAL -- see the banner on it)
//   AppendAllExpectedComponents    @0x824BB458 ( 31)   AppendExpectedScreenComponents
//                                                                     @0x824B3CB0 ( 83)
//   SelectSubstates                @0x824D59B0 ( 49)   UpdateSubstate @0x824DC188 (133, PARTIAL)
//   TickSubstateAndEndIfDone       @0x824BB4D8 ( 27)   HasSubstateTimedOut @0x824B48C8 ( 52)
//   TriggerExitResults             @0x824D58A8 ( 66)   WillShowCredits @0x824C5C38 ( 59)
//   GetNextSubstate                @0x824B3820         ResetStateTimer @0x824B38C0
//   SetEventIconResource           @0x824B39B0
//
// ⛔ STILL NOT RECONSTRUCTED (declared in the header, bodied as LOGGED stubs in
// BrnScreenStatesDataLinkStubs.cpp so the gap is visible in the log instead of silent):
//   SetupComponents (492) HandleAptTriggers (475) HandleControllerInput (124)
//   UpdateEventResults (184) UpdateSecondResultsPage (103) UpdateTakePhotoPage
//   UpdateRankUp (77) UpdateLicense UpdateCarUnlock UpdateFreeCarUnlock UpdateShowingRivals
//   UpdateLeaving (68) UpdatePhoto IsXSCarInUnlockedArray RenderDebug
// Those are the substate PRESENTATIONS. They are not on the path that puts the results movie
// on screen (that is Update case 1's PlayAptMovie, which runs before any substate does), and
// several of them need OfflinePostEventData flag slots the X360 asm does not yet pin -- see
// the ⛔ block in BrnGuiEventTypeDefs.h. Guessing those names is how a results page ends up
// printing the wrong string, so they are left for a wave that can attest them.
// ===================================================================================
#include "GameSource/Gui/Flow/PostEvent/States/Offline/BrnOfflineInstantResults.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDCompress
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // PlayAptMovie / Register
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"   // GuiEventShowHideSatNav / ShowHideBoostBar
#include "GameSource/GameState/Progression/BrnProfile.h"  // BrnProgression::Profile getters
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // BrnGameState::GameStateModuleIO::EGameModeType (assert bounds)

namespace BrnGui
{
    namespace
    {
        // The state's inbound GUI queue. CgsGui::State only holds an INCOMPLETE
        // `InputBuffer::GuiEventQueue*`, so the concrete queue type has to be named here to
        // drain it; <18432,16> is the committed GUI queue shape and this is the house idiom
        // (identical typedef and cast in BrnPreRaceFlyBy_wJ_03.cpp:51, BrnBootAttract.cpp:15).
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // ---- observed event ids ---------------------------------------------------------
        // maiEventToObserve[15] @0x82065684, READ OUT OF THE IMAGE (the IDA export set is
        // function-only, so no data symbol carries it): {21, 6, 14, 16, 64, 322, 296, 297,
        // 299, 300, 301, 307, 350, 569, 436}.
        // ⭐ TWO INDEPENDENT CHECKS ON THE EXTENT. (a) The DWARF declares
        // `miNumEventsObserved = 15` (BrnOfflineInstantResults.cpp:83) and RegisterForEvents
        // is called with a literal 15; (b) the very next word in the image, @0x820656C0,
        // reads 15 -- i.e. the count constant sits immediately after its array, exactly as a
        // `const int32_t[15]` followed by `const int32_t = 15` would. And every one of the
        // fifteen ids appears as a case in HandleIncomingEvents' switch, which is a third.
        const s32 KI_NUM_EVENTS_OBSERVED = 15;
        const s32 maiEventToObserve[KI_NUM_EVENTS_OBSERVED] =
        {
             21,    // CgsGui::GuiEventAptTrigger        -> HandleAptTriggers
              6,    // CgsGui::GuiEventControllerInputPressed -> HandleControllerInput
             14,    // load notification                 -> LargeCarComponent::HandleLoadNotification
             16,    // unload notification               -> LargeCarComponent::HandleUnloadNotification
             64,    // the GuiCache-is-ready event       -> latch mpGuiCache + copy the results
            322,    // (observed; handled by an arm this wave does not land)
            296,    // (observed; handled by an arm this wave does not land)
            297,    // (observed; handled by an arm this wave does not land)
            299,    // "told to start looking at new rivals"
            300,    // "told to look at new rival"
            301,    // "told to end rival unlocking sequence"
            307,    // -> SetupComponents  (the medal-counter refresh the finish also posts)
            350,    // progression-profile pointer       -> LicenseComponent::SetProfilePointer
            569,    // compressed still image            -> PhotoBoothComponent
            436,    // percentage complete               -> LicenseComponent::SetPercentageComplete
        };

        // ---- Apt FRAME-NAME tables (.rdata) ----------------------------------------------
        // ⭐⭐ THESE SIX TABLES WERE THE STANDING BLOCKER ON THIS SCREEN. A previous wave
        // reported them as "cannot be read on this box" and named producing an image dump as
        // the prerequisite. The reader existed all along -- it just lived in a %TEMP%
        // scratchpad, invisible to every wave. It is now `tools/re/x360rd.py` in the repo.
        //
        // ⚠️ THAT READER WAS ONCE MIS-CALIBRATED BY 1594 BYTES WITH ITS OWN SELF-TEST
        // PASSING, so nothing below is trusted on its say-so. Every value here survives THREE
        // independent checks:
        //
        //   1. THE STRING POOL TILES WITH NO GAPS. The 25 strings these tables point at run
        //      unbroken from 0x82065084 to 0x82065274: each one's length + NUL, rounded up to
        //      4, lands EXACTLY on the next string's address, 24 times in a row. A reader off
        //      by any non-zero amount cannot produce that.
        //   2. THE DWARF DECLARES THE ARRAY SIZES, AND ALL TWENTY MATCH. e.g.
        //      KAC_RESULTS_ICON_NAME[6] vs "Medal"(5+1); KAC_CAR_UNLOCK_MANUFACTURER_ICON_NAME[21]
        //      vs "CarUnlockManuIcon_mc"(20+1); KAC_NEW_XS_CAR_LARGE_ICON_NAME[15] vs
        //      "carLargeXs_cpt"(14+1); KAC_UPGRADE_STATE_ANIMATOR[18] vs "mainStateAnimator"(17+1).
        //      Not one length is off by one.
        //   3. THE SEMANTICS LINE UP ONE-FOR-ONE. KAC_RESULTS_FRAMES[6] matches
        //      EResultsAnimations' six values in order (DETAILED_WIN -> "WinWithDetails",
        //      WIN_WITH_TARGETS -> "WinWithTargets", PLAIN_WIN -> "MiscTextPos", then the three
        //      losing twins); KAC_FINISH_POS_STRINGIDS[8] reads FIRST..EIGHTH in order.
        //   4. (bonus, from the code) UpdateEventResults @0x824BE228 SnPrintf's "%sOut" over
        //      KAC_RESULTS_FRAMES[mResultsIcon.GetState()] -- i.e. the strings must be Apt
        //      frame-label PREFIXES that take an "Out" suffix. "WinWithDetails"/"WinWithDetailsOut"
        //      is exactly that shape.
        //
        // The already-committed table below (maResourcesToLoad @0x82F26AFC) is a FIFTH check
        // on the reader's calibration at this very address: an earlier wave published
        // {217, type 4}, {55, type 4}, count 2 from an independent read, and the reader
        // reproduces those five words exactly.
        //
        // Getting one of these wrong shows an EMPTY MOVIE rather than an error, so they were
        // read, never guessed.
        const char* const KAC_HELPITEM_NAMES[3] =                    // @0x82F26B10, DWARF cpp:117
        {
            "HelpItem1", "HelpItem2", "HelpItem3"
        };
        const char* const KAC_RESULTS_FRAMES[6] =                    // @0x82F26B1C, DWARF cpp:132
        {
            "WinWithDetails",    // E_RESULTS_DETAILED_WIN      = 0
            "WinWithTargets",    // E_RESULTS_WIN_WITH_TARGETS  = 1
            "MiscTextPos",       // E_RESULTS_PLAIN_WIN         = 2
            "LoseWithDetails",   // E_RESULTS_DETAILED_LOSS     = 3
            "LoseWithTargets",   // E_RESULTS_LOSS_WITH_TARGETS = 4
            "MiscTextNeg",       // E_RESULTS_PLAIN_LOSS        = 5
        };
        const char* const KAC_SECOND_RESULTS_FRAMES[3] =             // @0x82F26B34, DWARF cpp:142
        {
            "transIn", "transOut", "invisible"
        };
        const char* const KAC_RANK_TEXT_FRAMES[3] =                  // @0x82F26B40, DWARF cpp:149
        {
            "transIn", "transOut", "invisible"
        };
        const char* const KAC_CAR_TEXT_FRAMES[5] =                   // @0x82F26B4C, DWARF cpp:156
        {
            "transIn", "fadeOutText", "fadeInText", "transOut", "invisible"
        };
        const char* const KAC_NEW_RIVALS_TEXT_FRAMES[6] =            // @0x82F26B60, DWARF cpp:165
        {
            "transInIntro", "transOutIntro", "transInName",
            "transOutName", "transInOutro", "transOutOutro"
        };

        // ---- component names (DWARF cpp:97..128; the image strings, lengths cross-checked
        //      against the DWARF's declared array sizes as described above) -----------------
        const char KAC_SHUTDOWN_CAR_TEXT_NAME[]             = "CarShutdown";           // [12]
        const char KAC_SECOND_RESULTS_ICON_NAME[]           = "SecondResultsIcon";     // [18]
        const char KAC_CAR_UNLOCK_MANUFACTURER_ICON_NAME[]  = "CarUnlockManuIcon_mc";  // [21]
        const char KAC_CAR_UNLOCK_TEXT_NAME[]               = "CarUnlock";             // [10]
        const char KAC_CAR_UNLOCK_DESC_NAME[]               = "CarUnlockDescText_cpt"; // [22]
        const char KAC_CAR_UNLOCK_ICON_NAME[]               = "CarIcon";               // [8]
        const char KAC_RANK_UP_ICON_NAME[]                  = "RankIcon";              // [9]
        const char KAC_LICENSE_COMPONENT_NAME[]             = "License_cpt";           // [12]
        const char KAC_PHOTO_COMPONENT_NAME[]               = "PhotoBooth_cpt";        // [15]
        const char KAC_LARGE_EVENT_ICON_NAME[]              = "postModeIcon";          // [13]
        const char KAC_NEW_RIVAL_MANUFACTURER_ICON_NAME[]   = "RivalManuIcon_mc";      // [17]
        const char KAC_NEW_RIVAL_CAR_TEXT_NAME[]            = "NewRivalModel";         // [14]
        const char KAC_NEW_RIVAL_DESC_TEXT_NAME[]           = "NewRival";              // [9]
        const char KAC_NEW_RIVAL_ICON_NAME[]                = "NewRivals";             // [10]
        const char KAC_NEW_XS_CAR_LARGE_ICON_NAME[]         = "carLargeXs_cpt";        // [15]
        const char KAC_NEW_RIVAL_CAR_LARGE_ICON_NAME[]      = "carLarge_cpt";          // [13]
        const char KAC_RESULTS_ICON_NAME[]                  = "Medal";                 // [6]
        const char KAC_FINISHED_TEXT_NAME[]                 = "Finished";              // [9]
        const char KAC_TARGET_RESULT_TEXT_NAME[]            = "TargetResult";          // [13]
        const char KAC_UPGRADE_STATE_ANIMATOR[]             = "mainStateAnimator";     // [18]
        const char KAC_UPGRADE_TEXTFIELD[]                  = "upgradeTextOne_cpt";    // [19]

        // ---- durations (.rdata floats read at the addresses OnEnter/ResetStateTimer load
        //      them from; DWARF names from BrnOfflineInstantResults.cpp:41..57) -------------
        const f32 KF_SHOW_LICENSE_PAUSE  = 4.5f;   // flt_82F2740C
        // ⚠️ flt_82FB4C10 reads 0.0f. That is NOT a "flagged zero" placeholder: it is the
        // expression's own value and it MEANS "immediately". UpdateEventResults fires
        // LicenseComponent::AddWin on `mfTimeToIncrementWin <= 0.0f && !mbWinsIncremented`,
        // so a 0.0f seed increments the win on the first tick of the results substate. Read,
        // not assumed -- but flagged here because a 0.0f duration is exactly the shape of a
        // placeholder and the next reader should not have to re-derive that it is real.
        const f32 KF_WIN_INCREMENT_PAUSE = 0.0f;   // flt_82FB4C10

        // ---- GUI event ids used by name below --------------------------------------------
        const s32 KI_EVENT_APT_TRIGGER              = 21;
        const s32 KI_EVENT_CONTROLLER_INPUT_PRESSED = 6;
        const s32 KI_EVENT_LOAD_NOTIFICATION        = 14;
        const s32 KI_EVENT_UNLOAD_NOTIFICATION      = 16;
        const s32 KI_EVENT_GUI_CACHE                = 64;
        const s32 KI_EVENT_SETUP_COMPONENTS         = 307;
        const s32 KI_EVENT_PROGRESSION_PROFILE      = 350;
        const s32 KI_EVENT_PERCENTAGE_COMPLETE      = 436;
        const s32 KI_EVENT_COMPRESSED_STILL_IMAGE   = 569;
    }

    // ---- static resource list -------------------------------------------------------------
    // GetResourcesToLoad's asm @0x82500808 pins both addresses (`lis/addi unk_82F26AFC` -> *r4,
    // `lwz dword_82F26B0C` -> *r5). The extent is self-confirming: 0x82F26B0C - 0x82F26AFC ==
    // 0x10 == exactly two 8-byte tuples, the count word itself reads 2, and the DWARF spells
    // the member `sResourceTuple[2]`. Each id is named via gGuiResourceIdentifier[id].
    const CgsGui::sResourceTuple InstantResultsState::maResourcesToLoad[] =
    {
        { 217u, CgsGui::E_GUI_RESOURCETYPE_APT },   // Results
        {  55u, CgsGui::E_GUI_RESOURCETYPE_APT },   // B5ManufacturersIcon
    };
    const u32 InstantResultsState::muNumResourcesToLoad = 2;    // @0x82F26B0C

    // @0x825006D8 -- default constructor.
    // ⭐ CORRECTION TO WHAT WAS COMMITTED HERE. The previous body read
    //      mCarUnlockId = -1;  mPendingRivalId = -1;
    // on the strength of the two `stw r11(-1)` at 0x22E8 / 0x2310. Those are NOT those
    // members. mCarUnlockId is at 0x2240 and mPendingRivalId at 0x2258 -- both proved by
    // OnEnter's own `std` (EIGHT-byte) zero stores, which is also why they are CgsID (u64)
    // and not s32. 0x22E8 and 0x2310 land INSIDE mResults, at record-relative +112 and +152,
    // and both are CgsArray "used before Construct" sentinels: +112 is
    // maCarsToUnlockFromSpecialEvent's count word, +152 an un-homed second sub-array's.
    // The whole X360 body is otherwise inlined member default-construction (one vtable store
    // per embedded component, in address order), which the host does implicitly.
    InstantResultsState::InstantResultsState()
    {
        mResults.maCarsToUnlockFromSpecialEvent.MarkUnconstructed();  // +0x22E8
        mResults.miCtorSentinel98 = -1;                               // +0x2310
    }

    // @0x824B3820 -- return the next active sub-state whose enable flag is raised, scanning
    // forward from meActiveSubState+1; if none is enabled before E_ACTIVE_SUBSTATE_EVENT_COUNT
    // the state machine falls through to E_ACTIVE_SUBSTATE_EVENT_DONE.
    InstantResultsState::EResultsActiveSubStates InstantResultsState::GetNextSubstate()
    {
        CGS_ASSERT(meActiveSubState < E_ACTIVE_SUBSTATE_EVENT_COUNT,
                   "meActiveSubState < E_ACTIVE_SUBSTATE_EVENT_COUNT");

        for (s32 liNext = meActiveSubState + 1; liNext < E_ACTIVE_SUBSTATE_EVENT_COUNT; ++liNext)
        {
            if (mabSubStateFlags[liNext])
                return static_cast<EResultsActiveSubStates>(liNext);
        }

        return E_ACTIVE_SUBSTATE_EVENT_DONE;
    }

    // @0x824B38C0 -- prime mfTimeRemaining with the on-screen duration for the current active
    // sub-state. The X360 switches on meActiveSubState and loads a distinct .rdata float per
    // case; the E_ACTIVE_SUBSTATE_EVENT_LEAVING(8) case picks a longer dwell on a losing result.
    void InstantResultsState::ResetStateTimer()
    {
        switch (meActiveSubState)
        {
        case E_ACTIVE_SUBSTATE_EVENT_RESULTS:            // 0
            mfTimeRemaining = 4.5f;
            break;
        case E_ACTIVE_SUBSTATE_EVENT_RESULTS_TWO:        // 1
            mfTimeRemaining = 3.0f;
            break;
        case E_ACTIVE_SUBSTATE_EVENT_TAKE_PHOTO:         // 2
        case E_ACTIVE_SUBSTATE_EVENT_RANK_UP_TEXT:       // 3
            mfTimeRemaining = 2.0f;
            break;
        case E_ACTIVE_SUBSTATE_EVENT_RANK_UP_LICENSE:    // 4
            mfTimeRemaining = 9.0f;
            break;
        case E_ACTIVE_SUBSTATE_EVENT_CAR_UNLOCK:         // 5
            mfTimeRemaining = 7.4000001f;
            break;
        case E_ACTIVE_SUBSTATE_EVENT_LEAVING:            // 8
            if (meWinState == E_RESULTS_DETAILED_LOSS || meWinState == E_RESULTS_PLAIN_LOSS)
                mfTimeRemaining = 1.5f;
            else
                mfTimeRemaining = 0.5f;
            break;
        default:                                         // 6, 7 and out-of-range
            mfTimeRemaining = 0.0f;
            break;
        }
    }

    // @0x824B39B0 -- pick the large event-icon resource for the current offline game mode and
    // push it into mLargeIconResource. Recognised modes map to a fixed BrnGuiResourceId
    // (116..120); any other mode falls back to the default icon (116) after a debug-only
    // complaint.
    void InstantResultsState::SetEventIconResource()
    {
        CGS_ASSERT(mpGuiCache, "mpGuiCache");
        CGS_ASSERT(mpGuiCache->GetGameMode() >= BrnGameState::GameStateModuleIO::E_MODE_NONE,
                   "mpGuiCache->GetGameMode() >= BrnGameState::GameStateModuleIO::E_MODE_NONE");
        CGS_ASSERT(mpGuiCache->GetGameMode() < BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_COUNT,
                   "mpGuiCache->GetGameMode() < BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_COUNT");

        const s32 liGameMode = mpGuiCache->GetGameMode();
        switch (liGameMode)
        {
        case 3:
            mLargeIconResource.muId = 118;
            break;
        case 5:
            mLargeIconResource.muId = 120;
            break;
        case 7:
            mLargeIconResource.muId = 119;
            break;
        case 8:
            mLargeIconResource.muId = 117;
            break;
        default:
            if ((CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "Invalid game mode in InstantResultsState::SetEventIconResource- "
                    << liGameMode
                    << ", should really be an error but for debug purposes we'll play nice\n";
            }
            mLargeIconResource.muId = 116;
            break;
        }
    }

    // -----------------------------------------------------------------------------------
    // OnEnter  @0x824C3398  (cpp:265, 357 instructions)
    //
    // Register for the fifteen observed events, reset the state/substate machine, then
    // CONSTRUCT ALL TWENTY-FOUR EMBEDDED COMPONENTS. The X360 emits the construction as a
    // flat run of calls in exactly the order below.
    //
    // ⚠️⚠️ THE ORDER OF THE COMPONENT CONSTRUCTION IS LOAD-BEARING, NOT COSMETIC. Several
    // components are parented by NAME to a component constructed EARLIER, and the X360 passes
    // that parent as `mResultsIcon.GetName()` / `mNewRivalsIcon.GetName()` -- i.e. it reads
    // the earlier component's own macName buffer, which GuiComponent::SetName only fills
    // during ITS Construct. So the five icons must be constructed BEFORE the text fields that
    // hang off them, or the parent string is an empty buffer and the child's composed
    // "<parent>_<name>" is wrong. (In the asm this is unmistakable: the parent argument for
    // "Finished"/"TargetResult" is `a1 + 7964`, which is mResultsIcon + 4 == its macName, and
    // for the rival trio `a1 + 8556` == mNewRivalsIcon.macName -- while "CarShutdown" and the
    // car-unlock trio get plain string literals instead.)
    // -----------------------------------------------------------------------------------
    void InstantResultsState::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, KI_NUM_EVENTS_OBSERVED);

        mpGuiCache       = 0;
        meCurrentState   = E_RESULTS_STATE_UNLOADED;
        meActiveSubState = E_ACTIVE_SUBSTATE_EVENT_NONE;

        if ((CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "INSTANT RESULTS DEBUG: " << "OnEnter"
                                       << " (meCurrentState = " << meCurrentState << ")\n";
        }

        // meCurrentMainMovie is resource id 217 -- the same "Results" apt the state's own
        // maResourcesToLoad[0] requests. Update case 1 turns it into the PlayAptMovie call
        // that actually puts this screen on the display.
        meCurrentMainMovie = 217;
        meWinState         = E_RESULTS_COUNT;      // the out-of-range "not decided yet" seed

        for (s32 liSubState = 0; liSubState < E_ACTIVE_SUBSTATE_EVENT_COUNT; ++liSubState)
            mabSubStateFlags[liSubState] = false;

        meSubStateState = E_SUBSTATE_INVALID;

        // LEAVING and DONE are always reachable; SelectSubstates adds the rest once the
        // results record has arrived.
        mabSubStateFlags[E_ACTIVE_SUBSTATE_EVENT_LEAVING] = true;
        mabSubStateFlags[E_ACTIVE_SUBSTATE_EVENT_DONE]    = true;

        mfTimeToShowLicense  = KF_SHOW_LICENSE_PAUSE;
        mbLicenseShown       = false;
        mfTimeToIncrementWin = KF_WIN_INCREMENT_PAUSE;
        mbWinsIncremented    = false;

        mePhotoPresentationStage  = E_PHOTO_PRESENTATION_COUNT;    // 6, the "inactive" seed
        meRankUpPresentationStage = E_RANKUP_PRESENTATION_COUNT;   // 9, ditto
        mbStartedUpgradeTransOut  = false;

        meCarUnlockPresentationStage         = E_CAR_UNLOCK_PRESENTATION_WAITING;
        mCarUnlockId                         = 0;
        mfCarUnlockPresentationTimeRemaining = 7.4000001f;
        miCurrentCarUnlockIndex              = 0;

        meNewRivalsPresentationStage     = E_NEW_RIVALS_PRESENTATION_WAITING;
        mPendingRivalId                  = 0;
        mfRivalPresentationTimeRemaining = 3.0f;

        meFreeCarPresentationStages        = E_FREE_CAR_UNLOCK_PRESENTATION_WAITING;
        mPendingFreeCarId                  = 0;
        mfFreeCarPresentationTimeRemaining = 7.4000001f;

        // ---- three state-interface events, posted before any component exists -----------
        // The X360 stack-builds two wrapper records and posts them on three channels. Both
        // records decode unambiguously against the wrapper shapes CgsGuiStateInterface.h
        // already documents:
        //   {12, 213, 12} + 3 words on channels 41 AND 42 -> GuiEventShowHideSatNav
        //        (sizeof 12, type 213, payload offset 12 -> total 24)  == OutputViewState
        //        then OutputInternalState of the same object, the attested pair shape.
        //        Payload words are (1, 0.0f, 0) == (E_MAPTYPE_GPS, no fade, DO NOT SHOW).
        //   {1, 214, 12} + 1 byte on channel 41   -> GuiEventShowHideBoostBar (total 16),
        //        payload byte 0 == hide.
        // i.e. the results screen takes the GPS map and the boost bar off the display first.
        GuiEventShowHideSatNav lHideSatNav;
        lHideSatNav.Construct(GuiEventShowHideSatNav::E_MAPTYPE_GPS, false, 0.0f);
        mpStateInterface->OutputViewState(lHideSatNav);
        mpStateInterface->OutputInternalState(lHideSatNav);

        GuiEventShowHideBoostBar lHideBoostBar;
        lHideBoostBar.maData[0] = 0;
        mpStateInterface->OutputViewState(lHideBoostBar);

        for (s32 liHelpItem = 0; liHelpItem < KI_HELPITEMS; ++liHelpItem)
        {
            mHelpItems[liHelpItem].Construct(KAC_HELPITEM_NAMES[liHelpItem], mpStateInterface, 0);
        }

        // ---- the five icons FIRST: two of them are named parents below --------------------
        mResultsIcon.Construct(KAC_RESULTS_ICON_NAME, mpStateInterface, KAC_RESULTS_FRAMES, 0);
        mSecondResultsIcon.Construct(KAC_SECOND_RESULTS_ICON_NAME, mpStateInterface,
                                     KAC_SECOND_RESULTS_FRAMES, 0);
        mCarUnlockIcon.Construct(KAC_CAR_UNLOCK_ICON_NAME, mpStateInterface,
                                 KAC_CAR_TEXT_FRAMES, 0);
        mRankUpIcon.Construct(KAC_RANK_UP_ICON_NAME, mpStateInterface, KAC_RANK_TEXT_FRAMES, 0);
        mNewRivalsIcon.Construct(KAC_NEW_RIVAL_ICON_NAME, mpStateInterface,
                                 KAC_NEW_RIVALS_TEXT_FRAMES, 0);

        // ---- then everything parented off them --------------------------------------------
        mFinishedText.Construct(KAC_FINISHED_TEXT_NAME, mpStateInterface, mResultsIcon.GetName());
        mTargetResultText.Construct(KAC_TARGET_RESULT_TEXT_NAME, mpStateInterface,
                                    mResultsIcon.GetName());
        mShutdownText.Construct(KAC_SHUTDOWN_CAR_TEXT_NAME, mpStateInterface,
                                KAC_SECOND_RESULTS_ICON_NAME);
        mCarUnlockManuIcon.Construct(KAC_CAR_UNLOCK_MANUFACTURER_ICON_NAME, mpStateInterface,
                                     KAC_CAR_UNLOCK_ICON_NAME);
        mCarUnlockText.Construct(KAC_CAR_UNLOCK_TEXT_NAME, mpStateInterface,
                                 KAC_CAR_UNLOCK_ICON_NAME);
        mCarUnlockDescText.Construct(KAC_CAR_UNLOCK_DESC_NAME, mpStateInterface,
                                     KAC_CAR_UNLOCK_ICON_NAME);
        mNewRivalManuIcon.Construct(KAC_NEW_RIVAL_MANUFACTURER_ICON_NAME, mpStateInterface,
                                    mNewRivalsIcon.GetName());
        mNewRivalCarText.Construct(KAC_NEW_RIVAL_CAR_TEXT_NAME, mpStateInterface,
                                   mNewRivalsIcon.GetName());
        mNewRivalDescText.Construct(KAC_NEW_RIVAL_DESC_TEXT_NAME, mpStateInterface,
                                    mNewRivalsIcon.GetName());

        mLicense.Construct(KAC_LICENSE_COMPONENT_NAME, mpStateInterface, 0);
        mPhotoBoothComponent.Construct(KAC_PHOTO_COMPONENT_NAME, mpStateInterface,
                                       static_cast<ButtonIconComponent::EPadButton>(5),
                                       static_cast<ButtonIconComponent::EPadButton>(4),
                                       static_cast<PhotoBoothComponent::ETakePhotoStringType>(1),
                                       static_cast<PhotoBoothComponent::EBackStringType>(2),
                                       0);
        mLargeEventIcon.Construct(KAC_LARGE_EVENT_ICON_NAME, mpStateInterface, 0, 0);

        // The large event icon's own resource slot. The id half is filled in by
        // SetEventIconResource once the game mode is known; the TYPE half is seeded here.
        // Update asserts "0 != mLargeIconResource.muId" before using it.
        mLargeIconResource.muId   = 0;
        mLargeIconResource.meType = CgsGui::E_GUI_RESOURCETYPE_APT;

        mUnlockedXSCarComponent.Construct(KAC_NEW_XS_CAR_LARGE_ICON_NAME, mpStateInterface, 0);
        mUnlockedRivalCarComponent.Construct(KAC_NEW_RIVAL_CAR_LARGE_ICON_NAME, mpStateInterface, 0);
        mUnlockedFreeCarComponent.Construct(KAC_NEW_RIVAL_CAR_LARGE_ICON_NAME, mpStateInterface, 0);

        mUpgradeText.Construct(KAC_UPGRADE_TEXTFIELD, mpStateInterface, 0);
        mUpgradeStateAnimator.Construct(KAC_UPGRADE_STATE_ANIMATOR, mpStateInterface, 0);

        mpcAnimatingComponentName = 0;

        ResetStateTimer();
    }

    // -----------------------------------------------------------------------------------
    // Update  @0x824DF760  (cpp:504, 244 instructions)
    //
    // The four-step load ladder that ends with the results movie on screen:
    //   UNLOADED -> prime the licence panel and pick the event icon -> LOADING_RESOURCES
    //   LOADING_RESOURCES -> once every resource is in, PLAY THE APT MOVIES -> LOADING_COMPONENTS
    //   LOADING_COMPONENTS -> once every expected apt component reports in -> ACTIVE
    //   ACTIVE -> run the substate machine
    // ⭐ THE PIXELS HAPPEN IN CASE LOADING_RESOURCES: the two PlayAptMovie calls are what put
    // the "Results" movie and the mode icon on the display. Everything after that is content.
    // -----------------------------------------------------------------------------------
    void InstantResultsState::Update()
    {
        switch (meCurrentState)
        {
        case E_RESULTS_STATE_INVALID:
            CGS_ASSERT(false, "Invalid state");            // cpp:622
            break;

        case E_RESULTS_STATE_UNLOADED:
            if (mpGuiCache != 0)
            {
                // A rank-up run shows "1 point to the next rank"; otherwise the licence panel
                // gets the live remaining-points count + 1 (X360 `*(cache + 47220) + 1`, the
                // 0xB874 halfword GetLicencePointsToNextRank already homes).
                const s32 liPointsToNextRank =
                    mResults.mbHasRankedUp ? 1
                                           : (mpGuiCache->GetLicencePointsToNextRank() + 1);

                BrnProgression::Profile* lpProgressionProfile = mpGuiCache->GetProfile();
                CGS_ASSERT(lpProgressionProfile, "lpProgressionProfile");   // cpp:527

                const bool lbSeen100Percent =
                    lpProgressionProfile->GetSeen100PercentCompletionSequence();
                const bool lbSeenElite =
                    lpProgressionProfile->GetSeenEliteCompletionSequence();

                mLicense.SetPlayerInfo(mpGuiCache->GetPlayerName(), lbSeenElite, lbSeen100Percent,
                                       mResults.miPlayerOldRank, liPointsToNextRank, true, true);

                // The photo booth's own resource id: 93 when the player ranked up this event,
                // 91 otherwise (X360 `*(a1 + 5540)`, i.e. mPhotoBoothComponent + 0x94).
                mPhotoBoothComponent.SetPhotoResourceId(mResults.mbHasRankedUp ? 93u : 91u);

                SetEventIconResource();
                CGS_ASSERT(0 != mLargeIconResource.muId, "0 != mLargeIconResource.muId"); // cpp:555

                meCurrentState = E_RESULTS_STATE_LOADING_RESOURCES;
                if ((CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "INSTANT RESULTS DEBUG: " << "Update"
                                               << " (meCurrentState = " << meCurrentState << ")\n";
                }
            }
            break;

        case E_RESULTS_STATE_LOADING_RESOURCES:
            if (mpGuiCache != 0
                && mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad)
                && mLicense.EnsureResourcesAreLoaded()
                && mPhotoBoothComponent.EnsureResourcesAreLoaded()
                && mpGuiCache->EnsureResourceIsLoaded(mLargeIconResource))
            {
                // ⭐⭐ THIS IS THE LINE THE WHOLE CAMPAIGN WAS ABOUT.
                mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[meCurrentMainMovie], 3);
                mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[mLargeIconResource.muId], 2);

                mLicense.OnLoad();
                mPhotoBoothComponent.OnLoad();

                mpGuiCache->ClearExpectedAptComponentList(static_cast<GuiFlow>(0));
                AppendAllExpectedComponents();

                meCurrentState = E_RESULTS_STATE_LOADING_COMPONENTS;
                if ((CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "INSTANT RESULTS DEBUG: " << "Update"
                                               << " (meCurrentState = " << meCurrentState << ")\n";
                }
            }
            break;

        case E_RESULTS_STATE_LOADING_COMPONENTS:
            if (mpGuiCache->AreAllAptComponentsInitialised(static_cast<GuiFlow>(0)))
            {
                meCurrentState = E_RESULTS_STATE_ACTIVE;
                if ((CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "INSTANT RESULTS DEBUG: " << "Update"
                                               << " (meCurrentState = " << meCurrentState << ")\n";
                }
            }
            break;

        case E_RESULTS_STATE_ACTIVE:
            UpdateSubstate();
            break;

        case E_RESULTS_STATE_PHOTO_INTERRUPT:
            UpdatePhoto();
            break;

        default:
            break;
        }

        HandleIncomingEvents();

        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        lpInQueue->Clear();
    }

    // -----------------------------------------------------------------------------------
    // OnLeave  @0x824C3930  (cpp:399, 208 instructions)
    // Post the three tear-down state-interface events, release every streamed component
    // resource, drop the large event icon, and unregister.
    // ⛔ PARTIAL, AND SAID SO OUT LOUD: the X360 body also (a) emits a
    // GuiEventTickerCustomMessage("NO_LICENCE_WIN_ACQUIRED") on a losing result whose gate
    // reads mResults +0xB5, and (b) computes a "suggested game mode" from the cache
    // (a `% 10` walk skipping modes 2/4/6/9, asserted against E_MODE_OFFLINE_COUNT at
    // cpp:468/469) whose result the visible asm never stores -- it feeds an arm this wave
    // could not attribute. Neither is on the results-screen path; both are named here so the
    // next wave finds them instead of assuming OnLeave is complete.
    // -----------------------------------------------------------------------------------
    void InstantResultsState::OnLeave()
    {
        // Two "play the EMPTY apt movie" view-state events, at apt levels 3 and 2 -- i.e.
        // exactly the two levels Update played the results movie and the mode icon into, so
        // this clears them. The record is {8, 18, 12} + {level, name} on channel 41, which is
        // GuiEventPlayAptMovie's own attested shape (GuiEvent<18>(8, 12), and
        // StateInterface::PlayAptMovie @0x82436F10 posts precisely that). The name pointer is
        // &unk_820046A7, and the image byte at 0x820046A7 is 0 -- the EMPTY STRING (it is the
        // NUL of the "%s%s%s" literal at 0x820046A0, reused as ""). Read, not assumed.
        mpStateInterface->PlayAptMovie("", 3);
        mpStateInterface->PlayAptMovie("", 2);

        mLicense.ReleaseResources();
        mPhotoBoothComponent.ReleaseResources();
        mUnlockedXSCarComponent.ReleaseResources();
        mUnlockedRivalCarComponent.ReleaseResources();
        mUnlockedFreeCarComponent.ReleaseResources();

        if (mpGuiCache != 0 && mLargeIconResource.muId != 0)
        {
            mpGuiCache->EnsureResourceIsUnloaded(mLargeIconResource);
            mLargeIconResource.muId = 0;
        }

        mpStateInterface->UnRegisterForEvents(maiEventToObserve, KI_NUM_EVENTS_OBSERVED);
        meCurrentState = E_RESULTS_STATE_INVALID;

        if ((CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "INSTANT RESULTS DEBUG: " << "OnLeave"
                                       << " (meCurrentState = " << meCurrentState << ")\n";
        }

        mpGuiCache = 0;
    }

    // -----------------------------------------------------------------------------------
    // AppendExpectedScreenComponents  @0x824B3CB0  (cpp:772, 83 instructions)
    // Register every component whose apt counterpart must report in before the screen counts
    // as initialised. The X360 passes each component's OWN name buffer (this + <component> +
    // 4 == its macName), i.e. the composed "<parent>_<name>", not the raw literal -- so the
    // parented children register under their composed names.
    // ⚠️ mUpgradeText / mUpgradeStateAnimator / the three LargeCarComponents / the licence and
    // photo booth are deliberately NOT in this list; the X360 list is exactly the seventeen
    // below (three help items, five icons, nine text/manufacturer components).
    // -----------------------------------------------------------------------------------
    void InstantResultsState::AppendExpectedScreenComponents()
    {
        CGS_ASSERT(mpGuiCache, "mpGuiCache");     // cpp:774

        const GuiFlow leFlow = static_cast<GuiFlow>(0);

        for (s32 liHelpItem = 0; liHelpItem < KI_HELPITEMS; ++liHelpItem)
        {
            mpGuiCache->AppendExpectedAptComponent(leFlow, mHelpItems[liHelpItem].GetName());
        }

        mpGuiCache->AppendExpectedAptComponent(leFlow, mResultsIcon.GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mSecondResultsIcon.GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mCarUnlockIcon.GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mRankUpIcon.GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mNewRivalsIcon.GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mFinishedText.GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mTargetResultText.GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mShutdownText.GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mCarUnlockManuIcon.GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mCarUnlockText.GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mCarUnlockDescText.GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mNewRivalManuIcon.GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mNewRivalCarText.GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mNewRivalDescText.GetName());
    }

    // AppendAllExpectedComponents  @0x824BB458  (cpp:753, 31 instructions)
    void InstantResultsState::AppendAllExpectedComponents()
    {
        CGS_ASSERT(mpGuiCache, "mpGuiCache");     // cpp:755

        AppendExpectedScreenComponents();

        const GuiFlow leFlow = static_cast<GuiFlow>(0);
        mpGuiCache->AppendExpectedAptComponent(leFlow, mPhotoBoothComponent.GetName());
        mpGuiCache->AppendExpectedAptComponent(leFlow, mLargeEventIcon.GetName());
    }

    // -----------------------------------------------------------------------------------
    // SelectSubstates  @0x824D59B0  (49 instructions)
    // Decide which of the ten sub-states this results run will actually visit. OnEnter has
    // already raised LEAVING and DONE; this adds the rest from the results record.
    // -----------------------------------------------------------------------------------
    void InstantResultsState::SelectSubstates()
    {
        // The cache's "skip the presentation" flag (GuiCache +0x4B77, `lbz`). When it is
        // clear the ordinary results pages run.
        if (!mpGuiCache->IsPostEventPresentationSuppressed())
        {
            mabSubStateFlags[E_ACTIVE_SUBSTATE_EVENT_RESULTS] = true;
            if (mResults.meFinishedGameModeType == 4)
                mabSubStateFlags[E_ACTIVE_SUBSTATE_EVENT_RESULTS_TWO] = true;

            mabSubStateFlags[E_ACTIVE_SUBSTATE_EVENT_TAKE_PHOTO] = true;
            if (mResults.mbHasRankedUp)
                mabSubStateFlags[E_ACTIVE_SUBSTATE_EVENT_RANK_UP_LICENSE] = true;
        }

        if (!WillShowCredits())
        {
            if (IsXSCarInUnlockedArray())
                mabSubStateFlags[E_ACTIVE_SUBSTATE_EVENT_CAR_UNLOCK] = true;
            if (mResults.mbHasUnlockedFreeCar)
                mabSubStateFlags[E_ACTIVE_SUBSTATE_EVENT_FREE_CAR_UNLOCK] = true;
            if (mResults.mNewlyUnlockedRivalID != 0)
                mabSubStateFlags[E_ACTIVE_SUBSTATE_EVENT_RANK_UP_SHOWING_RIVALS] = true;
        }
    }

    // HasSubstateTimedOut  @0x824B48C8  (cpp:3250, 52 instructions)
    // Tick the current substate's dwell down by one frame and report whether it expired.
    bool InstantResultsState::HasSubstateTimedOut()
    {
        CGS_ASSERT(meActiveSubState > E_ACTIVE_SUBSTATE_EVENT_NONE
                       && meActiveSubState < E_ACTIVE_SUBSTATE_EVENT_COUNT,
                   "(E_ACTIVE_SUBSTATE_EVENT_NONE < meActiveSubState) && "
                   "(E_ACTIVE_SUBSTATE_EVENT_COUNT > meActiveSubState)");        // cpp:3250
        CGS_ASSERT(meSubStateState == E_SUBSTATE_RUNNING,
                   "E_SUBSTATE_RUNNING == meSubStateState");                     // cpp:3251
        CGS_ASSERT(mpGuiCache, "mpGuiCache");                                    // cpp:3255

        mfTimeRemaining -= mpGuiCache->GetTimeStep();
        return mfTimeRemaining <= 0.0f;
    }

    // TickSubstateAndEndIfDone  @0x824BB4D8  (27 instructions)
    bool InstantResultsState::TickSubstateAndEndIfDone()
    {
        if (!HasSubstateTimedOut())
            return false;

        meActiveSubState = GetNextSubstate();
        ResetStateTimer();
        meSubStateState = E_SUBSTATE_SET_UP_COMPONENTS;
        return true;
    }

    // -----------------------------------------------------------------------------------
    // UpdateSubstate  @0x824DC188  (133 instructions)
    // ⛔ PARTIAL. The X360 dispatches all ten sub-states; the eight presentation updaters are
    // not reconstructed yet (see the ⛔ list in this file's banner), so they route through the
    // logged stubs in BrnScreenStatesDataLinkStubs.cpp rather than being silently skipped.
    // The dispatch structure itself, its two asserts and the two trailing picture pumps are
    // faithful.
    // -----------------------------------------------------------------------------------
    void InstantResultsState::UpdateSubstate()
    {
        CGS_ASSERT(meCurrentState == E_RESULTS_STATE_ACTIVE, "Invalid state");   // cpp:1632

        switch (meActiveSubState)
        {
        case E_ACTIVE_SUBSTATE_EVENT_NONE:
            meActiveSubState = GetNextSubstate();
            ResetStateTimer();
            meSubStateState = E_SUBSTATE_SET_UP_COMPONENTS;
            break;
        case E_ACTIVE_SUBSTATE_EVENT_RESULTS:               UpdateEventResults();       break;
        case E_ACTIVE_SUBSTATE_EVENT_RESULTS_TWO:           UpdateSecondResultsPage();  break;
        case E_ACTIVE_SUBSTATE_EVENT_TAKE_PHOTO:            UpdateTakePhotoPage();      break;
        case E_ACTIVE_SUBSTATE_EVENT_RANK_UP_TEXT:          UpdateRankUp();             break;
        case E_ACTIVE_SUBSTATE_EVENT_RANK_UP_LICENSE:       UpdateLicense();            break;
        case E_ACTIVE_SUBSTATE_EVENT_CAR_UNLOCK:            UpdateCarUnlock();          break;
        case E_ACTIVE_SUBSTATE_EVENT_FREE_CAR_UNLOCK:       UpdateFreeCarUnlock();      break;
        case E_ACTIVE_SUBSTATE_EVENT_RANK_UP_SHOWING_RIVALS: UpdateShowingRivals();     break;
        case E_ACTIVE_SUBSTATE_EVENT_LEAVING:               UpdateLeaving();            break;
        case E_ACTIVE_SUBSTATE_EVENT_DONE:                  TriggerExitResults();       break;
        default:
            CGS_ASSERT(false, "Invalid substate");                               // cpp:1706
            break;
        }

        mLicense.SendPlayerPictureEvent();
        mPhotoBoothComponent.SendPlayerPictureEvent();
    }

    // -----------------------------------------------------------------------------------
    // HandleIncomingEvents  @0x824DBAD8  (cpp:812, 427 instructions)
    //
    // ⭐ THE CASE-64 ARM IS WHY THE SCREEN CAN LOAD AT ALL. OnEnter deliberately leaves
    // mpGuiCache NULL, and Update's E_RESULTS_STATE_UNLOADED arm does nothing until it is
    // non-NULL -- so without this the state never leaves state 0 and never reaches the
    // PlayAptMovie in state 1. It is not optional decoration.
    //
    // ⛔ PARTIAL, itemised. Landed: 64, 14, 16, 307, 350, 436, 569 plus the two dispatch
    // forwards (21, 6). NOT landed: 296, 297, 299, 300, 301 -- the new-rival presentation
    // arms. They assert `E_ACTIVE_SUBSTATE_EVENT_RANK_UP_SHOWING_RIVALS == meActiveSubState`
    // and drive meNewRivalsPresentationStage / mNewRivalsIcon, i.e. they only ever run inside
    // a sub-state this wave does not reconstruct. Listing them here rather than dropping them
    // silently is the point: a missing arm that "does nothing plausible" is precisely the
    // silent-drop failure this file's banner is about.
    // -----------------------------------------------------------------------------------
    void InstantResultsState::HandleIncomingEvents()
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
            case KI_EVENT_GUI_CACHE:
                // Latched ONCE: the X360 guards the whole arm on `if (!mpGuiCache)`.
                if (mpGuiCache == 0)
                {
                    CGS_ASSERT(*reinterpret_cast<GuiCache* const*>(lpEvent),
                               "Invalid cache in InstantResultsState::UpdateLoadingScreen"); // cpp:831

                    mpGuiCache = *reinterpret_cast<GuiCache* const*>(lpEvent);

                    mLicense.SetCachePointer(mpGuiCache);
                    mPhotoBoothComponent.SetCachePointer(mpGuiCache);
                    mUnlockedXSCarComponent.SetCachePointer(mpGuiCache);
                    mUnlockedRivalCarComponent.SetCachePointer(mpGuiCache);
                    mUnlockedFreeCarComponent.SetCachePointer(mpGuiCache);

                    // X360: memcpy(&mResults, (u8*)cache + 40552, 192). Expressed as the
                    // struct copy it is, so no offset arithmetic survives into the C++.
                    mResults = mpGuiCache->GetOfflinePostEventData();

                    SelectSubstates();
                }
                break;

            case KI_EVENT_APT_TRIGGER:
                HandleAptTriggers(lpEvent);
                break;

            case KI_EVENT_CONTROLLER_INPUT_PRESSED:
                // The X360 gates controller input on the screen being at least ACTIVE.
                if (meCurrentState >= E_RESULTS_STATE_ACTIVE)
                    HandleControllerInput(lpEvent);
                break;

            case KI_EVENT_LOAD_NOTIFICATION:
                mUnlockedXSCarComponent.HandleLoadNotification(
                    reinterpret_cast<const CgsGui::GuiEventLoadNotification*>(lpEvent));
                mUnlockedRivalCarComponent.HandleLoadNotification(
                    reinterpret_cast<const CgsGui::GuiEventLoadNotification*>(lpEvent));
                mUnlockedFreeCarComponent.HandleLoadNotification(
                    reinterpret_cast<const CgsGui::GuiEventLoadNotification*>(lpEvent));
                break;

            case KI_EVENT_UNLOAD_NOTIFICATION:
                mUnlockedXSCarComponent.HandleUnloadNotification(
                    reinterpret_cast<const CgsGui::GuiEventUnloadNotification*>(lpEvent));
                mUnlockedRivalCarComponent.HandleUnloadNotification(
                    reinterpret_cast<const CgsGui::GuiEventUnloadNotification*>(lpEvent));
                mUnlockedFreeCarComponent.HandleUnloadNotification(
                    reinterpret_cast<const CgsGui::GuiEventUnloadNotification*>(lpEvent));
                break;

            case KI_EVENT_SETUP_COMPONENTS:
                // ⭐ GUI event 307 -- the id the finish also posts via game action 200. It
                // cannot OPEN this screen (that is 291 -> TO_OFF_POST), but it IS what this
                // state uses to (re)fill its components.
                SetupComponents();
                break;

            case KI_EVENT_PROGRESSION_PROFILE:
            {
                BrnProgression::Profile* lpProfile =
                    *reinterpret_cast<BrnProgression::Profile* const*>(lpEvent);
                CGS_ASSERT(lpProfile, "lpProfileEvent->mpProgressionProfile");   // cpp:957
                mLicense.SetProfilePointer(lpProfile);
                CGS_ASSERT(lpProfile, "NULL != lpProfile");   // BrnPhotoBoothComponent.h:280
                mPhotoBoothComponent.SetProfilePointer(lpProfile);
                break;
            }

            case KI_EVENT_PERCENTAGE_COMPLETE:
                // The X360 reads the payload's word 49 (+0xC4) as the percentage.
                mLicense.SetPercentageComplete(
                    static_cast<s32>(reinterpret_cast<const u32*>(lpEvent)[49]));
                break;

            case KI_EVENT_COMPRESSED_STILL_IMAGE:
                mPhotoBoothComponent.HandleCompressedStillImageEvent(lpEvent);
                break;

            default:
                // 296 / 297 / 299 / 300 / 301: observed and registered for, but their
                // handlers belong to the un-reconstructed new-rivals presentation. See the
                // banner above -- this is a KNOWN gap, not an accident.
                break;
            }
        }
    }

    // -----------------------------------------------------------------------------------
    // TriggerExitResults  @0x824D58A8  (cpp:3294, 66 instructions)
    // Leave the results screen: either into the credits, or back to the front-end FSM.
    //
    // ⛔ PARTIAL, and the missing piece is named. The X360 "ADVANCE" arm also (a) posts a
    // GuiEventPlayMusicOnMenuStream when the cache's presentation-suppressed byte is set,
    // (b) posts a {12, 0x124, 12}-shaped record on channel 40, and (c) posts a
    // BrnGui::GuiEventRunFsm carrying CgsIDCompress("BRNFBFSM") + two trailing 1s -- i.e. it
    // hands the front-end FSM back its own name. GuiEventRunFsm has no home in the tree yet,
    // so rather than invent its payload shape this wave sends the state event and stops.
    // CONSEQUENCE, stated plainly: the results screen will show and time out, but the handover
    // back to the front-end FSM is not complete -- expect the flow to sit after ADVANCE.
    // -----------------------------------------------------------------------------------
    void InstantResultsState::TriggerExitResults()
    {
        CGS_ASSERT(mpGuiCache, "mpGuiCache");     // cpp:3294

        if (WillShowCredits())
        {
            SendStateEvent("TO_CREDITS");
            return;
        }

        SendStateEvent("ADVANCE");
    }
}
