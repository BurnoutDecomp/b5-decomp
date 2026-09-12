// ===================================================================================
// BrnGui::CompletedGame  -- the offline post-event "completed game" presentation state
//   class:BrnGui::CompletedGame
//
//   CompletedGame (ctor)
//   (the remaining twelve in source order below)
//   Construct  (cpp:80)
//   OnEnter  (cpp:112)
//   OnLeave  (cpp:163)
//   Update  (cpp:191)
//   UpdateBurnoutLicensePresentation  (cpp:373)
//   UpdateEliteLicensePresentation  (cpp:481)
//   UpdateOneHundredPercentPartOnePresentation  (cpp:680)
//   UpdateOneHundredPercentPartTwoPresentation  (cpp:873)
//   HandleIncomingEvents  (cpp:998)
//   HandleControllerInput  (cpp:1122)
//   TriggerExitState  (cpp:1203)
// Reconstructed store-for-store from the console build.
//
// WHAT THIS SCREEN IS. The results screen hands over here when the event that just
// finished completed the GAME in one of four ways, and Update's first frame decides
// which by reading the cache's last-results record and the profile:
//   * the player's new rank == the last burnout-licence rank  -> WON_BURNOUT_LICENSE
//   * otherwise, the elite completion sequence is unseen      -> WON_ELITE_LICENSE
//   * otherwise, 100% -- part ONE or part TWO, picked by the cache's
//     post-event-presentation-suppressed byte.
// From then on Update runs exactly ONE of the four presentation ladders per frame,
// each of which drives the same shared components (the licence card, the photo booth,
// the two animators and the two text fields), and every ladder ends by dropping
// meCompletedGameState to DONE, which makes the next frame TriggerExitState.
// ===================================================================================
#include "GameSource/Gui/Flow/PostEvent/States/Offline/BrnCompletedGame.h"

#include <cstring>                                                        // strstr

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // GuiEventAptTriggerPayload
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // E_FORMAT_ID_LOOKUP
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // VariableEventQueue<18432,16>
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"              // Playback::Name::MakeHash
#include "GameSource/GameState/Progression/BrnProfile.h"                  // BrnProgression::Profile
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // the four raw payload events
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include "GameSource/Gui/BrnGuiWorldDataController.h"                     // GetProgressionData
#include "SharedClasses/Progression/BrnProgressionData.h"                 // GetProgressionRankCount
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"         // ButtonIconComponent::EPadButton

namespace BrnGui
{
namespace
{
    // The state IN-queue is the 18KB variable event queue every GUI state reads.
    typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

    // ---- the six observed event ids (cpp:32; the same set the switch below answers) ----
    const s32 KI_EVENT_APT_TRIGGER              = 21;
    const s32 KI_EVENT_CONTROLLER_INPUT_PRESSED = 6;
    const s32 KI_EVENT_GUI_CACHE                = 64;
    const s32 KI_EVENT_PROGRESSION_PROFILE      = 350;
    const s32 KI_EVENT_PERCENTAGE_COMPLETE      = 436;
    const s32 KI_EVENT_COMPRESSED_STILL_IMAGE   = 569;

    // ---- component names (cpp:46..53; the reference array lengths match each literal) -----
    const char KAC_LICENSE_COMPONENT_NAME[]  = "License_cpt";         // char[12]
    const char KAC_PHOTO_COMPONENT_NAME[]    = "PhotoBooth_cpt";      // char[15]
    const char KAC_UPGRADE_ANIM_NAME[]       = "mainStateAnimator";   // char[18]
    const char KAC_UPGRADE_TEXTFIELD_NAME[]  = "upgradeTextOne_cpt";  // char[19]
    const char KAC_CARS_ANIM_NAME[]          = "moveStateAnim_cpt";   // char[18]
    const char KAC_CARS_TEXTFIELD_NAME[]     = "congratTextOne_cpt";  // char[19]

    // ---- page durations (cpp:56..60). The reference names five floats in this order and the
    // asm supplies five distinct literals, each used exactly where its name says:
    //   4.0 on the two "..._NEXT_STEP" caption pages and the 100% "Complete" page,
    //   8.0 on the two reward-car pages (GOLD / PLATINUM),
    //  11.0 on the final rank-up,
    //   2.0 as ShowUpgradedLicense's time-to-show-next-rank,
    //   1.0 as the head start the background transition-out gets over the page end.
    const f32 KF_NEXT_STEP_DURATION      = 4.0f;
    const f32 KF_CARS_UNLOCKED_DURATION  = 8.0f;
    const f32 KF_FINAL_UPGRADED_DURATION = 11.0f;
    const f32 KF_LICENSE_TICK_UP_DURATION = 2.0f;
    const f32 KF_BACKGROUND_HEADSTART    = 1.0f;

    // ---- the apt movies each presentation mounts. The asm reads them out of
    // gGuiResourceIdentifier at exactly the index it has just written into
    // mResourceToLoad, so the id and the movie name are the same number twice.
    const u32 KU_UPGRADE_MOVIE_RESOURCE  = 218;   // "BrnUpgrade"
    const u32 KU_GOLD_MOVIE_RESOURCE     = 222;   // "goldCarUnlock"
    const u32 KU_PLATINUM_MOVIE_RESOURCE = 223;   // "platinumCarUnlock"
    const s32 KI_MOVIE_LEVEL             = 3;

    // The apt component/parameter names the animators are driven through.
    const char KAC_APT_TRANSITION[] = "apt_Transition";

    // The controller action sub-ids the photo booth answers (cpp:1122).
    const s32 KI_ACTION_SELECT = 49;
    const s32 KI_ACTION_BACK   = 50;

