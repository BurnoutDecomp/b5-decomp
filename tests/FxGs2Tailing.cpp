// FX-GS2 (crash parity 2026-09-23, G10-D11): the PRODUCTION GameStateModule::CheckForTailingRivals
// and Get/SetRivalTailingTime (extracted from src/GameSource/GameState/GameStateModule_gUI_00.cpp),
// plus GameStateToGuiInterface::AddOnTailEvent / Construct (BrnGameStateToGuiIOInterfaces.cpp), by
// run_fxgs2_tailing.py. GameStateModule is far too large to stand up here, so this TU supplies a
// fixture GameStateModule carrying exactly the members and accessors the extracted bodies name, and
// small physics / scoring fixtures; the GUI interface is the real one.
//
// Checked against the ARTIST asm of CheckForTailingRivals @0x82375F90:
//   gates   mode type == 0 (offline race) or the current mode is online; controller state == 3;
//           the player's used-car bit; |mLinearVelocity| > 50.0 (flt_820138DC)
//   per slot (not the player's): clock += dt while player < rival && !((player + 20.0) < rival)
//           && the rival's used bit && rival mfSpeedMPH > 50.0; else fsel(clock, 0.0, clock);
//           !(clock < 3.0 (flt_8202AC20)) -> AddOnTailEvent(GetRivalId(slot), slot), clock = 0.0
//   slot 7 is the word after the f32[7] clock array: muNetworkGameRandomSeed
#include "types.hpp"
#include "BrnCommonTypes.h"                                                 // Vector3, CgsID
#include "GameSource/BurnoutConstants.h"                                    // ::EActiveRaceCarIndex (+ its operator++)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                      // GameStateModuleIO::EGameModeType
#include "GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.h"    // the real GameStateToGuiInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"    // the real RaceCarState
#include "rw/math/vpu/vector3_operation.h"                                  // Magnitude
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // the [tailing] witness (silent here)
#include <cstdio>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include <string.h>
#include <limits>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }   // the [tailing] witness stays silent
}

// ---- physics fixtures: the vehicle output's used-car bits around the REAL RaceCarState ---------
// (BrnVehicleEvents.h reaches this TU through the GUI interface header since G11-D5 typed its crash
// queue; the two fields the body reads are mLinearVelocity, console +0x330, and mfSpeedMPH, +0x3CC.)
namespace BrnPhysics
{
namespace Vehicle
{
    struct UsedCarBits
    {
        u8 muBits;
        bool IsBitSet(u32 luIndex) const
        {
            CGS_ASSERT(luIndex < 8u, "invalid index");
            return ((muBits >> luIndex) & 1u) != 0u;
        }
    };
    struct VehicleOutputInterface
    {
        UsedCarBits  mUsedRaceCars;
        RaceCarState maRaceCars[8];
        const UsedCarBits&  GetUsedCarsBitArray() const { return mUsedRaceCars; }
        const RaceCarState* GetRaceCar(u32 luRaceCarIndex) const { return &maRaceCars[luRaceCarIndex]; }
    };
}
}

// ---- game-state fixtures ------------------------------------------------------------------------
namespace BrnGameState
{
class GameMode
{
public:
    bool mbIsOnline = false;
    bool IsOnline() const { return mbIsOnline; }
};

class ScoringSystem
{
public:
    f32 mafDistanceToFinish[8] = {};
    f32 GetRaceCarDistanceToFinish(::EActiveRaceCarIndex leIndex) const { return mafDistanceToFinish[leIndex]; }
};

struct ModeManagerFixture
{
    GameStateModuleIO::EGameModeType meCurrentGameModeType = GameStateModuleIO::E_MODE_OFFLINE_RACE;
    const GameMode*                  mpCurrentGameMode     = nullptr;
    ScoringSystem                    mScoringSystem;
    GameStateModuleIO::EGameModeType GetCurrentGameModeType() const { return meCurrentGameModeType; }
    const GameMode*                  GetCurrentGameMode() const { return mpCurrentGameMode; }
    ScoringSystem*                   GetScoringSystem() { return &mScoringSystem; }
};

struct ActiveRaceCarInterfaceFixture
{
    CgsID GetRivalId(::EActiveRaceCarIndex leIndex) const { return 0x1000u + static_cast<CgsID>(leIndex); }
};

namespace GameStateModuleIO
{
struct OutputBuffer
{
    GameStateToGuiInterface  mGui;
    GameStateToGuiInterface* GetGameStateToGuiInterface() { return &mGui; }
};
}

class GameStateModule
{
public:
    enum EControllerState
    {
        E_CONTROLLERSTATE_NOT_IN_GAME            = 0,
        E_CONTROLLERSTATE_ACTIVE_GAME_MODE_STATE = 3
    };

