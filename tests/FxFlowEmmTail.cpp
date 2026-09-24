// FX-FLOW (crash parity 2026-09-24, NEW-EMMTAIL): the PRODUCTION
// GameStateModule::EmmPreWorldUpdateTailBringUp (GameStateModule_gUI_00.cpp), the PRODUCTION
// GameStateToGuiInterface::Construct / AddOvertakeEvent / GetOvertakeEventQueue
// (BrnGameStateToGuiIOInterfaces.cpp), the PRODUCTION ScoringSystem::GetCarData /
// GetRaceCarTotalTime / GetCarRacePosition (BrnScoringSystem_Lookup.cpp / _Queries.cpp) and the
// PRODUCTION GameStateToNetworkInterface::SetActiveRaceCarIndex / GetActiveRaceCarIndex
// (Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.cpp), extracted by
// run_fxflow_emm_tail.py, against the real ScoringSystem / CarData / CarScoreData,
// GameStateToGuiInterface and GameStateToNetworkInterface. ScoringSystem::GetOvertakenRival is the
// real header inline. The module, its ModeManager hop and the output buffer are fixtures.
//
// Checked against the ARTIST asm of EmmPreWorldUpdate @0x8238EF50:
//   0x8238F1BC..0x8238F214  t = Time(0.0f); if (player != -1) t = GetRaceCarTotalTime(player,
//                           <sim time>); SetGameModeElapsedTime(&t)
//   0x8238F218..0x8238F288  for slot 0..7: lp = GetCarData(slot); if (lp)
//                           GetGameStateToNetworkInterface()->SetActiveRaceCarIndex(lp+0x148, lp+0x144)
//   0x8238F28C..0x8238F33C  if (player != -1 && GetOvertakenRival(player) [`lbz 0x44`]) the
//                           overtake record {u8 GetCarRacePosition(player) @+0, player @+4} onto
//                           GameStateToGuiInterface +0xC8
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"
#include "GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.h"
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++gAsserts; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    DebugPrint* gpDebugPrint = nullptr;   // the [overtake] witness stays silent
    StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
}
namespace Message { u64 gxMessageFilterFlags = 0; }
}

namespace BrnGameState
{
namespace GameStateModuleIO
{
    // The output buffer, as far as the tail reaches it (the real one's three accessors are
    // lock-asserted views of these three members).
    class OutputBuffer
    {
    public:
        CgsSystem::Time mGameModeElapsedTime;
        s32             miElapsedTimeSets = 0;
        BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface mGameStateToNetworkInterface;
        GameStateToGuiInterface                                     mGameStateToGuiInterface;

        void SetGameModeElapsedTime(const CgsSystem::Time* lpTime) { mGameModeElapsedTime = *lpTime; ++miElapsedTimeSets; }
        BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface* GetGameStateToNetworkInterface() { return &mGameStateToNetworkInterface; }
        GameStateToGuiInterface* GetGameStateToGuiInterface() { return &mGameStateToGuiInterface; }
    };
}

class ModeManager
{
public:
    ScoringSystem* mpScoringSystem = nullptr;
    ScoringSystem* GetScoringSystem() { return mpScoringSystem; }
};

class GameStateModule
{
public:
    ModeManager                       mModeManager;
    GameStateModuleIO::OutputBuffer*  mpOutputBuffer = nullptr;
    ::EActiveRaceCarIndex             mePlayerActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;

    ::EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() { return mePlayerActiveRaceCarIndex; }
    void EmmPreWorldUpdateTailBringUp(const CgsSystem::TimerStatusInterface& lrTimerStatusInterface);
};
}

// The production bodies under test (or the runner's labelled empty stand-ins).
#include "emm_tail.inc"

using BrnGameState::ScoringSystem;
using BrnGameState::CarData;
using BrnGameState::GameStateModule;
namespace GsmIO = BrnGameState::GameStateModuleIO;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("  FAIL %s\n", lpcName);
    }
    else
    {
        std::printf("  ok   %s\n", lpcName);
    }
}

alignas(16) static unsigned char gaScoringStorage[sizeof(ScoringSystem)];
alignas(16) static unsigned char gaOutputStorage[sizeof(GsmIO::OutputBuffer)];

