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
//   SetEventIconResource           @0x824B39B0         SetupComponents @0x824B3FF0 (492)
//   UpdateEventResults             @0x824BE228 (184)   UpdateSecondResultsPage
//                                                                     @0x824BE508 (103)
//   UpdateTakePhotoPage            @0x824C3C70         UpdateRankUp    @0x824BE6A8 ( 77)
//   UpdateLeaving                  @0x824BE7E0 ( 68)   HandleAptTriggers @0x824BDAB8 (475)
//
// ⛔ STILL NOT RECONSTRUCTED (declared in the header, bodied as LOGGED stubs in
// BrnScreenStatesDataLinkStubs.cpp so the gap is visible in the log instead of silent):
//   HandleControllerInput (124) UpdateLicense UpdateCarUnlock UpdateFreeCarUnlock
//   UpdateShowingRivals UpdatePhoto IsXSCarInUnlockedArray RenderDebug
// Those are the remaining substate PRESENTATIONS. Several of them need OfflinePostEventData
// flag slots the X360 asm does not yet pin -- see the ⛔ block in BrnGuiEventTypeDefs.h.
// Guessing those names is how a results page ends up printing the wrong string, so they are
// left for a wave that can attest them.
// ⚠️ THIS BLOCK WAS STALE FOR A WHOLE WAVE: it still listed SetupComponents,
// UpdateEventResults, UpdateSecondResultsPage, UpdateTakePhotoPage, UpdateRankUp and
// UpdateLeaving as un-reconstructed while all six had real bodies a few hundred lines below.
// A brief written off this list would have re-scoped work that was already done. If you land
// a body, move its name -- the list is load-bearing.
// ===================================================================================
#include "GameSource/Gui/Flow/PostEvent/States/Offline/BrnOfflineInstantResults.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDCompress
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // PlayAptMovie / Register
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h"  // GuiEventAptTrigger(Payload)
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"              // Playback::Name::MakeHash
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // GetProgressionData
#include "SharedClasses/Progression/BrnProgressionData.h"                 // GetProgressionRankCount
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"   // GuiEventShowHideSatNav / ShowHideBoostBar
#include "GameSource/GameState/Progression/BrnProfile.h"  // BrnProgression::Profile getters
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf / SnPrintf
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"  // ParameterFormatType
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"  // ButtonIconComponent::EPadButton
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // BrnGameState::GameStateModuleIO::EGameModeType (assert bounds)

