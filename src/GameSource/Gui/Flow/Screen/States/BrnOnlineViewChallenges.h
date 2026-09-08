#pragma once
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggle.h"
#include "GameSource/Gui/BrnChallengeListComponent.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"

namespace BrnGui {
class GuiCache;
// DecFIGS BrnOnlineViewChallenges.h:46; ARTIST state at 0x824A1D68.
struct OnlineViewChallenges : public CgsGui::State
{
    OnlineViewChallenges();
    void OnEnter() override;
    void OnLeave() override;
    void Update() override;
    void GetResourcesToLoad(const CgsGui::sResourceTuple** resources, u32* count) const override
    { *resources = maResourceTuplesToLoad; *count = miNumResourcesToLoad; }
private:
    enum ESubState { E_SUBSTATE_LOADING_SCREEN, E_SUBSTATE_LOADING_COMPONENTS, E_SUBSTATE_SELECTING_PARAMS };
    void HandleControllerInput(const s32* event);
    void HandleControllerInputSelectParams(const s32* event);
    void HandleGuiCacheEvent(const CgsModule::Event* event);
    void CheckForCompletedLoads();
    void ShowScreen();
    void UpdateArrows(); // shared inlined tail of ShowScreen / input handling
    static const s32 maiEventToObserve[5];
    static const CgsGui::sResourceTuple maResourceTuplesToLoad[];
    static const s32 miNumResourcesToLoad;
    ESubState meSubState;
    MenuToggle mChallengeToggle;
    ChallengeListComponent mChallengeListComponent;
    AnimationComponent mUpArrowAnimation;
    AnimationComponent mDownArrowAnimation;
    GuiCache* mpGuiCache;
};
}
