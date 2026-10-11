#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

using s32 = int;
using u32 = unsigned int;
using CgsID = unsigned long long;

static int giChecks = 0;
static int giFailures = 0;
static int giAssertions = 0;

static void Check(bool lbCondition, const char* lpcName)
{
    ++giChecks;
    if (!lbCondition)
    {
        ++giFailures;
        std::printf("FAIL %s\n", lpcName);
    }
}

#define CGS_ASSERT(condition, message) do { if (!(condition)) { ++giAssertions; } } while (0)

namespace BrnResource
{
    class ChallengeListEntry
    {
    public:
        explicit ChallengeListEntry(CgsID lId) : mId(lId) {}
        CgsID GetChallengeID() const { return mId; }

    private:
        CgsID mId;
    };

    class ChallengeList
    {
    public:
        s32 GetChallengeCount() const { return static_cast<s32>(maEntries.size()); }
        const ChallengeListEntry* GetChallengeData(s32 liIndex) const
        {
            return liIndex >= 0 && liIndex < GetChallengeCount() ? maEntries[liIndex] : nullptr;
        }

        std::vector<const ChallengeListEntry*> maEntries;
    };
}

namespace BrnGameState
{
    class AchievementManagerBase
    {
    public:
        void OnFreeburnChallengeComplete(u32 luCompletedCount)
        {
            maCompletedCounts.push_back(luCompletedCount);
        }

        std::vector<u32> maCompletedCounts;
    };
}

namespace BrnProgression
{
    class Profile
    {
    public:
        bool HasPlayerCompletedFreeburnChallenge(CgsID lId) const
        {
            return std::find(maCompleted.begin(), maCompleted.end(), lId) != maCompleted.end();
        }

        u32 CompleteFreeburnChallenge(CgsID lId)
        {
            maCompleted.push_back(lId);
            return static_cast<u32>(maCompleted.size());
        }

        std::vector<CgsID> maCompleted;
    };

    class ProgressionManager
    {
    public:
        Profile* GetProfile() { return &mProfile; }
        BrnGameState::AchievementManagerBase* GetAchievementManager() { return &mAchievementManager; }

        Profile mProfile;
        BrnGameState::AchievementManagerBase mAchievementManager;
    };
}

namespace BrnGameState
{
    class LocalChallengeCompletionData
    {
    public:
        void SetBit(u32 luIndex) { maSetBits.push_back(luIndex); }
        std::vector<u32> maSetBits;
    };

    class ChallengeManager
    {
    public:
        static bool mbChallengesAreAllOnePlayer;
        static bool mbChallengesAreAllTwoPlayer;

        const BrnResource::ChallengeList* GetFreeburnChallengeList() const { return &mChallengeList; }
        BrnProgression::ProgressionManager* GetProgressionManager() { return &mProgressionManager; }
        LocalChallengeCompletionData& GetLocalChallengeCompletionData() { return mCompletionData; }
        void CheckForOnlineChallengeUnlocks() { ++miUnlockChecks; }

        BrnResource::ChallengeList mChallengeList;
        BrnProgression::ProgressionManager mProgressionManager;
        LocalChallengeCompletionData mCompletionData;
        int miUnlockChecks = 0;
        int miChallengeStartCalls = 0;
    };
}

bool BrnGameState::ChallengeManager::mbChallengesAreAllOnePlayer = false;
bool BrnGameState::ChallengeManager::mbChallengesAreAllTwoPlayer = false;

namespace CgsDev
{
    class DebugComponent
    {
    public:
        struct VariableBinding
        {
            void* mpValue;
            std::string mLabel;
            s32 miMinimum = 0;
            s32 miMaximum = 0;
            bool mbHasRange = false;
        };

        struct FunctionBinding
        {
            void (*mpCallback)(void*);
            void* mpUserData;
            std::string mLabel;
        };

        void RegisterVariable(bool* lpValue, const char* lpcLabel)
        {
            maVariables.push_back({lpValue, lpcLabel});
        }

        void RegisterVariable(s32* lpValue, const char* lpcLabel)
        {
            maVariables.push_back({lpValue, lpcLabel});
        }

        void SetRange(s32* lpValue, s32 liMinimum, s32 liMaximum)
        {
            for (VariableBinding& lrBinding : maVariables)
            {
                if (lrBinding.mpValue == lpValue)
                {
                    lrBinding.miMinimum = liMinimum;
                    lrBinding.miMaximum = liMaximum;
                    lrBinding.mbHasRange = true;
                }
            }
        }

