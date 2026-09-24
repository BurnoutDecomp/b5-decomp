// FX-RCEM2 (crash-parity 2026-09-23): the per-slot pre-scene pass and the intro countdown --
// ActiveRaceCar::Update_PreScene @0x822EAE08, RaceCarEntityModule::UpdateRaceCars_PreScene
// @0x822F5578 and HandleGameActions case 29 @0x8230C770 -- extracted VERBATIM by
// run_rcem2_prescene.py and replayed against fixture cars. Neither function had a body before
// the fix; the runner then substitutes empty bodies (== the pre-fix PC, where nothing ran).
//   G61-D2  0x822EB15C  `stb r27(0), 0x783` on EVERY path: mbCrashedIntoWater is cleared once per
//           PreScene, so ActiveRaceCar::Update's time-in-water only counts frames the watchdog
//           (CheckForResetOnTrackConditions) actually saw the car on water.
//   G61-D3  the meOnlineState arms 0..3 (0x822EAE58 / 0x822EAEF0 / 0x822EB034 / 0x822EB0F0).
//   G68-D5  case 29: mbSpawnAIBehindStartGrid ? mfIntroTimer = duration - 1.4f (flt_820148A0);
//           UpdateRaceCars_PreScene: t > 0 -> t -= dt ; t < 0 -> t = -1 (flt_820037C8) and
//           SetAllCarsOnStartLine(1 ROLLING_START, 0 not the player).
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

