// FX-RCEM2 (crash-parity 2026-09-23): RaceCarEntityModule::HandleGameActions' drive-thru speed,
// end-of-event, repair-shop, Road Rage damage and Showtime crash-play arms (ARTIST 0x8230BE08),
// extracted VERBATIM from BrnRaceCarEntityModule.cpp by run_rcem2_game_actions.py and dispatched
// against fixture race cars. A missing arm is replayed as the console's `default: break;`, so the
// pre-fix source reports per-check failures rather than failing to build.
//   G68-D3 case 7   0x8230CBE8..0x8230CC20 / 0x8230CC70..0x8230CC7C  regain: fabs(max(min(last,
//                   maxReset), K)); lose: fabs(K); K = flt_82FAD3F4 = 0.44704f * 60.0f (CRT
//                   0x82C4BC58..0x82C4BC70)
//   G68-D4 case 35  0x8230C884..0x8230C914  indicators off; online -> +0x18346 = 1; OFFLINE_RACE &&
//                   byte0 -> RemoveRivals(lpOutput, 0) + mbWonLastEvent = 0, else mbWonLastEvent = 1
//   G68-D6 case 205 0x8230D0EC..0x8230D18C  totalled -> mirror = saved pair; else type 0, amount f
//   G68-D7 case 97  0x8230C608..0x8230C6E8  matching ACTIVE slots (no break): glass flags/fractures
//                   and mfBaseDeformAmount cleared; player: saved/live amount 0, one-shot 1, saved type -1
//   G68-D8 cases 128/140/144/201/273 on mCrashPlayManager, 144's boosted bounce timer = 0.6f
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
using namespace BrnWorld;

struct Vector3 { f32 x, y, z, w; };
static Vector3 operator-(const Vector3& a) { return { -a.x, -a.y, -a.z, -a.w }; }
static Vector3 operator-(const Vector3& a, const Vector3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w }; }
static Vector3 operator*(const Vector3& a, f32 s) { return { a.x * s, a.y * s, a.z * s, a.w * s }; }

// Shadows the real BrnTrigger::BoxRegion inside this namespace (unqualified lookup of the
// leading `BrnTrigger` finds Fixture::BrnTrigger first). 36 bytes == the record's box image.
namespace BrnTrigger {
struct BoxRegion {
    f32 mafPosition[3]; f32 mafDirection[3]; f32 mafDimension[3];
    Vector3 ComputeDirection() const { return { mafDirection[0], mafDirection[1], mafDirection[2], 0.0f }; }
    Vector3 GetPosition() const { return { mafPosition[0], mafPosition[1], mafPosition[2], 0.0f }; }
    f32 GetDimensionZ() const { return mafDimension[2]; }
};
}
namespace BrnAI { namespace AIModuleIO {
struct PlayerControlChangedEvent { bool mbPlayerIsInControl; };
enum { E_EVENT_PLAYER_TAKEN_OVER = 7 };
} }

struct ManagementQueue {
    int miEvents = 0; bool mbLastInControl = false;
    void AddEvent(const BrnAI::AIModuleIO::PlayerControlChangedEvent* lp, int) { ++miEvents; mbLastInControl = lp->mbPlayerIsInControl; }
};
struct RaceCarAIInterface { ManagementQueue mManagementQueue; };
struct OutputFixture {
    RaceCarAIInterface mAI;
    RaceCarAIInterface* GetRaceCarAIInterface() { return &mAI; }
};

struct EntityIdFixture { u32 muValue; };
struct PhysicsState { f32 mfSpeedMPH = 0.0f; EntityIdFixture mEntityId = { 0 }; };

