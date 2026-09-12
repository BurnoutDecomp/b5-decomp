#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiTextField.h"                                // BrnGui::TextField (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"   // BrnGui::AnimationComponent (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnLicenseComponent.h"     // BrnGui::LicenseComponent (by value)
#include "GameSource/Gui/Flow/Screen/Components/BrnPhotoBoothComponent.h"  // BrnGui::PhotoBoothComponent (by value)

namespace BrnProgression { class Profile; }   // mpProfile (pointer only)

// BrnGui::CompletedGame - the offline post-event "completed game" presentation flow
// state: the end-of-game celebration the results screen hands over to. One outer state
// machine (ECompletedGameState) picks ONE of four presentations from what the profile and
// the last event results imply (burnout licence won, elite licence won, just hit 100%
// part one, just hit 100% part two), and each presentation runs its own substate ladder
// over the shared licence card, photo booth, two animators and two text fields. Layout,
// enums and method set are the reference shape (BrnCompletedGame.h); every literal is attested.
namespace BrnGui
{
    class GuiCache;

    struct CompletedGame : public CgsGui::State
    {
        CompletedGame();

        // ---- header h:82 ------------------------------------------------------------
        enum ECompletedGameState
        {
            E_COMPLETEDGAMESTATE_NONE    = 0,
            E_COMPLETEDGAMESTATE_RUNNING = 1,
            E_COMPLETEDGAMESTATE_DONE    = 2,
            E_COMPLETEDGAMESTATE_COUNT   = 3,
        };

        // ---- header h:91 ------------------------------------------------------------
        enum ECompletionType
        {
            E_COMPLETION_NOT_SET              = 0,
            E_COMPLETION_WON_BURNOUT_LICENSE  = 1,
            E_COMPLETION_WON_ELITE_LICENSE    = 2,
            E_COMPLETION_JUST_HIT_100_PERCENT = 3,
            E_COMPLETION_FINISHED_GAME        = 4,
            E_COMPLETED_COUNT                 = 5,
        };

        // ---- header h:103 -----------------------------------------------------------
        enum EBurnoutSubstates
        {
            E_BURNOUT_PRESENTATION_INVALID               = 0,
            E_BURNOUT_PRESENTATION_LOAD_RESOURCES        = 1,
            E_BURNOUT_PRESENTATION_INITIALISE_COMPONENTS = 2,
            E_BURNOUT_PRESENTATION_RUNNING               = 3,
            E_BURNOUT_PRESENTATION_CLEANUP               = 4,
            E_BURNOUT_PRESENTATION_COUNT                 = 5,
        };

        // ---- header h:115 -----------------------------------------------------------
        enum EEliteSubstates
        {
            E_ELITE_PRESENTATION_INVALID                       = 0,
            E_ELITE_CAR_PRESENTATION_LOAD_RESOURCES            = 1,
            E_ELITE_CAR_PRESENTATION_INITIALISE_COMPONENTS     = 2,
            E_ELITE_CAR_PRESENTATION_RUNNING                   = 3,
            E_ELITE_CAR_PRESENTATION_LEAVING                   = 4,
            E_ELITE_CAR_PRESENTATION_CLEANUP                   = 5,
            E_ELITE_LICENSE_PRESENTATION_LOAD_RESOURCES        = 6,
            E_ELITE_LICENSE_PRESENTATION_INITIALISE_COMPONENTS = 7,
            E_ELITE_LICENSE_PRESENTATION_RUNNING               = 8,
            E_ELITE_LICENSE_PRESENTATION_CLEANUP               = 9,
            E_ELITE_PRESENTATION_COUNT                         = 10,
        };