static unsigned guAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { ++guAssertions; std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

namespace Fixture {

// Shadow the three interface types inside this namespace (the extracted signatures qualify them).
namespace CgsSceneManager { namespace SceneManagerIO { struct InSceneUpdateInterface { int miTag = 1; }; } }
namespace BrnPhysics { namespace Vehicle { struct VehicleInputInterface { int miTag = 2; }; } }
namespace BrnAI { namespace AIModuleIO { struct RaceCarAIInterface { int miTag = 3; }; } }

enum ERaceCarType { E_RACE_CAR_TYPE_PLAYER = 0, E_RACE_CAR_TYPE_AI = 1, E_RACE_CAR_TYPE_NETWORK = 2,
                    E_RACE_CAR_TYPE_INACTIVE = 3 };
struct RaceCar { ERaceCarType meType = E_RACE_CAR_TYPE_AI; ERaceCarType GetType() const { return meType; } };

static std::vector<std::string> gaAccessorOrder;

struct ActiveRaceCar {
    enum EOnlineState : s32 { E_ONLINE_STATE_CONNECTING = 0, E_ONLINE_STATE_NORMAL = 1,
                              E_ONLINE_STATE_LOST_CONTACT = 2, E_ONLINE_STATE_DISCONNECTED = 3 };
    enum ERaceStartState : s32 { E_RACE_START_STATE_ON_START_LINE = 0, E_RACE_START_STATE_ROLLING_START = 1,
                                 E_RACE_START_STATE_RACING = 2 };
    EOnlineState meOnlineState = E_ONLINE_STATE_NORMAL;
    EActiveRaceCarIndex meActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
    bool mbReceivedNetworkDriverControls = false, mbRenderThisFrame = true, mbIsInCarSelectOnline = false;
    bool mbIsDisconnectedFromNetwork = false, mbNotSendingNetworkUpdates = false;
    bool mbCarSelectOnlineStateChanged = false, mbAddedForCollision = true, mbCrashedIntoWater = false;
    bool mbChangeCollisionState = false, mbCollisionStateToChangeTo = false, mbChangeCullingGroup = false;
    s32  mCullingGrouptoChangeTo = 0, miFlashFrequency = 0;
    bool mbActive = true, mbCrashing = false;
    RaceCar  mRaceCar;
    std::vector<std::string> maCalls;
    const void* mpScene = nullptr; const void* mpVehicle = nullptr; const void* mpAI = nullptr;
    int miUpdates = 0;
    bool IsActive() const { return mbActive; }
    bool IsCrashing() const { return mbCrashing; }
    RaceCar* GetGlobalRaceCar() { return &mRaceCar; }
    void AddToCollision(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* s, BrnPhysics::Vehicle::VehicleInputInterface* v) {
        maCalls.push_back("add"); mpScene = s; mpVehicle = v; mbAddedForCollision = true; }
    void RemoveFromCollision(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* s, BrnPhysics::Vehicle::VehicleInputInterface* v) {
        maCalls.push_back("remove"); mpScene = s; mpVehicle = v; mbAddedForCollision = false; }
    void Update_PreScene(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInterface,
                         BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInterface,
                         BrnAI::AIModuleIO::RaceCarAIInterface* lpRaceCarAIInterface);
};
#include "rcem2_update_prescene.inc"

namespace RaceCarEntityModuleIO {
struct OutputBuffer_PreScene {
    typedef CgsSceneManager::SceneManagerIO::InSceneUpdateInterface SceneInputInterface;
    typedef BrnPhysics::Vehicle::VehicleInputInterface VehicleInputInterface;
    typedef BrnAI::AIModuleIO::RaceCarAIInterface RaceCarAIInterface;
    SceneInputInterface mScene; VehicleInputInterface mVehicle; RaceCarAIInterface mAI;
    SceneInputInterface* GetSceneInputInterface() { gaAccessorOrder.push_back("scene"); return &mScene; }
    VehicleInputInterface* GetVehicleInputInterface() { gaAccessorOrder.push_back("vehicle"); return &mVehicle; }
    RaceCarAIInterface* GetRaceCarAIInterface() { gaAccessorOrder.push_back("ai"); return &mAI; }
};
}
typedef RaceCarEntityModuleIO::OutputBuffer_PreScene OutputFixture;

struct RaceCarEntityModule {
    ActiveRaceCar maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    f32  mfIntroTimer = -1.0f;   // Construct @0x822FE2B0
    f32  mfTimeStep = 1.0f / 30.0f;
    bool mbSpawnAIBehindStartGrid = false;
    int  miStartLineCalls = 0;
    f32  mfTimerAtStartLineCall = 0.0f;
    ActiveRaceCar::ERaceStartState meStartLineState = ActiveRaceCar::E_RACE_START_STATE_RACING;
    bool mbStartLineIncludePlayer = true;
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leIndex) {
        CGS_ASSERT(leIndex >= 0 && leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "active index");
        return &maActiveRaceCars[leIndex];
    }
    void SetAllCarsOnStartLine(ActiveRaceCar::ERaceStartState leState, bool lbIncludePlayer) {
        ++miStartLineCalls; meStartLineState = leState; mbStartLineIncludePlayer = lbIncludePlayer;
        mfTimerAtStartLineCall = mfIntroTimer; }
    void UpdateRaceCars_PreScene(RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput);
    void Dispatch(s32 liType, const CgsModule::Event* lpEvent, OutputFixture* lpOutput);
};
#include "rcem2_update_racecars_prescene.inc"
#include "rcem2_prescene_actions.inc"
}   // namespace Fixture

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