    // TriggerExitState posts the post-event TEARDOWN, {1, 292, 12} on channel 40 -- the
    // same record the offline results screen posts on its way out. GuiCache::RecEvent
    // case 292 clears the cached post-event data.
    struct GuiEventPostEventTeardown : public CgsGui::GuiEvent<292>
    {
        u8 mucPad;   // +0x0C (the 1-byte empty payload; the console leaves it unwritten)

        GuiEventPostEventTeardown() : CgsGui::GuiEvent<292>(1, 12), mucPad(0) {}
    };
    const s32 KI_CHANNEL_GUI_OUT = 40;

    // The console streams the fixed prefix, the offending state and a suffix into the
    // global assert message buffer and fires unconditionally; building into a stack
    // buffer is the committed BrnOfflineInstantResults / BrnGuiFsmController precedent.
    void FireUnexpectedStateAssert(const char* lpacMessage, s32 liState,
                                   const char* lpacSuffix)
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStrStream << lpacMessage << liState << lpacSuffix;
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
        CgsDev::Assert::EndAssert();
    }

    // The four presentation ladders share ONE malformed-substate diagnostic: all four
    // stream meBurnoutSubstate, even the three that switched on a different substate
    // word. That is the console's own copy-paste; it is a diagnostic, so it is
    // reproduced rather than "corrected" -- correcting it would make our log disagree
    // with a real console log for the same frame.
    const char KAC_INVALID_SUBSTATE_PREFIX[] = "Invalid burnout state in update (";
    const char KAC_INVALID_SUBSTATE_SUFFIX[] = ") \n";
}

// cpp:32 / cpp:43 -- read out of the image at the RegisterForEvents call site.
const s32 CompletedGame::maiEventToObserve[] =
{
    KI_EVENT_APT_TRIGGER,               // 21
    KI_EVENT_CONTROLLER_INPUT_PRESSED,  // 6
    KI_EVENT_GUI_CACHE,                 // 64
    KI_EVENT_PROGRESSION_PROFILE,       // 350
    KI_EVENT_COMPRESSED_STILL_IMAGE,    // 569
    KI_EVENT_PERCENTAGE_COMPLETE,       // 436
};
const s32 CompletedGame::miNumEventsObserved = 6;

// Default constructor. The console image inlines this as a flat run of
// vtable-pointer stores: the CompletedGame own vtable at +0 followed by the
// default-construction vtable pointer of every embedded GUI sub-component (the license /
// photo / animation / textfield components and their substate machines) at their fixed byte
// offsets (+0x38, +0xB0, +0x1D8 ... +0xE78). There are NO explicit non-vtable member stores
// in the asm at all -- no scalar or sentinel initialisations. On the host each of those
// vtable pointers is written implicitly by the base ctor chain and the members' own default
// constructors (mirrors the committed InstantResultsState sibling), so the reconstructed
// body carries no explicit statements.
CompletedGame::CompletedGame()
{
}

// ---- Construct --------------------------------------------------------------------
// cpp:80 -- Assert the fsm pointer, run the base Construct, then seed every
// state word. Note mResourceToLoad is seeded with the id ONE PAST the last real resource
// (E_GUI_RESOURCEID_NUM), which is this screen's "no resource selected" marker -- the
// presentation ladders overwrite it before the first EnsureResourceIsLoaded.
void CompletedGame::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
{
    CGS_ASSERT(lpFsm != 0, "Invalid ScriptedFsm ptr");   // cpp:82

    CgsGui::State::Construct(liId, lpFsm);

    meCompletedGameState     = E_COMPLETEDGAMESTATE_NONE;
    meBurnoutSubstate        = E_BURNOUT_PRESENTATION_INVALID;
    meEliteSubstate          = E_ELITE_PRESENTATION_INVALID;
    meFirstFinishedSubstate  = E_FINISHED_FIRST_PRESENTATION_INVALID;
    mfTimeRemaining          = 0.0f;
    meSecondFinishedSubstate = E_FINISHED_SECOND_PRESENTATION_INVALID;
    mpGuiCache               = 0;
    mpProfile                = 0;
    mbStartedUpgradeTransOut = false;
    mResourceToLoad.muId     = static_cast<u32>(E_GUI_RESOURCEID_NUM);
    mResourceToLoad.meType   = CgsGui::E_GUI_RESOURCETYPE_APT;
}

// ---- OnEnter ----------------------------------------------------------------------
// cpp:112 -- Register the six observed events, Construct the six embedded
// components against this state's interface, and reset the whole state machine. Note
// meCompletionType is reset here but NOT in Construct, and mbStartedUpgradeTransOut is
// reset in Construct but NOT here -- both asymmetries are the console's.
void CompletedGame::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

    mLicense.Construct(KAC_LICENSE_COMPONENT_NAME, mpStateInterface, 0);
    mPhotoBoothComponent.Construct(KAC_PHOTO_COMPONENT_NAME, mpStateInterface,
                                   ButtonIconComponent::E_PADBUTTON_BACK,
                                   ButtonIconComponent::E_PADBUTTON_SELECT,
                                   PhotoBoothComponent::E_TAKEPHOTOSTRING_TAKEPHOTO,
                                   PhotoBoothComponent::E_BACKSTRING_USEOLDPHOTO, 0);
    mUpgradeStateAnimator.Construct(KAC_UPGRADE_ANIM_NAME, mpStateInterface, 0);
    mUpgradeTextfield.Construct(KAC_UPGRADE_TEXTFIELD_NAME, mpStateInterface, 0);
    mRewardCarsStateAnimator.Construct(KAC_CARS_ANIM_NAME, mpStateInterface, 0);
    mRewardCarsTextfield.Construct(KAC_CARS_TEXTFIELD_NAME, mpStateInterface, 0);

    mfTimeRemaining          = 0.0f;
    meCompletedGameState     = E_COMPLETEDGAMESTATE_NONE;
    meCompletionType         = E_COMPLETION_NOT_SET;
    meBurnoutSubstate        = E_BURNOUT_PRESENTATION_INVALID;
    meEliteSubstate          = E_ELITE_PRESENTATION_INVALID;
    meFirstFinishedSubstate  = E_FINISHED_FIRST_PRESENTATION_INVALID;
    meSecondFinishedSubstate = E_FINISHED_SECOND_PRESENTATION_INVALID;
    mpGuiCache               = 0;
    mpProfile                = 0;
    mResourceToLoad.muId     = static_cast<u32>(E_GUI_RESOURCEID_NUM);
    mResourceToLoad.meType   = CgsGui::E_GUI_RESOURCETYPE_APT;
}

