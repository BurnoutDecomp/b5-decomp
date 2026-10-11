#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using s32 = int;
using u32 = unsigned int;
using u64 = unsigned long long;
using f32 = float;

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

static int giValidEventData = 0;

namespace CgsDev
{
    class DebugComponent
    {
    public:
        struct VariableBinding
        {
            void* mpValue;
            std::string mGroup;
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
            maVariables.push_back({lpValue, "", lpcLabel});
        }
        void RegisterVariable(s32* lpValue, const char* lpcLabel)
        {
            maVariables.push_back({lpValue, "", lpcLabel});
        }
        void RegisterVariable(f32* lpValue, const char* lpcLabel)
        {
            maVariables.push_back({lpValue, "", lpcLabel});
        }
        void RegisterVariable(s32* lpValue, const char* lpcGroup, const char* lpcLabel)
        {
            maVariables.push_back({lpValue, lpcGroup, lpcLabel});
        }
        void RegisterVariable(f32* lpValue, const char* lpcGroup, const char* lpcLabel)
        {
            maVariables.push_back({lpValue, lpcGroup, lpcLabel});
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

    namespace Message
    {
        extern int gxMessageFilterFlags;
    }
    namespace Log
    {
        class DebugPrint
        {
        public:
            template <typename T>
            DebugPrint& operator<<(const T&) { return *this; }
        };
        extern DebugPrint* gpDebugPrint;
    }
}

int CgsDev::Message::gxMessageFilterFlags = 0;
CgsDev::Log::DebugPrint* CgsDev::Log::gpDebugPrint = nullptr;

enum ERouteFindingStyle_Stub : int { ROUTE_STYLE_UNUSED = 0 };
enum EAISpeedSelMethod_Stub : int { AI_SPEED_UNUSED = 0 };

namespace BrnGameState
{
    class StartGameModeParams;
    class GameModeParams;
    class ModeManager;

    class ProgressionRankData
    {
    public:
        f32 GetTrafficDensitySurvival() const { return mfTrafficDensitySurvival; }
        f32 GetLargeVehicleProbability() const { return mfLargeVehicleProbability; }
        void GetOvertakingDifficulty(f32& lfOut) const { lfOut = mfOvertakingDifficulty; }

        f32 mfTrafficDensitySurvival = 1.25f;
        f32 mfLargeVehicleProbability = 0.25f;
        f32 mfOvertakingDifficulty = 0.75f;
    };

    class StartGameModeParams
    {
    public:
        const void* GetEventData() const { return mpEventData; }
        const ProgressionRankData* GetProgressionRankData() const { return mpRankData; }
        f32 GetProgressionRankAsRatio() const { return mfRankRatio; }
        u32 GetGameModeType() const { return muGameModeType; }
        u32 GetJunctionID() const { return muJunctionID; }
        u32 GetEventJunctionId() const { return muEventJunctionID; }
        f32 GetTrafficDensity() const { return mfTrafficDensity; }
        s32 GetStartMechanism() const { return miStartMechanism; }
        s32 GetTrafficLightTriggerId() const { return miTrafficLightTriggerId; }

        const void* mpEventData = &giValidEventData;
        const ProgressionRankData* mpRankData = nullptr;
        f32 mfRankRatio = 0.0f;
        u32 muGameModeType = 0x31;
        u32 muJunctionID = 12;
        u32 muEventJunctionID = 34;
        f32 mfTrafficDensity = 0.8f;
        s32 miStartMechanism = 2;
        s32 miTrafficLightTriggerId = 7;
    };

    class GameModeParams
    {
    public:
        enum : u64
        {
            KU_FLAG_SET_CARS_TO_START_GRID = 1ull << 0,
            KU_FLAG_REMOVE_RIVALS_FROM_WORLD = 1ull << 1,
            KU_FLAG_WRAP_AI_CARS_WHEN_OUT_OF_RANGE = 1ull << 2,
            KU_FLAG_CLEAR_NEARBY_TRAFFIC = 1ull << 3,
            KU_FLAG_HAS_ROUTE = 1ull << 4,
            KU_FLAG_AI_DRIVE_BY_START = 1ull << 5,
            KU_FLAG_SET_ALL_CARS_TO_STARTING_AI_CONTROL = 1ull << 6,
            KU_FLAG_ROLLING_START = 1ull << 7,
            KU_FLAG_USES_NAVIGATION = 1ull << 8,
            KU_FLAG_AI_PERSISTENT_DAMAGE = 1ull << 9,
            KU_FLAG_AI_RESET_ON_TRACK_BEHIND = 1ull << 10,
            KU_FLAG_ENFORCE_SOFT_TAKEDOWNS = 1ull << 11,
            KU_FLAG_SET_OPPONENTS_TO_COPS = 1ull << 12
        };