        void RegisterFunction(void (*lpCallback)(void*), void* lpUserData, const char* lpcLabel)
        {
            maFunctions.push_back({lpCallback, lpUserData, lpcLabel});
        }

        std::vector<VariableBinding> maVariables;
        std::vector<FunctionBinding> maFunctions;
    };
}

namespace BrnGameState
{
    class ChallengeManagerDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void OnActivate();
        static void StartChallenge(void* lpContext);
        static void CompleteAllChallenges(void* lpContext);

        ChallengeManager* mpChallengeManager = nullptr;
        s32 miChallengeIndex = 2;
        bool mbDebugBeginChallengePending = false;
    };
}

#include "challenge_debug_menu.inc"

int main()
{
    using namespace BrnGameState;

    BrnResource::ChallengeListEntry lAlreadyComplete(11);
    BrnResource::ChallengeListEntry lChallengeA(22);
    BrnResource::ChallengeListEntry lChallengeB(33);
    ChallengeManager lManager;
    lManager.mChallengeList.maEntries = {&lAlreadyComplete, &lChallengeA, nullptr, &lChallengeB};
    lManager.mProgressionManager.mProfile.maCompleted.push_back(11);

    ChallengeManagerDebugComponent lComponent;
    lComponent.mpChallengeManager = &lManager;
    lComponent.OnActivate();

    Check(lComponent.maVariables.size() == 3, "all three original variable controls are registered");
    Check(lComponent.maFunctions.size() == 2, "both original action controls are registered");
    Check(lComponent.maVariables[0].mLabel == "Make All Challenges One Player" &&
          lComponent.maVariables[0].mpValue == &ChallengeManager::mbChallengesAreAllOnePlayer,
          "one-player toggle binds the original global");
    Check(lComponent.maVariables[1].mLabel == "Make All Challenges Two Player" &&
          lComponent.maVariables[1].mpValue == &ChallengeManager::mbChallengesAreAllTwoPlayer,
          "two-player toggle binds the original global");
    Check(lComponent.maVariables[2].mLabel == "Challenge Index" &&
          lComponent.maVariables[2].mpValue == &lComponent.miChallengeIndex &&
          lComponent.maVariables[2].mbHasRange &&
          lComponent.maVariables[2].miMinimum == 0 && lComponent.maVariables[2].miMaximum == 3,
          "challenge index binds the component field with the loaded-list range");

    *static_cast<bool*>(lComponent.maVariables[0].mpValue) = true;
    *static_cast<bool*>(lComponent.maVariables[1].mpValue) = true;
    Check(ChallengeManager::mbChallengesAreAllOnePlayer && ChallengeManager::mbChallengesAreAllTwoPlayer,
          "both menu toggles update their original backing bytes");

    const CgsDev::DebugComponent::FunctionBinding& lrStart = lComponent.maFunctions[0];
    Check(lrStart.mLabel == "Start Challenge" &&
          lrStart.mpCallback == &ChallengeManagerDebugComponent::StartChallenge &&
          lrStart.mpUserData == &lComponent,
          "Start Challenge keeps its original callback and component context");
    lrStart.mpCallback(lrStart.mpUserData);
    Check(lComponent.mbDebugBeginChallengePending,
          "Start Challenge performs the original pending-byte write");
    Check(lComponent.miChallengeIndex == 2 && lManager.miChallengeStartCalls == 0 &&
          lManager.miUnlockChecks == 0,
          "Start Challenge does not consume the selector or start a challenge");
    Check(giAssertions == 0, "Start Challenge preserves the original quiet latch write");

    const CgsDev::DebugComponent::FunctionBinding& lrComplete = lComponent.maFunctions[1];
    Check(lrComplete.mLabel == "Complete all challenges" &&
          lrComplete.mpCallback == &ChallengeManagerDebugComponent::CompleteAllChallenges &&
          lrComplete.mpUserData == &lComponent,
          "complete-all action keeps its original callback and component context");
    lrComplete.mpCallback(lrComplete.mpUserData);
    Check(lManager.mProgressionManager.mProfile.maCompleted == std::vector<CgsID>({11, 22, 33}),
          "complete-all adds only previously incomplete non-null challenge entries");
    Check(lManager.mProgressionManager.mAchievementManager.maCompletedCounts == std::vector<u32>({2, 3}),
          "complete-all notifies achievements for newly completed entries only");
    Check(lManager.mCompletionData.maSetBits == std::vector<u32>({0, 1, 3}),
          "complete-all sets the matching local bits and skips a null list entry");
    Check(lManager.miUnlockChecks == 1,
          "complete-all rechecks online challenge unlocks");

    std::printf("ChallengeDebugMenu: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