// ---- OnLeave ----------------------------------------------------------------------
// cpp:163 -- Unbind the apt movie by re-issuing PlayAptMovie with the empty
// name at the same level, hand both streaming components' resources back, drop the six
// observed channels, and clear the two state words plus the cache pointer. mpProfile is
// deliberately NOT cleared here -- that too is the console's.
void CompletedGame::OnLeave()
{
    mpStateInterface->PlayAptMovie("", KI_MOVIE_LEVEL);

    mLicense.ReleaseResources();
    mPhotoBoothComponent.ReleaseResources();

    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

    meCompletedGameState = E_COMPLETEDGAMESTATE_NONE;
    meCompletionType     = E_COMPLETION_NOT_SET;
    mpGuiCache           = 0;
}

// ---- Update -----------------------------------------------------------------------
// cpp:191 -- Drain the in-queue, then either PICK the presentation (the
// first frame with a cache) or PUMP the one already picked, and finally keep both
// picture feeds alive. The tail pair runs on EVERY path, including the asserting ones.
void CompletedGame::Update()
{
    HandleIncomingEvents();

    switch (meCompletedGameState)
    {
    case E_COMPLETEDGAMESTATE_NONE:
    {
        // Nothing can be decided until event 64 has handed the cache over.
        if (mpGuiCache == 0)
        {
            break;
        }

        const GuiEventOfflinePostEvent::OfflinePostEventData* const lpLastResults =
            &mpGuiCache->GetOfflinePostEventData();
        CGS_ASSERT(lpLastResults != 0, "lpLastResults");                      // cpp:203

        const s32 liBurnoutLicenseRank = static_cast<s32>(
            mpGuiCache->GetWorldDataController()->GetProgressionData()
                ->GetProgressionRankCount()) - 1;
        CGS_ASSERT(liBurnoutLicenseRank > 0, "liBurnoutLicenseRank > 0");     // cpp:208

        // Every presentation shows the DMV upgrade photo frame.
        mPhotoBoothComponent.SetPhotoResourceId(
            static_cast<u32>(E_GUI_RESOURCEID_APT_COMPONENT_PHOTOBOOTH_DMV_UPGRADE));
        meCompletedGameState = E_COMPLETEDGAMESTATE_RUNNING;

        // The card always opens on the rank the player came FROM, with the points line on.
        const s32 liOldRank = lpLastResults->miPlayerOldRank;

        if (lpLastResults->miPlayerNewRank == liBurnoutLicenseRank)
        {
            meCompletionType  = E_COMPLETION_WON_BURNOUT_LICENSE;
            meBurnoutSubstate = E_BURNOUT_PRESENTATION_LOAD_RESOURCES;
            mLicense.SetPlayerInfo(mpGuiCache->GetPlayerName(), false, false, liOldRank,
                                   static_cast<s32>(mpGuiCache->GetLicencePointsToNextRank()),
                                   false, true);
            break;
        }

        if (!mpProfile->GetSeenEliteCompletionSequence())
        {
            meEliteSubstate  = E_ELITE_CAR_PRESENTATION_LOAD_RESOURCES;
            meCompletionType = E_COMPLETION_WON_ELITE_LICENSE;
            mLicense.SetPlayerInfo(mpGuiCache->GetPlayerName(), true, false, liOldRank,
                                   static_cast<s32>(mpGuiCache->GetLicencePointsToNextRank()),
                                   false, true);
            break;
        }

        // The cache's post-event-presentation-suppressed byte tells the two halves of the
        // 100% sequence apart. (The console also carries a third arm here -- the byte
        // being neither 0 nor 1 fires "We're in the completed game state but we don't
        // know why!!?!  Talk to Ian and Dave." and exits -- but the cache declares this
        // slot a bool, so that arm is unreachable by construction and is not spelled.)
        if (mpGuiCache->IsPostEventPresentationSuppressed())
        {
            CGS_ASSERT(mpProfile->AreGoldCarsUnlocked(),
                       "We think we've just hit 100% (second half), but the profile is not "
                       "reporting 100% completed.\n");                        // cpp:283
            CGS_ASSERT(!mpProfile->GetSeen100PercentCompletionSequence(),
                       "We think we've just hit 100% (second half), but we've already seen "
                       "the whole 100% complete sequence.\n");                // cpp:285

            meSecondFinishedSubstate = E_FINISHED_SECOND_PRESENTATION_LOAD_RESOURCES;
            meCompletionType         = E_COMPLETION_FINISHED_GAME;
            mLicense.SetPlayerInfo(mpGuiCache->GetPlayerName(), true, true, liOldRank,
                                   static_cast<s32>(mpGuiCache->GetLicencePointsToNextRank()),
                                   false, true);
            break;
        }

        CGS_ASSERT(mpProfile->AreGoldCarsUnlocked(),
                   "We think we've just hit 100%, but the profile is not reporting 100% "
                   "completed.\n");                                           // cpp:254
        CGS_ASSERT(!mpProfile->GetSeen100PercentCompletionSequence(),
                   "We think we've just hit 100%, but we've already seen the whole 100% "
                   "complete sequence.\n");                                   // cpp:256

        GuiEvent100PerCentComplete lCompleteEvent;
        lCompleteEvent.maData[0] = 1;
        mpStateInterface->OutputGuiEvent(lCompleteEvent);

        meFirstFinishedSubstate = E_FINISHED_FIRST_LICENSE_PRESENTATION_LOAD_RESOURCES;
        meCompletionType        = E_COMPLETION_JUST_HIT_100_PERCENT;
        mLicense.SetPlayerInfo(mpGuiCache->GetPlayerName(), true, false, liOldRank,
                               static_cast<s32>(mpGuiCache->GetLicencePointsToNextRank()),
                               false, true);
        break;
    }

    case E_COMPLETEDGAMESTATE_RUNNING:
        switch (meCompletionType)
        {
        case E_COMPLETION_WON_BURNOUT_LICENSE:
            UpdateBurnoutLicensePresentation();
            break;
        case E_COMPLETION_WON_ELITE_LICENSE:
            UpdateEliteLicensePresentation();
            break;
        case E_COMPLETION_JUST_HIT_100_PERCENT:
            UpdateOneHundredPercentPartOnePresentation();
            break;
        case E_COMPLETION_FINISHED_GAME:
            UpdateOneHundredPercentPartTwoPresentation();
            break;
        default:
            FireUnexpectedStateAssert("Invalid completion state - ",
                                      static_cast<s32>(meCompletionType), "\n");  // cpp:341
            break;
        }
        break;

    case E_COMPLETEDGAMESTATE_DONE:
        TriggerExitState();
        break;

    default:
        FireUnexpectedStateAssert("Unhandled CompletedGameState ",
                                  static_cast<s32>(meCompletedGameState),
                                  " in CompletedGame::Update()\n");             // cpp:355
        break;
    }

    // Both picture feeds are pumped every frame, on every path above.
    mLicense.SendPlayerPictureEvent();
    mPhotoBoothComponent.SendPlayerPictureEvent();
}

