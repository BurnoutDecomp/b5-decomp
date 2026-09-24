// FX-TAILS-A item 2 (crash parity 2026-09-24): GameStateModule::ClearData @0x8236B3A8, run on the REAL
// BrnGameStateModule.h layout. The production body (extracted from BrnGameStateModule.cpp by
// run_fxtailsa_clear_data.py, with its file-scope K_INVALID_VEHICLE_INDEX) is compiled here; the module is
// raw static storage filled with 0xA5 (no ctor runs, as the rumble chain test does with InputModule), so
// every member ClearData writes must come out with the console's value and every member it does NOT write
// must still read 0xA5. The real AICarOutputInterface::Construct (BrnAICarOutputInterface.cpp) is linked;
// the two RCEntity*OutputInterface::Clear bodies are recording stand-ins (verified in their own tests).
//
// The console's stores (ARTIST export 0x8236B3A8, every one):
//   std 0 +0x456D8/+0x456E0 (car / wheel id) ; stw -1 +0x32DB0/+0x32DB4/+0x32DAC (active index, global
//   index, network seed) ; stw 0 +0x32DBC/+0x32DC0 (the 8 crashing bytes) ; std 0 x3 + stw 0 from +0x32D90
//   (7 tailing times) ; the two interface Clears ; 35 x {FLT_MAX (flt_82CDB9AC), 0x7FFF} on the AI car
//   interface ; carry-queue Clear ; stw 0 +0x3D058 (takedown cache length) ; stb 0 +0x38B71/+0x38B72 ;
//   std 0 +0x38B80 (the cached car-select action's junkyard id -- 8 bytes only) ; stw 0 +0x38B6C/+0x38B60/
//   +0x38B64 ; stb 1 +0x38B73..+0x38B76 ; stb 0 +0x45761 ; stw 0 +0x45758 (stack length) ; sth -1 +0x4575C ;
//   stw 1 +0x45744 ; stw -1 +0x456C8/+0x456CC ; stb 0 +0x456D0..+0x456D2.
// NOT written (checked still 0xA5): +0x38B70 mbWaitForStreaming, +0x32DC4 mbIsFirstUpdate
// (mbSendSetupPlayerCarPending), +0x45760 mbToggleShowtimeBehaviour, +0x4575E, +0x45740, the rest of the
// cached car-select action, the AI interface's route node index.
#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameSource/GameState/BrnGameStateTakedownCache.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cfloat>
#include <cstdio>
#include <cstring>

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0, gActiveClears = 0, gGlobalClears = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        std::printf("  [assert] %s\n", lpcMessage ? lpcMessage : "");
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
}

namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{
    void RCEntityActiveRaceCarOutputInterface::Clear() { ++gActiveClears; }
    void RCEntityGlobalRaceCarOutputInterface::Clear() { ++gGlobalClears; }
}
}

namespace BrnGameState
{
// The production ClearData (and its file-scope constant), extracted verbatim.
#include "fxtailsa_clear_data.inc"
}

using BrnGameState::GameStateModule;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
        ++gFailures;
    std::printf("  %s %s\n", lbPass ? "ok  " : "FAIL", lpcName);
}

template <typename T>
static bool Untouched(const T& lrMember)
{
    const unsigned char* lpBytes = reinterpret_cast<const unsigned char*>(&lrMember);
    for (size_t i = 0; i < sizeof(T); ++i)
        if (lpBytes[i] != 0xA5)
            return false;
    return true;
}

template <typename T>
static unsigned Byte(const T& lrMember) { return *reinterpret_cast<const unsigned char*>(&lrMember); }

alignas(16) static unsigned char gModuleStorage[sizeof(GameStateModule)];
alignas(16) static unsigned char gCacheStorage[sizeof(BrnGameState::TakedownPostWorldCache)];