    ModeManagerFixture            mModeManager;
    EControllerState              meControllerState = E_CONTROLLERSTATE_ACTIVE_GAME_MODE_STATE;
    ::EActiveRaceCarIndex         mePlayer = ::E_ACTIVE_RACE_CAR_INDEX_2;
    ActiveRaceCarInterfaceFixture mLastActiveRaceCarInterface;
    f32                           mafRivalTailingTimes[7] = {};
    u32                           muNetworkGameRandomSeed = 0u;

    ::EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() { return mePlayer; }

    void CheckForTailingRivals(GameStateModuleIO::OutputBuffer* lpOutput,
                               const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutput,
                               f32 lfTimeStep);
    f32  GetRivalTailingTime(s32 liRaceCarIndex) const;
    void SetRivalTailingTime(s32 liRaceCarIndex, f32 lfTime);
};
}

// The production bodies under test (or the runner's labelled empty stand-ins).
#include "tailing_methods.inc"

using namespace BrnGameState;
typedef GameStateModuleIO::GameStateToGuiInterface::OnTailEventQueue OnTailQueue;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static Vector3 V3(f32 lfX, f32 lfY, f32 lfZ)
{
    Vector3 lVector;
    lVector.x = lfX; lVector.y = lfY; lVector.z = lfZ; lVector.w = 0.0f;
    return lVector;
}

static GameStateModule                          gModule;
static GameStateModuleIO::OutputBuffer          gOutput;
static BrnPhysics::Vehicle::VehicleOutputInterface gVehicles;
static GameMode                                 gMode;

// An offline race in progress: every slot in use, everybody at 60 mph, the player (slot 2) doing
// 60 m/s, 1000 m from the finish; every rival level with the player (not behind).
static void Fresh()
{
    gModule = GameStateModule();
    gMode   = GameMode();
    gOutput.mGui.Construct();
    gVehicles.mUsedRaceCars.muBits = 0xFFu;
    for (s32 liSlot = 0; liSlot < 8; ++liSlot)
    {
        gVehicles.maRaceCars[liSlot].mLinearVelocity = V3(60.0f, 0.0f, 0.0f);
        gVehicles.maRaceCars[liSlot].mfSpeedMPH      = 60.0f;
        gModule.mModeManager.mScoringSystem.mafDistanceToFinish[liSlot] = 1000.0f;
    }
}

static void Tick(f32 lfStep) { gModule.CheckForTailingRivals(&gOutput, &gVehicles, lfStep); }
static const OnTailQueue& OnTail() { return gOutput.mGui.mOnTailEventQueue; }
static f32 Clock(s32 liSlot) { return gModule.GetRivalTailingTime(liSlot); }
static void SetClock(s32 liSlot, f32 lfTime) { gModule.SetRivalTailingTime(liSlot, lfTime); }
static f32& Distance(s32 liSlot) { return gModule.mModeManager.mScoringSystem.mafDistanceToFinish[liSlot]; }