// ---- UpdateBurnoutLicensePresentation ----------------------------------------------
// cpp:373 -- The shortest ladder: mount the upgrade movie, caption it with
// the burnout "next step" line, run the licence card's rank-upgrade animation for four
// seconds (starting the background's transition-out one second early), then hand the
// card's resources back once it has finished hiding.
void CompletedGame::UpdateBurnoutLicensePresentation()
{
    switch (meBurnoutSubstate)
    {
    case E_BURNOUT_PRESENTATION_LOAD_RESOURCES:
        mResourceToLoad.muId = KU_UPGRADE_MOVIE_RESOURCE;
        if (mpGuiCache->EnsureResourceIsLoaded(mResourceToLoad)
            && mLicense.EnsureResourcesAreLoaded())
        {
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            mLicense.AppendExpectedAptComponent(E_GUIFLOW_SCREEN);
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   mUpgradeStateAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   mUpgradeTextfield.GetName());
            mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_UPGRADE_MOVIE_RESOURCE],
                                           KI_MOVIE_LEVEL);
            mLicense.OnLoad();
            meBurnoutSubstate = E_BURNOUT_PRESENTATION_INITIALISE_COMPONENTS;
        }
        break;

    case E_BURNOUT_PRESENTATION_INITIALISE_COMPONENTS:
        if (mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            mUpgradeStateAnimator.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                        "upgradeBurnout", false);
            mUpgradeTextfield.SetLocalisedText("COMPLETION_SEQUENCE_BURNOUT_NEXT_STEP",
                                               CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
            mLicense.ShowUpgradedLicense(KF_LICENSE_TICK_UP_DURATION, true);
            mfTimeRemaining   = KF_NEXT_STEP_DURATION;
            meBurnoutSubstate = E_BURNOUT_PRESENTATION_RUNNING;
        }
        break;

    case E_BURNOUT_PRESENTATION_RUNNING:
    {
        const f32 lfTimeRemaining = mfTimeRemaining - mpGuiCache->GetTimeStep();
        mfTimeRemaining = lfTimeRemaining;
        if (lfTimeRemaining > 0.0f)
        {
            mLicense.Update();
            if (!mbStartedUpgradeTransOut && mfTimeRemaining <= KF_BACKGROUND_HEADSTART)
            {
                mbStartedUpgradeTransOut = true;
                mUpgradeStateAnimator.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                            "upgradeBurnoutOut", false);
            }
        }
        else
        {
            mLicense.HideLicense();
            meBurnoutSubstate = E_BURNOUT_PRESENTATION_CLEANUP;
        }
        break;
    }

    case E_BURNOUT_PRESENTATION_CLEANUP:
        if (!mLicense.IsVisible())
        {
            mLicense.ReleaseResources();
            meCompletedGameState = E_COMPLETEDGAMESTATE_DONE;
        }
        break;

    default:
        FireUnexpectedStateAssert(KAC_INVALID_SUBSTATE_PREFIX,
                                  static_cast<s32>(meBurnoutSubstate),
                                  KAC_INVALID_SUBSTATE_SUFFIX);               // cpp:467
        break;
    }
}