int main()
{
    std::memset(gModuleStorage, 0xA5, sizeof(gModuleStorage));
    std::memset(gCacheStorage, 0xA5, sizeof(gCacheStorage));
    GameStateModule& lrModule = *reinterpret_cast<GameStateModule*>(gModuleStorage);
    lrModule.mpTakedownCache = reinterpret_cast<BrnGameState::TakedownPostWorldCache*>(gCacheStorage);
    lrModule.mGameEventCarryQueue.Construct();          // Construct @0x82380388 builds it before ClearData
    lrModule.mGameEventCarryQueue.miLength = 7;          // something for ClearData's Clear to undo

    lrModule.ClearData();

    std::printf("player car / race-car indices / seed / crashing / tailing:\n");
    Check(lrModule.mActivePlayerCarId == 0 && lrModule.mActivePlayerWheelId == 0,
          "+0x456D8 / +0x456E0 car and wheel ids = 0 (std 0 @0x8236B3D4 / 0x8236B3E8)");
    Check(lrModule.mePlayerActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
          "+0x32DB0 mePlayerActiveRaceCarIndex = -1 (0x8236B400)");
    Check(lrModule.miPlayerGlobalRaceCarIndex == -1, "+0x32DB4 player global index = -1 (0x8236B404)");
    Check(lrModule.muNetworkGameRandomSeed == 0xFFFFFFFFu, "+0x32DAC muNetworkGameRandomSeed = -1 (0x8236B410)");
    bool lbCrashing = true;
    for (s32 i = 0; i < ::E_ACTIVE_RACE_CAR_INDEX_COUNT; ++i)
        lbCrashing = lbCrashing && Byte(lrModule.maRaceCarCrashing[i]) == 0;
    Check(lbCrashing, "+0x32DBC..+0x32DC3 the 8 crashing bytes = 0 (0x8236B418 / 0x8236B41C)");
    bool lbTailing = true;
    for (u32 i = 0; i < 7; ++i)
        lbTailing = lbTailing && lrModule.mafRivalTailingTimes[i] == 0.0f;
    Check(lbTailing, "+0x32D90..+0x32DAB the 7 rival tailing times = 0 (0x8236B420..0x8236B42C)");

    std::printf("the cached interfaces and queues:\n");
    Check(gActiveClears == 1 && gGlobalClears == 1,
          "RCEntityActive / Global RaceCarOutputInterface::Clear once each (0x8236B430 / 0x8236B43C)");
    bool lbDistances = true, lbSections = true;
    for (s32 i = 0; i < BrnWorld::KI_MAX_OUT_OF_RANGE_RACE_CARS; ++i)
    {
        lbDistances = lbDistances && lrModule.mLastAICarOutputInterface.mafDistanceToCheckpoint[i] == FLT_MAX;
        lbSections  = lbSections  && lrModule.mLastAICarOutputInterface.mauAISections[i] == 0x7FFF;
    }
    Check(lbDistances, "AI car interface: 35 distances = FLT_MAX (flt_82CDB9AC = 0x7F7FFFFF)");
    Check(lbSections, "AI car interface: 35 AI sections = 0x7FFF");
    Check(Untouched(lrModule.mLastAICarOutputInterface.miPlayerRouteNodeIndex),
          "AI car interface: the route node index is not written");
    Check(lrModule.mGameEventCarryQueue.GetLength() == 0, "carry queue Clear()ed (0x8236B484)");
    Check(lrModule.mpTakedownCache->mTakedownEventQueue.GetLength() == 0,
          "takedown event cache length = 0 (stw 0 +0x3D058 @0x8236B4A4)");
    Check(Untouched(lrModule.mpTakedownCache->mRaceCarCrashEventQueue),
          "the crash-event cache beside it is not touched");

    std::printf("streaming / junkyard / pause / controller:\n");
    Check(Byte(lrModule.mbWaitingForStreaming) == 0, "+0x38B71 mbWaitingForStreaming = 0 (0x8236B4AC)");
    Check(Byte(lrModule.mbWaitingToPutPlayerInJunkyard) == 0,
          "+0x38B72 mbWaitingToPutPlayerInJunkyard = 0 (0x8236B4BC)");
    Check(lrModule.mCachedCarSelectChangedAction.mJunkyardId == 0,
          "+0x38B80 mCachedCarSelectChangedAction.mJunkyardId = 0 (std 0 @0x8236B4CC)");
    Check(Untouched(lrModule.mCachedCarSelectChangedAction.mPosition)
          && Untouched(lrModule.mCachedCarSelectChangedAction.mbJunkyardPosIsLeft),
          "...and only those 8 bytes of the cached action");
    Check(lrModule.miStreamingWaitCountdown == 0, "+0x38B6C miStreamingWaitCountdown = 0 (0x8236B4D4)");
    Check(lrModule.miSimPauseFlags == 0, "+0x38B60 pause flags = 0 (0x8236B4E0)");
    Check(lrModule.meControllerState == GameStateModule::E_CONTROLLERSTATE_NOT_IN_GAME,
          "+0x38B64 meControllerState = 0 (0x8236B4F0)");
    bool lbStreams = true;
    for (u32 i = 0; i < 4; ++i)
        lbStreams = lbStreams && Byte(lrModule.mabModuleStreamingComplete[i]) == 1;
    Check(lbStreams, "+0x38B73..+0x38B76 the four module streaming flags = 1 (0x8236B4FC..0x8236B514)");
    Check(Untouched(lrModule.mbWaitForStreaming), "+0x38B70 mbWaitForStreaming is not written");
    Check(Untouched(lrModule.mbSendSetupPlayerCarPending),
          "+0x32DC4 mbIsFirstUpdate (mbSendSetupPlayerCarPending) is not written by ClearData");

    std::printf("the showtime hand-off:\n");
    Check(Byte(lrModule.mbWasInShowtimeGameMode) == 0, "+0x45761 mbWasInShowtimeGameMode = 0 (0x8236B534)");
    Check(lrModule.mShowtimePendingTrafficIndexStack.miLength == 0,
          "+0x45758 the pending-index stack length = 0 (0x8236B53C)");
    Check(lrModule.muShowtimeRequestedTrafficIndex == 0xFFFF,
          "+0x4575C muShowtimeRequestedTrafficIndex = 0xFFFF (sth -1 @0x8236B548)");
    Check(lrModule.miShowtimePendingFrameDelay == 1, "+0x45744 miShowtimePendingFrameDelay = 1 (0x8236B550)");
    Check(Untouched(lrModule.mbToggleShowtimeBehaviour) && Untouched(lrModule.mbShowtimeIntroHasTouchedGround)
          && Untouched(lrModule.meShowtimeBehaviour),
          "+0x45760 / +0x4575E / +0x45740 are not written");

    std::printf("the junction cache:\n");
    Check(lrModule.muCachedJunctionLightTriggerId == 0xFFFFFFFFu && lrModule.muCachedJunctionLogicBoxId == 0xFFFFFFFFu,
          "+0x456C8 / +0x456CC = -1 (0x8236B554 / 0x8236B558)");
    Check(Byte(lrModule.mbJunctionNewlyDiscovered) == 0 && Byte(lrModule.mbCanEnterEventAtJunction) == 0
          && Byte(lrModule.mbAtJunctionWithEvent) == 0,
          "+0x456D0..+0x456D2 = 0 (0x8236B55C..0x8236B564)");

    Check(gAsserts == 0, "no asserts");
    std::printf("FxTailsAClearData: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
