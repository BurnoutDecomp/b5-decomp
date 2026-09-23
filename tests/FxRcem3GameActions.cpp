// FX-RCEM3 (crash-parity 2026-09-23): RaceCarEntityModule::HandleGameActions arms (ARTIST
// 0x8230BE08), extracted VERBATIM from BrnRaceCarEntityModule.cpp by run_fxrcem3_game_actions.py and
// dispatched against fixture race cars. A missing arm is replayed as the console's
// `default: break;`, so the pre-fix source reports per-check failures rather than failing to build.
//   G68-D9  case 34  0x8230C7A0..0x8230C880  after mbModeStartedPlaying = 1 (0x8230C7F8):
//                    mbPlayerDonutsOnEventStart (+0x18352) -> player->RequestPlaceOnTrack(
//                    GetPosition(), GetDirection(), flt_82FAD4FC = 0.44704f * 15.0f)
//   G67-D6  case 76  0x8230C260..0x8230C298  assert "IsInCarSelect()" (:6685); mbInCarModScreen
//                    (+0x186CA) = record byte +4 (CarSelectModificationScreen::mbEntering)
//   G68-D11 case 276 0x8230D894..0x8230D8C0  player->SetIndicatorState(+0x14C == 2, +0x150 == 2)
//   G68-D11 cases 68 / 73 / 98 / 122 / 123 / 125 / 126 / 170 (+ HandleSetBoost 0x822A4648) / 192 / 194
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
struct alignas(16) Matrix44Affine { f32 maf[16]; };
enum { E_NUM_PALETTES = 4 };

struct OutputFixture {};
namespace RaceCarEntityModuleIO { typedef OutputFixture OutputBuffer_PreScene; }

struct RaceCar {
    s32 miColourPalette = 1, miColourIndex = 2;
    void SetColourPalette(s32 li) { miColourPalette = li; }
    void SetColourIndex(s32 li) { miColourIndex = li; }
    s32 GetColourPalette() const { return miColourPalette; }
    s32 GetColourIndex() const { return miColourIndex; }
};
struct PlayerCarColourPalette { s32 miNumColours = 10; s32 GetNumColours() const { return miNumColours; } };
struct GlobalColourPalette { PlayerCarColourPalette maPalettes[E_NUM_PALETTES]; };
struct ColourResourceFixture {
    GlobalColourPalette mPalette;
    const GlobalColourPalette* operator->() const { return &mPalette; }
};

struct ActiveRaceCar {
    enum ERaceStartState : s32 { E_RACE_START_STATE_ON_START_LINE = 0, E_RACE_START_STATE_RACING = 2 };
    bool    mbActive = true;
    Vector3 mPosition  = { 10.0f, 20.0f, 30.0f, 1.0f };
    Vector3 mDirection = { 0.0f, 0.0f, 1.0f, 0.0f };
    int     miPlaceRequests = 0;
    Vector3 mPlacePosition = {}, mPlaceDirection = {};
    f32     mfPlaceSpeed = -99.0f;
    int     miIndicatorCalls = 0; bool mbIndicatorLeftArg = false, mbIndicatorRightArg = false;
    RaceCar mRaceCar;
    bool    mbEnableEngineSwitchOff = true; int miSwitchOffCalls = 0;
    RaceCar* GetGlobalRaceCar() { return &mRaceCar; }
    void EnableEngineSwitchOff(bool lb) { ++miSwitchOffCalls; mbEnableEngineSwitchOff = lb; }
    void SetIndicatorState(bool lbLeft, bool lbRight) { ++miIndicatorCalls; mbIndicatorLeftArg = lbLeft; mbIndicatorRightArg = lbRight; }
    bool IsActive() const { return mbActive; }
    Vector3 GetPosition() const { return mPosition; }
    Vector3 GetDirection() const { return mDirection; }
    void RequestPlaceOnTrack(const Vector3& lPosition, const Vector3& lDirection, f32 lfSpeed) {
        ++miPlaceRequests; mPlacePosition = lPosition; mPlaceDirection = lDirection; mfPlaceSpeed = lfSpeed;
    }
};