// ---- UpdateEliteLicensePresentation ------------------------------------------------
// cpp:481 -- Two pages back to back. First the GOLD reward-car page on its
// own movie (with the gold-award audio sting), which leaves itself when the apt
// transition completes -- see HandleIncomingEvents' transition arm -- and then unloads
// its movie; then the shared upgrade movie for the elite "next step" caption and the
// licence card's score reveal. The last page also marks the sequence seen and asks for
// an autosave.
void CompletedGame::UpdateEliteLicensePresentation()
{
    switch (meEliteSubstate)
    {
    case E_ELITE_CAR_PRESENTATION_LOAD_RESOURCES:
        mResourceToLoad.muId = KU_GOLD_MOVIE_RESOURCE;
        if (mpGuiCache->EnsureResourceIsLoaded(mResourceToLoad)
            && mLicense.EnsureResourcesAreLoaded())
        {
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            mLicense.AppendExpectedAptComponent(E_GUIFLOW_SCREEN);
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   mRewardCarsStateAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   mRewardCarsTextfield.GetName());
            mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_GOLD_MOVIE_RESOURCE],
                                           KI_MOVIE_LEVEL);
            mLicense.OnLoad();
            meEliteSubstate = E_ELITE_CAR_PRESENTATION_INITIALISE_COMPONENTS;
        }
        break;

    case E_ELITE_CAR_PRESENTATION_INITIALISE_COMPONENTS:
        if (mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            GuiEventAudioGenericSequence lAudioEvent;
            *reinterpret_cast<u32*>(lAudioEvent.maData) =
                static_cast<u32>(CgsSound::Playback::Name::MakeHash("GoldAward"));
            mpStateInterface->OutputGuiEvent(lAudioEvent);

            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            mRewardCarsStateAnimator.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                           "transIn", false);
            mRewardCarsTextfield.SetLocalisedText("COMPLETION_SEQUENCE_GOLD",
                                                  CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
            mLicense.ShowLicense(true);
            mfTimeRemaining = KF_CARS_UNLOCKED_DURATION;
            meEliteSubstate = E_ELITE_CAR_PRESENTATION_RUNNING;
        }
        break;

    case E_ELITE_CAR_PRESENTATION_RUNNING:
        mfTimeRemaining -= mpGuiCache->GetTimeStep();
        mLicense.Update();
        if (mfTimeRemaining <= 0.0f)
        {
            mRewardCarsStateAnimator.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                           "transOut", false);
            meEliteSubstate = E_ELITE_CAR_PRESENTATION_LEAVING;
        }
        break;

    case E_ELITE_CAR_PRESENTATION_LEAVING:
        // Inert: the transition-complete arm of HandleIncomingEvents drops the movie and
        // advances this ladder.
        break;

    case E_ELITE_CAR_PRESENTATION_CLEANUP:
        if (mpGuiCache->EnsureResourceIsUnloaded(mResourceToLoad))
        {
            meEliteSubstate = E_ELITE_LICENSE_PRESENTATION_LOAD_RESOURCES;
        }
        break;

    case E_ELITE_LICENSE_PRESENTATION_LOAD_RESOURCES:
        mResourceToLoad.muId  = KU_UPGRADE_MOVIE_RESOURCE;
        miNumResourcesToLoad  = 1;
        if (mpGuiCache->EnsureResourceIsLoaded(mResourceToLoad))
        {
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   mUpgradeStateAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   mUpgradeTextfield.GetName());
            mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_UPGRADE_MOVIE_RESOURCE],
                                           KI_MOVIE_LEVEL);
            meEliteSubstate = E_ELITE_LICENSE_PRESENTATION_INITIALISE_COMPONENTS;
        }
        break;

    case E_ELITE_LICENSE_PRESENTATION_INITIALISE_COMPONENTS:
        if (mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            mUpgradeStateAnimator.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                        "upgradeElite", false);
            mUpgradeTextfield.SetLocalisedText("COMPLETION_SEQUENCE_ELITE_NEXT_STEP",
                                               CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
            mLicense.ShowScore();
            mfTimeRemaining = KF_NEXT_STEP_DURATION;
            meEliteSubstate = E_ELITE_LICENSE_PRESENTATION_RUNNING;
        }
        break;

    case E_ELITE_LICENSE_PRESENTATION_RUNNING:
    {
        const f32 lfTimeRemaining = mfTimeRemaining - mpGuiCache->GetTimeStep();
        mfTimeRemaining = lfTimeRemaining;
        if (lfTimeRemaining > 0.0f)
        {
            mLicense.Update();
            if (!mbStartedUpgradeTransOut && mfTimeRemaining <= KF_BACKGROUND_HEADSTART)
            {
                mbStartedUpgradeTransOut = true;
                mUpgradeStateAnimator.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                            "upgradeEliteOut", false);
            }
        }
        else
        {
            mLicense.HideLicense();
            meEliteSubstate = E_ELITE_LICENSE_PRESENTATION_CLEANUP;
        }
        break;
    }

    case E_ELITE_LICENSE_PRESENTATION_CLEANUP:
    {
        mpProfile->SetSeenEliteCompletionSequence();

        GuiAutosaveRequestEvent lAutosaveEvent;
        lAutosaveEvent.maData[0] = 1;
        mpStateInterface->OutputGuiEvent(lAutosaveEvent);

        if (!mLicense.IsVisible())
        {
            mLicense.ReleaseResources();
            meCompletedGameState = E_COMPLETEDGAMESTATE_DONE;
        }
        break;
    }

    default:
        FireUnexpectedStateAssert(KAC_INVALID_SUBSTATE_PREFIX,
                                  static_cast<s32>(meBurnoutSubstate),
                                  KAC_INVALID_SUBSTATE_SUFFIX);               // cpp:666
        break;
    }
}