        void Construct(u32 luType) { muConstructedType = luType; }
        void SetNumRivals(s32 liValue) { miNumRivals = liValue; }
        s32 GetNumRivals() const { return miNumRivals; }
        void SetTrafficDensityScale(f32 lfValue) { mfTrafficDensityScale = lfValue; }
        void SetLargeVehicleProbability(f32 lfValue) { mfLargeVehicleProbability = lfValue; }
        void SetProgressionRankAsRatio(f32 lfValue) { mfProgressionRankAsRatio = lfValue; }
        void SetDefaultPlayerRouteFindingStyle(ERouteFindingStyle_Stub) {}
        void SetDefaultAIRouteFindingStyle(ERouteFindingStyle_Stub) {}
        void SetFlag(u64 luValue) { muFlags = luValue; }
        void SetStartMechanism(s32 liValue) { miStartMechanism = liValue; }
        void SetTrafficLightTriggerId(s32 liValue) { miTrafficLightTriggerId = liValue; }
        s32 GetStartLocationCount() const { return miStartLocationCount; }
        void SetAISpeedSelectionMethod(EAISpeedSelMethod_Stub) {}
        void SetAIAggresiveCarCount(s32 liValue) { miAggressiveCarCount = liValue; }
        void SetPlayerWreckCount(s32 liValue) { miPlayerWreckCount = liValue; }

        u32 muConstructedType = 0;
        u32 muJunctionID = 0;
        u32 muEventJunctionID = 0;
        s32 miNumNetworkPlayers = -1;
        s32 miNumRivals = 0;
        s32 miAggressiveCarCount = -1;
        s32 miPlayerWreckCount = -1;
        s32 miStartMechanism = -1;
        s32 miTrafficLightTriggerId = -1;
        s32 miStartLocationCount = 3;
        u64 muFlags = 0;
        f32 mfTrafficDensityScale = 0.0f;
        f32 mfLargeVehicleProbability = 0.0f;
        f32 mfProgressionRankAsRatio = 0.0f;
        f32 mfOvertakingDifficulty = 0.0f;
    };

    class StuntScorer
    {
    public:
        bool* GetEndlessStuntRunFlag() { return &mbEndlessStuntRun; }
        bool mbEndlessStuntRun = false;
    };

    class ScoringSystem
    {
    public:
        StuntScorer* GetStuntScorer() { return &mStuntScorer; }
        StuntScorer mStuntScorer;
    };

    class ModeManager
    {
    public:
        ScoringSystem* GetScoringSystem() { return &mScoringSystem; }
        void SetStartingGrid(GameModeParams*, s32 liCarCount, bool lbRolling)
        {
            miGridCarCount = liCarCount;
            mbGridRolling = lbRolling;
        }

        ScoringSystem mScoringSystem;
        bool mbWinIfSecond = false;
        bool mbFinishCurrentModeNextUpdate = false;
        s32 miDebugFinishPosition = 0;
        s32 miGridCarCount = 0;
        bool mbGridRolling = true;
    };

    class OfflineGameMode
    {
    public:
        s32 CalculateMaxPlayerWrecks(const StartGameModeParams*) const { return 2; }
        ModeManager* mpModeManager = nullptr;
    };

    class SurvivorMode : public OfflineGameMode
    {
    public:
        void Start(const StartGameModeParams*, GameModeParams*, ScoringSystem*);
        bool mbInShortcut = false;
        f32 mfRampTimer = 0.0f;
        f32 mfMaxRampTimer = 0.0f;
        s32 miBroadcastOpponentCount = 0;
        s32 miMaxOpponentCount = 0;
        f32 mfTimeInReverse = 0.0f;
    };

