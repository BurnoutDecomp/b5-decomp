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

    // ========================================================================================
    // TRAP STUB -- ChallengeManagerDebugComponent::StartChallenge  (UNRECOVERABLE, NOT A BODY)
    // ========================================================================================
    // [challenge-manager mount 2026-09-07] This is a NAMED TRAP, not a reconstruction, and it is
    // here only so the mount links. The body is genuinely unrecoverable from the X360 build:
    //
    //   * The callback address OnActivate registers for "Start Challenge" resolves, in the ARTIST
    //     image, to CgsDev::DebugComponentPerfMonCpu::DebugCallbackResetCounters @0x82817350 --
    //     an unrelated perf-monitor callback. That is IDENTICAL-CODE FOLDING: the linker merged
    //     this static wrapper onto a byte-identical function, so following the registration
    //     yields the WRONG function's code, and there is no other reference to disambiguate it.
    //   * The DWARF (BrnChallengeManagerDebugComponent.cpp:114) names the real callback
    //     StartChallenge and gives it this signature, which is why the declaration stands and
    //     why OnActivate registers it -- but a name is not a body.
    //   * Nothing else in the image calls it, so there is no second call site to recover the
    //     behaviour from either.
    //
    // Guessing it would be an invention on a developer-only path: the plausible shape (set
    // mbDebugBeginChallengePending, or drive the manager's start path with miChallengeIndex) is
    // exactly the kind of guess STRATEGY.md forbids, and a silently-wrong debug action is worse
    // than one that says so. It logs ONCE and asserts, so a developer who reaches this menu item
    // is told what happened instead of watching nothing happen.
    //
    // ⭐ DELETE-WHEN the real body is recovered (an unfolded build, or a second reference that
    // disambiguates the ICF target). The retail game never reaches here -- this is a debug-menu
    // action -- so the trap is inert in normal play.
    void ChallengeManagerDebugComponent::StartChallenge(void* lpContext)
    {
        (void)lpContext;

        static bool sbTrapReported = false;
        if (!sbTrapReported)
        {
            sbTrapReported = true;
            CGS_ASSERT(false,
                       "TRAP STUB: ChallengeManagerDebugComponent::StartChallenge has no recovered "
                       "body (its X360 registration is ICF-folded onto an unrelated callback)");
        }
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
