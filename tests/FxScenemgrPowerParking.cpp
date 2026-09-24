// FX-SCENEMGR (crash parity 2026-09-24, item 4): the Power Parking CONSUMER -- the embedded
// PowerParkingManager, RaceCarEntityModule::ProcessPowerParking (ARTIST 0x822CDF10) and
// UpdatePowerParking (0x822FF5B0), both absent before (no body, no call, no member).
// run_fxscenemgr_power_parking.py extracts VERBATIM:
//   RaceCarEntityModule::ProcessPowerParking            (BrnRaceCarEntityModule_CrashExit.cpp)
//   RaceCarEntityModule::UpdatePowerParking             (BrnRaceCarEntityModule_NearMissTailgate.cpp)
//   PowerParkingManager::Construct / SetNearbyParkedTrafficData   (PowerParking/BrnPowerParkingManager.cpp)
//   PowerParkingDebugComponent::Construct               (PowerParking/BrnPowerParkingDebugComponent.cpp)
//   RaceCarToTrafficInterface::SetFlag                  (SharedIO/BrnRaceCarToTrafficInterface.h)
//   the two call-site GATES (PostSceneUpdate / PostPhysicsUpdate conditions)
// A body the revision lacks is replayed as an empty stand-in, a missing gate as `false`.
//   ProcessPowerParking 0x822CDF2C  traffic parked data seeds the running count + four minima
//                       0x822CDF90  35 global cars: IsInWorld && IsNetworkDriven && !IsCrashing ->
//                                   CheckVehicleForPowerPark(player pos, player dir, car pos, car dir)
//                                   true -> ++count, ++players
//                       0x822CE104  IsPowerParking -> SetNearbyParkedTrafficData (+0x6C..+0x80)
//                       0x822CE144  RaceCarToTrafficInterface bit 0 = IsPowerParking (ori 1 / clrrwi)
//   UpdatePowerParking  0x822FF5CC  Update(meGameModeType, mfTimeStep, GetActiveRaceCar(player),
//                                   &mPlayerVehicleControls, lpOutput->GetGameEventQueue())
//   Construct (inlined) 0x822FDB14  stb 0 +0x94 ; 0x822FDB1C stw this -> debug component +0x0C
//   gates               0x822FE558 / 0x82307758  !mbIsInGameMode || meGameModeType == 15
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h"
#include <cstdio>
#include <cstring>
#include <vector>

static unsigned guAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { ++guAssertions; std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); return 0; }
void* EndAssert() { return nullptr; }
}
}

// BrnGameStateSharedIO.h:59 EGameModeType -- the values the gates and the tests name.
namespace BrnGameState { namespace GameStateModuleIO {
enum EGameModeType { E_MODE_NONE = -1, E_MODE_OFFLINE_RACE = 0, E_MODE_STUNT_ATTACK = 7, E_MODE_ONLINE_FREE_BURN_LOBBY = 15 };
} }

static Vector3 V(f32 x, f32 y, f32 z) { Vector3 v; v.x = x; v.y = y; v.z = z; v.w = 0.0f; return v; }

namespace BrnWorld {

// ---- the two pieces of the scorer this item touches ---------------------------------------------
struct PowerParkingManager;
class PowerParkingDebugComponent {
public:
    PowerParkingManager* mpPowerParkingManager = reinterpret_cast<PowerParkingManager*>(0x1);
    void Construct(PowerParkingManager* lpPowerParkingManager);
};

class ActiveRaceCar;
namespace RaceCarEntityModuleIO { struct GameEventQueue; }

struct UpdateCall { BrnGameState::GameStateModuleIO::EGameModeType meMode; f32 mfStep; ActiveRaceCar* mpCar;
                    PlayerVehicleControls* mpControls; RaceCarEntityModuleIO::GameEventQueue* mpQueue; };
static std::vector<UpdateCall> gaUpdateCalls;
static bool gbUpdateStartsPark = false;

struct PowerParkingManager {
    bool mbPowerParkInProgress = false;
    f32  mfTimeUntilDisplayOutcome = 0.0f;
    u32  muNearbyParkedCarCount = 777u, muNearbyParkedPlayerCount = 777u;
    f32  mfClosestDistanceSq = 777.0f, mfSecondClosestDistanceSq = 777.0f, mfClosestAngleDiff = 777.0f, mfClosestPerpendicularDist = 777.0f;
    PowerParkingDebugComponent mPowerParkingDebugComponent;
    bool mbDebugForcePowerPark = true;