struct BoostStrategy {
    std::vector<std::string> maCalls;
    bool mbInfinite = false; f32 mfAmount = -1.0f; s32 miSegments = -1;
    void SetInfiniteBoost(bool lb) { maCalls.push_back("infinite"); mbInfinite = lb; }
    void SetBoostAmount(f32 lf) { maCalls.push_back("amount"); mfAmount = lf; }
    void SetBoostSegments(s32 li) { maCalls.push_back("segments"); miSegments = li; }
    void RemoveAllBoostAndChunks() { maCalls.push_back("removeall"); }
};
struct BoostManager {
    bool mbEarning = false; int miEarningCalls = 0;
    BoostStrategy mStrategy;
    void SetBoostEarningEnabled(bool lb) { ++miEarningCalls; mbEarning = lb; }
    BoostStrategy* GetBoostStrategy() { return &mStrategy; }
};

struct RaceCarEntityModule {
    ActiveRaceCar       maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_3;
    BoostManager        mBoostManager;
    bool mbIsInGameMode = true, mbModeStartedPlaying = false, mbPlayerDonutsOnEventStart = false;
    bool mbInCarSelectScreen = true, mbInCarModScreen = false;
    int  miStartLineCalls = 0; s32 miStartLineState = -1; bool mbStartLineArg = false;
    bool mbRenderRaceCarCoronas = true, mbSixaxisSteeringEnabled = false, mbWaitingForStreaming = false;
    s32  meCarSelectResetType = 2;
    int  miRemoveRivals = 0; const OutputFixture* mpRemoveRivalsOutput = nullptr; bool mbRemoveRivalsArg = true;
    ColourResourceFixture mCarColoursResource;
    struct SetBoostActionRecord;
    void HandleSetBoost(const SetBoostActionRecord* lpAction, RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput);
    void RemoveRivals(OutputFixture* lpOutput, bool lb) { ++miRemoveRivals; mpRemoveRivalsOutput = lpOutput; mbRemoveRivalsArg = lb; }
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

    // ---- G68-D11 arm 276: the upcoming-road turn signal --------------------------------------
    {
        UpcomingRoadChangeAction lRoad; std::memset(&lRoad, 0, sizeof(lRoad));
        Fixture::RaceCarEntityModule lModule; Fixture::OutputFixture lOut;
        const Fixture::ActiveRaceCar& lrPlayer = lModule.maActiveRaceCars[3];
        lRoad.miLeftRoadHighlightState = 2; lRoad.miRightRoadHighlightState = 1;
        lModule.Dispatch(276, AsEvent(&lRoad), &lOut);
        Check(lrPlayer.miIndicatorCalls == 1 && lrPlayer.mbIndicatorLeftArg && !lrPlayer.mbIndicatorRightArg,
              "G68-D11 276: left road highlighted (+0x14C == 2) -> player SetIndicatorState(1, 0)");
        lRoad.miLeftRoadHighlightState = 1; lRoad.miRightRoadHighlightState = 2;
        lModule.Dispatch(276, AsEvent(&lRoad), &lOut);
        Check(lrPlayer.miIndicatorCalls == 2 && !lrPlayer.mbIndicatorLeftArg && lrPlayer.mbIndicatorRightArg,
              "G68-D11 276: right road highlighted (+0x150 == 2) -> SetIndicatorState(0, 1)");
        lRoad.miLeftRoadHighlightState = 0; lRoad.miRightRoadHighlightState = 0;
        lModule.Dispatch(276, AsEvent(&lRoad), &lOut);
        Check(lrPlayer.miIndicatorCalls == 3 && !lrPlayer.mbIndicatorLeftArg && !lrPlayer.mbIndicatorRightArg,
              "G68-D11 276: no highlight -> SetIndicatorState(0, 0) (cntlzw(x-2) test, not x != 0)");
        bool lbOthers = true;
        for (int i = 0; i < 8; ++i) if (i != 3 && lModule.maActiveRaceCars[i].miIndicatorCalls != 0) lbOthers = false;
        Check(lbOthers, "G68-D11 276: only the player's car (lwzx +0x182F8)");
    }