struct ActiveRaceCar {
    struct RenderParams {
        u8  mu8RenderDamageFlags = 0;
        f32 mafCrackedGlassFractureAmount[8] = {};
        void SetRenderDamageFlag(u8 lu8Flags) { mu8RenderDamageFlags = lu8Flags; }
        void SetCrackedGlassFractureAmountN(u32 n, f32 lfValue) {
            CGS_ASSERT(n < 8, "( 0 <= n ) && ( 8 > n )");
            mafCrackedGlassFractureAmount[n] = lfValue;
        }
    };
    bool         mbActive = true;
    PhysicsState mPhysicsState;
    RenderParams mRenderParams;
    f32          mfBaseDeformAmount = -7.0f;
    bool         mbWonLastEvent = false;
    int          miIndicatorCalls = 0;
    bool         mbLeftIndicator = true, mbRightIndicator = true;
    Vector3      mPosition = { 10.0f, 20.0f, 30.0f, 0.0f };
    Vector3      mDirection = { 0.0f, 0.0f, 1.0f, 0.0f };
    Vector3      mVelocity = { 0.0f, 0.0f, 5.0f, 0.0f };
    int          miPlaceRequests = 0;
    Vector3      mPlacePosition = {}, mPlaceDirection = {};
    f32          mfPlaceSpeed = -99.0f;
    bool IsActive() const { return mbActive; }
    const PhysicsState* GetPhysicsState() const { return &mPhysicsState; }
    RenderParams* GetRenderParams() { return &mRenderParams; }
    void SetIndicatorState(bool lbA, bool lbB) { ++miIndicatorCalls; mbRightIndicator = lbA; mbLeftIndicator = lbB; }
    Vector3 GetPosition() const { return mPosition; }
    Vector3 GetDirection() const { return mDirection; }
    Vector3 GetVelocity() const { return mVelocity; }
    void RequestPlaceOnTrack(const Vector3& lPosition, const Vector3& lDirection, f32 lfSpeed) {
        ++miPlaceRequests; mPlacePosition = lPosition; mPlaceDirection = lDirection; mfPlaceSpeed = lfSpeed;
    }
};

struct BoostStrategy { int miForce = 0; bool mbForce = false; void SetForceBoost(bool lb) { ++miForce; mbForce = lb; } };
struct BoostManager {
    BoostStrategy mStrategy;
    f32 mfJustBounceBoostedTimer = -5.0f;
    int miBounceBoosts = 0;
    BoostStrategy* GetBoostStrategy() { return &mStrategy; }
#include "rcem2_bounce_boost.inc"
};

struct CrashPlayManager {
    std::vector<std::string> maCalls;
    s32 miBase = 0, miCombo = 0, miTotal = 0;
    const void* mpLastRecord = nullptr;
    void OnHitOverheadSign() { maCalls.push_back("sign"); }
    void OnVehicleHitConfirmed(s32 liBase, s32 liCombo, s32 liTotal) { maCalls.push_back("hit"); miBase = liBase; miCombo = liCombo; miTotal = liTotal; }
    void OnBounce(const BrnGameState::GameStateModuleIO::JustBouncedAction* lp) { maCalls.push_back("bounce"); mpLastRecord = lp; }
    void OnEnterJunction(const BrnGameState::GameStateModuleIO::SendJunctionPlayerIsAtAction* lp) { maCalls.push_back("junction"); mpLastRecord = lp; }
    void OnEnterRoad(const BrnGameState::GameStateModuleIO::RoadRulesEnterRoadAction* lp) { maCalls.push_back("road"); mpLastRecord = lp; }
};

struct RaceCarEntityModule {
    ActiveRaceCar       maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_2;
    BoostManager        mBoostManager;
    CrashPlayManager    mCrashPlayManager;
    f32  mfLastPlayerCarSpeed = 0.0f;
    bool mbIsInGameMode = false, mbIsInOnlineGameMode = false, mbOnlineModeJustFinished = false;
    BrnGameState::GameStateModuleIO::EGameModeType meGameModeType = BrnGameState::GameStateModuleIO::E_MODE_NONE;
    s32  miPlayerBaseDeformationTypeMirror = 1, miPlayerBaseDeformationTypeSaved = 4;
    f32  mfPlayerBaseDeformAmountMirror = 0.25f, mfPlayerBaseDeformAmountSaved = 0.5f;
    bool mbPlayerBaseDeformRequestPending = false;
    int  miRemoveRivals = 0; const OutputFixture* mpRemoveRivalsOutput = nullptr; bool mbRemoveRivalsArg = true;
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leIndex) {
        CGS_ASSERT(leIndex >= 0 && leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "active index");
        return &maActiveRaceCars[leIndex];
    }
    void RemoveRivals(OutputFixture* lpOutput, bool lb) { ++miRemoveRivals; mpRemoveRivalsOutput = lpOutput; mbRemoveRivalsArg = lb; }
    void Dispatch(s32 liType, const CgsModule::Event* lpEvent, OutputFixture* lpOutput);
};
#include "rcem2_game_actions.inc"
}   // namespace Fixture

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

