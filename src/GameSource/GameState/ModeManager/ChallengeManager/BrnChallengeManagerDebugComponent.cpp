#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.h"

#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h"   // BrnGameState::ChallengeManager (+ its static toggle flags)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"                   // BrnProgression::ProgressionManager (GetProfile / GetAchievementManager)
#include "GameSource/GameState/Progression/BrnProfile.h"                              // BrnProgression::Profile (Has/CompleteFreeburnChallenge)
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // BrnGameState::AchievementManagerBase::OnFreeburnChallengeComplete
#include "SharedClasses/DataLists/ChallengeList.h"                                    // BrnResource::ChallengeList / ChallengeListEntry
#include "SharedClasses/DataLists/ChallengeListEntry.h"                               // BrnResource::ChallengeListEntry::GetChallengeID
#include "GameShared/GameClasses/Core/CgsAssert.h"                                    // CgsDev::Assert
#include "GameShared/GameClasses/Development/CgsStrStream.h"                           // streamed FastBitArray assertion

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The freeburn challenge-manager debug menu. Four
// functions are bodied here:
//   * GetName              (0x823171E0) -- the menu label.
//   * OnActivate           (0x8233EA18) -- registers the two "make all challenges N-player" toggles,
//                                          the challenge-index selector (ranged 0..count-1), and the
//                                          "Start Challenge" / "Complete all challenges" actions.
//   * StartChallenge      (0x82817350, ICF-shared) -- sets only the component's pending byte.
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

    // X360 0x82817350: ICF folded with DebugCallbackResetCounters. The registered context is this
    // component, and ARTIST only writes byte +0x14. DWARF names that field
    // mbDebugBeginChallengePending. Preserve the write; do not start a challenge here.
    void ChallengeManagerDebugComponent::StartChallenge(void* lpContext)
    {
        static_cast<ChallengeManagerDebugComponent*>(lpContext)->mbDebugBeginChallengePending = true;
    }

    // "Complete all challenges" debug action: walk the loaded freeburn challenge list and mark every
    // challenge as completed on the local player's profile -- notifying the achievement manager for
    // each challenge that was not already complete -- then set the matching bit in the local
    // completion store, and finally re-check online challenge unlocks. The void* context is this
    // component (handed back as the RegisterFunction user-data).
    void ChallengeManagerDebugComponent::CompleteAllChallenges(void* lpContext)
    {
        ChallengeManagerDebugComponent* lpDebugComponent = static_cast<ChallengeManagerDebugComponent*>(lpContext);
        if (lpDebugComponent == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "lpDebugComponent",
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.cpp",
                148);
            CgsDev::Assert::EndAssert();
        }

        ChallengeManager* lpManager = lpDebugComponent->mpChallengeManager;

        const BrnResource::ChallengeList* lpChallengeList = lpManager->GetFreeburnChallengeList();
        for (s32 liIndex = 0; liIndex < lpChallengeList->GetChallengeCount(); ++liIndex)
        {
            const BrnResource::ChallengeListEntry* lpChallengeData = lpChallengeList->GetChallengeData(liIndex);
            if (lpChallengeData == 0)
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

            // ARTIST inlines FastBitArray<2000>::SetBit here, including its non-gating range assert
            // (the container's shared C++ SetBit body does not carry this caller-side check).
            if (liIndex >= 2000)
            {
                // BeginAssert precedes the local stream construction in the X360 call sequence.
                CgsDev::Assert::BeginAssert();

                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lMessage(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lMessage << "Index " << liIndex << " is out of range (max bits: " << 2000 << ")\n";

                CgsDev::Assert::FireAssert(
                    lMessage.GetBuffer(),
                    "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsFastBitArray.h",
                    431);
                CgsDev::Assert::EndAssert();
            }

            // The X360 continues into the bit store after FireAssert/EndAssert.
            lpManager->GetLocalChallengeCompletionData().SetBit(static_cast<u32>(liIndex));
        }

        lpManager->CheckForOnlineChallengeUnlocks();
    }
}