        // ---- header h:133 -----------------------------------------------------------
        enum EFinishedFirstPartSubstates
        {
            E_FINISHED_FIRST_PRESENTATION_INVALID                       = 0,
            E_FINISHED_FIRST_LICENSE_PRESENTATION_LOAD_RESOURCES        = 1,
            E_FINISHED_FIRST_LICENSE_PRESENTATION_INITIALISE_COMPONENTS = 2,
            E_FINISHED_FIRST_LICENSE_PRESENTATION_RUNNING               = 3,
            E_FINISHED_FIRST_TAKING_PHOTO                               = 4,
            E_FINISHED_FIRST_TAKING_PHOTO_CLEANUP                       = 5,
            E_FINISHED_FIRST_UPGRADE_PRESENTATION_BEGIN                 = 6,
            E_FINISHED_FIRST_UPGRADE_PRESENTATION_RUNNING               = 7,
            E_FINISHED_FIRST_UPGRADE_PRESENTATION_CLEANUP               = 8,
            E_FINISHED_FIRST_PRESENTATION_COUNT                         = 9,
        };

        // ---- header h:151 -----------------------------------------------------------
        enum EFinishedSecondPartSubstates
        {
            E_FINISHED_SECOND_PRESENTATION_INVALID               = 0,
            E_FINISHED_SECOND_PRESENTATION_LOAD_RESOURCES        = 1,
            E_FINISHED_SECOND_PRESENTATION_INITIALISE_COMPONENTS = 2,
            E_FINISHED_SECOND_PRESENTATION_RUNNING               = 3,
            E_FINISHED_SECOND_PRESENTATION_LEAVING               = 4,
            E_FINISHED_SECOND_PRESENTATION_CLEANUP               = 5,
            E_FINISHED_SECOND_PRESENTATION_COUNT                 = 6,
        };

        // cpp:80 -- assert the fsm pointer, run the base Construct, then seed
        // every state word, the timer and the two latched pointers.
        virtual void Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm);

        virtual void OnEnter();     // (cpp:112)
        virtual void OnLeave();     // (cpp:163)
        virtual void Update();      // (cpp:191)

        // This state requests no extra resources up front; the console body writes
        // null/0 to both out-params. The
        // per-presentation resource (mResourceToLoad) is populated and loaded later from
        // the substate machine, not handed out here.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = 0;
            *lpuNumberOfResources = 0;
        }

    private:
        // The four presentations, one per ECompletionType. Update dispatches to exactly one
        // of them per frame, and each runs its own substate ladder.
        void UpdateBurnoutLicensePresentation();              // (cpp:373)
        void UpdateEliteLicensePresentation();                // (cpp:481)
        void UpdateOneHundredPercentPartOnePresentation();    // (cpp:680)
        void UpdateOneHundredPercentPartTwoPresentation();    // (cpp:873)

        void HandleIncomingEvents();                          // (cpp:998)

        // cpp:1122 -- the photo booth take/cancel pair, the only controller
        // input this screen answers.
        virtual void HandleControllerInput(const CgsModule::Event* lpEvent, s32 liEventType);

        // cpp:1203 -- post the post-event teardown and send the state event
        // that leaves this screen (the credits, or straight back to the front end).
        void TriggerExitState();

        // ---- statics (cpp:32/43) ---------------------------------------------
        static const s32 maiEventToObserve[];
        static const s32 miNumEventsObserved;

        // ---- members (h:187..h:219) ------------------------------------------
        LicenseComponent    mLicense;
        PhotoBoothComponent mPhotoBoothComponent;

        AnimationComponent  mUpgradeStateAnimator;
        TextField           mUpgradeTextfield;

        AnimationComponent  mRewardCarsStateAnimator;
        TextField           mRewardCarsTextfield;

        ECompletedGameState          meCompletedGameState;
        ECompletionType              meCompletionType;
        EBurnoutSubstates            meBurnoutSubstate;
        EEliteSubstates              meEliteSubstate;
        EFinishedFirstPartSubstates  meFirstFinishedSubstate;
        EFinishedSecondPartSubstates meSecondFinishedSubstate;

        f32  mfTimeRemaining;
        bool mbStartedUpgradeTransOut;

        CgsGui::sResourceTuple mResourceToLoad;
        s32                    miNumResourcesToLoad;

        GuiCache*                mpGuiCache;
        BrnProgression::Profile* mpProfile;
    };
}