// ---- UpdateOneHundredPercentPartOnePresentation ------------------------------------
// cpp:680 -- The long ladder: the upgrade movie captioned with the final
// line, an optional voice-over when the event did not also rank the player up, an
// OPTIONAL photo-booth detour (only when a camera is attached -- without one the ladder
// jumps straight past both photo pages), and then the final animated rank-up.
void CompletedGame::UpdateOneHundredPercentPartOnePresentation()
{
    switch (meFirstFinishedSubstate)
    {
    case E_FINISHED_FIRST_LICENSE_PRESENTATION_LOAD_RESOURCES:
        mResourceToLoad.muId = KU_UPGRADE_MOVIE_RESOURCE;
        if (mpGuiCache->EnsureResourceIsLoaded(mResourceToLoad)
            && mLicense.EnsureResourcesAreLoaded()
            && mPhotoBoothComponent.EnsureResourcesAreLoaded())
        {
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            mLicense.AppendExpectedAptComponent(E_GUIFLOW_SCREEN);
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   mUpgradeStateAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   mUpgradeTextfield.GetName());
            mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_UPGRADE_MOVIE_RESOURCE],
                                           KI_MOVIE_LEVEL);
            mLicense.OnLoad();
            mPhotoBoothComponent.OnLoad();
            meFirstFinishedSubstate = E_FINISHED_FIRST_LICENSE_PRESENTATION_INITIALISE_COMPONENTS;
        }
        break;

    case E_FINISHED_FIRST_LICENSE_PRESENTATION_INITIALISE_COMPONENTS:
        if (mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            mUpgradeStateAnimator.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                        "Complete", false);
            mUpgradeTextfield.SetLocalisedText("COMPLETION_SEQUENCE_FINAL",
                                               CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
            mLicense.ShowLicense(true);
            mfTimeRemaining = KF_NEXT_STEP_DURATION;

            // Only when the finishing event did NOT also rank the player up: the combined
            // "elite then 100%" line would otherwise talk over the rank-up presentation.
            if (!mpGuiCache->GetOfflinePostEventData().mbHasRankedUp)
            {
                GuiEventAudioVoiceOver lVoiceOverEvent;
                *reinterpret_cast<u32*>(lVoiceOverEvent.maData) =
                    static_cast<u32>(CgsSound::Playback::Name::MakeHash("Elite_Then_100_Percent"));
                mpStateInterface->OutputGuiEvent(lVoiceOverEvent);
            }

            meFirstFinishedSubstate = E_FINISHED_FIRST_LICENSE_PRESENTATION_RUNNING;
        }
        break;

    case E_FINISHED_FIRST_LICENSE_PRESENTATION_RUNNING:
        mfTimeRemaining -= mpGuiCache->GetTimeStep();
        if (mfTimeRemaining <= 0.0f)
        {
            // No camera attached -> skip both photo pages entirely and restart the page
            // timer for the rank-up.
            if (mpGuiCache->GetCamStatus() == 0)
            {
                mfTimeRemaining         = KF_NEXT_STEP_DURATION;
                meFirstFinishedSubstate = E_FINISHED_FIRST_UPGRADE_PRESENTATION_BEGIN;
                break;
            }

            mLicense.HideLicense();
            mUpgradeStateAnimator.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                        "takePhoto", false);
            mPhotoBoothComponent.ShowComponent(false);
            meFirstFinishedSubstate = E_FINISHED_FIRST_TAKING_PHOTO;
        }
        break;

    case E_FINISHED_FIRST_TAKING_PHOTO:
        // The photo booth owns the frame until the camera goes away (HandleControllerInput
        // is what normally moves this on, via Select/Cancel).
        if (mpGuiCache->GetCamStatus() == 0)
        {
            mPhotoBoothComponent.HideComponent(true);
            meFirstFinishedSubstate = E_FINISHED_FIRST_TAKING_PHOTO_CLEANUP;
        }
        break;

    case E_FINISHED_FIRST_TAKING_PHOTO_CLEANUP:
        mLicense.ShowLicense(true);
        mUpgradeStateAnimator.AddOutputAptViewState(KAC_APT_TRANSITION, "Complete", false);
        mUpgradeTextfield.SetLocalisedText("COMPLETION_SEQUENCE_FINAL",
                                           CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
        mfTimeRemaining         = KF_NEXT_STEP_DURATION;
        meFirstFinishedSubstate = E_FINISHED_FIRST_UPGRADE_PRESENTATION_BEGIN;
        break;

    case E_FINISHED_FIRST_UPGRADE_PRESENTATION_BEGIN:
        mfTimeRemaining -= mpGuiCache->GetTimeStep();
        if (mfTimeRemaining <= 0.0f)
        {
            mLicense.RankUp(0.0f, 0.0f, false);
            mfTimeRemaining         = KF_FINAL_UPGRADED_DURATION;
            meFirstFinishedSubstate = E_FINISHED_FIRST_UPGRADE_PRESENTATION_RUNNING;
        }
        break;

    case E_FINISHED_FIRST_UPGRADE_PRESENTATION_RUNNING:
    {
        const f32 lfTimeRemaining = mfTimeRemaining - mpGuiCache->GetTimeStep();
        mfTimeRemaining = lfTimeRemaining;
        if (lfTimeRemaining > 0.0f)
        {
            mLicense.Update();
            if (!mbStartedUpgradeTransOut && mfTimeRemaining <= KF_BACKGROUND_HEADSTART)
            {
                mbStartedUpgradeTransOut = true;
                mUpgradeStateAnimator.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                            "CompleteOut", false);
            }
        }
        else
        {
            if (mLicense.IsVisible())
            {
                mLicense.HideLicense();
            }
            meFirstFinishedSubstate = E_FINISHED_FIRST_UPGRADE_PRESENTATION_CLEANUP;
        }
        break;
    }

    case E_FINISHED_FIRST_UPGRADE_PRESENTATION_CLEANUP:
        if (!mLicense.IsVisible())
        {
            mLicense.ReleaseResources();
            meCompletedGameState = E_COMPLETEDGAMESTATE_DONE;
        }
        break;

    default:
        FireUnexpectedStateAssert(KAC_INVALID_SUBSTATE_PREFIX,
                                  static_cast<s32>(meBurnoutSubstate),
                                  KAC_INVALID_SUBSTATE_SUFFIX);               // cpp:859
        break;
    }
}

