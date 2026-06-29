#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"   // BrnGameState::ChallengeManager (+ its static toggle flags)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"                   // BrnProgression::ProgressionManager (GetProfile / GetAchievementManager)
#include "GameSource/GameState/Progression/BrnProfile.h"                              // BrnProgression::Profile (Has/CompleteFreeburnChallenge)
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // BrnGameState::AchievementManagerBase::OnFreeburnChallengeComplete
#include "SharedClasses/DataLists/ChallengeList.h"                                    // BrnResource::ChallengeList / ChallengeListEntry
#include "SharedClasses/DataLists/ChallengeListEntry.h"                               // BrnResource::ChallengeListEntry::GetChallengeID
#include "GameShared/GameClasses/Core/CgsAssert.h"                                    // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The freeburn challenge-manager debug menu. Three
// functions are bodied here (the X360 ledger attests exactly these for this TU):
//   * GetName              (0x823171E0) -- the menu label.
//   * OnActivate           (0x8233EA18) -- registers the two "make all challenges N-player" toggles,
//                                          the challenge-index selector (ranged 0..count-1), and the
//                                          "Start Challenge" / "Complete all challenges" actions.
//   * CompleteAllChallenges(0x82335010) -- the "Complete all challenges" action body.
//
// The X360 issues the menu registrations through the (unnamed) CgsDev::DebugComponent registration
// helpers; those are the base-class methods reconstructed in CgsDebugComponent.h, called by name
// (RegisterVariable / SetRange / RegisterFunction) -- matching the sibling debug components
// (e.g. BrnModeManagerDebugComponent).

namespace BrnGameState
{
    const char* ChallengeManagerDebugComponent::GetName() const
    {
        return "Challenge Manager";
    }

    void ChallengeManagerDebugComponent::OnActivate()
    {
        // The two "make all challenges one/two player" toggles are the manager's static (file-scope)
        // hack flags; the menu binds the bool* directly (X360 byte_82FAD9C1 / byte_82FAD9C0).
        RegisterVariable(&ChallengeManager::mbChallengesAreAllOnePlayer, "Make All Challenges One Player");
        RegisterVariable(&ChallengeManager::mbChallengesAreAllTwoPlayer, "Make All Challenges Two Player");

        // Challenge-index selector, ranged across the loaded freeburn challenge list.
        RegisterVariable(&miChallengeIndex, "Challenge Index");
        SetRange(&miChallengeIndex, 0, mpChallengeManager->GetFreeburnChallengeList()->GetChallengeCount() - 1);

        RegisterFunction(&ChallengeManagerDebugComponent::StartChallenge, this, "Start Challenge");
        RegisterFunction(&ChallengeManagerDebugComponent::CompleteAllChallenges, this, "Complete all challenges");
    }

    // "Complete all challenges" debug action: walk the loaded freeburn challenge list and mark every
    // challenge as completed on the local player's profile -- notifying the achievement manager for
    // each challenge that was not already complete -- then set the matching bit in the local
    // completion store, and finally re-check online challenge unlocks. The void* context is this
    // component (handed back as the RegisterFunction user-data).
    void ChallengeManagerDebugComponent::CompleteAllChallenges(void* lpContext)
    {
        ChallengeManagerDebugComponent* lpDebugComponent = static_cast<ChallengeManagerDebugComponent*>(lpContext);
        CGS_ASSERT(lpDebugComponent != nullptr, "lpDebugComponent");

        ChallengeManager* lpManager = lpDebugComponent->mpChallengeManager;

        const BrnResource::ChallengeList* lpChallengeList = lpManager->GetFreeburnChallengeList();
        for (s32 liIndex = 0; liIndex < lpChallengeList->GetChallengeCount(); ++liIndex)
        {
            const BrnResource::ChallengeListEntry* lpChallengeData = lpChallengeList->GetChallengeData(liIndex);
            if (lpChallengeData == nullptr)
            {
                continue;
            }

            BrnProgression::ProgressionManager* lpProgression = lpManager->GetProgressionManager();
            BrnProgression::Profile* lpProfile = lpProgression->GetProfile();

            const CgsID lChallengeID = lpChallengeData->GetChallengeID();
            if (!lpProfile->HasPlayerCompletedFreeburnChallenge(lChallengeID))
            {
                const u32 luCompletedCount = lpProfile->CompleteFreeburnChallenge(lChallengeID);
                lpProgression->GetAchievementManager()->OnFreeburnChallengeComplete(luCompletedCount);
            }

            // Mark this challenge index complete in the local completion bit store (the inlined
            // FastBitArray<2000>::SetBit, max bits == KI_MAX_PROFILE_CHALLENGES).
            lpManager->GetLocalChallengeCompletionData().SetBit(static_cast<u32>(liIndex));
        }

        lpManager->CheckForOnlineChallengeUnlocks();
    }
}