int main() {
    using namespace BrnGameState::GameStateModuleIO;
    const auto AsEvent = [](const void* lp) { return reinterpret_cast<const CgsModule::Event*>(lp); };
    const f32 kfEntrySpeed = 0.44704f * 60.0f;   // flt_82FAD3F4, CRT 0x82C4BC58

    // ---- G68-D3: action 7, the drive-thru placements ------------------------------------------
    {
        SetPlayerCarDriverAction lAction; std::memset(&lAction, 0, sizeof(lAction));
        lAction.mbIsDriveThru = true;
        const f32 lafBox[9] = { 100.0f, 0.0f, 200.0f,  0.0f, 0.0f, 1.0f,  4.0f, 2.0f, 8.0f };
        std::memcpy(lAction.maDriveThruBoxRegion, lafBox, sizeof(lafBox));

        struct Case { f32 last, max, expect; const char* name; };
        const Case laCases[] = {
            { 10.0f, 40.0f, kfEntrySpeed, "G68-D3 regain: slow entry (10 m/s) leaves at the 60 mph floor (fsel vs flt_82FAD3F4)" },
            { 50.0f, 40.0f, 40.0f,        "G68-D3 regain: fast entry clamps to mfMaxResetSpeed (fsel min)" },
            { 30.0f, 35.0f, 30.0f,        "G68-D3 regain: 30 m/s entry keeps its speed (above the floor, below the cap)" },
            { 5.0f,  20.0f, kfEntrySpeed, "G68-D3 regain: a cap below the floor still leaves at the floor (max after min)" },
        };
        for (const Case& c : laCases) {
            Fixture::RaceCarEntityModule lModule; Fixture::OutputFixture lOut;
            lModule.mfLastPlayerCarSpeed = c.last;
            lAction.meCarControl = BrnWorld::E_CAR_CONTROL_ENTITY_MODULE;
            lAction.mfMaxResetSpeed = c.max;
            lModule.Dispatch(E_ACTION_SET_PLAYER_CAR_DRIVER, AsEvent(&lAction), &lOut);
            const Fixture::ActiveRaceCar& lrCar = lModule.maActiveRaceCars[2];
            Check(lrCar.miPlaceRequests == 1 && lrCar.mfPlaceSpeed == c.expect, c.name);
        }
        {   // lose control: stationary in the pre-fix source, K on the console
            Fixture::RaceCarEntityModule lModule; Fixture::OutputFixture lOut;
            lAction.meCarControl = static_cast<BrnWorld::CarControl>(2);
            lModule.Dispatch(E_ACTION_SET_PLAYER_CAR_DRIVER, AsEvent(&lAction), &lOut);
            const Fixture::ActiveRaceCar& lrCar = lModule.maActiveRaceCars[2];
            Check(lrCar.miPlaceRequests == 1 && lrCar.mfPlaceSpeed == kfEntrySpeed,
                  "G68-D3 lose: placed at fabs(flt_82FAD3F4) == 26.8224 m/s, not at rest");
            Check(lrCar.mPlacePosition.z == 200.0f - 1.0f * (8.0f * 0.5f) && lrCar.mPlaceDirection.z == 1.0f,
                  "G68-D3 lose: box entrance position/direction unchanged");
        }
    }

    // ---- G68-D4: action 35 ---------------------------------------------------------------------
    {
        FinishedModeNotifyAction lNotify; lNotify.mbPlayerWon = false;
        Fixture::RaceCarEntityModule lModule; Fixture::OutputFixture lOut;
        lModule.meGameModeType = E_MODE_ROAD_RAGE;
        lModule.Dispatch(E_ACTION_FINISHED_MODE_NOTIFY, AsEvent(&lNotify), &lOut);
        const Fixture::ActiveRaceCar& lrPlayer = lModule.maActiveRaceCars[2];
        Check(lrPlayer.mbWonLastEvent, "G68-D4 35: mbWonLastEvent = 1 (stb r23, 0x78C @0x8230C910)");
        Check(lrPlayer.miIndicatorCalls == 1 && !lrPlayer.mbLeftIndicator && !lrPlayer.mbRightIndicator,
              "G68-D4 35: player indicators off (SetIndicatorState(0,0) @0x8230C89C)");
        Check(!lModule.mbOnlineModeJustFinished && lModule.miRemoveRivals == 0,
              "G68-D4 35: offline -> no park latch, no RemoveRivals");

        Fixture::RaceCarEntityModule lOnline; lOnline.mbIsInOnlineGameMode = true;
        lOnline.Dispatch(E_ACTION_FINISHED_MODE_NOTIFY, AsEvent(&lNotify), &lOut);
        Check(lOnline.mbOnlineModeJustFinished, "G68-D4 35: online -> mbOnlineModeJustFinished = 1 (+0x18346)");

        Fixture::RaceCarEntityModule lRace; lRace.meGameModeType = E_MODE_OFFLINE_RACE;
        lNotify.mbPlayerWon = true;
        lRace.maActiveRaceCars[2].mbWonLastEvent = true;
        lRace.Dispatch(E_ACTION_FINISHED_MODE_NOTIFY, AsEvent(&lNotify), &lOut);
        Check(lRace.miRemoveRivals == 1 && lRace.mpRemoveRivalsOutput == &lOut && !lRace.mbRemoveRivalsArg
                  && !lRace.maActiveRaceCars[2].mbWonLastEvent,
              "G68-D4 35: OFFLINE_RACE with byte0 -> RemoveRivals(lpOutput, 0) and mbWonLastEvent = 0");
    }

    // ---- G68-D6: action 205 --------------------------------------------------------------------
    {
        RoadRagePlayerDamageAction lDamage; std::memset(&lDamage, 0, sizeof(lDamage));
        lDamage.mfHowCloseToTotalled = 0.6f;
        Fixture::RaceCarEntityModule lModule; Fixture::OutputFixture lOut;
        lModule.Dispatch(E_ACTION_ROAD_RAGE_PLAYER_DAMAGE, AsEvent(&lDamage), &lOut);
        Check(lModule.miPlayerBaseDeformationTypeMirror == 0 && lModule.mfPlayerBaseDeformAmountMirror == 0.6f,
              "G68-D6 205: not totalled -> mirror = (type 0, amount = mfHowCloseToTotalled)");
        Check(lModule.miPlayerBaseDeformationTypeSaved == 4 && lModule.mfPlayerBaseDeformAmountSaved == 0.5f,
              "G68-D6 205: the saved pair is not written");
        lDamage.mbPlayerTotalled = true; lDamage.mfHowCloseToTotalled = 1.0f;
        lModule.Dispatch(E_ACTION_ROAD_RAGE_PLAYER_DAMAGE, AsEvent(&lDamage), &lOut);
        Check(lModule.miPlayerBaseDeformationTypeMirror == 4 && lModule.mfPlayerBaseDeformAmountMirror == 0.5f,
              "G68-D6 205: totalled -> mirror restored from the saved pair (+0x184D4/+0x184E0)");
    }

    // ---- G68-D7: action 97 ---------------------------------------------------------------------
    {
        u8 lacPayload[144]; std::memset(lacPayload, 0, sizeof(lacPayload));
        const u32 luEntity = 0x02020005u; std::memcpy(lacPayload + 128, &luEntity, 4);
        Fixture::RaceCarEntityModule lModule; Fixture::OutputFixture lOut;
        for (int i = 0; i < 8; ++i) {
            Fixture::ActiveRaceCar& lrCar = lModule.maActiveRaceCars[i];
            lrCar.mPhysicsState.mEntityId.muValue = 0x02020000u + static_cast<u32>(i);
            lrCar.mRenderParams.mu8RenderDamageFlags = 0x33;
            for (int p = 0; p < 8; ++p) lrCar.mRenderParams.mafCrackedGlassFractureAmount[p] = 0.5f;
            lrCar.mfBaseDeformAmount = 0.75f;
        }
        lModule.mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_5;
        lModule.maActiveRaceCars[3].mPhysicsState.mEntityId.muValue = luEntity;   // a second match
        lModule.maActiveRaceCars[3].mbActive = false;                              // ...but inactive
        lModule.Dispatch(E_ACTION_BODY_SHOP_DRIVE_THRU, AsEvent(lacPayload), &lOut);
        const Fixture::ActiveRaceCar& lrCar = lModule.maActiveRaceCars[5];
        bool lbGlass = lrCar.mRenderParams.mu8RenderDamageFlags == 0;
        for (int p = 0; p < 8; ++p) if (lrCar.mRenderParams.mafCrackedGlassFractureAmount[p] != 0.0f) lbGlass = false;
        Check(lbGlass, "G68-D7 97: matching car's glass flags + 8 fractures cleared (0x1BF6 / 0x1BF8..)");
        Check(lrCar.mfBaseDeformAmount == 0.0f, "G68-D7 97: mfBaseDeformAmount = 0.0 (stfs f30, 0x7CC)");
        Check(lModule.mfPlayerBaseDeformAmountSaved == 0.0f && lModule.mfPlayerBaseDeformAmountMirror == 0.0f
                  && lModule.mbPlayerBaseDeformRequestPending && lModule.miPlayerBaseDeformationTypeSaved == -1,
              "G68-D7 97: player -> saved/live amount 0, one-shot armed, saved type -1");
        Check(lModule.miPlayerBaseDeformationTypeMirror == 1, "G68-D7 97: the live TYPE mirror (+0x184D0) is not written");
        Check(lModule.maActiveRaceCars[3].mfBaseDeformAmount == 0.75f && lModule.maActiveRaceCars[4].mfBaseDeformAmount == 0.75f,
              "G68-D7 97: inactive or non-matching slots untouched");

        Fixture::RaceCarEntityModule lAi; lAi.mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
        lAi.maActiveRaceCars[6].mPhysicsState.mEntityId.muValue = luEntity;
        lAi.Dispatch(E_ACTION_BODY_SHOP_DRIVE_THRU, AsEvent(lacPayload), &lOut);
        Check(lAi.maActiveRaceCars[6].mfBaseDeformAmount == 0.0f && !lAi.mbPlayerBaseDeformRequestPending,
              "G68-D7 97: a non-player match is repaired without arming the player one-shot");
    }

    // ---- G68-D8: the crash-play arms -----------------------------------------------------------
    {
        Fixture::RaceCarEntityModule lModule; Fixture::OutputFixture lOut;
        OverheadSignHitAction lSign; std::memset(&lSign, 0, sizeof(lSign));
        lModule.Dispatch(E_ACTION_OVERHEAD_SIGN_HIT, AsEvent(&lSign), &lOut);
        Check(lModule.mCrashPlayManager.maCalls.size() == 1 && lModule.mCrashPlayManager.maCalls[0] == "sign",
              "G68-D8 128: OnHitOverheadSign (0x8230D798)");

        VehicleHitAction lHit; std::memset(&lHit, 0, sizeof(lHit));
        lHit.miTotalVehiclesCrashed = 7; lHit.miVehicleBaseScore = 1500; lHit.miComboBonusEarned = 250;
        lModule.Dispatch(E_ACTION_VEHICLE_HIT, AsEvent(&lHit), &lOut);
        Check(lModule.mCrashPlayManager.miBase == 1500 && lModule.mCrashPlayManager.miCombo == 250
                  && lModule.mCrashPlayManager.miTotal == 7,
              "G68-D8 140: OnVehicleHitConfirmed(r4=+0xC, r5=+0x1C, r6=+0x8)");

        JustBouncedAction lBounce; std::memset(&lBounce, 0, sizeof(lBounce));
        lModule.Dispatch(E_ACTION_JUST_BOUNCED, AsEvent(&lBounce), &lOut);
        Check(lModule.mCrashPlayManager.mpLastRecord == &lBounce && lModule.mBoostManager.mfJustBounceBoostedTimer == -5.0f,
              "G68-D8 144: OnBounce(record); unboosted bounce leaves the timer");
        lBounce.mbBoostedBounce = true;
        lModule.Dispatch(E_ACTION_JUST_BOUNCED, AsEvent(&lBounce), &lOut);
        Check(lModule.mBoostManager.mfJustBounceBoostedTimer == 0.6f,
              "G68-D8 144: boosted bounce -> mfJustBounceBoostedTimer = flt_820147F4 (0.6)");

        JunctionInfoAction lJunction; std::memset(&lJunction, 0, sizeof(lJunction));
        lJunction.muJunctionLogicBoxId = 42;
        const size_t luBefore = lModule.mCrashPlayManager.maCalls.size();
        lModule.Dispatch(E_ACTION_EVENT_AT_JUNCTION_AVAILABLE, AsEvent(&lJunction), &lOut);
        Check(lModule.mCrashPlayManager.maCalls.size() == luBefore, "G68-D8 201: departure post (mbOnEntry 0) -> no call");
        lJunction.mbOnEntry = true;
        lModule.Dispatch(E_ACTION_EVENT_AT_JUNCTION_AVAILABLE, AsEvent(&lJunction), &lOut);
        Check(!lModule.mCrashPlayManager.maCalls.empty() && lModule.mCrashPlayManager.maCalls.back() == "junction" && lModule.mCrashPlayManager.mpLastRecord == &lJunction,
              "G68-D8 201: arrival (lbz 0x1E) -> OnEnterJunction(record)");

        RoadRulesEnterRoadAction lRoad; std::memset(&lRoad, 0, sizeof(lRoad));
        lModule.Dispatch(273, AsEvent(&lRoad), &lOut);
        Check(!lModule.mCrashPlayManager.maCalls.empty() && lModule.mCrashPlayManager.maCalls.back() == "road" && lModule.mCrashPlayManager.mpLastRecord == &lRoad,
              "G68-D8 273: OnEnterRoad(record)");
    }

    Check(guAssertions == 0, "valid fixtures fire no assertions");
    std::printf("Rcem2GameActions: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