// ---- UpdateOneHundredPercentPartTwoPresentation ------------------------------------
// cpp:873 -- The platinum reward-car page: its own movie, the platinum audio
// sting, eight seconds of licence card, then the transition out. The LEAVING rung is
// driven by HandleIncomingEvents, and the cleanup rung marks the 100% sequence seen,
// asks for an autosave and posts the 100%-complete notification a second time (with a
// ZERO payload byte this time, against part one's 1).
void CompletedGame::UpdateOneHundredPercentPartTwoPresentation()
{
    switch (meSecondFinishedSubstate)
    {
    case E_FINISHED_SECOND_PRESENTATION_LOAD_RESOURCES:
        mResourceToLoad.muId = KU_PLATINUM_MOVIE_RESOURCE;
        if (mpGuiCache->EnsureResourceIsLoaded(mResourceToLoad)
            && mLicense.EnsureResourcesAreLoaded())
        {
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            mLicense.AppendExpectedAptComponent(E_GUIFLOW_SCREEN);
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   mRewardCarsStateAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   mRewardCarsTextfield.GetName());
            mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_PLATINUM_MOVIE_RESOURCE],
                                           KI_MOVIE_LEVEL);
            mLicense.OnLoad();
            meSecondFinishedSubstate = E_FINISHED_SECOND_PRESENTATION_INITIALISE_COMPONENTS;
        }
        break;

    case E_FINISHED_SECOND_PRESENTATION_INITIALISE_COMPONENTS:
        if (mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            GuiEventAudioGenericSequence lAudioEvent;
            *reinterpret_cast<u32*>(lAudioEvent.maData) =
                static_cast<u32>(CgsSound::Playback::Name::MakeHash("PlatinumAward"));
            mpStateInterface->OutputGuiEvent(lAudioEvent);

            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            mRewardCarsStateAnimator.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                           "transIn", false);
            mRewardCarsTextfield.SetLocalisedText("COMPLETION_SEQUENCE_PLATINUM",
                                                  CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
            mLicense.ShowLicense(true);
            mfTimeRemaining          = KF_CARS_UNLOCKED_DURATION;
            meSecondFinishedSubstate = E_FINISHED_SECOND_PRESENTATION_RUNNING;
        }
        break;

    case E_FINISHED_SECOND_PRESENTATION_RUNNING:
        mfTimeRemaining -= mpGuiCache->GetTimeStep();
        mLicense.Update();
        if (mfTimeRemaining <= 0.0f)
        {
            mRewardCarsStateAnimator.AddOutputAptViewState(KAC_APT_TRANSITION,
                                                           "transOut", false);
            mLicense.HideLicense();
            meSecondFinishedSubstate = E_FINISHED_SECOND_PRESENTATION_LEAVING;
        }
        break;

    case E_FINISHED_SECOND_PRESENTATION_LEAVING:
        // Inert: the transition-complete arm of HandleIncomingEvents advances this ladder.
        break;

    case E_FINISHED_SECOND_PRESENTATION_CLEANUP:
        if (mLicense.IsVisible())
        {
            break;
        }

        mLicense.ReleaseResources();

        CGS_ASSERT(mpProfile != 0, "NULL != mpProfile");                          // cpp:963
        CGS_ASSERT(meCompletionType == E_COMPLETION_FINISHED_GAME,
                   "E_COMPLETION_FINISHED_GAME == meCompletionType");             // cpp:964

        mpProfile->SetSeen100PercentCompletionSequence();

        {
            GuiAutosaveRequestEvent lAutosaveEvent;
            lAutosaveEvent.maData[0] = 1;
            mpStateInterface->OutputGuiEvent(lAutosaveEvent);

            GuiEvent100PerCentComplete lCompleteEvent;
            lCompleteEvent.maData[0] = 0;
            mpStateInterface->OutputGuiEvent(lCompleteEvent);
        }

        meCompletedGameState = E_COMPLETEDGAMESTATE_DONE;
        break;

    default:
        FireUnexpectedStateAssert(KAC_INVALID_SUBSTATE_PREFIX,
                                  static_cast<s32>(meBurnoutSubstate),
                                  KAC_INVALID_SUBSTATE_SUFFIX);                   // cpp:984
        break;
    }
}