int main()
{
    const f32 lfNaN = std::numeric_limits<f32>::quiet_NaN();

    // ---- the clock ------------------------------------------------------------------------------
    {
        Fresh();
        Distance(4) = 1010.0f;                 // 10 m behind the player
        Tick(1.0f);
        Check(Clock(4) == 1.0f && OnTail().GetLength() == 0, "a rival 10 m behind at 60 mph: its clock += dt  @0x82376328");
        Tick(1.0f);
        Tick(1.0f);                            // 3.0: !(3.0 < 3.0)
        Check(OnTail().GetLength() == 1 && OnTail().GetEvent(0).mOfflineRivalCarID == 0x1004u
                  && OnTail().GetEvent(0).meOnTailActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_4,
              "at 3.0 s the on-tail record {GetRivalId(4), 4} goes to gui+0x160  @0x82376378");
        Check(Clock(4) == 0.0f, "...and the clock restarts at 0.0  @0x8237637C");
    }
    {
        Fresh();
        Distance(5) = 1020.0f;                 // exactly 20 m behind: !((1000 + 20) < 1020)
        Distance(6) = 1020.5f;                 // 20.5 m behind
        SetClock(6, 1.0f);
        Tick(0.5f);
        Check(Clock(5) == 0.5f, "exactly 20 m behind still counts (fcmpu / blt)  @0x82376248");
        Check(Clock(6) == 0.0f, "20.5 m behind resets the clock  @0x82376338");
    }
    {
        Fresh();
        SetClock(3, 1.0f);                     // level with the player: player < rival is false
        Distance(4) = 1005.0f; gVehicles.maRaceCars[4].mfSpeedMPH = 50.0f;   // not > 50
        Distance(5) = 1005.0f; gVehicles.maRaceCars[5].mfSpeedMPH = 50.5f;
        Distance(6) = 1005.0f; gVehicles.mUsedRaceCars.muBits = static_cast<u8>(0xFFu & ~(1u << 6));   // not in use
        SetClock(4, 1.0f); SetClock(6, 1.0f);
        Tick(0.5f);
        Check(Clock(3) == 0.0f, "a rival level with the player is not behind: reset  @0x82376238");
        Check(Clock(4) == 0.0f && Clock(5) == 0.5f, "the rival must be doing MORE than 50 mph  @0x82376324");
        Check(Clock(6) == 0.0f, "a rival slot not in use resets  @0x82376308");
    }
    {
        Fresh();
        SetClock(3, -1.0f);
        SetClock(4, lfNaN);
        Tick(0.5f);
        Check(Clock(3) == -1.0f, "fsel keeps a negative clock  @0x82376338");
        Check(OnTail().GetLength() == 1 && OnTail().GetEvent(0).meOnTailActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_4
                  && Clock(4) == 0.0f,
              "a NaN clock survives fsel, passes !(t < 3.0) and posts, then restarts  @0x82376348");
    }
    {
        Fresh();
        gModule.mafRivalTailingTimes[2] = 1.5f;   // the player's own slot
        Distance(4) = 1010.0f;
        Tick(1.0f);
        Check(gModule.mafRivalTailingTimes[2] == 1.5f && Clock(4) == 1.0f, "the player's own slot is skipped  @0x82376224");
    }

    // ---- the slot-7 aliasing ---------------------------------------------------------------------
    {
        Fresh();
        gModule.muNetworkGameRandomSeed = 0xFFFFFFFFu;   // ClearData's -1: a NaN clock for slot 7
        Tick(0.5f);
        Check(OnTail().GetLength() == 1 && OnTail().GetEvent(0).meOnTailActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_7
                  && gModule.muNetworkGameRandomSeed == 0u,
              "slot 7's clock IS muNetworkGameRandomSeed: -1 reads as NaN, posts once, and the word becomes 0.0f  @0x82376384");
    }
    {
        Fresh();
        gModule.mePlayer = ::E_ACTIVE_RACE_CAR_INDEX_7;
        gModule.muNetworkGameRandomSeed = 0xFFFFFFFFu;
        Tick(0.5f);
        Check(OnTail().GetLength() == 0 && gModule.muNetworkGameRandomSeed == 0xFFFFFFFFu,
              "with the player in slot 7 the seed word is skipped like any player slot");
    }

    // ---- the gates -------------------------------------------------------------------------------
    {
        Fresh();
        Distance(4) = 1010.0f;
        gModule.mModeManager.meCurrentGameModeType = static_cast<GameStateModuleIO::EGameModeType>(1);
        gModule.mModeManager.mpCurrentGameMode = &gMode;     // an offline mode, not a race
        Tick(1.0f);
        const bool lbOfflineOther = Clock(4) == 0.0f;
        gMode.mbIsOnline = true;
        gModule.mModeManager.meCurrentGameModeType = GameStateModuleIO::E_MODE_ONLINE_RACE;
        Tick(1.0f);
        Check(lbOfflineOther && Clock(4) == 1.0f,
              "runs in an offline race or any ONLINE mode, not in other offline modes  @0x82375FBC..0x82375FEC");
    }
    {
        Fresh();
        Distance(4) = 1010.0f;
        gModule.meControllerState = GameStateModule::E_CONTROLLERSTATE_NOT_IN_GAME;
        Tick(1.0f);
        Check(Clock(4) == 0.0f, "only in the active-game-mode controller state (3)  @0x82375FFC");
    }
    {
        Fresh();
        Distance(4) = 1010.0f;
        gVehicles.mUsedRaceCars.muBits = static_cast<u8>(0xFFu & ~(1u << 2));   // the player's slot
        Tick(1.0f);
        Check(Clock(4) == 0.0f, "only while the player's car is in use  @0x823760E4");
    }
    {
        Fresh();
        Distance(4) = 1010.0f;
        gVehicles.maRaceCars[2].mLinearVelocity = V3(30.0f, 0.0f, 40.0f);   // |v| == 50: not > 50
        Tick(1.0f);
        const bool lbAtFifty = Clock(4) == 0.0f;
        gVehicles.maRaceCars[2].mLinearVelocity = V3(30.0f, 1.0f, 40.0f);   // |v| > 50
        Tick(1.0f);
        Check(lbAtFifty && Clock(4) == 1.0f, "only while the player's |velocity| is MORE than 50.0  @0x8237618C");
    }

    Check(gAsserts == 0, "valid fixtures fire no assert");
    std::printf("FxGs2Tailing: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
