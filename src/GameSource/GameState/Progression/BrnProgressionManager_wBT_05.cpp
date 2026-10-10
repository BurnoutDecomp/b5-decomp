// BrnProgression::ProgressionManager::OnMugshotSent (debug info BrnProgressionManager.h).
// No out-of-line copy exists on the console: it is inlined into GameStateModule::ProcessGameEvents'
// case 147 (E_EVENT_ONLINE_MUGSHOT_SENT), which is where this body is read from. The arm bumps the
// profile's sent-mugshot count (+0x170 profile, +117992 count) and only then reads the count back
// for the achievement manager the progression manager holds (+0x20938).

#include "GameSource/GameState/Progression/BrnProgressionManager.h"

#include "GameSource/GameState/Progression/BrnProfile.h"
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h"   // OnMugshotSent(s32)

namespace BrnProgression
{

void ProgressionManager::OnMugshotSent()
{
    mProfile.OnMugshotSent();
    mpAchievementManager->OnMugshotSent(mProfile.GetNumMugshotsSent());
}

}