// ---- HandleIncomingEvents -----------------------------------------------------------
// cpp:998 -- Drain the state in-queue. The 64 and 350 arms are what make the
// screen work at all -- Update does nothing until the cache arrives, and the completion
// decision reads the profile -- and both latch ONCE. The apt-trigger arm is also the only
// thing that moves the two "LEAVING" rungs on.
void CompletedGame::HandleIncomingEvents()
{
    StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;

    for (s32 liEventType = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
         lpEvent != 0;
         liEventType = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        switch (liEventType)
        {
        case KI_EVENT_PROGRESSION_PROFILE:
            if (mpProfile == 0)
            {
                mpProfile = *reinterpret_cast<BrnProgression::Profile* const*>(lpEvent);
                mLicense.SetProfilePointer(mpProfile);
                CGS_ASSERT(mpProfile != 0, "NULL != lpProfile");   // the inlined photo-booth setter
                mPhotoBoothComponent.SetProfilePointer(mpProfile);
            }
            break;

        case KI_EVENT_CONTROLLER_INPUT_PRESSED:
            HandleControllerInput(lpEvent, KI_EVENT_CONTROLLER_INPUT_PRESSED);
            break;

        case KI_EVENT_APT_TRIGGER:
        {
            CGS_ASSERT(lpEvent != 0, "lpEvent");                                  // cpp:1013

            const CgsGui::GuiEventAptTriggerPayload* const lpAptTrigger =
                reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent);

            // The licence owns a family of sub-clips whose names all CONTAIN its own, so
            // the match is strstr, not strcmp (the same test the results screen uses).
            if (lpAptTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD)
            {
                if (std::strstr(lpAptTrigger->mpacComponentName, mLicense.GetName()) != 0)
                {
                    mLicense.HandleAptLoadTriggers(lpAptTrigger);
                }
            }
            else if (lpAptTrigger->meEventType
                         == CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE)
            {
                if (std::strstr(lpAptTrigger->mpacComponentName, mLicense.GetName()) != 0)
                {
                    mLicense.HandleAptTransitionTriggers(lpAptTrigger);
                }
                else if (meEliteSubstate == E_ELITE_CAR_PRESENTATION_LEAVING)
                {
                    // The reward-car page has finished transitioning out: drop its movie
                    // and go and unload it.
                    mpStateInterface->PlayAptMovie("", KI_MOVIE_LEVEL);
                    meEliteSubstate = E_ELITE_CAR_PRESENTATION_CLEANUP;
                }
                else if (meSecondFinishedSubstate == E_FINISHED_SECOND_PRESENTATION_LEAVING)
                {
                    mpStateInterface->PlayAptMovie("", KI_MOVIE_LEVEL);
                    meSecondFinishedSubstate = E_FINISHED_SECOND_PRESENTATION_CLEANUP;
                }
            }
            break;
        }

        case KI_EVENT_GUI_CACHE:
            if (mpGuiCache == 0)
            {
                mpGuiCache = *reinterpret_cast<GuiCache* const*>(lpEvent);
                mLicense.SetCachePointer(mpGuiCache);
                mPhotoBoothComponent.SetCachePointer(mpGuiCache);
            }
            break;

        case KI_EVENT_PERCENTAGE_COMPLETE:
            // The console reads the payload's word 49 (+0xC4) as the percentage.
            mLicense.SetPercentageComplete(
                static_cast<s32>(reinterpret_cast<const u32*>(lpEvent)[49]));
            break;

        case KI_EVENT_COMPRESSED_STILL_IMAGE:
            mPhotoBoothComponent.HandleCompressedStillImageEvent(lpEvent);
            break;

        default:
            FireUnexpectedStateAssert("Unhandled event ", liEventType,
                                      " in CompletedGame::Update()\n");           // cpp:1104
            break;
        }
    }
}

// ---- HandleControllerInput ----------------------------------------------------------
// cpp:1122 -- The only controller input this screen answers: the photo
// booth's take/cancel pair, and only while the 100%-part-one ladder is on its photo page.
// Either component answer that returns true advances the ladder to the photo cleanup.
void CompletedGame::HandleControllerInput(const CgsModule::Event* lpEvent, s32 liEventType)
{
    CGS_ASSERT(lpEvent != 0, "lpEvent");                                          // cpp:1124

    if (liEventType != KI_EVENT_CONTROLLER_INPUT_PRESSED)
    {
        return;
    }

    switch (meCompletedGameState)
    {
    case E_COMPLETEDGAMESTATE_NONE:
        break;

    case E_COMPLETEDGAMESTATE_RUNNING:
    {
        const s32 liAction =
            *reinterpret_cast<const s32*>(reinterpret_cast<const u8*>(lpEvent) + 4);

        bool lbHandled = false;
        if (liAction == KI_ACTION_SELECT)
        {
            if (meFirstFinishedSubstate != E_FINISHED_FIRST_TAKING_PHOTO)
            {
                return;
            }
            lbHandled = mPhotoBoothComponent.Select();
        }
        else
        {
            if (liAction != KI_ACTION_BACK
                || meFirstFinishedSubstate != E_FINISHED_FIRST_TAKING_PHOTO)
            {
                return;
            }
            lbHandled = mPhotoBoothComponent.Cancel();
        }

        if (lbHandled)
        {
            meFirstFinishedSubstate = E_FINISHED_FIRST_TAKING_PHOTO_CLEANUP;
        }
        break;
    }

    default:
        FireUnexpectedStateAssert("Unhandled CompletedGame state ",
                                  static_cast<s32>(meCompletedGameState),
                                  " in CompletedGame::HandleControllerInput()\n");  // cpp:1181
        break;
    }
}

// ---- TriggerExitState ---------------------------------------------------------------
// cpp:1203 -- Post the post-event teardown, then leave: the 100%-part-one
// completion is the one that rolls the credits, everything else goes back to the front
// end.
void CompletedGame::TriggerExitState()
{
    GuiEventPostEventTeardown lTeardownEvent;
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lTeardownEvent), KI_CHANNEL_GUI_OUT, 16);

    SendStateEvent(meCompletionType == E_COMPLETION_JUST_HIT_100_PERCENT ? "TO_CREDITS"
                                                                        : "ADVANCE");
}
}