    void Construct();
    bool IsPowerParking() const { return mbPowerParkInProgress; }
    void SetNearbyParkedTrafficData(u32, u32, f32, f32, f32, f32);
    void Update(BrnGameState::GameStateModuleIO::EGameModeType leMode, f32 lfStep, ActiveRaceCar* lpCar,
                PlayerVehicleControls* lpControls, RaceCarEntityModuleIO::GameEventQueue* lpQueue)
    {
        gaUpdateCalls.push_back(UpdateCall{ leMode, lfStep, lpCar, lpControls, lpQueue });
        if (gbUpdateStartsPark) mbPowerParkInProgress = true;
    }
};
#include "fxsm_pp_manager.inc"

// ---- race cars ------------------------------------------------------------------------------------
class ActiveRaceCar {
public:
    bool mbCrashing = false;
    Vector3 mPosition = V(0, 0, 0), mDirection = V(0, 0, 1);
    mutable int miCrashingReads = 0;
    bool IsCrashing() const { ++miCrashingReads; return mbCrashing; }
    Vector3 GetPosition() const { return mPosition; }
    Vector3 GetDirection() const { return mDirection; }
};

class RaceCar {
public:
    u8 muType = 3;   // E_RACE_CAR_TYPE_INACTIVE
    Vector3 mPosition = V(0, 0, 0), mDirection = V(0, 0, 1);
    ActiveRaceCar* mpActiveRaceCar = nullptr;
    bool IsInWorld() const { return muType != 3; }
    bool IsNetworkDriven() const { return muType == 2; }
    ActiveRaceCar* GetActiveRaceCar() { return mpActiveRaceCar; }
    Vector3 GetPosition() const { return mPosition; }
    Vector3 GetDirection() const { return mDirection; }
};

// The candidacy test, scripted: its verdict is lVehiclePos.y > 0, and a true answer folds every
// argument into the four running values so the caller's wiring is visible in the result.
struct CheckCall { Vector3 mPlayerPos, mPlayerDir, mVehiclePos, mVehicleDir; f32 mafIn[4]; };
static std::vector<CheckCall> gaCheckCalls;
bool CheckVehicleForPowerPark(Vector3 lPlayerPos, Vector3 lPlayerDir, Vector3 lVehiclePos, Vector3 lVehicleDir,
                              f32& lfClosestDistanceSq, f32& lfSecondClosestDistanceSq,
                              f32& lfClosestAngleDiff, f32& lfClosestPerpendicularDist)
{
    gaCheckCalls.push_back(CheckCall{ lPlayerPos, lPlayerDir, lVehiclePos, lVehicleDir,
                                      { lfClosestDistanceSq, lfSecondClosestDistanceSq, lfClosestAngleDiff, lfClosestPerpendicularDist } });
    if (!(lVehiclePos.y > 0.0f)) return false;
    if (lVehiclePos.x < lfClosestDistanceSq) lfClosestDistanceSq = lVehiclePos.x;
    lfSecondClosestDistanceSq += 1.0f;
    lfClosestAngleDiff = lVehicleDir.x;
    lfClosestPerpendicularDist = lPlayerDir.z;
    return true;
}

// ---- IO -------------------------------------------------------------------------------------------
namespace RaceCarEntityModuleIO {
struct TrafficToRaceCarInterface_PreScene {
    u32 muCount = 0; f32 mafData[4] = { 0, 0, 0, 0 };
    mutable int miReads = 0;
    void GetNearbyParkedTrafficData(u32* lpuCount, f32* lpfA, f32* lpfB, f32* lpfC, f32* lpfD) const
    { ++miReads; *lpuCount = muCount; *lpfA = mafData[0]; *lpfB = mafData[1]; *lpfC = mafData[2]; *lpfD = mafData[3]; }
};
struct InputBuffer_PostScene {
    TrafficToRaceCarInterface_PreScene mTraffic;
    const TrafficToRaceCarInterface_PreScene* GetTrafficToRaceCarInterface_PreScene() const { return &mTraffic; }
};
struct RaceCarToTrafficInterface {
    enum Flag : s32 { E_FLAG_PLAYER_IS_POWER_PARKING = 0, E_FLAG_PLAYER_IS_IN_SHOWTIME_ON_GROUND = 1, E_FLAG_COUNT = 2 };
    u32 muFlags = 0;
#include "fxsm_pp_setflag.inc"
};
struct OutputBuffer_PostScene {
    RaceCarToTrafficInterface mRaceCarToTraffic;
    int miPublishes = 0;
    RaceCarToTrafficInterface* GetRaceCarToTrafficInterface() { ++miPublishes; return &mRaceCarToTraffic; }
};
struct InputBuffer_PostPhysics { int miUnused = 0; };
struct GameEventQueue {
    s32 miLength = 0; mutable int miLengthReads = 0;
    s32 GetLength() const { ++miLengthReads; return miLength; }
};
struct OutputBuffer_PostPhysics {
    GameEventQueue mGameEventQueue;
    int miQueueFetches = 0;
    GameEventQueue* GetGameEventQueue() { ++miQueueFetches; return &mGameEventQueue; }
};
}

// ---- the [DIAG] helpers the production bodies call (NOT IN THE X360 BINARY): observed, not printed -
static bool gbDiag = false;
static int giDiagReports = 0, giPublishDiags = 0;
static bool gbDiagWasParking = true;
bool PowerParkDiagEnabled() { return gbDiag; }
void PowerParkDiagReport(const PowerParkingManager&, bool lbWasParking, f32, s32, f32) { ++giDiagReports; gbDiagWasParking = lbWasParking; }
void PowerParkPublishDiag(bool, u32, u32, f32, f32) { ++giPublishDiags; }

// ---- the module -----------------------------------------------------------------------------------
struct RaceCarEntityModule {
    RaceCar maRaceCars[E_GLOBAL_RACE_CAR_INDEX_COUNT];
    ActiveRaceCar maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
    PowerParkingManager mPowerParkingManager;
    bool mbIsInGameMode = false;
    BrnGameState::GameStateModuleIO::EGameModeType meGameModeType = BrnGameState::GameStateModuleIO::E_MODE_NONE;
    f32 mfTimeStep = 0.0f;
    PlayerVehicleControls mPlayerVehicleControls;
    std::vector<int> maGlobalVisits;

