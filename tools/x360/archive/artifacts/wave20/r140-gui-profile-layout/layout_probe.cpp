#include <xtl.h>
#include <stddef.h>

#define private public
#define protected public
#include "GameSource/Game/BrnGameModule.hpp"
#undef protected
#undef private

extern "C" const u32 gR140Layout[] = {
    __alignof(BrnGui::ProfileHost),
    sizeof(BrnGui::ProfileHost),
    offsetof(BrnGui::ProfileHost, mProfileManager),
    sizeof(BrnGui::ProfileManager),
    offsetof(BrnGui::ProfileManager, mSaveLoadSystem),
    offsetof(BrnGui::ProfileManager, mStoredData),
    sizeof(BrnGui::GuiModule),
    offsetof(BrnGui::GuiModule, mProfileHost),
    offsetof(BrnGui::GuiModule, mSystemUserProfile),
    offsetof(BrnGui::GuiModule, mbPerformingInvite),
    offsetof(BrnGui::GuiModule, mbInviteRequiresLogin),
    offsetof(BrnGame::BrnGameModule, mGuiModule),
    offsetof(BrnGame::BrnGameModule, mEffectsModule),
    __alignof(BrnEffects::EffectsModule),
    offsetof(BrnGame::BrnGameModule, mMainFlowStateMachine),
    offsetof(BrnGame::BrnGameModule, mbRequestStreamingWait),
    offsetof(BrnGame::BrnGameModule, mbWorldStreamingRequestedStall),
    offsetof(BrnGame::BrnGameModule, miGuiFsmStage),
    offsetof(BrnGame::BrnGameModule, mbGuiPhaseComplete),
    offsetof(BrnGame::BrnGameModule, mbGuiPreAccept),
};