static ScoringSystem& Scoring() { return *reinterpret_cast<ScoringSystem*>(gaScoringStorage); }

// A fresh scoring system: every record unowned (slot -1), the race one lap long, the mode timer
// started at 10.25 s.
static void ResetScoring()
{
    std::memset(gaScoringStorage, 0, sizeof(gaScoringStorage));
    for (s32 liSlot = 0; liSlot < 8; ++liSlot)
    {
        Scoring().maCarData[liSlot].SetActiveRaceCarIndex(::E_ACTIVE_RACE_CAR_INDEX_INVALID);
        Scoring().maCarData[liSlot].SetNetworkPlayerID(-1);
    }
    Scoring().SetTotalLaps(1);
    Scoring().mStartTime = CgsSystem::Time(10, 0.25f);
}

static CarData& Record(s32 liSlot, ::EActiveRaceCarIndex leIndex, BrnNetwork::NetworkPlayerID lID,
                       s32 liPosition, bool lbImproved)
{
    CarData& lrCar = Scoring().maCarData[liSlot];
    lrCar.SetActiveRaceCarIndex(leIndex);
    lrCar.SetNetworkPlayerID(lID);
    lrCar.GetScoreData()->SetRacePosition(liPosition);
    lrCar.GetScoreData()->SetRacePositionImproved(lbImproved);
    lrCar.GetScoreData()->SetCompletedLaps(0);
    return lrCar;
}

static GsmIO::OutputBuffer& FreshOutput()
{
    std::memset(gaOutputStorage, 0, sizeof(gaOutputStorage));
    GsmIO::OutputBuffer& lrOut = *reinterpret_cast<GsmIO::OutputBuffer*>(gaOutputStorage);
    lrOut.mGameModeElapsedTime = CgsSystem::Time(99, 0.5f);   // a stale value the tail must overwrite
    for (s32 liRow = 0; liRow < 8; ++liRow)
    {
        lrOut.mGameStateToNetworkInterface.maMapping[liRow].mNetworkPlayerID     = -1;
        lrOut.mGameStateToNetworkInterface.maMapping[liRow].meActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
    }
    lrOut.mGameStateToGuiInterface.Construct();
    return lrOut;
}

