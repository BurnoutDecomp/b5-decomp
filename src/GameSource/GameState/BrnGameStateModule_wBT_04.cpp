// BrnGameState::GameStateModule::SendAllRivalryData -- the answer to the GUI's "all rivals" request
// (ProcessGameEvents case 101): every rival of the progression data, its car, and whether the
// player has met it yet, posted as action 188.

#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // AddEvent
#include "GameSource/GameState/BrnGameActions.h"                         // RivalryOverviewAction
#include "GameSource/GameState/Progression/BrnProgressionManager.h"      // ProgressionManager
#include "GameSource/GameState/Progression/BrnProfile.h"                 // Profile::FindRival, KI_MAX_RIVAL_COUNT
#include "SharedClasses/Progression/BrnProgressionData.h"                // ProgressionData::GetRival / GetRivalCount
#include "SharedClasses/Progression/BrnRival.h"                          // Rival::GetId / GetCarId

namespace BrnGameState
{

void GameStateModule::SendAllRivalryData(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    CGS_ASSERT(mProgressionManager.GetProgressionData(), "mProgressionManager.GetProgressionData()");
    BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
    CGS_ASSERT(lpProfile, "mProgressionManager.GetProfile()");
    CGS_ASSERT(BrnProgression::Profile::KI_MAX_RIVAL_COUNT > mProgressionManager.GetProgressionData()->GetRivalCount(),
               "BrnProgression::Profile::KI_MAX_RIVAL_COUNT > mProgressionManager.GetProgressionData()->GetRivalCount()");

    GameStateModuleIO::RivalryOverviewAction lRivalryOverview;
    lRivalryOverview.miRivalsReturned = mProgressionManager.GetProgressionData()->GetRivalCount();

    for (s32 liIndex = 0; liIndex < lRivalryOverview.miRivalsReturned; ++liIndex)
    {
        const BrnProgression::Rival* lpRival = mProgressionManager.GetProgressionData()->GetRival(liIndex);
        // The console streams the index between "Rival " and the rest of this message.
        CGS_ASSERT(lpRival, "Rival does not exist! \n");

        lRivalryOverview.mRivalIDs[liIndex]    = lpRival->GetId();
        lRivalryOverview.mRivalCarIDs[liIndex] = lpRival->GetCarId();

        // A rival the profile already holds has been met; any other is not yet in play.
        if (lpProfile->FindRival(lpRival->GetId()) != 0)
        {
            lRivalryOverview.mRivalryStatus[liIndex] = GameStateModuleIO::RivalryOverviewAction::E_RIVALRY_STAGE_DRIVER;
        }
        else
        {
            lRivalryOverview.mRivalryStatus[liIndex] = GameStateModuleIO::RivalryOverviewAction::E_RIVALRY_STAGE_INVALID;
        }
    }

    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRivalryOverview),
                            GameStateModuleIO::E_ACTION_ALL_RIVALRY_DATA_RESPONSE,
                            static_cast<s32>(sizeof(lRivalryOverview)));
}

}