int main() {
    using namespace Fixture;
    using namespace BrnGameState::GameStateModuleIO;
    const auto AsEvent = [](const void* lp) { return reinterpret_cast<const CgsModule::Event*>(lp); };

    // ---- G61-D2: the water flag is cleared on every slot, every PreScene -------------------------
    {
        RaceCarEntityModule lModule; OutputFixture lOut;
        for (ActiveRaceCar& lrCar : lModule.maActiveRaceCars) lrCar.mbCrashedIntoWater = true;
        lModule.maActiveRaceCars[4].meOnlineState = ActiveRaceCar::E_ONLINE_STATE_DISCONNECTED;
        lModule.maActiveRaceCars[6].meOnlineState = ActiveRaceCar::E_ONLINE_STATE_CONNECTING;
        lModule.maActiveRaceCars[7].mbActive = false;
        gaAccessorOrder.clear();
        lModule.UpdateRaceCars_PreScene(&lOut);
        bool lbAllClear = true;
        for (const ActiveRaceCar& lrCar : lModule.maActiveRaceCars) if (lrCar.mbCrashedIntoWater) lbAllClear = false;
        Check(lbAllClear, "G61-D2 0x822EB15C: mbCrashedIntoWater cleared for all 8 slots (every online state, active or not)");
        Check(gaAccessorOrder.size() == 24 && gaAccessorOrder[0] == "ai" && gaAccessorOrder[1] == "vehicle"
                  && gaAccessorOrder[2] == "scene",
              "0x822F55A4..0x822F55BC: per slot, the AI (:301), vehicle (:283), scene (:286) accessors");

        // The verifier's scenario: 10 frames on water, 60 dry, 10 on water. Frame order is the
        // console's: PreScene (Update_PreScene) -> PostScene (the watchdog sets the flag while
        // on water) -> PrePhysics (ActiveRaceCar::Update adds dt while the flag is set).
        RaceCarEntityModule lWater; ActiveRaceCar& lrCar = lWater.maActiveRaceCars[0];
        f32 lfTimeInWater = 0.0f;
        const f32 lfDt = 1.0f / 30.0f;
        for (int liFrame = 0; liFrame < 80; ++liFrame) {
            lWater.UpdateRaceCars_PreScene(&lOut);
            const bool lbOnWater = (liFrame < 10) || (liFrame >= 70);
            if (lbOnWater) lrCar.mbCrashedIntoWater = true;
            if (lrCar.mbCrashedIntoWater) lfTimeInWater += lfDt;
        }
        Check(lfTimeInWater < 20.5f * lfDt && lfTimeInWater > 19.5f * lfDt,
              "G61-D2: time in water counts only the 20 water frames (not the 60 dry ones)");
        Check(lfTimeInWater <= 1.0f, "G61-D2: two short water contacts stay under KF_MAX_TIME_IN_WATER (1.0 s)");
    }

    // ---- G61-D3: the online state machine ------------------------------------------------------
    {
        RaceCarEntityModule lModule; OutputFixture lOut;
        ActiveRaceCar* c = lModule.maActiveRaceCars;
        // slot 0 CONNECTING, controls received, active, not in car select -> NORMAL + add
        c[0].meOnlineState = ActiveRaceCar::E_ONLINE_STATE_CONNECTING; c[0].mbReceivedNetworkDriverControls = true;
        c[0].mbAddedForCollision = false; c[0].mbRenderThisFrame = false;
        // slot 1 CONNECTING, no controls -> hidden, stays CONNECTING
        c[1].meOnlineState = ActiveRaceCar::E_ONLINE_STATE_CONNECTING;
        // slot 2 NORMAL, disconnected, not crashing -> remove + DISCONNECTED
        c[2].mbIsDisconnectedFromNetwork = true;
        // slot 3 NORMAL, not sending updates but crashing -> nothing
        c[3].mbNotSendingNetworkUpdates = true; c[3].mbCrashing = true;
        // slot 4 NORMAL, car-select changed, in car select, added -> remove, flag consumed
        c[4].mbCarSelectOnlineStateChanged = true; c[4].mbIsInCarSelectOnline = true; c[4].mRaceCar.meType = E_RACE_CAR_TYPE_NETWORK;
        // slot 5 LOST_CONTACT, updates resumed, in car select -> no add, render, NORMAL
        c[5].meOnlineState = ActiveRaceCar::E_ONLINE_STATE_LOST_CONTACT; c[5].mbIsInCarSelectOnline = true; c[5].mbRenderThisFrame = false;
        // slot 6 LOST_CONTACT, still not sending, not disconnected -> the flash counter
        c[6].meOnlineState = ActiveRaceCar::E_ONLINE_STATE_LOST_CONTACT; c[6].mbNotSendingNetworkUpdates = true; c[6].miFlashFrequency = 30;
        // slot 7 DISCONNECTED -> hidden
        c[7].meOnlineState = ActiveRaceCar::E_ONLINE_STATE_DISCONNECTED;
        lModule.UpdateRaceCars_PreScene(&lOut);
        Check(c[0].meOnlineState == ActiveRaceCar::E_ONLINE_STATE_NORMAL && c[0].mbRenderThisFrame
                  && c[0].maCalls.size() == 1 && c[0].maCalls[0] == "add" && c[0].mpScene == &lOut.mScene && c[0].mpVehicle == &lOut.mVehicle,
              "G61-D3 CONNECTING: controls + active -> NORMAL, rendered, AddToCollision(scene, vehicle)");
        Check(c[1].meOnlineState == ActiveRaceCar::E_ONLINE_STATE_CONNECTING && !c[1].mbRenderThisFrame && c[1].maCalls.empty(),
              "G61-D3 CONNECTING: no controls -> hidden (stb 0, 0x79D), stays connecting");
        Check(c[2].meOnlineState == ActiveRaceCar::E_ONLINE_STATE_DISCONNECTED && c[2].maCalls.size() == 1 && c[2].maCalls[0] == "remove",
              "G61-D3 NORMAL: disconnected, not crashing -> RemoveFromCollision, state 3");
        Check(c[3].meOnlineState == ActiveRaceCar::E_ONLINE_STATE_NORMAL && c[3].maCalls.empty(),
              "G61-D3 NORMAL: a crashing car is not removed");
        Check(c[4].maCalls.size() == 1 && c[4].maCalls[0] == "remove" && !c[4].mbCarSelectOnlineStateChanged,
              "G61-D3 NORMAL: car-select entered while added -> RemoveFromCollision, 0x79B cleared");
        Check(c[5].meOnlineState == ActiveRaceCar::E_ONLINE_STATE_NORMAL && c[5].mbRenderThisFrame && c[5].maCalls.empty(),
              "G61-D3 LOST: updates resumed in car select -> NORMAL, rendered, no AddToCollision");
        Check(c[6].miFlashFrequency == 31 && !c[6].mbRenderThisFrame, "G61-D3 LOST: flash frame 31 -> hidden");
        c[6].miFlashFrequency = 60; lModule.UpdateRaceCars_PreScene(&lOut);
        Check(c[6].miFlashFrequency == 0 && !c[6].mbRenderThisFrame, "G61-D3 LOST: frame 61 -> counter wraps to 0 (still hidden)");
        lModule.UpdateRaceCars_PreScene(&lOut);
        Check(c[6].miFlashFrequency == 1 && c[6].mbRenderThisFrame, "G61-D3 LOST: frames 1..30 -> drawn");
        Check(!c[7].mbRenderThisFrame, "G61-D3 DISCONNECTED: hidden");
    }

    // ---- G68-D5: case 29 arms the intro timer, the countdown releases the rivals ---------------
    {
        StartModeIntroAction lIntro; std::memset(&lIntro, 0, sizeof(lIntro));
        lIntro.mfDurationSeconds = 5.0f;
        RaceCarEntityModule lModule; OutputFixture lOut;
        lModule.UpdateRaceCars_PreScene(&lOut);
        Check(lModule.miStartLineCalls == 0 && lModule.mfIntroTimer == -1.0f, "G68-D5: an unarmed timer (-1) does nothing");

        lModule.Dispatch(E_ACTION_START_MODE_INTRO, AsEvent(&lIntro), &lOut);
        Check(lModule.mfIntroTimer == -1.0f, "G68-D5 case 29: no drive-by start -> timer untouched (lbzx +0x18350 ; beq)");
        lModule.mbSpawnAIBehindStartGrid = true;
        lModule.Dispatch(E_ACTION_START_MODE_INTRO, AsEvent(&lIntro), &lOut);
        Check(lModule.mfIntroTimer == 5.0f - 1.4f, "G68-D5 case 29: mfIntroTimer = duration - flt_820148A0 (1.4)");

        f32 lfExpect = 5.0f - 1.4f; int liExpectFrame = -1;
        for (int liFrame = 0; liFrame < 200 && liExpectFrame < 0; ++liFrame) {
            lfExpect -= lModule.mfTimeStep;
            if (lfExpect < 0.0f) liExpectFrame = liFrame;
        }
        int liCallFrame = -1;
        for (int liFrame = 0; liFrame < 200; ++liFrame) {
            lModule.UpdateRaceCars_PreScene(&lOut);
            if (lModule.miStartLineCalls == 1 && liCallFrame < 0) liCallFrame = liFrame;
        }
        Check(lModule.miStartLineCalls == 1, "G68-D5: SetAllCarsOnStartLine fires exactly once");
        Check(liCallFrame == liExpectFrame, "G68-D5: ...on the frame the countdown goes negative");
        Check(lModule.meStartLineState == ActiveRaceCar::E_RACE_START_STATE_ROLLING_START && !lModule.mbStartLineIncludePlayer,
              "G68-D5: SetAllCarsOnStartLine(1 ROLLING_START, 0 = not the player)");
        Check(lModule.mfIntroTimer == -1.0f, "G68-D5: timer parked at flt_820037C8 (-1.0) after expiry");
        Check(lModule.mfTimerAtStartLineCall == -1.0f,
              "G68-D5: the -1.0 store precedes the call (stfs @0x822F5658, bl @0x822F565C)");
    }

    // ---- G68-D5 NaN POLARITY (FX-RCEM4, reviewer A on 65eadffe, 2026-09-24) ---------------------
    // `ble cr6` @0x822F5624 (bc 4,25: branch when GT is clear) and `bge cr6` @0x822F5640 (bc 4,24:
    // branch when LT is clear) are both TAKEN on an unordered fcmpu, so a NaN SKIPS each leg.
    {
        const f32 lfNan = std::numeric_limits<f32>::quiet_NaN();
        OutputFixture lOut;
        RaceCarEntityModule lNanTimer; lNanTimer.mfIntroTimer = lfNan;
        lNanTimer.UpdateRaceCars_PreScene(&lOut);
        Check(lNanTimer.miStartLineCalls == 0 && lNanTimer.mfIntroTimer != lNanTimer.mfIntroTimer,
              "G68-D5 NaN: a NaN timer is skipped (ble @0x822F5624 taken): no rolling start, the NaN stays");
        RaceCarEntityModule lNanStep; lNanStep.mfIntroTimer = 0.5f; lNanStep.mfTimeStep = lfNan;
        lNanStep.UpdateRaceCars_PreScene(&lOut);
        Check(lNanStep.miStartLineCalls == 0 && lNanStep.mfIntroTimer != lNanStep.mfIntroTimer,
              "G68-D5 NaN: a NaN step stores the NaN difference, then the expiry is skipped (bge @0x822F5640 taken)");
        RaceCarEntityModule lZero; lZero.mfIntroTimer = 0.0f;
        lZero.UpdateRaceCars_PreScene(&lOut);
        Check(lZero.miStartLineCalls == 0 && lZero.mfIntroTimer == 0.0f, "G68-D5: a zero timer is skipped (0 > 0 is false)");
        RaceCarEntityModule lExact; lExact.mfIntroTimer = 0.25f; lExact.mfTimeStep = 0.25f;
        lExact.UpdateRaceCars_PreScene(&lOut);
        Check(lExact.miStartLineCalls == 0 && lExact.mfIntroTimer == 0.0f,
              "G68-D5: counting down to exactly 0 does not expire (0 < 0 is false)");
    }

    Check(guAssertions == 0, "valid fixtures fire no assertions");
    std::printf("Rcem2PreScene: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