    // ---- G68-D11: the remaining arms --------------------------------------------------------------
    {
        Fixture::OutputFixture lOut;
        // 68: coronas on/off
        {
            Fixture::RaceCarEntityModule lModule;
            bool lbOff = false;
            lModule.Dispatch(68, AsEvent(&lbOff), &lOut);
            Check(!lModule.mbRenderRaceCarCoronas, "G68-D11 68: mbRenderRaceCarCoronas = record byte 0 (stbx +0x1834F)");
        }
        // 73: junkyard transition-in
        {
            Fixture::RaceCarEntityModule lModule; lModule.mbInCarSelectScreen = false;
            bool labRecord[2] = { true, false };
            lModule.Dispatch(73, AsEvent(labRecord), &lOut);
            Check(lModule.miRemoveRivals == 1 && lModule.mpRemoveRivalsOutput == &lOut && !lModule.mbRemoveRivalsArg,
                  "G68-D11 73: mbStart -> RemoveRivals(lpOutput, 0) (bl @0x8230C240)");
            Check(lModule.mbInCarSelectScreen && lModule.meCarSelectResetType == 0,
                  "G68-D11 73: mbInCarSelectScreen = 1 (+0x186C9), meCarSelectResetType = 0 (+0x186CC)");
            Fixture::RaceCarEntityModule lEnd; lEnd.mbInCarSelectScreen = false;
            labRecord[0] = false;
            lEnd.Dispatch(73, AsEvent(labRecord), &lOut);
            Check(lEnd.miRemoveRivals == 0 && !lEnd.mbInCarSelectScreen && lEnd.meCarSelectResetType == 2,
                  "G68-D11 73: !mbStart -> nothing (beq @0x8230C230)");
        }
        // 98: the paint shop
        {
            Fixture::RaceCarEntityModule lModule;
            u8 lacPaint[144]; std::memset(lacPaint, 0, sizeof(lacPaint));
            const u32 luColour = 5, luPalette = 3;
            std::memcpy(lacPaint + 0x80, &luColour, 4); std::memcpy(lacPaint + 0x84, &luPalette, 4);
            lModule.Dispatch(98, AsEvent(lacPaint), &lOut);
            const Fixture::RaceCar& lrCar = lModule.maActiveRaceCars[3].mRaceCar;
            Check(lrCar.miColourPalette == 3 && lrCar.miColourIndex == 5,
                  "G68-D11 98: player global car palette = +0x84, colour = +0x80 (stw +0x98 / +0x94)");
            Check(lModule.maActiveRaceCars[2].mRaceCar.miColourPalette == 1 && lModule.maActiveRaceCars[2].mRaceCar.miColourIndex == 2,
                  "G68-D11 98: other cars untouched");
        }
        // 122 / 123: the award sequence
        {
            Fixture::RaceCarEntityModule lModule;
            lModule.Dispatch(122, AsEvent(&lModule), &lOut);
            Check(lModule.maActiveRaceCars[3].miSwitchOffCalls == 1 && !lModule.maActiveRaceCars[3].mbEnableEngineSwitchOff,
                  "G68-D11 122: player mbEnableEngineSwitchOff = 0 (stb r24 @0x8230D8D4)");
            lModule.Dispatch(123, AsEvent(&lModule), &lOut);
            Check(lModule.maActiveRaceCars[3].miSwitchOffCalls == 2 && lModule.maActiveRaceCars[3].mbEnableEngineSwitchOff,
                  "G68-D11 123: player mbEnableEngineSwitchOff = 1 (stb r23 @0x8230D8E8)");
        }
        // 125: sixaxis steering
        {
            Fixture::RaceCarEntityModule lModule;
            bool lbOn = true;
            lModule.Dispatch(125, AsEvent(&lbOn), &lOut);
            Check(lModule.mbSixaxisSteeringEnabled, "G68-D11 125: mbSixaxisSteeringEnabled = record byte 0 (stbx +0x1834D)");
        }
        // 126: switch one car's colour
        {
            Fixture::RaceCarEntityModule lModule;
            struct { s32 idx; u32 colour; } lRecord = { 5, 7 };
            lModule.Dispatch(126, AsEvent(&lRecord), &lOut);
            Check(lModule.maActiveRaceCars[5].mRaceCar.miColourIndex == 7 && lModule.maActiveRaceCars[5].mRaceCar.miColourPalette == 1,
                  "G68-D11 126: car [record +0] colour = record +4, palette kept (stw +0x94 @0x8230D874)");
        }
        // 170: HandleSetBoost
        {
            struct { s32 idx; s32 flags; f32 amount; s32 segments; bool infinite; u8 pad[3]; } lRecord = { 3, 2, 1.0f, -9, true, {} };
            Fixture::RaceCarEntityModule lModule;
            lModule.Dispatch(170, AsEvent(&lRecord), &lOut);
            const Fixture::BoostStrategy& lrStrategy = lModule.mBoostManager.mStrategy;
            Check(lrStrategy.maCalls.size() == 1 && lrStrategy.maCalls[0] == "amount" && lrStrategy.mfAmount == 1.0f,
                  "G68-D11 170: StuntAttackMode's {player, 2, 1.0} -> SetBoostAmount(1.0) only (slot 39, lfs +8)");
            lRecord.flags = 7; lRecord.amount = 100.0f; lRecord.segments = 5;
            Fixture::RaceCarEntityModule lAll;
            lAll.Dispatch(170, AsEvent(&lRecord), &lOut);
            const Fixture::BoostStrategy& lrAll = lAll.mBoostManager.mStrategy;
            Check(lrAll.maCalls.size() == 3 && lrAll.maCalls[0] == "infinite" && lrAll.maCalls[1] == "amount" && lrAll.maCalls[2] == "segments"
                      && lrAll.mbInfinite && lrAll.mfAmount == 100.0f && lrAll.miSegments == 5,
                  "G68-D11 170: flags 7 -> SetInfiniteBoost(+0x10), SetBoostAmount(+0x08), SetBoostSegments(+0x0C), in that order");
            lRecord.idx = 2;
            Fixture::RaceCarEntityModule lOther;
            lOther.Dispatch(170, AsEvent(&lRecord), &lOut);
            Check(lOther.mBoostManager.mStrategy.maCalls.empty(), "G68-D11 170: not the player's slot -> nothing (bne @0x822A4670)");
        }
        // 192: wait for streaming
        {
            Fixture::RaceCarEntityModule lModule;
            lModule.Dispatch(192, AsEvent(&lModule), &lOut);
            Check(lModule.mbWaitingForStreaming, "G68-D11 192: mbWaitingForStreaming = 1 (stbx r23, +0x18348)");
        }
        // 194: load profile
        {
            Fixture::RaceCarEntityModule lModule;
            lModule.Dispatch(194, AsEvent(&lModule), &lOut);
            Check(lModule.mBoostManager.mStrategy.maCalls.size() == 1 && lModule.mBoostManager.mStrategy.maCalls[0] == "removeall",
                  "G68-D11 194: strategy->RemoveAllBoostAndChunks() (vtable +0xA4, slot 41)");
        }
    }

    Check(guAssertions == 0, "valid fixtures fire no assertions");
    std::printf("FxRcem3GameActions: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