    class ModeManagerDebugComponent : public CgsDev::DebugComponent
    {
    public:
        void Construct(ModeManager* lpModeManager);
        void OnActivate();
        static void FinshMode(void* lpContext);
        ModeManager* mpModeManager = nullptr;
        bool mbShowModeInfo = false;
        bool mbInfiniteLives = false;
        s32 miFinishPosition = 0;
    };
}

namespace BrnProgression
{
    using ProgressionRankData = BrnGameState::ProgressionRankData;
}

#include "mode_survivor_debug_menu.inc"

static void StartMarkedMan(BrnGameState::ModeManager& lrManager, f32 lfRatio,
                           s32 liExpectedOpponents, f32 lfExpectedRamp, const char* lpcCase)
{
    using namespace BrnGameState;
    ProgressionRankData lRank;
    StartGameModeParams lStart;
    lStart.mpRankData = &lRank;
    lStart.mfRankRatio = lfRatio;
    GameModeParams lParams;
    SurvivorMode lSurvivor;
    lSurvivor.mpModeManager = &lrManager;
    lSurvivor.Start(&lStart, &lParams, nullptr);

    const std::string lPrefix = lpcCase;
    Check(lSurvivor.miMaxOpponentCount == liExpectedOpponents,
          (lPrefix + " opponent count follows the registered bounds").c_str());
    Check(lParams.GetNumRivals() == liExpectedOpponents &&
          lParams.miAggressiveCarCount == liExpectedOpponents,
          (lPrefix + " rivals and aggressive-car count receive the computed value").c_str());
    Check(lrManager.miGridCarCount == liExpectedOpponents + 1 && !lrManager.mbGridRolling,
          (lPrefix + " starting grid receives opponents plus the player").c_str());
    Check(std::fabs(lSurvivor.mfMaxRampTimer - lfExpectedRamp) < 0.0001f,
          (lPrefix + " ramp duration follows the registered bounds").c_str());
    Check(lParams.muJunctionID == lStart.muJunctionID &&
          lParams.muEventJunctionID == lStart.muEventJunctionID &&
          lParams.miNumNetworkPlayers == 0 && lParams.miPlayerWreckCount == 2,
          (lPrefix + " original event setup side effects remain").c_str());
}

int main()
{
    using namespace BrnGameState;
    ModeManager lManager;
    ModeManagerDebugComponent lComponent;
    lComponent.Construct(&lManager);
    lComponent.OnActivate();

    Check(lComponent.maVariables.size() == 9, "all nine original Mode Manager variables are registered");
    Check(lComponent.maFunctions.size() == 1, "the original action control is registered");
    const char* lLabels[] = {
        "Endless Stunt Run", "Infinite lives", "Show mode info", "Win if second",
        "Min opponent count", "Max opponent count", "Min ramp time", "Max ramp time",
        "Finish Position"
    };
    for (size_t i = 0; i < 9; ++i)
        Check(lComponent.maVariables[i].mLabel == lLabels[i], "control label and registration order are preserved");

    Check(lComponent.maVariables[0].mpValue == lManager.GetScoringSystem()->GetStuntScorer()->GetEndlessStuntRunFlag(),
          "Endless Stunt Run binds the StuntScorer flag");
    Check(lComponent.maVariables[1].mpValue == &lComponent.mbInfiniteLives &&
          lComponent.maVariables[2].mpValue == &lComponent.mbShowModeInfo,
          "Infinite lives and Show mode info bind component state");
    Check(lComponent.maVariables[3].mpValue == &lManager.mbWinIfSecond,
          "Win if second binds ModeManager state");

    Check(lComponent.maVariables[4].mpValue == &giMarkedManMinOpponentCount &&
          lComponent.maVariables[5].mpValue == &giMarkedManMaxOpponentCount &&
          lComponent.maVariables[6].mpValue == &gfMarkedManMinRampTime &&
          lComponent.maVariables[7].mpValue == &gfMarkedManMaxRampTime,
          "the four Marked Man controls share the globals consumed by SurvivorMode::Start");
    Check(lComponent.maVariables[4].mGroup == "Marked man tweaks" &&
          lComponent.maVariables[5].mGroup == "Marked man tweaks" &&
          lComponent.maVariables[6].mGroup == "Marked man tweaks" &&
          lComponent.maVariables[7].mGroup == "Marked man tweaks",
          "the four tunables remain under their original menu group");
    Check(giMarkedManMinOpponentCount == 2 && giMarkedManMaxOpponentCount == 4 &&
          gfMarkedManMinRampTime == 5.0f && gfMarkedManMaxRampTime == 5.0f,
          "the original image defaults remain 2, 4, 5, and 5 seconds");

    Check(lComponent.maVariables[8].mpValue == &lComponent.miFinishPosition &&
          lComponent.maVariables[8].mbHasRange && lComponent.maVariables[8].miMinimum == 1 &&
          lComponent.maVariables[8].miMaximum == 8,
          "Finish Position retains its component field and 1..8 range");
    lComponent.miFinishPosition = 6;
    const CgsDev::DebugComponent::FunctionBinding& lrFinish = lComponent.maFunctions[0];
    Check(lrFinish.mLabel == "End Current Event" &&
          lrFinish.mpCallback == &ModeManagerDebugComponent::FinshMode &&
          lrFinish.mpUserData == &lComponent,
          "End Current Event retains its callback and context");
    lrFinish.mpCallback(lrFinish.mpUserData);
    Check(lManager.mbFinishCurrentModeNextUpdate && lManager.miDebugFinishPosition == 6,
          "End Current Event forwards the selected position to ModeManager");

    StartMarkedMan(lManager, 0.5f, 3, 5.0f, "default tunables");
    *static_cast<s32*>(lComponent.maVariables[4].mpValue) = 3;
    *static_cast<s32*>(lComponent.maVariables[5].mpValue) = 7;
    *static_cast<f32*>(lComponent.maVariables[6].mpValue) = 2.0f;
    *static_cast<f32*>(lComponent.maVariables[7].mpValue) = 10.0f;
    StartMarkedMan(lManager, 0.5f, 5, 6.0f, "edited tunables at mid-rank");
    StartMarkedMan(lManager, 1.0f, 7, 2.0f, "edited tunables at max-rank");
    StartMarkedMan(lManager, -0.5f, 0, 14.0f, "negative ratio edge");
    Check(giAssertions == 0, "valid setup inputs preserve the original assert conditions");

    std::printf("ModeSurvivorDebugMenu: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