#include <cstring>   // strcmp / strstr (HandleAptTriggers' component-name matching)

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

        // The SEVENTH table, recovered with the six above and self-evident in a way none of
        // them is: it reads FIRST..EIGHTH in order, and the DWARF declares both
        // `const char*[8] KAC_FINISH_POS_STRINGIDS` and `KI_NUM_FINISH_POS_STRINGS = 8`
        // beside it. SetupComponents indexes it with (miPlayerFinishPosition - 1) and asserts
        // that index against the count, which is a third check on the extent.
        const s32 KI_NUM_FINISH_POS_STRINGS = 8;                     // DWARF cpp:189
        const char* const KAC_FINISH_POS_STRINGIDS[KI_NUM_FINISH_POS_STRINGS] =  // @0x82F26B78
        {
            "HUDMESSAGE_GAME_FINISH_FIRST",   "HUDMESSAGE_GAME_FINISH_SECOND",
            "HUDMESSAGE_GAME_FINISH_THIRD",   "HUDMESSAGE_GAME_FINISH_FOURTH",
            "HUDMESSAGE_GAME_FINISH_FIFTH",   "HUDMESSAGE_GAME_FINISH_SIXTH",
            "HUDMESSAGE_GAME_FINISH_SEVENTH", "HUDMESSAGE_GAME_FINISH_EIGHTH",
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

        // The apt clip HandleAptTriggers drives the licence-upgrade animation on (the X360's
        // aAptTransition_1, shared with HandleControllerInput's "takePhotoOut" post).
        const char KAC_APT_TRANSITION[]                     = "apt_Transition";
        // The credits music stream WillShowCredits starts. ⭐ The X360 does NOT hold this as
        // an inline literal: it loads a `const char*` GLOBAL (`lwz r3, GunsAndRoses@l(r11)`)
        // whose value is the string at 0x820A9ADC, read out of the image here. It sits in the
        // shared sound-name pool, not this file's string block, which is why it is a pointer.
        const char KAC_CREDITS_MUSIC_STREAM[]               = "Guns_And_Roses";

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

        // ---- what TriggerExitResults posts on its way out --------------------------------
        // 292 (0x124) is the post-event TEARDOWN. Not a new id: GuiCache::RecEvent case 292
        // clears mOfflinePostEventData and drops mbSuppressPostEventPresentation, and
        // BridgeGuiToGameState's case 292 turns it into game event 26.
        const s32 KI_EVENT_POST_EVENT_TEARDOWN      = 292;
        // The state-output channel every OutputGuiEvent<T> body passes (`li r5, 0x28`).
        const s32 KI_CHANNEL_GUI_OUT                = 40;
        // The FSM this screen hands the HUD flow back to. Spelled exactly as the image's
        // literal at the CgsIDCompress call site; BrnGameModule::BridgeGameToGui's stage-5
        // twin spells it "BrnFBFsm", and CgsIDCompress is case-folding, so the two agree.
        const char KAC_FREEBURN_FSM_ID[]            = "BRNFBFSM";

        // ---- the two apt movies this screen swaps between --------------------------------
        // 217 is the "Results" apt (it is also maResourcesToLoad[0], and OnEnter seeds
        // meCurrentMainMovie with it). 218 is the movie the rank-up/photo page swaps in
        // (UpdateTakePhotoPage @0x824C3E28 `li r11, 0xDA` -> meCurrentMainMovie); the same two
        // literals appear in the asm as 0xD9 / 0xDA. Named here so the swap reads as a swap.
        const BrnGuiResourceId KU_RESULTS_MOVIE_RESOURCE = 217;   // "Results"
        const BrnGuiResourceId KU_PHOTO_MOVIE_RESOURCE   = 218;
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
        meCurrentMainMovie = KU_RESULTS_MOVIE_RESOURCE;
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
    // logged stubs in BrnScreenStatesDataLinkStubs.cpp, so a run that reaches one says so in
    // BrnGame.log instead of doing nothing and looking correct.
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
    // SetupComponents  @0x824B3FF0  (cpp:1300+, 492 instructions)
    //
    // ⭐⭐ THIS IS THE FUNCTION THAT REVEALS THE SCREEN. Everything before it only LOADS.
    // Measured 2026-08-29: with OnEnter/Update landed but this still a stub, the results
    // movie mounts and COMPOSES -- apt levels 2, 3, 4 and 5 all report live=1 composed=1 --
    // and the display shows nothing, because nothing has pushed a frame onto the components.
    // The tail below is what does:
    //     mResultsIcon.SetState(meWinState)   -> KAC_RESULTS_FRAMES[meWinState] as "apt_state"
    //     mLargeEventIcon.SetState("transIn") -> the mode icon transitions in (win only)
    //
    // ⛔ PARTIAL, ARM BY ARM, AND DELIBERATELY SO. The X360 switches on
    // meFinishedGameModeType across nine arms. Several of them (0, 4, 5, 6, 8) gate on bytes
    // in OfflinePostEventData's +0xB1..+0xB5 flag run, and that run is NOT attributable: the
    // PS3 DWARF's declaration order is already proven wrong for this struct, and following it
    // would put mbEliminated at +0xB3 where the X360's own string is "POSTRACE_OUTOFTIME".
    // Guessing there prints the WRONG RESULT LINE at the player, silently. So those arms are
    // left out, loudly, and meWinState keeps the E_RESULTS_PLAIN_LOSS the X360 itself seeds
    // BEFORE the switch (`li r11, 5` / store to +0x2208 ahead of the dispatch) -- a real
    // frame, not an out-of-range index, so the panel still appears.
    // LANDED arms: 1 (won/lost by finish position) and 7/9 (the SCORE modes -- the stunt run
    // and its sibling), whose every input is attested: miModeScore by its own string id,
    // GetTargetScoreInEvent by its X360 symbol, miPlayerFinishPosition by the debug print.
    // -----------------------------------------------------------------------------------
    void InstantResultsState::SetupComponents()
    {
        mHelpItems[0].SetItem("$GENERAL_OPTION_RETRY",
                              ButtonIconComponent::E_PADBUTTON_OPTION0,
                              ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        mHelpItems[1].SetItem("$GENERAL_OPTION_CONTINUE",
                              ButtonIconComponent::E_PADBUTTON_SELECT,
                              ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        // The third help item's caption is &unk_820046A7 -- the image byte there is 0, i.e.
        // the EMPTY STRING (it is the NUL of the "%s%s%s" literal at 0x820046A0). Read.
        mHelpItems[2].SetItem("",
                              ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                              ButtonIconComponent::E_PADBUTTON_INVISIBLE);

        CGS_ASSERT(mResults.meFinishedGameModeType >= 0, "Invalid finish position");  // cpp:1312

        const s32 liGameMode            = mResults.meFinishedGameModeType;
        const s32 liFinishPositionIndex = mResults.miPlayerFinishPosition - 1;

        // The X360 seeds PLAIN_LOSS before the switch; every arm below only overrides it.
        meWinState = E_RESULTS_PLAIN_LOSS;

        switch (liGameMode)
        {
        case 1:
            CGS_ASSERT(liFinishPositionIndex >= 0, "liFinishPositionIndex >= 0");          // cpp:1359
            CGS_ASSERT(liFinishPositionIndex < KI_NUM_FINISH_POS_STRINGS,
                       "liFinishPositionIndex < KI_NUM_FINISH_POS_STRINGS");               // cpp:1360
            if (mResults.miPlayerFinishPosition != 0)
            {
                mFinishedText.SetLocalisedText("POSTRACE_EVENT_LOST",
                                               CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
                meWinState = E_RESULTS_PLAIN_LOSS;
            }
            else
            {
                mFinishedText.SetLocalisedText("POSTRACE_EVENT_WON",
                                               CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
                meWinState = E_RESULTS_PLAIN_WIN;
            }
            break;

        case 7:
        case 9:
        {
            CGS_ASSERT(liFinishPositionIndex >= 0, "liFinishPositionIndex >= 0");          // cpp:1398
            CGS_ASSERT(liFinishPositionIndex < KI_NUM_FINISH_POS_STRINGS,
                       "liFinishPositionIndex < KI_NUM_FINISH_POS_STRINGS");               // cpp:1399

            const CgsLanguage::LanguageManager::ParameterFormatType laeFormats[1] =
                { CgsLanguage::LanguageManager::E_FORMAT_INTEGER };

            char lacPlayerScore[1024];
            CgsCore::SPrintf(lacPlayerScore, sizeof(lacPlayerScore), "%d", mResults.miModeScore);
            const char* lapacPlayerParams[1] = { lacPlayerScore };
            mFinishedText.SetLocalisedText("POSTRACE_FINISH_YOUR_SCORE_POINTS",
                                           CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                                           1, lapacPlayerParams, laeFormats);

            char lacTargetScore[1024];
            CgsCore::SPrintf(lacTargetScore, sizeof(lacTargetScore), "%d",
                             mpGuiCache->GetTargetScoreInEvent());
            const char* lapacTargetParams[1] = { lacTargetScore };
            mTargetResultText.SetLocalisedText("POSTRACE_FINISH_TARGET_SCORE_POINTS",
                                               CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                                               1, lapacTargetParams, laeFormats);

            meWinState = (liFinishPositionIndex != 0) ? E_RESULTS_LOSS_WITH_TARGETS
                                                      : E_RESULTS_WIN_WITH_TARGETS;
            break;
        }

        default:
            // ⛔ Arms 0 / 3 / 4 / 5 / 6 / 8 are NOT reconstructed -- see the banner. The X360's
            // own default arm also lands here and prints exactly this line.
            if ((CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
            {
                *CgsDev::Log::gpDebugPrint << "Unhandled game mode! (" << liGameMode << ")\n\n";
            }
            break;
        }

        // ---- the reveal ------------------------------------------------------------------
        mResultsIcon.SetState(static_cast<u32>(meWinState));
        if (meWinState <= E_RESULTS_PLAIN_WIN)          // 0/1/2 == the three WIN animations
        {
            mLargeEventIcon.SetState("transIn");
        }
        mpcAnimatingComponentName = mLargeEventIcon.GetName();
    }

    // -----------------------------------------------------------------------------------
    // UpdateEventResults  @0x824BE228  (cpp:1727, 184 instructions)
    // The RESULTS sub-state: set the components up on the first tick, then run the licence
    // win-increment / reveal timers until the dwell expires and hand over to the next one.
    // ⛔ ONE FLAGGED GATE: the X360 guards its ShowLicense call (and a +4.0 s dwell bump) with
    // mResults +0xB5, a byte in the un-attributable flag run. Reading it by offset would be a
    // guess, so the licence reveal is not driven here; it is left to the wave that pins that
    // run. Everything else in this body is faithful.
    // -----------------------------------------------------------------------------------
    void InstantResultsState::UpdateEventResults()
    {
        CGS_ASSERT(meActiveSubState == E_ACTIVE_SUBSTATE_EVENT_RESULTS,
                   "E_ACTIVE_SUBSTATE_EVENT_RESULTS == meActiveSubState");            // cpp:1727

        if (meSubStateState == E_SUBSTATE_SET_UP_COMPONENTS)
        {
            SetupComponents();
            mfTimeToShowLicense  = KF_SHOW_LICENSE_PAUSE;
            mbLicenseShown       = false;
            mfTimeToIncrementWin = KF_WIN_INCREMENT_PAUSE;
            mbWinsIncremented    = false;
            meSubStateState      = E_SUBSTATE_RUNNING;
            return;
        }

        CGS_ASSERT(meSubStateState == E_SUBSTATE_RUNNING,
                   "Should not be updating car unlock when substate is in state");    // cpp:1796

        const f32 lfTimeStep = mpGuiCache->GetTimeStep();
        mfTimeToShowLicense  -= lfTimeStep;
        mfTimeToIncrementWin -= lfTimeStep;

        if (TickSubstateAndEndIfDone())
        {
            // Push the "<frame>Out" transition onto the results icon, then start the mode
            // icon's own trans-out on a winning result.
            char lacFrame[32];
            CgsCore::SnPrintf(lacFrame, sizeof(lacFrame), "%sOut",
                              KAC_RESULTS_FRAMES[mResultsIcon.GetState()]);
            mResultsIcon.SetState(lacFrame);

            if (meWinState <= E_RESULTS_PLAIN_WIN)
            {
                mLargeEventIcon.SetState("transOut");
                mpcAnimatingComponentName = mLargeEventIcon.GetName();
            }
        }
        else if (mfTimeToShowLicense > 0.0f || mbLicenseShown)
        {
            if (mfTimeToIncrementWin <= 0.0f && !mbWinsIncremented)
            {
                mLicense.AddWin();
                mbWinsIncremented = true;
            }
        }
        else
        {
            mbLicenseShown = true;
        }
    }

    // -----------------------------------------------------------------------------------
    // IsAWinningResult -- the three-way `meWinState` test the X360 emits inline in
    // UpdateTakePhotoPage @0x824C4044 and again @0x824C4168, and that SetupComponents /
    // UpdateEventResults spell as `meWinState <= E_RESULTS_PLAIN_WIN`.
    //
    // ⭐ The asm is worth recording, because it is NOT a `<=`: it compares against 1, then 0,
    // then 2, and sets the flag on any of the three (`cmpwi 1 / beq; cmpwi 0 / beq; cmpwi 2 /
    // bne`). Over EResultsAnimations that is exactly the set {DETAILED_WIN, WIN_WITH_TARGETS,
    // PLAIN_WIN} -- i.e. "the player won" -- so the source-level predicate is a membership
    // test, not an ordering one. Outlined here (AGENTS.md "inlining reversal") so the two call
    // sites read as the question they are asking.
    // -----------------------------------------------------------------------------------
    static bool IsAWinningResult(InstantResultsState::EResultsAnimations leWinState)
    {
        return leWinState == InstantResultsState::E_RESULTS_WIN_WITH_TARGETS
            || leWinState == InstantResultsState::E_RESULTS_DETAILED_WIN
            || leWinState == InstantResultsState::E_RESULTS_PLAIN_WIN;
    }

    // -----------------------------------------------------------------------------------
    // UpdateSecondResultsPage  @0x824BE508  (cpp:1814, 103 instructions)
    // The RESULTS_TWO sub-state -- the second results page, which only the mode-4 (pursuit)
    // result raises (SelectSubstates gates it on `meFinishedGameModeType == 4`). Set-up pushes
    // the shutdown car's name into mShutdownText and shows mSecondResultsIcon on frame 0; the
    // running tick hands over on the dwell and flips the icon to frame 1 on the way out.
    // -----------------------------------------------------------------------------------
    void InstantResultsState::UpdateSecondResultsPage()
    {
        CGS_ASSERT(meActiveSubState == E_ACTIVE_SUBSTATE_EVENT_RESULTS_TWO,
                   "E_ACTIVE_SUBSTATE_EVENT_RESULTS_TWO == meActiveSubState");         // cpp:1814

        if (meSubStateState == E_SUBSTATE_SET_UP_COMPONENTS)
        {
            // "CAR_CAPS_<id>" -- the shutdown car's localised display name, built from the
            // cache's pursued-car id. The X360 caps the SPrintf at 31 and NUL-terminates the
            // 32nd byte itself.
            char lacCarId[24];
            CgsIDConvertToString(mpGuiCache->GetPursuitCarID(), lacCarId);

            char lacCarStringId[32];
            CgsCore::SPrintf(lacCarStringId, 31, "CAR_CAPS_%s", lacCarId);
            lacCarStringId[31] = 0;

            const char* lapacParams[1] = { lacCarStringId };
            const CgsLanguage::LanguageManager::ParameterFormatType laeFormats[1] =
                { CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP };

            mSecondResultsIcon.SetState(0u);
            mShutdownText.SetLocalisedText("POSTRACE_FINISH_PURSUIT_CAR_SHUTDOWN",
                                           CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP,
                                           1, lapacParams, laeFormats);

            meSubStateState = E_SUBSTATE_RUNNING;
            return;
        }

        CGS_ASSERT(meSubStateState == E_SUBSTATE_RUNNING,
                   "Should not be updating car unlock when substate is in state");     // cpp:1858

        if (TickSubstateAndEndIfDone())
            mSecondResultsIcon.SetState(1u);
    }

    // -----------------------------------------------------------------------------------
    // UpdateTakePhotoPage  @0x824C3C70  (cpp:1876..2061)
    //
    // ⭐⭐ THIS IS THE SUB-STATE THE RESULTS PRESENTATION WAS STUCK IN. Measured 2026-08-29
    // (five runs, rs1..rs5): the "YOU WIN" stamp/banner presentation DOES draw, the RESULTS
    // sub-state dwells out on schedule and hands over to TAKE_PHOTO -- and then the trap stub
    // logged 2,795 identical lines to the end of the run, because nothing advanced the
    // sub-state machine again. So the panel did not fail to appear; it appeared and then had
    // nowhere to go.
    //
    // TWO PATHS, chosen by whether a Live Vision camera is attached (mpGuiCache->miCamStatus,
    // already homed at +0x13B58 with all fifteen readers listed):
    //   * camera present AND a winning result -> hide the results icon + the licence panel and
    //     run the real mugshot: either the WAITING_FOR_CLEANUP ladder below (rank-up run) or a
    //     straight jump into E_RESULTS_STATE_PHOTO_INTERRUPT.
    //   * otherwise (the PC case -- there is no camera and no producer for GUI event 570, so
    //     miCamStatus is 0, which is exactly a 360 with nothing plugged in) -> seed the stage
    //     to OLD_LICENSE_RECAP on a win / DONE on a loss and let the dwell carry it out.
    //
    // ⚠️ TWO ARGUMENTS THE PSEUDOCODE DROPS, RECOVERED FROM THE ASM (AGENTS.md rung 1). Both
    // EnsureResource calls are rendered `EnsureResourceIsUnloaded(mpGuiCache)` with no second
    // argument at all; the asm shows `addi r4, r31, 0x19D8` (i.e. &mLargeIconResource) for the
    // first, and a STACK-BUILT sResourceTuple{ meCurrentMainMovie, E_GUI_RESOURCETYPE_APT } for
    // the second and for the case-1 EnsureResourceIsLoaded (`stw meCurrentMainMovie, var_50` /
    // `li 4` / `stw var_4C`). Taking the pseudocode literally would have unloaded nothing.
    // -----------------------------------------------------------------------------------
    void InstantResultsState::UpdateTakePhotoPage()
    {
        CGS_ASSERT(meActiveSubState == E_ACTIVE_SUBSTATE_EVENT_TAKE_PHOTO,
                   "E_ACTIVE_SUBSTATE_EVENT_TAKE_PHOTO == meActiveSubState");          // cpp:1876

        if (meSubStateState == E_SUBSTATE_SET_UP_COMPONENTS)
        {
            const bool lbWon = IsAWinningResult(meWinState);

            if (lbWon && mpGuiCache->GetCamStatus() != 0)
            {
                mResultsIcon.SetState("Invisible");
                if (mLicense.IsVisible())
                    mLicense.SetVisible(false);

                if (mResults.mbHasRankedUp)
                {
                    // The rank-up run tears the results movie down first, so the photo page
                    // starts from the WAITING_FOR_CLEANUP rung of the ladder below.
                    mePhotoPresentationStage = E_PHOTO_PRESENTATION_WAITING_FOR_CLEANUP;
                }
                else
                {
                    // No rank-up: hand straight over to the mugshot interrupt, which Update
                    // routes to UpdatePhoto instead of the sub-state machine.
                    meCurrentState = E_RESULTS_STATE_PHOTO_INTERRUPT;
                    mPhotoBoothComponent.ShowComponent(false);
                    mePhotoPresentationStage = E_PHOTO_PRESENTATION_RUNNING;

                    if ((CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
                    {
                        *CgsDev::Log::gpDebugPrint << "INSTANT RESULTS DEBUG: "
                                                   << "UpdateTakePhotoPage"
                                                   << " (meCurrentState = " << meCurrentState << ")\n";
                    }
                }
            }
            else
            {
                mePhotoPresentationStage = lbWon ? E_PHOTO_PRESENTATION_OLD_LICENSE_RECAP
                                                 : E_PHOTO_PRESENTATION_DONE;
            }

            meSubStateState = E_SUBSTATE_RUNNING;
            return;
        }

        CGS_ASSERT(meSubStateState == E_SUBSTATE_RUNNING,
                   "Should not be updating car unlock when substate is in state");     // cpp:2061

        switch (mePhotoPresentationStage)
        {
        case E_PHOTO_PRESENTATION_WAITING_FOR_CLEANUP:
            // Wait for the results movie to finish animating out, then swap resource 217
            // (Results) for 218 (the rank-up/photo movie).
            if (!mLicense.IsVisible() && mpcAnimatingComponentName == 0
                && meCurrentMainMovie == KU_RESULTS_MOVIE_RESOURCE)
            {
                mpStateInterface->PlayAptMovie("", 3);
                mpStateInterface->PlayAptMovie("", 2);

                if (mpGuiCache->EnsureResourceIsUnloaded(mLargeIconResource))
                {
                    CgsGui::sResourceTuple lMainMovie;
                    lMainMovie.muId   = meCurrentMainMovie;
                    lMainMovie.meType = CgsGui::E_GUI_RESOURCETYPE_APT;
                    if (mpGuiCache->EnsureResourceIsUnloaded(lMainMovie))
                    {
                        meCurrentMainMovie       = KU_PHOTO_MOVIE_RESOURCE;
                        mePhotoPresentationStage = E_PHOTO_PRESENTATION_LOADING;
                    }
                }
            }
            break;

        case E_PHOTO_PRESENTATION_LOADING:
        {
            CgsGui::sResourceTuple lPhotoMovie;
            lPhotoMovie.muId   = meCurrentMainMovie;
            lPhotoMovie.meType = CgsGui::E_GUI_RESOURCETYPE_APT;
            if (mpGuiCache->EnsureResourceIsLoaded(lPhotoMovie))
            {
                mpGuiCache->ClearExpectedAptComponentList(static_cast<GuiFlow>(0));
                mpGuiCache->AppendExpectedAptComponent(static_cast<GuiFlow>(0),
                                                       mUpgradeText.GetName());
                mpGuiCache->AppendExpectedAptComponent(static_cast<GuiFlow>(0),
                                                       mUpgradeStateAnimator.GetName());
                mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[meCurrentMainMovie], 3);
                mePhotoPresentationStage = E_PHOTO_PRESENTATION_INITIALISING;
            }
            break;
        }

        case E_PHOTO_PRESENTATION_INITIALISING:
            if (mpGuiCache->AreAllAptComponentsInitialised(static_cast<GuiFlow>(0)))
            {
                mpGuiCache->ClearExpectedAptComponentList(static_cast<GuiFlow>(0));
                mUpgradeStateAnimator.AddOutputAptViewState("apt_Transition", "takePhoto", false);
                meCurrentState = E_RESULTS_STATE_PHOTO_INTERRUPT;
                mPhotoBoothComponent.ShowComponent(false);

                if ((CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "INSTANT RESULTS DEBUG: "
                                               << "UpdateTakePhotoPage"
                                               << " (meCurrentState = " << meCurrentState << ")\n";
                }

                mePhotoPresentationStage = E_PHOTO_PRESENTATION_RUNNING;
            }
            break;

        case E_PHOTO_PRESENTATION_RUNNING:
            if (mResults.mbHasRankedUp)
            {
                mePhotoPresentationStage = E_PHOTO_PRESENTATION_DONE;
            }
            else
            {
                mLicense.SetVisible(true);
                mePhotoPresentationStage = E_PHOTO_PRESENTATION_OLD_LICENSE_RECAP;
            }
            break;

        case E_PHOTO_PRESENTATION_OLD_LICENSE_RECAP:
            if (HasSubstateTimedOut())
            {
                if (!mResults.mbHasRankedUp && mLicense.IsVisible())
                    mLicense.HideLicense();
                mePhotoPresentationStage = E_PHOTO_PRESENTATION_DONE;
            }
            break;

        case E_PHOTO_PRESENTATION_DONE:
            meActiveSubState = GetNextSubstate();
            ResetStateTimer();
            meSubStateState = E_SUBSTATE_SET_UP_COMPONENTS;
            break;

        default:
            CGS_ASSERT(false, "Invalid substate for taking new photo");                // cpp:2053
            break;
        }
    }

    // -----------------------------------------------------------------------------------
    // UpdateRankUp  @0x824BE6A8  (cpp:2576, 77 instructions)
    // The RANK_UP_TEXT sub-state: show mRankUpIcon on frame 0, dwell, flip it to frame 1 on
    // the way out. (SelectSubstates never raises this flag in the shipped offline path -- it
    // raises RANK_UP_LICENSE instead -- but the dispatch reaches it and the body is three
    // lines, so it is landed rather than left as a stub that would look like a real gap.)
    // -----------------------------------------------------------------------------------
    void InstantResultsState::UpdateRankUp()
    {
        CGS_ASSERT(meActiveSubState == E_ACTIVE_SUBSTATE_EVENT_RANK_UP_TEXT,
                   "E_ACTIVE_SUBSTATE_EVENT_RANK_UP_TEXT == meActiveSubState");        // cpp:2576

        if (meSubStateState == E_SUBSTATE_SET_UP_COMPONENTS)
        {
            mRankUpIcon.SetState(0u);
            meSubStateState = E_SUBSTATE_RUNNING;
            return;
        }

        CGS_ASSERT(meSubStateState == E_SUBSTATE_RUNNING,
                   "Should not be updating rank up when substate is in state");        // cpp:2600

        if (TickSubstateAndEndIfDone())
            mRankUpIcon.SetState(1u);
    }

    // -----------------------------------------------------------------------------------
    // UpdateLeaving  @0x824BE7E0  (cpp:3151, 68 instructions)
    // The LEAVING sub-state -- the last dwell before DONE hands the flow back to the FSM.
    // Set-up does nothing but arm the timer state; the running tick is the shared
    // TickSubstateAndEndIfDone, whose expiry moves meActiveSubState on to DONE and so reaches
    // TriggerExitResults. Without this body the presentation could never leave the screen.
    // -----------------------------------------------------------------------------------
    void InstantResultsState::UpdateLeaving()
    {
        CGS_ASSERT(meActiveSubState == E_ACTIVE_SUBSTATE_EVENT_LEAVING,
                   "E_ACTIVE_SUBSTATE_EVENT_LEAVING == meActiveSubState");             // cpp:3151

        if (meSubStateState == E_SUBSTATE_SET_UP_COMPONENTS)
        {
            meSubStateState = E_SUBSTATE_RUNNING;
            return;
        }

        CGS_ASSERT(meSubStateState == E_SUBSTATE_RUNNING,
                   "Should not be waiting to leave when substate is");                 // cpp:3171

        TickSubstateAndEndIfDone();
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
                HandleAptTriggers(
                    reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent));
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
    // WillShowCredits  @0x824C5C38  (cpp:3433, 59 instructions)
    //
    // ⛔ THE STANDING NOTE ON THIS FUNCTION WAS STALE. It said the body "needs
    // BrnGui::WorldDataController::GetProgressionData, which has no home in the tree" -- that
    // has had a real body since BrnGuiWorldDataController.cpp landed (@0x82428818,
    // BrnGuiWorldDataController.cpp:466), and GuiCache::GetWorldDataController has one too.
    // ⭐ AND ALL THREE RECORD SLOTS IT READS ARE ALREADY NAMED AND ATTESTED -- none of them is
    // in the un-attributable +0xB1..+0xB5 flag run this file's banner warns about:
    //     this+0x2330 -> mResults.mbHasRankedUp       (record +0xB8, `lbz`)
    //     this+0x231C -> mResults.miPlayerNewRank     (record +0xA4, `lwz`)
    //     this+0x232E -> mResults.mbCompletedLastRank (record +0xB6, `lbz`, compared to 1)
    // So there was nothing left to block on. The credits roll when the player has just ranked
    // up INTO the last licence rank, or has completed it.
    //
    // NOTE the side effect in the true arm: it does not just answer the question, it starts
    // the credits music. That is why TriggerExitResults may not call it twice.
    // -----------------------------------------------------------------------------------
    bool InstantResultsState::WillShowCredits()
    {
        CGS_ASSERT(mpGuiCache, "mpGuiCache");     // cpp:3433

        if (!mResults.mbHasRankedUp)
            return false;

        if (mpGuiCache->IsPostEventPresentationSuppressed())
            return false;

        // The last rank's index: the rank table's length minus one (`lwz r11, 0x14(r3)` on the
        // ProgressionData -- the same word ProgressionManager::GetProgressionRank clamps with,
        // which is what pins it as muProgressionRankCount rather than a rank value).
        const s32 liBurnoutLicenseRank = static_cast<s32>(
            mpGuiCache->GetWorldDataController()->GetProgressionData()
                ->GetProgressionRankCount()) - 1;
        CGS_ASSERT(liBurnoutLicenseRank > 0, "liBurnoutLicenseRank > 0");   // cpp:3444

        if (mResults.miPlayerNewRank != liBurnoutLicenseRank && !mResults.mbCompletedLastRank)
            return false;

        CgsGui::GuiEventPlayMusicOnMenuStream lMusicEvent(
            static_cast<u32>(CgsSound::Playback::Name::MakeHash(KAC_CREDITS_MUSIC_STREAM)),
            true, false);
        mpStateInterface->OutputGuiEvent(lMusicEvent);
        return true;
    }

    // -----------------------------------------------------------------------------------
    // The X360 emits the same StrStream + FireAssert sequence four times inside
    // HandleAptTriggers -- a fixed message, a stage/sub-state value, a suffix. Outlined once
    // here (AGENTS.md "inlining reversal"; the same treatment IsAWinningResult gets above).
    // The console streams into the global CgsDev::Assert::gpcMessageBuffer; building into a
    // stack buffer is the committed BrnGuiFsmController.cpp / BrnTriggerData.cpp precedent.
    // -----------------------------------------------------------------------------------
    static void FireUnexpectedStateAssert(const char* lpacMessage, s32 liState,
                                          const char* lpacSuffix)
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << lpacMessage << liState << lpacSuffix;
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }

    // -----------------------------------------------------------------------------------
    // HandleAptTriggers  @0x824BDAB8  (cpp:1005, 475 instructions)
    //
    // ⛔⛔ THIS IS NOT "HOW A PLAYER SKIPS THE PRESENTATION", AND CALLING IT THAT SENT THE
    // LAST BRIEF LOOKING FOR A CONTROL THAT DOES NOT EXIST ON THIS SCREEN. Event 21 is
    // CgsGui::GuiEventAptTrigger -- the APT MOVIE's own callback, fired when a clip finishes
    // loading (E_APT_EVENT_ONLOAD) or finishes a transition (E_APT_EVENT_TRANSITION_COMPLETE).
    // The only controller-input handler this class has is HandleControllerInput @0x824B3E00,
    // and it answers exactly two buttons (49 / 50) and only while meCurrentState ==
    // E_RESULTS_STATE_PHOTO_INTERRUPT -- i.e. it is the PHOTO BOOTH's take/cancel pair, not a
    // skip. Every other page on this screen is timer-driven (HasSubstateTimedOut).
    //
    // ⭐⭐ WHAT IT ACTUALLY IS: THE HANG. Its last arm is the ONLY writer that clears
    // mpcAnimatingComponentName -- and UpdateTakePhotoPage's E_PHOTO_PRESENTATION_
    // WAITING_FOR_CLEANUP arm will not swap the results movie out until that pointer is null
    // (`!mLicense.IsVisible() && mpcAnimatingComponentName == 0 && ...`). SetupComponents and
    // UpdateEventResults both SET it to mLargeEventIcon.GetName(). So with this a stub the
    // screen reaches TAKE_PHOTO and waits forever for a transition-complete nothing consumes:
    // the same shape as the 2,795-identical-lines stall the previous wave fixed one step
    // earlier, moved one step later. A finished screen and a hung screen are the same picture,
    // which is why it survived.
    //
    // STRUCTURE (asm order; every strcmp target is `component + 4`, i.e. GuiComponent::macName,
    // so they are spelled GetName() here):
    //   ONLOAD (1):
    //     * mFinishedText / mTargetResultText -- re-push the field's stored text once its clip
    //       exists. The X360 passes `field + 0xA4`, which is macText: TextField::GetText().
    //     * mLicense -- matched with strstr, not strcmp (`strstr(Str = the trigger name,
    //       SubStr = License_cpt)`), because the licence owns a family of sub-clips whose
    //       names all CONTAIN its own. Inside, an exact match while the RANK_UP_LICENSE
    //       sub-state is running picks the upgrade caption+animation; then, matched or not,
    //       the trigger is forwarded to the component.
    //     * the three LargeCarComponents -- each gated on ITS OWN sub-state (XS car <-> 5,
    //       rival car <-> 7, free car <-> 6). Note the pairing is not positional: the RIVAL
    //       car is the one gated on 7.
    //   TRANSITION_COMPLETE (4):
    //     * an unconditional per-sub-state forward to the matching LargeCarComponent (no name
    //       test at all -- the component does its own),
    //     * the licence's transition forward,
    //     * the NewRivals icon -- drives meNewRivalsPresentationStage,
    //     * the CarUnlock icon -- drives whichever of the two unlock stage machines is live,
    //     * and finally the mpcAnimatingComponentName clear described above.
    //
    // ⚠️ ONE CONSOLE QUIRK PRESERVED: the FREE-car arm's failure assert streams
    // meCarUnlockPresentationStage (`lwz r29, 0x223C(r29)` at 0x824BE0DC), not the free-car
    // stage it just switched on. That is the console's own copy-paste; it is a diagnostic, so
    // it is reproduced rather than "corrected" -- correcting it would make our log disagree
    // with a real X360 log for the same event.
    // -----------------------------------------------------------------------------------
    void InstantResultsState::HandleAptTriggers(const CgsGui::GuiEventAptTriggerPayload* lpAptTrigger)
    {
        CGS_ASSERT(lpAptTrigger, "lpEvent");     // cpp:1005

        const char* const lpacName = lpAptTrigger->mpacComponentName;

        if (lpAptTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD)
        {
            if (std::strcmp(mFinishedText.GetName(), lpacName) == 0)
            {
                mFinishedText.SetText(mFinishedText.GetText());
                return;
            }

            if (std::strcmp(mTargetResultText.GetName(), lpacName) == 0)
            {
                mTargetResultText.SetText(mTargetResultText.GetText());
                return;
            }

            if (std::strstr(lpacName, mLicense.GetName()) != 0)
            {
                if (meActiveSubState == E_ACTIVE_SUBSTATE_EVENT_RANK_UP_LICENSE
                    && std::strcmp(mLicense.GetName(), lpacName) == 0)
                {
                    const s32 liBurnoutLicenseRank = static_cast<s32>(
                        mpGuiCache->GetWorldDataController()->GetProgressionData()
                            ->GetProgressionRankCount()) - 1;
                    CGS_ASSERT(liBurnoutLicenseRank > 0,
                               "liBurnoutLicenseRank > 0");                       // cpp:1026

                    if (mResults.mbCompletedLastRank)
                    {
                        mUpgradeStateAnimator.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                                    "upgradeElite", false);
                        mUpgradeText.SetLocalisedText(
                            "COMPLETION_SEQUENCE_ELITE",
                            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
                    }
                    else if (liBurnoutLicenseRank == mResults.miPlayerNewRank)
                    {
                        mUpgradeStateAnimator.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                                    "upgradeBurnout", false);
                        mUpgradeText.SetLocalisedText(
                            "COMPLETION_SEQUENCE_BURNOUT",
                            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
                    }
                    else
                    {
                        mUpgradeStateAnimator.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                                    "upgrade", false);
                        mUpgradeText.SetLocalisedText(
                            "LICENSE_UPGRADE_PENDING",
                            CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
                    }
                }

                mLicense.HandleAptLoadTriggers(lpAptTrigger);
                return;
            }

            if (std::strcmp(mUnlockedXSCarComponent.GetName(), lpacName) == 0)
            {
                if (meActiveSubState == E_ACTIVE_SUBSTATE_EVENT_CAR_UNLOCK)
                    mUnlockedXSCarComponent.HandleAptLoadTriggers(lpAptTrigger);
                return;
            }

            if (std::strcmp(mUnlockedRivalCarComponent.GetName(), lpacName) == 0)
            {
                if (meActiveSubState == E_ACTIVE_SUBSTATE_EVENT_RANK_UP_SHOWING_RIVALS)
                    mUnlockedRivalCarComponent.HandleAptLoadTriggers(lpAptTrigger);
                return;
            }

            if (std::strcmp(mUnlockedFreeCarComponent.GetName(), lpacName) == 0
                && meActiveSubState == E_ACTIVE_SUBSTATE_EVENT_FREE_CAR_UNLOCK)
            {
                mUnlockedFreeCarComponent.HandleAptLoadTriggers(lpAptTrigger);
            }
            return;
        }

        if (lpAptTrigger->meEventType
                != CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE)
        {
            return;
        }

        // The unconditional per-sub-state forward. No name test here -- each component's own
        // HandleAptTransitionTriggers compares the name and returns whether it claimed it.
        switch (meActiveSubState)
        {
        case E_ACTIVE_SUBSTATE_EVENT_CAR_UNLOCK:
            mUnlockedXSCarComponent.HandleAptTransitionTriggers(lpAptTrigger);
            break;
        case E_ACTIVE_SUBSTATE_EVENT_RANK_UP_SHOWING_RIVALS:
            mUnlockedRivalCarComponent.HandleAptTransitionTriggers(lpAptTrigger);
            break;
        case E_ACTIVE_SUBSTATE_EVENT_FREE_CAR_UNLOCK:
            mUnlockedFreeCarComponent.HandleAptTransitionTriggers(lpAptTrigger);
            break;
        default:
            break;
        }

        if (std::strcmp(mLicense.GetName(), lpacName) == 0)
        {
            mLicense.HandleAptTransitionTriggers(lpAptTrigger);
            return;
        }

        if (std::strcmp(mNewRivalsIcon.GetName(), lpacName) == 0)
        {
            CGS_ASSERT(meActiveSubState == E_ACTIVE_SUBSTATE_EVENT_RANK_UP_SHOWING_RIVALS,
                       "E_ACTIVE_SUBSTATE_EVENT_RANK_UP_SHOWING_RIVALS == meActiveSubState");
                                                                                  // cpp:1088
            switch (meNewRivalsPresentationStage)
            {
            case E_NEW_RIVALS_PRESENTATION_INTRO:
                if ((CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "\n\n\n**********Ready to set up new rival viewing (intro done)\n\n\n";
                }
                meNewRivalsPresentationStage = E_NEW_RIVALS_PRESENTATION_SHOWING_RIVAL_SET_UP_TEXT;
                break;

            case E_NEW_RIVALS_PRESENTATION_SHOWING_RIVAL:
                if (mPendingRivalId != 0)
                {
                    if ((CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "\n\n\n**********Ready to set up new rival viewing "
                               "(rival pending)\n\n\n";
                    }
                    meNewRivalsPresentationStage =
                        E_NEW_RIVALS_PRESENTATION_SHOWING_RIVAL_SET_UP_TEXT;
                }
                else
                {
                    meNewRivalsPresentationStage = E_NEW_RIVALS_PRESENTATION_OUTRO_SET_UP;
                }
                break;

            case E_NEW_RIVALS_PRESENTATION_OUTRO_ENDING:
                mUnlockedRivalCarComponent.ReleaseResources();
                meNewRivalsPresentationStage = E_NEW_RIVALS_PRESENTATION_CLEANING_UP;
                break;

            default:
                FireUnexpectedStateAssert(
                    "InstantResultsState::HandleAptTriggers : Should not be receiving a "
                    "transition complete when not expecting one (currently in state ",
                    meNewRivalsPresentationStage, " )\n");                        // cpp:1126
                break;
            }
            return;
        }

        if (std::strcmp(mCarUnlockIcon.GetName(), lpacName) == 0)
        {
            CGS_ASSERT(meActiveSubState == E_ACTIVE_SUBSTATE_EVENT_CAR_UNLOCK
                           || meActiveSubState == E_ACTIVE_SUBSTATE_EVENT_FREE_CAR_UNLOCK,
                       "( E_ACTIVE_SUBSTATE_EVENT_CAR_UNLOCK == meActiveSubState ) || "
                       "( E_ACTIVE_SUBSTATE_EVENT_FREE_CAR_UNLOCK == meActiveSubState )");
                                                                                  // cpp:1134
            if (meActiveSubState == E_ACTIVE_SUBSTATE_EVENT_CAR_UNLOCK)
            {
                switch (meCarUnlockPresentationStage)
                {
                case E_CAR_UNLOCK_PRESENTATION_SHOWING_CAR:
                    meCarUnlockPresentationStage = E_CAR_UNLOCK_PRESENTATION_OUTRO_SET_UP;
                    break;
                case E_CAR_UNLOCK_PRESENTATION_OUTRO_ENDING:
                    meCarUnlockPresentationStage = E_CAR_UNLOCK_PRESENTATION_CLEANING_UP;
                    break;
                default:
                    FireUnexpectedStateAssert(
                        "InstantResultsState::HandleAptTriggers : Got a car unlock substate "
                        "transition complete in state ",
                        meCarUnlockPresentationStage, ".\n");                      // cpp:1156
                    break;
                }
            }
            else if (meActiveSubState == E_ACTIVE_SUBSTATE_EVENT_FREE_CAR_UNLOCK)
            {
                switch (meFreeCarPresentationStages)
                {
                case E_FREE_CAR_UNLOCK_PRESENTATION_SHOWING_CAR:
                    meFreeCarPresentationStages = E_FREE_CAR_UNLOCK_PRESENTATION_OUTRO_SET_UP;
                    break;
                case E_FREE_CAR_UNLOCK_PRESENTATION_OUTRO_ENDING:
                    meFreeCarPresentationStages = E_FREE_CAR_UNLOCK_PRESENTATION_CLEANING_UP;
                    break;
                default:
                    // ⚠️ the console streams the CAR-UNLOCK stage here, not the free-car one.
                    // See the banner: reproduced, not corrected.
                    FireUnexpectedStateAssert(
                        "InstantResultsState::HandleAptTriggers : Got a free car unlock "
                        "substate transition complete in state ",
                        meCarUnlockPresentationStage, ".\n");                      // cpp:1181
                    break;
                }
            }
            else
            {
                FireUnexpectedStateAssert(
                    "InstantResultsState::HandleAptTriggers : Should not be receiving a car "
                    "unlock transition complete when not expecting one (currently in state ",
                    meActiveSubState, " )\n");                                     // cpp:1190
            }
            return;
        }

        // ⭐ THE ONE ARM THE WHOLE SUB-STATE MACHINE WAITS ON -- see the banner.
        if (mpcAnimatingComponentName != 0
            && std::strcmp(mpcAnimatingComponentName, lpacName) == 0)
        {
            mpcAnimatingComponentName = 0;
        }
    }

    // -----------------------------------------------------------------------------------
    // TriggerExitResults  @0x824D58A8  (cpp:3294, 66 instructions)
    // Leave the results screen: either into the credits, or back to the front end.
    //
    // ⭐⭐ COMPLETE NOW, AND THE PREVIOUS BANNER HERE WAS STALE IN THREE PLACES. It said
    // "GuiEventRunFsm has no home in the tree yet" (it has had one since 2026-08-27 --
    // BrnGuiEventTypeDefs.h:126, with the 144 wire id and a sizeof==24 static_assert), it
    // called the middle record "{12, 0x124, 12}-shaped" (the asm writes {1, 292, 12}: `stw
    // r30(=1)` at +0, `li r11,0x124` at +4, `li r11,0xC` at +8 -- a payload of ONE byte, the
    // C++ sizeof of an empty event struct), and it described the record as un-decoded when
    // 292 is already a named, consumed id on both sides of the bridge.
    //
    // THE EXIT SEQUENCE, read off @0x824D58A8 instruction by instruction:
    //   1. assert mpGuiCache                                                (cpp:3294)
    //   2. WillShowCredits() -> SendStateEvent("TO_CREDITS") and RETURN. Note the asm
    //      compares the returned byte against 1 (`clrlwi r11,r3,24 / cmplwi r11,1`), which
    //      is why WillShowCredits is a bool and not an int.
    //   3. SendStateEvent("ADVANCE")
    //   4. if the cache's presentation-suppressed byte (+0x4B77) is SET, post
    //      GuiEventPlayMusicOnMenuStream{ dword_830082A8, flagA=1, flagB=0 }.
    //      ⭐ dword_830082A8 is not opaque: its ONLY writer is the dyn-init
    //      sub_82C64F20, whose entire body is `dword_830082A8 =
    //      CgsSound::Playback::Name::MakeHash(&unk_820046A7)`, and 0x820046A7 is the second
    //      NUL after the "%s%s%s" literal at 0x820046A0 -- i.e. the EMPTY STRING (read out of
    //      the image here, and independently recorded by the wave that landed
    //      SetupComponents' third help item). So it is the "no stream / silence" hash, and
    //      the arm means "the presentation was skipped, so kill the menu-music stream".
    //   5. UNCONDITIONALLY post the post-event TEARDOWN, {1, 292, 12} on channel 40.
    //      GUI 292 is already homed at both ends: GuiCache::RecEvent case 292 clears
    //      mOfflinePostEventData and drops mbSuppressPostEventPresentation, and the GUI->game
    //      bridge's case 292 emits game event 26.
    //   6. UNCONDITIONALLY post GuiEventRunFsm{ CgsIDCompress("BRNFBFSM"), 0,
    //      E_GUI_HUD_FREEBURN, E_GUIFLOW_HUD } -- i.e. put the HUD flow back on the FreeBurn
    //      FSM. ⭐ That record is CHARACTER-FOR-CHARACTER the second of the two posts
    //      BrnGameModule::BridgeGameToGui makes at its stage 5 ("BrnFBFsm", 0,
    //      E_GUI_HUD_FREEBURN, E_GUIFLOW_HUD) -- the in-game handoff. Two independent
    //      producers, same record: that is the corroboration that the trailing pair of 1s
    //      really are (meFsmToRun, meFlowToUse) and not something else.
    //
    // WIRE SHAPES (asm, not inference). Both channel-40 posts are written through
    // GetOutputEventQueue()->AddEvent -- the standing accommodation documented in
    // CgsGuiStateInterface.h, because this build's OutputGuiEvent<T> direct-passes with
    // GetEventType() as the channel and GuiEventRunFsm is NOT a GuiEvent<N> subclass, so it
    // would carry no wrapper header at all. The console's own wrapper is
    // @0x824938D0: {sizeof=0x18, type=0x90, offset=0x10} + a 24-byte copy, AddEvent(..., 40,
    // 40). The offset is SIXTEEN, not twelve, because GuiEventRunFsm leads with a CgsID and
    // is 8-aligned -- exactly the "+0x0C for align<=4, +0x10 for align 8" rule
    // CgsGuiEvent.h's GuiEventWrapper note records.
    //
    // ⚠️ DELIVERY: on the console this record leaves on channel 40, the interpreter's
    // ProcessOutEvents case '(' UNWRAPS it (`lwz r11,8(r22)` / `lwz r6,0(r22)` /
    // `lwz r5,4(r22)` -> AddEvent into the module's out-event buffer @0x8285E568) and it
    // comes back around the module bus as GUI in-event 144, where GuiModule::Update's
    // case 144 hands it to GuiFsmController::RunFsm. This build's flow-out drain had no
    // seat for the inner id, so the paired change in BrnGuiModule.cpp's case-40 arm answers
    // it at the drain -- the same call, one hop earlier, exactly like the 507 arm already
    // sitting beside it. Without that half this post is a silent drop.
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

        if (mpGuiCache->IsPostEventPresentationSuppressed())
        {
            // The console's cached MakeHash("") -- see the banner. Cached here too (one-time
            // init at first use) rather than re-hashed per exit, which is what the dyn-init
            // global buys on the console.
            static const u32 KU_NO_MUSIC_STREAM_HASH =
                static_cast<u32>(CgsSound::Playback::Name::MakeHash(""));

            CgsGui::GuiEventPlayMusicOnMenuStream lMusicEvent(KU_NO_MUSIC_STREAM_HASH,
                                                              true, false);
            mpStateInterface->OutputGuiEvent(lMusicEvent);
        }

        // ---- 5. the post-event teardown, {1, 292, 12} on channel 40 ---------------------
        // The one payload byte at +12 is never written by the console either (the `std r29`
        // that lands there runs AFTER the AddEvent, as part of building the NEXT record) --
        // it is the sizeof of an empty event struct, not a value. Zeroed here rather than
        // left indeterminate; no consumer reads it.
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

        // ---- 6. hand the HUD flow back to the FreeBurn FSM ------------------------------
        struct RunFsmRecord
        {
            s32            miOutEventSize;
            s32            miOutEventType;
            s32            miOutEventOffset;
            s32            miAlignmentPad;      // the CgsID payload is 8-aligned
            GuiEventRunFsm mOutEvent;
        } lRunFsm;
        // The console's `li r6, 0x28` -- if this is not 40 the queued record is not the
        // console's, and RunFsm would read a shifted CgsID.
        static_assert(sizeof(RunFsmRecord) == 40,
                      "GuiEventRunFsm channel-40 wrapper is 40 bytes (16-byte header + 24)");
        static_assert(__builtin_offsetof(RunFsmRecord, mOutEvent) == 16,
                      "the wrapper's payload offset is 16 (GuiEventRunFsm is 8-aligned)");
        lRunFsm.miOutEventSize   = static_cast<s32>(sizeof(GuiEventRunFsm));   // 0x18
        lRunFsm.miOutEventType   = lRunFsm.mOutEvent.GetEventType();           // 0x90 == 144
        lRunFsm.miOutEventOffset = 16;                                         // 0x10
        lRunFsm.miAlignmentPad   = 0;
        lRunFsm.mOutEvent.mFsmId          = CgsIDCompress(KAC_FREEBURN_FSM_ID);
        lRunFsm.mOutEvent.mInitialStateId = static_cast<CgsID>(0);
        lRunFsm.mOutEvent.meFsmToRun      = E_GUI_HUD_FREEBURN;
        lRunFsm.mOutEvent.meFlowToUse     = E_GUIFLOW_HUD;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRunFsm), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lRunFsm)));
    }
}