    RaceCar* GetGlobalRaceCar(EGlobalRaceCarIndex leIndex) { maGlobalVisits.push_back(static_cast<int>(leIndex)); return &maRaceCars[leIndex]; }
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leIndex) { return &maActiveRaceCars[leIndex]; }

    void ProcessPowerParking(const RaceCarEntityModuleIO::InputBuffer_PostScene* lpInput,
                             RaceCarEntityModuleIO::OutputBuffer_PostScene* lpOutput);
    void UpdatePowerParking(const RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput,
                            RaceCarEntityModuleIO::OutputBuffer_PostPhysics* lpOutput);
};
#include "fxsm_pp_process.inc"
#include "fxsm_pp_update.inc"

// ---- the two call-site gates, extracted from PostSceneUpdate / PostPhysicsUpdate ------------------
struct GateProbe {
    bool mbIsInGameMode;
    BrnGameState::GameStateModuleIO::EGameModeType meGameModeType;
#include "fxsm_pp_gates.inc"
};
}   // namespace BrnWorld

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static bool Same(const Vector3& a, const Vector3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

int main()
{
    using namespace BrnWorld;
    typedef BrnGameState::GameStateModuleIO::EGameModeType Mode;

    // ---- the inlined PowerParkingManager::Construct (0x822FDB14 / 0x822FDB1C) ----------------------
    {
        PowerParkingManager lManager;
        lManager.Construct();
        Check(lManager.mPowerParkingDebugComponent.mpPowerParkingManager == &lManager,
              "C1 Construct: the debug component's back pointer is the manager (stw r10, 0x90(r10) @0x822FDB1C)");
        Check(!lManager.mbDebugForcePowerPark, "C2 Construct: mbDebugForcePowerPark = false (stb 0, 0x94 @0x822FDB14)");
    }

    // ---- ProcessPowerParking: a park in progress -------------------------------------------------
    RaceCarEntityModule* lpModule = new RaceCarEntityModule();
    RaceCarEntityModule& m = *lpModule;
    m.mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_2;
    m.maActiveRaceCars[2].mPosition = V(100.0f, 1.0f, 200.0f);
    m.maActiveRaceCars[2].mDirection = V(0.0f, 0.0f, 7.0f);
    ActiveRaceCar lNetA, lNetCrashing, lNetFar;
    lNetCrashing.mbCrashing = true;
    m.maRaceCars[0].muType = 0;                                      // the player (E_RACE_CAR_TYPE_PLAYER)
    m.maRaceCars[0].mPosition = V(0.5f, 9.0f, 0.0f); m.maRaceCars[0].mpActiveRaceCar = &m.maActiveRaceCars[2];
    m.maRaceCars[1].muType = 1;                                      // a rival (AI)
    m.maRaceCars[1].mPosition = V(0.5f, 9.0f, 0.0f); m.maRaceCars[1].mpActiveRaceCar = &m.maActiveRaceCars[3];
    m.maRaceCars[2].muType = 2;                                      // a network player, parked close
    m.maRaceCars[2].mPosition = V(30.0f, 5.0f, 0.0f); m.maRaceCars[2].mDirection = V(0.5f, 0.0f, 0.0f);
    m.maRaceCars[2].mpActiveRaceCar = &lNetA;
    m.maRaceCars[3].muType = 2;                                      // a network player, crashing
    m.maRaceCars[3].mPosition = V(1.0f, 5.0f, 0.0f); m.maRaceCars[3].mpActiveRaceCar = &lNetCrashing;
    m.maRaceCars[4].muType = 2;                                      // a network player, not nearby
    m.maRaceCars[4].mPosition = V(2.0f, -1.0f, 0.0f); m.maRaceCars[4].mpActiveRaceCar = &lNetFar;

    RaceCarEntityModuleIO::InputBuffer_PostScene lIn;
    lIn.mTraffic.muCount = 3u;
    lIn.mTraffic.mafData[0] = 40.0f; lIn.mTraffic.mafData[1] = 90.0f; lIn.mTraffic.mafData[2] = 0.25f; lIn.mTraffic.mafData[3] = 1.5f;
    RaceCarEntityModuleIO::OutputBuffer_PostScene lOut;
    lOut.mRaceCarToTraffic.muFlags = 2u;                              // bit 1 is the Showtime publish's
    m.mPowerParkingManager.mbPowerParkInProgress = true;
    gaCheckCalls.clear();
    m.ProcessPowerParking(&lIn, &lOut);

    Check(lIn.mTraffic.miReads == 1, "P1 the traffic module's parked data is read once (bl 0x822B54B8 ; lwz +0x20C, lfs +0x210..+0x21C)");
    {
        bool lbOrder = m.maGlobalVisits.size() == 35u;
        for (size_t i = 0; lbOrder && i < m.maGlobalVisits.size(); ++i) lbOrder = (m.maGlobalVisits[i] == static_cast<int>(i));
        Check(lbOrder, "P2 all 35 global race cars are visited in order (r19 = 0..34)");
    }
    Check(gaCheckCalls.size() == 2u && gaCheckCalls[0].mVehiclePos.x == 30.0f && gaCheckCalls[1].mVehiclePos.x == 2.0f,
          "P3 only in-world, NETWORK, not-crashing cars are tested (player, AI, crashing and inactive slots skipped)");
    Check(lNetA.miCrashingReads == 1 && lNetCrashing.miCrashingReads == 1 && lNetFar.miCrashingReads == 1
          && m.maActiveRaceCars[3].miCrashingReads == 0,
          "P4 IsCrashing is asked of each network car's active car only (lbz +0x52A after the type tests)");
    Check(gaCheckCalls.size() == 2u && Same(gaCheckCalls[0].mPlayerPos, V(100.0f, 1.0f, 200.0f))
          && Same(gaCheckCalls[0].mPlayerDir, V(0.0f, 0.0f, 7.0f))
          && Same(gaCheckCalls[0].mVehiclePos, V(30.0f, 5.0f, 0.0f)) && Same(gaCheckCalls[0].mVehicleDir, V(0.5f, 0.0f, 0.0f)),
          "P5 CheckVehicleForPowerPark(v1 player pos, v2 player dir, v3 car pos, v4 car dir) (0x822CE0A4..0x822CE0C0)");
    Check(gaCheckCalls.size() == 2u && gaCheckCalls[0].mafIn[0] == 40.0f && gaCheckCalls[0].mafIn[1] == 90.0f
          && gaCheckCalls[0].mafIn[2] == 0.25f && gaCheckCalls[0].mafIn[3] == 1.5f
          && gaCheckCalls[1].mafIn[0] == 30.0f && gaCheckCalls[1].mafIn[1] == 91.0f,
          "P6 the four minima are the traffic module's values, threaded through every test (var_E0..var_D4)");
    const PowerParkingManager& p = m.mPowerParkingManager;
    Check(p.muNearbyParkedCarCount == 4u && p.muNearbyParkedPlayerCount == 1u,
          "P7 parking: count = traffic 3 + 1 network car, players = 1 (stw r18 +0x6C, r17 +0x70)");
    Check(p.mfClosestDistanceSq == 30.0f && p.mfSecondClosestDistanceSq == 91.0f && p.mfClosestAngleDiff == 0.5f
          && p.mfClosestPerpendicularDist == 7.0f,
          "P8 parking: the four measurements after the ranking reach the scorer (stfs +0x74..+0x80)");
    Check(lOut.mRaceCarToTraffic.muFlags == 3u && lOut.miPublishes == 1,
          "P9 parking: RaceCarToTrafficInterface bit 0 set (ori r11, r11, 1 @0x822CE158), bit 1 untouched");

    // ---- ProcessPowerParking: no park in progress --------------------------------------------------
    {
        RaceCarEntityModule* lpIdle = new RaceCarEntityModule();
        RaceCarEntityModuleIO::OutputBuffer_PostScene lIdleOut;
        lIdleOut.mRaceCarToTraffic.muFlags = 3u;
        lpIdle->mPowerParkingManager.mbPowerParkInProgress = false;
        lpIdle->ProcessPowerParking(&lIn, &lIdleOut);
        const PowerParkingManager& q = lpIdle->mPowerParkingManager;
        Check(q.muNearbyParkedCarCount == 777u && q.muNearbyParkedPlayerCount == 777u && q.mfClosestDistanceSq == 777.0f
              && q.mfClosestPerpendicularDist == 777.0f,
              "P10 not parking: the scorer's nearby data is left alone (beq @0x822CE114 skips the stores)");
        Check(lIdleOut.mRaceCarToTraffic.muFlags == 2u && lIdleOut.miPublishes == 1,
              "P11 not parking: bit 0 is published CLEAR every frame (clrrwi r11, r11, 1 @0x822CE168), bit 1 untouched");
        delete lpIdle;
    }

    // ---- UpdatePowerParking -------------------------------------------------------------------------
    {
        RaceCarEntityModule* lpUpd = new RaceCarEntityModule();
        lpUpd->meGameModeType = BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY;
        lpUpd->mfTimeStep = 0.0333f;
        lpUpd->mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_5;
        RaceCarEntityModuleIO::OutputBuffer_PostPhysics lPhysOut;
        gaUpdateCalls.clear();
        lpUpd->UpdatePowerParking(nullptr, &lPhysOut);
        Check(gaUpdateCalls.size() == 1u, "U1 exactly one PowerParkingManager::Update per step (bl @0x822FF610)");
        Check(gaUpdateCalls.size() == 1u && gaUpdateCalls[0].meMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY
              && gaUpdateCalls[0].mfStep == 0.0333f,
              "U2 r4 = meGameModeType (+0x18368), f1 = mfTimeStep (+0x18398)");
        Check(gaUpdateCalls.size() == 1u && gaUpdateCalls[0].mpCar == &lpUpd->maActiveRaceCars[5]
              && gaUpdateCalls[0].mpControls == &lpUpd->mPlayerVehicleControls,
              "U3 r6 = GetActiveRaceCar(mePlayerActiveRaceCarIndex), r7 = &mPlayerVehicleControls (+0x183A8)");
        Check(gaUpdateCalls.size() == 1u
              && gaUpdateCalls[0].mpQueue == reinterpret_cast<RaceCarEntityModuleIO::GameEventQueue*>(&lPhysOut.mGameEventQueue),
              "U4 r8 = lpOutput->GetGameEventQueue() (bl 0x822B67D0); lpInput is never read (passed NULL here)");

        // the [DIAG] plumbing: pre-step state reaches the report, and the console call is unchanged
        gbDiag = true; gbUpdateStartsPark = true; gaUpdateCalls.clear(); giDiagReports = 0;
        lpUpd->mPowerParkingManager.mbPowerParkInProgress = false;
        lpUpd->UpdatePowerParking(nullptr, &lPhysOut);
        Check(gaUpdateCalls.size() == 1u && giDiagReports == 1 && !gbDiagWasParking && lpUpd->mPowerParkingManager.IsPowerParking(),
              "U5 with BRN_POWER_PARK_DIAG the step is still exactly one Update and the witness sees the pre-step state");
        gbDiag = false; gbUpdateStartsPark = false;
        delete lpUpd;
    }

    // ---- the call-site gates -------------------------------------------------------------------------
    {
        struct Row { bool mbInMode; Mode meMode; bool mbExpect; const char* mpcWhat; };
        const Row laRows[] = {
            { false, BrnGameState::GameStateModuleIO::E_MODE_NONE,                   true,  "free burn (no game mode)" },
            { false, BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE,          true,  "out of a mode, stale type 0" },
            { true,  BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY, true,  "the online free-burn lobby (15)" },
            { true,  BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE,          false, "a race" },
            { true,  BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK,          false, "stunt run (7): excluded at both call sites (Update's own mode-7 arm is not reached from here)" },
            { true,  BrnGameState::GameStateModuleIO::E_MODE_NONE,                   false, "in a mode with type -1" },
        };
        bool lbScene = true, lbPhysics = true;
        for (const Row& r : laRows)
        {
            GateProbe g{ r.mbInMode, r.meMode };
            if (g.SceneGate() != r.mbExpect) { lbScene = false; std::fprintf(stderr, "  scene gate wrong for %s\n", r.mpcWhat); }
            if (g.PhysicsGate() != r.mbExpect) { lbPhysics = false; std::fprintf(stderr, "  physics gate wrong for %s\n", r.mpcWhat); }
        }
        Check(lbScene, "G1 PostSceneUpdate calls ProcessPowerParking iff !mbIsInGameMode || meGameModeType == 15 (0x822FE558..0x822FE578)");
        Check(lbPhysics, "G2 PostPhysicsUpdate calls UpdatePowerParking iff !mbIsInGameMode || meGameModeType == 15 (0x82307758..0x8230777C)");
    }

    delete lpModule;
    Check(guAssertions == 0, "no assertions (the :84 enum-increment tripwire stays quiet at 35)");
    std::printf("FxScenemgrPowerParking: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