int main()
{
    static GameStateModule lModule;
    lModule.mModeManager.mpScoringSystem = &Scoring();

    CgsSystem::TimerStatusInterface lTimers;
    std::memset(&lTimers, 0, sizeof(lTimers));
    lTimers.GetSimTimerStatus()->mTime  = CgsSystem::Time(25, 0.75f);   // sim "now"
    lTimers.GetGameTimerStatus()->mTime = CgsSystem::Time(70, 0.0f);    // a different clock

    // ---- 1. the player (slot 2) gained a place this frame ---------------------------------------
    {
        ResetScoring();
        Record(0, ::E_ACTIVE_RACE_CAR_INDEX_2, 0x1234, 1, true);    // the player, now 1st
        Record(1, ::E_ACTIVE_RACE_CAR_INDEX_5, 0x0055, 2, false);   // a rival
        GsmIO::OutputBuffer& lrOut = FreshOutput();
        lModule.mpOutputBuffer = &lrOut;
        lModule.mePlayerActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_2;

        lModule.EmmPreWorldUpdateTailBringUp(lTimers);

        Check(lrOut.miElapsedTimeSets == 1 && lrOut.mGameModeElapsedTime.GetSeconds() == 15 &&
              lrOut.mGameModeElapsedTime.GetFraction() == 0.5f,
              "(1) SetGameModeElapsedTime(GetRaceCarTotalTime(player, SIM now)) == 25.75 - 10.25 == 15.5 s");
        const BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface& lrNet = lrOut.mGameStateToNetworkInterface;
        Check(lrNet.GetActiveRaceCarIndex(0x1234) == ::E_ACTIVE_RACE_CAR_INDEX_2 &&
              lrNet.GetActiveRaceCarIndex(0x0055) == ::E_ACTIVE_RACE_CAR_INDEX_5,
              "(2) the network mapping holds {0x1234 -> 2, 0x55 -> 5} (SetActiveRaceCarIndex(lp+0x148, lp+0x144))");
        s32 liRows = 0;
        for (s32 liRow = 0; liRow < 8; ++liRow)
            liRows += (lrNet.maMapping[liRow].meActiveRaceCarIndex != ::E_ACTIVE_RACE_CAR_INDEX_INVALID) ? 1 : 0;
        Check(liRows == 2, "(2) only the two scored cars are mapped (slots without a record are skipped)");
        const GsmIO::GameStateToGuiInterface::OvertakeEventQueue* lpOvertakes =
            lrOut.mGameStateToGuiInterface.GetOvertakeEventQueue();
        Check(lpOvertakes->GetLength() == 1 && lpOvertakes->GetEvent(0).mu8NewPosition == 1 &&
              lpOvertakes->GetEvent(0).meActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_2,
              "(3) one overtake record {position 1 @+0, slot 2 @+4} on the GUI interface's +0xC8 queue");
    }

    // ---- 2. no place gained: no record, the rest unchanged ----------------------------------------
    {
        ResetScoring();
        Record(3, ::E_ACTIVE_RACE_CAR_INDEX_0, 0x0777, 4, false);
        GsmIO::OutputBuffer& lrOut = FreshOutput();
        lModule.mpOutputBuffer = &lrOut;
        lModule.mePlayerActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_0;

        lModule.EmmPreWorldUpdateTailBringUp(lTimers);

        Check(lrOut.mGameStateToGuiInterface.GetOvertakeEventQueue()->GetLength() == 0,
              "(3) mbRacePositionImproved (+0x44) clear -> no overtake record");
        Check(lrOut.mGameModeElapsedTime.GetSeconds() == 15 &&
              lrOut.mGameStateToNetworkInterface.GetActiveRaceCarIndex(0x0777) == ::E_ACTIVE_RACE_CAR_INDEX_0,
              "(1)/(2) still run: elapsed time and mapping published");
    }

    // ---- 3. the race is over for the player: the elapsed time is the lap sum ------------------------
    {
        ResetScoring();
        CarData& lrPlayer = Record(0, ::E_ACTIVE_RACE_CAR_INDEX_1, 0x0101, 3, false);
        lrPlayer.GetScoreData()->SetCompletedLaps(1);
        lrPlayer.GetScoreData()->maaLapTimes[0] = CgsSystem::Time(61, 0.125f);
        GsmIO::OutputBuffer& lrOut = FreshOutput();
        lModule.mpOutputBuffer = &lrOut;
        lModule.mePlayerActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_1;

        lModule.EmmPreWorldUpdateTailBringUp(lTimers);

        Check(lrOut.mGameModeElapsedTime.GetSeconds() == 61 && lrOut.mGameModeElapsedTime.GetFraction() == 0.125f,
              "(1) a finished car publishes its lap-time sum (GetRaceCarTotalTime's completed arm)");
    }

    // ---- 4. no player car: Time(0.0f), no record, mapping still refreshed --------------------------
    {
        ResetScoring();
        Record(0, ::E_ACTIVE_RACE_CAR_INDEX_4, 0x0404, 1, true);
        GsmIO::OutputBuffer& lrOut = FreshOutput();
        lModule.mpOutputBuffer = &lrOut;
        lModule.mePlayerActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;

        lModule.EmmPreWorldUpdateTailBringUp(lTimers);

        Check(lrOut.miElapsedTimeSets == 1 && lrOut.mGameModeElapsedTime.GetSeconds() == 0 &&
              lrOut.mGameModeElapsedTime.GetFraction() == 0.0f,
              "(1) player -1: the published elapsed time is Time(flt_82001CC0 == 0.0f), not the stale value");
        Check(lrOut.mGameStateToGuiInterface.GetOvertakeEventQueue()->GetLength() == 0,
              "(3) player -1: no overtake record even though a car's flag is set");
        Check(lrOut.mGameStateToNetworkInterface.GetActiveRaceCarIndex(0x0404) == ::E_ACTIVE_RACE_CAR_INDEX_4,
              "(2) player -1: the mapping loop still runs");
    }

    Check(gAsserts == 0, "no assert on the way");

    std::printf("FxFlowEmmTail: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
