#define private public
#define protected public
#include "GameSource/Gui/Flow/HUD/States/BrnRaceMainHudState.h"
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/Gui/Events/BrnGuiMugshotControlEvent.h"
#undef protected
#undef private
extern "C" unsigned HudNativeLayoutValues[] = {
 sizeof(BrnGui::RaceMainHudState),
 offsetof(BrnGui::RaceMainHudState,mEventInfoComponent), sizeof(((BrnGui::RaceMainHudState*)0)->mEventInfoComponent),
 offsetof(BrnGui::RaceMainHudState,mSatNavAnimationComponent), sizeof(((BrnGui::RaceMainHudState*)0)->mSatNavAnimationComponent),
 offsetof(BrnGui::RaceMainHudState,mSatNavComponent), sizeof(((BrnGui::RaceMainHudState*)0)->mSatNavComponent),
 offsetof(BrnGui::RaceMainHudState,mPaybackComponent), sizeof(((BrnGui::RaceMainHudState*)0)->mPaybackComponent),
 offsetof(BrnGui::RaceMainHudState,mHudMessageComponent), sizeof(((BrnGui::RaceMainHudState*)0)->mHudMessageComponent),
 offsetof(BrnGui::RaceMainHudState,mDistrictMarker), sizeof(((BrnGui::RaceMainHudState*)0)->mDistrictMarker),
 offsetof(BrnGui::RaceMainHudState,mBoostMessageManager), sizeof(((BrnGui::RaceMainHudState*)0)->mBoostMessageManager),
 offsetof(BrnGui::RaceMainHudState,mEventCountdownIcon), sizeof(((BrnGui::RaceMainHudState*)0)->mEventCountdownIcon),
 offsetof(BrnGui::RaceMainHudState,mPositionIndicatorComponent), sizeof(((BrnGui::RaceMainHudState*)0)->mPositionIndicatorComponent),
 offsetof(BrnGui::RaceMainHudState,mPlayerPositionTable), sizeof(((BrnGui::RaceMainHudState*)0)->mPlayerPositionTable),
 offsetof(BrnGui::RaceMainHudState,mFriendsList), sizeof(((BrnGui::RaceMainHudState*)0)->mFriendsList),
 offsetof(BrnGui::RaceMainHudState,mFriendsListChangeIcon), sizeof(((BrnGui::RaceMainHudState*)0)->mFriendsListChangeIcon),
 offsetof(BrnGui::RaceMainHudState,mGeneralTransitionComponentApt), sizeof(((BrnGui::RaceMainHudState*)0)->mGeneralTransitionComponentApt),
 offsetof(BrnGui::RaceMainHudState,mGeneralTransitionComponentFlapt), sizeof(((BrnGui::RaceMainHudState*)0)->mGeneralTransitionComponentFlapt),
 offsetof(BrnGui::RaceMainHudState,mRoadRuleComponent), sizeof(((BrnGui::RaceMainHudState*)0)->mRoadRuleComponent),
 offsetof(BrnGui::RaceMainHudState,mMugShotComponent), sizeof(((BrnGui::RaceMainHudState*)0)->mMugShotComponent),
 offsetof(BrnGui::RaceMainHudState,mMugshotDIARRHiderComponent), sizeof(((BrnGui::RaceMainHudState*)0)->mMugshotDIARRHiderComponent),
 offsetof(BrnGui::RaceMainHudState,mMugshotOpponentGamertag), sizeof(((BrnGui::RaceMainHudState*)0)->mMugshotOpponentGamertag),
 offsetof(BrnGui::RaceMainHudState,mShowtimeBounceBoostButton), sizeof(((BrnGui::RaceMainHudState*)0)->mShowtimeBounceBoostButton),
 offsetof(BrnGui::RaceMainHudState,mIdentAnimator), sizeof(((BrnGui::RaceMainHudState*)0)->mIdentAnimator),
 offsetof(BrnGui::RaceMainHudState,mRoadRuleShotComponent), sizeof(((BrnGui::RaceMainHudState*)0)->mRoadRuleShotComponent),
 offsetof(BrnGui::RaceMainHudState,mOnlineTimeoutTimer), sizeof(((BrnGui::RaceMainHudState*)0)->mOnlineTimeoutTimer),
 offsetof(BrnGui::RaceMainHudState,mCompass), sizeof(((BrnGui::RaceMainHudState*)0)->mCompass),
 offsetof(BrnGui::RaceMainHudState,mChallengeComponent), sizeof(((BrnGui::RaceMainHudState*)0)->mChallengeComponent),
 offsetof(BrnGui::RaceMainHudState,mChallengeSelectorComponent), sizeof(((BrnGui::RaceMainHudState*)0)->mChallengeSelectorComponent),
 offsetof(BrnGui::RaceMainHudState,mChallengeOnComponent), sizeof(((BrnGui::RaceMainHudState*)0)->mChallengeOnComponent),
};
