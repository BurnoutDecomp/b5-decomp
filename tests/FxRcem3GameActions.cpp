// FX-RCEM3 (crash-parity 2026-09-23): RaceCarEntityModule::HandleGameActions arms (ARTIST
// 0x8230BE08), extracted VERBATIM from BrnRaceCarEntityModule.cpp by run_fxrcem3_game_actions.py and
// dispatched against fixture race cars. A missing arm is replayed as the console's
// `default: break;`, so the pre-fix source reports per-check failures rather than failing to build.
//   G68-D9  case 34  0x8230C7A0..0x8230C880  after mbModeStartedPlaying = 1 (0x8230C7F8):
//                    mbPlayerDonutsOnEventStart (+0x18352) -> player->RequestPlaceOnTrack(
//                    GetPosition(), GetDirection(), flt_82FAD4FC = 0.44704f * 15.0f)
//   G67-D6  case 76  0x8230C260..0x8230C298  assert "IsInCarSelect()" (:6685); mbInCarModScreen
//                    (+0x186CA) = record byte +4 (CarSelectModificationScreen::mbEntering)
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

struct OutputFixture {};

struct ActiveRaceCar {
    enum ERaceStartState : s32 { E_RACE_START_STATE_ON_START_LINE = 0, E_RACE_START_STATE_RACING = 2 };
    bool    mbActive = true;
    Vector3 mPosition  = { 10.0f, 20.0f, 30.0f, 1.0f };
    Vector3 mDirection = { 0.0f, 0.0f, 1.0f, 0.0f };
    int     miPlaceRequests = 0;
    Vector3 mPlacePosition = {}, mPlaceDirection = {};
    f32     mfPlaceSpeed = -99.0f;
    bool IsActive() const { return mbActive; }
    Vector3 GetPosition() const { return mPosition; }
    Vector3 GetDirection() const { return mDirection; }
    void RequestPlaceOnTrack(const Vector3& lPosition, const Vector3& lDirection, f32 lfSpeed) {
        ++miPlaceRequests; mPlacePosition = lPosition; mPlaceDirection = lDirection; mfPlaceSpeed = lfSpeed;
    }
};

struct BoostManager {
    bool mbEarning = false; int miEarningCalls = 0;
    void SetBoostEarningEnabled(bool lb) { ++miEarningCalls; mbEarning = lb; }
};

struct RaceCarEntityModule {
    ActiveRaceCar       maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_3;
    BoostManager        mBoostManager;
    bool mbIsInGameMode = true, mbModeStartedPlaying = false, mbPlayerDonutsOnEventStart = false;
    bool mbInCarSelectScreen = true, mbInCarModScreen = false;
    int  miStartLineCalls = 0; s32 miStartLineState = -1; bool mbStartLineArg = false;
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leIndex) {
        CGS_ASSERT(leIndex >= 0 && leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "active index");
        return &maActiveRaceCars[leIndex];
    }
    void SetAllCarsOnStartLine(ActiveRaceCar::ERaceStartState le, bool lb) { ++miStartLineCalls; miStartLineState = le; mbStartLineArg = lb; }
    void Dispatch(s32 liType, const CgsModule::Event* lpEvent, OutputFixture* lpOutput);
};
#include "fxrcem3_game_actions.inc"
}   // namespace Fixture

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }

int main() {
    using namespace BrnGameState::GameStateModuleIO;
    const auto AsEvent = [](const void* lp) { return reinterpret_cast<const CgsModule::Event*>(lp); };

    // ---- G68-D9: action 34, the donut-start placement ------------------------------------------
    {
        u8 lacRecord[16] = {};
        Fixture::RaceCarEntityModule lModule; Fixture::OutputFixture lOut;
        lModule.mbPlayerDonutsOnEventStart = true;
        Fixture::ActiveRaceCar& lrPlayer = lModule.maActiveRaceCars[3];
        lrPlayer.mPosition = { 101.5f, -2.25f, 330.0f, 1.0f };
        lrPlayer.mDirection = { 0.6f, 0.0f, 0.8f, 0.0f };
        lModule.Dispatch(E_ACTION_START_PLAYING_MODE, AsEvent(lacRecord), &lOut);
        Check(lModule.miStartLineCalls == 1 && lModule.miStartLineState == 2 && lModule.mbStartLineArg
                  && lModule.mBoostManager.mbEarning && lModule.mbModeStartedPlaying,
              "G68-D9 34: SetAllCarsOnStartLine(2,1), SetBoostEarningEnabled(1), mbModeStartedPlaying = 1 (unchanged head)");
        Check(lrPlayer.miPlaceRequests == 1,
              "G68-D9 34: donut start -> exactly one player RequestPlaceOnTrack (bl @0x8230C87C)");
        Check(lrPlayer.mPlacePosition.x == 101.5f && lrPlayer.mPlacePosition.y == -2.25f && lrPlayer.mPlacePosition.z == 330.0f
                  && lrPlayer.mPlaceDirection.x == 0.6f && lrPlayer.mPlaceDirection.z == 0.8f,
              "G68-D9 34: placed at the car's own GetPosition (v1) / GetDirection (v2)");
        Check(Bits(lrPlayer.mfPlaceSpeed) == Bits(0.44704f * 15.0f),
              "G68-D9 34: speed == flt_82FAD4FC == 0.44704f * 15.0f (KF_MIN_STUNT_RESET_SPEED, CRT 0x82C4BC10)");
        bool lbOthers = true;
        for (int i = 0; i < 8; ++i) if (i != 3 && lModule.maActiveRaceCars[i].miPlaceRequests != 0) lbOthers = false;
        Check(lbOthers, "G68-D9 34: no other slot is placed");

        Fixture::RaceCarEntityModule lPlain;
        lPlain.Dispatch(E_ACTION_START_PLAYING_MODE, AsEvent(lacRecord), &lOut);
        Check(lPlain.maActiveRaceCars[3].miPlaceRequests == 0 && lPlain.mbModeStartedPlaying,
              "G68-D9 34: no donut flag -> no placement (beq @0x8230C800)");
    }

    // ---- G67-D6: action 76, the car-modification-screen byte ------------------------------------
    {
        struct { s32 meCarSelectType; bool mbEntering; u8 pad[3]; } lRecord = { 0, true, { 0xCC, 0xCC, 0xCC } };
        Fixture::RaceCarEntityModule lModule; Fixture::OutputFixture lOut;
        lModule.Dispatch(76, AsEvent(&lRecord), &lOut);
        Check(lModule.mbInCarModScreen, "G67-D6 76: entering -> mbInCarModScreen = 1 (lbz 4(r27) ; stbx +0x186CA)");
        lRecord.mbEntering = false;
        lModule.Dispatch(76, AsEvent(&lRecord), &lOut);
        Check(!lModule.mbInCarModScreen, "G67-D6 76: leaving -> mbInCarModScreen = 0");
        const unsigned luBefore = guAssertions;
        Fixture::RaceCarEntityModule lOutside; lOutside.mbInCarSelectScreen = false;
        lRecord.mbEntering = true;
        lOutside.Dispatch(76, AsEvent(&lRecord), &lOut);
        Check(guAssertions == luBefore + 1 && lOutside.mbInCarModScreen,
              "G67-D6 76: outside car select -> the IsInCarSelect() tripwire fires, the store still happens");
        guAssertions = luBefore;
    }

    Check(guAssertions == 0, "valid fixtures fire no assertions");
    std::printf("FxRcem3GameActions: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
