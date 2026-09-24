// FX-DIRECTOR (crash parity 2026-09-24): the PRODUCTION BrnDirector::MainDirector::ProcessInputQueue
// @0x822372F8 -- the whole function, extracted from src/GameSource/Director/BrnMainDirector.cpp by
// run_fxdirector_input_queue.py (with ClearEventPresentationBlock, KF_CRASH_TIME_WINDOW and the
// TU-local action-42 record) -- driven through the REAL CgsModule::VariableEventQueue<13312,16>
// with the CONSOLE's wire records (built here byte by byte at the X360 offsets, not through the
// header's record types), against the revision's own BrnDirector::GameState header and
// GameState::Clear / ResetPerFrameData bodies.
//
// Checked against the ARTIST asm (MainDirector = r31, GameState = MainDirector + 0x337E0):
//   case 42  @0x82237E54  stbx 1 -> 0x338E5 (+0x105 mbImpactTimeActive);
//                         lfs 0(r30) -> stfsx 0x338E8 (+0x108 mfImpactTimeSloMoFactor)
//   case 43  @0x82237E74  stbx 0 -> 0x338E5; the factor is not touched
//   case 140 @0x822386CC  lwz 0x1C != 0 -> nothing; (lwz 8) % 10 == 0 -> 0x339CA (+0x1EA);
//                         lwz 0x14 > 0 -> 0x339CB (+0x1EB)   (signed)
//   case 144 @0x8223864C  (+0x1E0 < lwz 0x14) -> 0x339C8 (+0x1E8); lwz 0x14 -> +0x1E0;
//                         lwz 0x18 -> 0x339C4 (+0x1E4); (lbz 0x21 && lbz 0x23) -> 0x339C9 (+0x1E9)
//   case 145 @0x822386BC  stbx 1 -> 0x339CC (+0x1EC)
//   case 146 @0x82238608  lbz 0x10 -> 0x339CD (+0x1ED)
//   prologue 0x82237398..0x822373A8 clears +0x1E8..+0x1EC every drain (not +0x1ED, +0x1E0, +0x1E4)
//   case 39  0x82237DAC..0x82237DCC clears all nine ShowTimeInfo fields (+0x1DC..+0x1ED)
//   GameState::Clear 0x82218C30..0x82218C60: ShowTimeInfo cleared, +0x1F0 = 1 (THIRD_PERSON)
// [item 2, the rest of the console's arms + the tail]
//   case 0   @0x82238478  lbz 0x42 -> 0x33931 (+0x151 mbShouldResetPlayerCameraThisFrame, per-frame)
//   case 53  @0x822384E8  1 -> +0x1B1, 1 -> +0x1B2 ; case 54 @0x82238504  1 -> +0x1B1, 0 -> +0x1B2
//   case 107 @0x82238520  1 -> +0x1B3 ; case 120 @0x82237E44  1 -> +0xDC (NOT per-frame)
//   case 132 @0x82238530  (lwz 0xC > 0) -> +0x1B5 ; case 150/151 @0x82237E84/94  1/0 -> +0x1C2
//   case 215 @0x82238488  1 -> +0xF0, lwz 0 -> +0xF4 ; case 216 @0x822384C8  0 -> +0xF0, 3 -> +0xF4
//   case 218 @0x82238550  (lwz 0x10 == player RaceCarState +0x3C8 && lwz 0x14 == 1 && lwz 0x18 & bits 0..5)
//                         -> 1 -> +0xF8 (per-frame)
//   case 224 @0x822382C0  0 -> +0x152, 0.0 -> +0x154 (+0x158 untouched)
//   tail 0x8223886C       flag tail +0x35431 -> +0x100 mbCanUseSlomo = 0
//   tail 0x8223893C..0x82238A3C  +0x148 / +0x14C inactivity clocks (sim step; zeroed by mbAnyInput / sim
//                         paused, +0x14C also by the player's mbEngineOn), +0x150 latched by mbEngineOn
//   Arbitrator::Update 0x8226ADE0  GameState +0x151 -> SharedCameraContainer::mbUseGameplayExternal = 1
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h"
#include <cstddef>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"
#include "GameSource/Director/SharedIO/BrnDirectorControllerInfo.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0, gPrepareCalls = 0;

namespace CgsDev
{
namespace Log
{
    DebugPrint* gpDebugPrint = nullptr;   // every [diag] witness in the body stays silent
    StrStreamBase& DebugPrint::operator<<(const char*) { return *this; }
}
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++gAsserts; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Message
{
    u64 gxMessageFilterFlags = 0;   // the queue's overflow dump (OutputQueueContents) filter; never reached
}
}

// VehicleInfo's constructor clears its RaceCarState (physics TU, not linked here): zero it, which
// is all the tail of ProcessInputQueue reads (mbCrashing / mbEngineOn stay false).
void BrnPhysics::Vehicle::RaceCarState::Clear() { std::memset(static_cast<void*>(this), 0, sizeof(*this)); }

namespace BrnDirector
{
namespace DirectorIO
{
    // Stand-in for DirectorIO::InputBuffer: exactly the accessor surface ProcessInputQueue reads,
    // over the real queue / timer / vehicle-info / controller types.
    struct InputBuffer
    {
        CgsModule::VariableEventQueue<13312, 16> mGameActionQueue;
        Camera::VehicleInfo             maRaceCarInfo[8];
        CgsSystem::TimerStatusInterface mTimerStatus;
        ControllerInfo                  mControllerInfo;
        EActiveRaceCarIndex             mePlayerCarIndex;
        EActiveRaceCarIndex             mePlayerKillerCarIndex;
        s32                             miRankUpRivalInfo;
        bool                            mbPlayerTakenDown;
        bool                            mbSimPaused;

        const CgsModule::VariableEventQueue<13312, 16>* GetGameActionQueue() const { return &mGameActionQueue; }
        bool GetPlayerTakenDown() const { return mbPlayerTakenDown; }
        EActiveRaceCarIndex GetPlayerKillerCarIndex() const { return mePlayerKillerCarIndex; }
        s32 GetRankUpRivalInfo() const { return miRankUpRivalInfo; }
        const Camera::VehicleInfo* GetRaceCarInfo() const { return maRaceCarInfo; }
        EActiveRaceCarIndex GetPlayerCarIndex() const { return mePlayerCarIndex; }
        const CgsSystem::TimerStatusInterface* GetTimerStatusInterface() const { return &mTimerStatus; }
        const ControlInput* GetControll() const { return &mControllerInfo; }
        bool IsSimPaused() const { return mbSimPaused; }
    };
}

struct DirectorInputOutput
{
    const DirectorIO::InputBuffer* mpInputBuffer;
};

// Stand-in for MainDirector::mAllVehicleData: case 218 asks it for the player's VehicleInfo.
struct AllVehicleDataStandIn
{
    const Camera::VehicleInfo* mpPlayer = nullptr;
    const Camera::VehicleInfo& GetPlayer() const { return *mpPlayer; }
};

// Stand-ins for what Arbitrator::Update's +0x151 read touches, by the production member names.
struct SharedCameraContainerStandIn
{
    bool mbUseGameplayExternal;
    bool mbLookbackOverride;
};
struct ArbSharedInfoStandIn
{
    const GameState* mpGameState;
};
}

// The production bodies under test + the stand-in MainDirector (members / flag-tail indices from
// the revision's own BrnMainDirector.h) + this revision's GameState field accessors.
#include "director_input_queue.inc"

using namespace BrnDirector;
using namespace FxDirectorTest;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

// One console wire record: bytes at the X360 offsets, the rest junk (0xCD) like stack residue.
struct WireRecord
{
    alignas(16) u8 mau8Bytes[96];
    WireRecord() { std::memset(mau8Bytes, 0xCD, sizeof(mau8Bytes)); }
    void Word(u32 luOffset, s32 liValue)  { std::memcpy(mau8Bytes + luOffset, &liValue, sizeof(s32)); }
    void Float(u32 luOffset, f32 lfValue) { std::memcpy(mau8Bytes + luOffset, &lfValue, sizeof(f32)); }
    void Byte(u32 luOffset, u8 lu8Value)  { mau8Bytes[luOffset] = lu8Value; }
};

alignas(16) static DirectorIO::InputBuffer gInput;
alignas(16) static MainDirector gDirector;
static DirectorInputOutput gIO = { &gInput };

static void Post(const WireRecord& lrRecord, s32 liType, s32 liSize)
{
    gInput.mGameActionQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(lrRecord.mau8Bytes), liType, liSize);
}

// One director frame: drain whatever was posted, then empty the queue for the next frame.
static void Drain()
{
    gDirector.ProcessInputQueue(&gIO);
    gInput.mGameActionQueue.Clear();
}

// Console records.
static WireRecord ImpactTimeStart(f32 lfMultiplier)   // action 42, 8 bytes
{
    WireRecord lRecord;
    lRecord.Float(0x00, lfMultiplier);
    lRecord.Byte(0x04, 1);
    lRecord.Byte(0x05, 1);
    return lRecord;
}
static WireRecord VehicleHit(s32 liTotalHit, s32 liMultiplierEarned, s32 liComboBonus)   // action 140, 36 bytes
{
    WireRecord lRecord;
    lRecord.Word(0x00, 2);      // vehicle class
    lRecord.Word(0x04, 1);
    lRecord.Word(0x08, liTotalHit);
    lRecord.Word(0x0C, 500);
    lRecord.Word(0x10, 1);
    lRecord.Word(0x14, liMultiplierEarned);
    lRecord.Word(0x18, 4);
    lRecord.Word(0x1C, liComboBonus);
    return lRecord;
}
static WireRecord JustBounced(s32 liComboCount, s32 liTotalHit, u8 lu8OnCar, u8 lu8GoodImpact)   // action 144, 48 bytes
{
    WireRecord lRecord;
    lRecord.Word(0x10, 7);      // the event's word 0
    lRecord.Word(0x14, liComboCount);
    lRecord.Word(0x18, liTotalHit);
    lRecord.Word(0x1C, 9);      // the event's word 2
    lRecord.Byte(0x20, 1);
    lRecord.Byte(0x21, lu8OnCar);
    lRecord.Byte(0x22, 1);
    lRecord.Byte(0x23, lu8GoodImpact);
    return lRecord;
}
static WireRecord ShowtimeIntro(u8 lu8Start)   // action 146, 32 bytes
{
    WireRecord lRecord;
    lRecord.Float(0x00, 0.0f);
    lRecord.Float(0x04, 0.0f);
    lRecord.Float(0x08, 1.0f);
    lRecord.Float(0x0C, 0.0f);
    lRecord.Byte(0x10, lu8Start);
    return lRecord;
}
static WireRecord StopMode()   // action 39, 24 bytes
{
    WireRecord lRecord;
    lRecord.Word(0x00, 2);      // the mode being stopped
    lRecord.Byte(0x10, 0);      // offline -> mbCanUseSlomo
    lRecord.Byte(0x12, 0);
    lRecord.Byte(0x15, 0);
    return lRecord;
}
static WireRecord Empty() { return WireRecord(); }
static WireRecord ResetPlayerCar(u8 lu8ResetCamera)   // action 0, 80 bytes
{
    WireRecord lRecord;
    lRecord.Float(0x34, -1.0f);   // no unlock deform
    lRecord.Word(0x3C, 0);        // E_CAR_SELECT_DONT_DROP
    lRecord.Byte(0x40, 0);
    lRecord.Byte(0x41, 0);
    lRecord.Byte(0x42, lu8ResetCamera);
    lRecord.Byte(0x43, 0);
    return lRecord;
}
static WireRecord StuntPerformed(s32 liMultiplier)   // action 132, 24 bytes (StuntInfo)
{
    WireRecord lRecord;
    lRecord.Word(0x00, 3);        // stunt types
    lRecord.Word(0x04, 0);
    lRecord.Word(0x08, 1500);     // score
    lRecord.Word(0x0C, liMultiplier);
    return lRecord;
}
static WireRecord PaybackActivated(s32 liType)   // action 215, 12 bytes
{
    WireRecord lRecord;
    lRecord.Word(0x00, liType);
    lRecord.Word(0x04, 1);
    lRecord.Word(0x08, 0);
    return lRecord;
}
static WireRecord SoundTrigger(u32 luEntity, s32 liType, u32 luTriggers)   // action 218, 32 bytes
{
    WireRecord lRecord;
    lRecord.Float(0x00, 10.0f);
    lRecord.Float(0x04, 0.0f);
    lRecord.Float(0x08, -5.0f);
    lRecord.Word(0x10, static_cast<s32>(luEntity));
    lRecord.Word(0x14, liType);
    lRecord.Word(0x18, static_cast<s32>(luTriggers));
    return lRecord;
}

static bool ShowTimeUntouchedAfterClear(const GameState& lrGameState)
{
    return DeformationLevel(lrGameState) == 0.0f && ComboLevel(lrGameState) == 0 &&
           TotalVehiclesHit(lrGameState) == 0 && !ComboLevelIncreased(lrGameState) &&
           !VehicleImpact(lrGameState) && !CrushCombo(lrGameState) && !EarntMultiplier(lrGameState) &&
           !ExtraSpin(lrGameState) && !InIntro(lrGameState);
}

int main()
{
    gInput.mGameActionQueue.Construct();
    gInput.mePlayerCarIndex       = E_ACTIVE_RACE_CAR_INDEX_0;
    gInput.mePlayerKillerCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    gInput.mTimerStatus.mSimTimerStatus.mfBaseTimeStep       = 1.0f / 60.0f;
    gInput.mTimerStatus.mSimTimerStatus.mfTimeStepMultiplier = 1.0f;
    gInput.mTimerStatus.mSimTimerStatus.mbRunning            = true;

    GameState& lrGameState = gDirector.maGameState;
    lrGameState.Clear();

    // 0. GameState::Clear (0x82218C30..0x82218C60): ShowTimeInfo zero, meCameraMode THIRD_PERSON.
    Check(ShowTimeUntouchedAfterClear(lrGameState), "Clear: the nine ShowTimeInfo fields are zero (0x82218C30..54)");
    Check(CameraMode(lrGameState) == 1, "Clear: +0x1F0 = 1 (DirectorProfileData::Construct, 0x82218C60)");

    // 1. The Showtime intro latch (146, mbStart 1) -- and it is NOT a per-frame bit.
    Post(ShowtimeIntro(1), 146, 32);
    Drain();
    Check(InIntro(lrGameState), "146 @0x82238608: +0x1ED mbInIntro = record +0x10 (1)");
    Drain();
    Check(InIntro(lrGameState), "the prologue does not clear +0x1ED (only +0x1E8..+0x1EC)");

    // 2. Impact time starts: THE SIM SCALE ArbStateCrashMode requests (0x822356FC).
    Check(lrGameState.mfImpactTimeSloMoFactor == 0.0f && !lrGameState.mbImpactTimeActive,
          "before action 42 the factor is Clear's untouched 0 (the 0.005x floor the PC ran Showtime at)");
    Post(ImpactTimeStart(1.0f), 42, 8);
    Drain();
    Check(lrGameState.mbImpactTimeActive, "42 @0x82237E54: +0x105 mbImpactTimeActive = 1");
    Check(lrGameState.mfImpactTimeSloMoFactor == 1.0f,
          "42 @0x82237E68: +0x108 mfImpactTimeSloMoFactor = record +0x00 (1.0, flt_82001C98 at the producer)");
    Post(ImpactTimeStart(0.25f), 42, 8);
    Drain();
    Check(lrGameState.mfImpactTimeSloMoFactor == 0.25f, "42: the factor is the record's value, not a constant");
    Drain();
    Check(lrGameState.mbImpactTimeActive && lrGameState.mfImpactTimeSloMoFactor == 0.25f,
          "42: both fields persist across drains (no per-frame reset)");

    // 3. VEHICLE_HIT -> the close-up requests.
    Post(VehicleHit(20, 3, 0), 140, 36);
    Drain();
    Check(CrushCombo(lrGameState), "140 @0x82238714: +0x1EA = (total hit 20 % 10 == 0)");
    Check(EarntMultiplier(lrGameState), "140 @0x8223872C: +0x1EB = (multiplier earned 3 > 0)");
    Drain();
    Check(!CrushCombo(lrGameState) && !EarntMultiplier(lrGameState),
          "the next drain's prologue clears +0x1EA / +0x1EB (0x822373A0 / 0x822373A4)");
    Post(VehicleHit(7, 0, 0), 140, 36);
    Drain();
    Check(!CrushCombo(lrGameState) && !EarntMultiplier(lrGameState), "140: total 7, multiplier 0 -> neither request");
    Post(VehicleHit(-10, -1, 0), 140, 36);
    Drain();
    Check(CrushCombo(lrGameState) && !EarntMultiplier(lrGameState),
          "140: signed -- (-10) % 10 == 0 requests, multiplier -1 is not > 0");
    Post(VehicleHit(30, 2, 5), 140, 36);
    Drain();
    Check(!CrushCombo(lrGameState) && !EarntMultiplier(lrGameState),
          "140 @0x822386D4: a combo-bonus hit (+0x1C != 0) requests nothing");
    Post(VehicleHit(10, 1, 0), 140, 36);
    Post(VehicleHit(30, 0, 5), 140, 36);
    Drain();
    Check(CrushCombo(lrGameState) && EarntMultiplier(lrGameState),
          "140: a later combo-bonus hit in the same drain leaves the earlier requests standing");

    // 4. JUST_BOUNCED -> combo level, total, the shake-blur request.
    Post(JustBounced(3, 12, 1, 1), 144, 48);
    Drain();
    Check(ComboLevelIncreased(lrGameState), "144 @0x8223867C: +0x1E8 = (old +0x1E0 0 < record +0x14 3)");
    Check(ComboLevel(lrGameState) == 3, "144 @0x82238684: +0x1E0 miComboLevel = record +0x14");
    Check(TotalVehiclesHit(lrGameState) == 12, "144 @0x8223868C: +0x1E4 miTotalVehiclesHit = record +0x18");
    Check(VehicleImpact(lrGameState), "144 @0x822386B8: +0x1E9 = (record +0x21 && +0x23)");
    Post(JustBounced(3, 13, 1, 0), 144, 48);
    Drain();
    Check(!ComboLevelIncreased(lrGameState) && ComboLevel(lrGameState) == 3 && TotalVehiclesHit(lrGameState) == 13,
          "144: an equal combo count is not an increase (blt, strict)");
    Check(!VehicleImpact(lrGameState), "144: +0x23 clear -> +0x1E9 clear");
    Post(JustBounced(1, 14, 0, 1), 144, 48);
    Drain();
    Check(!ComboLevelIncreased(lrGameState) && ComboLevel(lrGameState) == 1 && !VehicleImpact(lrGameState),
          "144: a lower combo is stored without an increase; +0x21 clear -> +0x1E9 clear");
    Post(JustBounced(4, 15, 1, 1), 144, 48);
    Drain();
    Drain();
    Check(!ComboLevelIncreased(lrGameState) && !VehicleImpact(lrGameState) &&
          ComboLevel(lrGameState) == 4 && TotalVehiclesHit(lrGameState) == 15,
          "the prologue clears +0x1E8 / +0x1E9 and keeps +0x1E0 / +0x1E4");

    // 5. JUST_APPLIED_EXTRA_SPIN -> the long blur request.
    Post(Empty(), 145, 1);
    Drain();
    Check(ExtraSpin(lrGameState), "145 @0x822386BC: +0x1EC mbExtraSpinThisFrame = 1");
    Drain();
    Check(!ExtraSpin(lrGameState), "the prologue clears +0x1EC (0x822373A8)");

    // 6. Impact time ends: the active bit drops, the factor stays.
    Post(Empty(), 43, 1);
    Drain();
    Check(!lrGameState.mbImpactTimeActive, "43 @0x82237E74: +0x105 mbImpactTimeActive = 0");
    Check(lrGameState.mfImpactTimeSloMoFactor == 0.25f, "43: +0x108 is not touched");

    // 7. The intro cancel post (mbStart 0).
    Post(ShowtimeIntro(0), 146, 32);
    Drain();
    Check(!InIntro(lrGameState), "146: mbStart 0 clears +0x1ED");

    // 8. STOP_MODE clears the whole ShowTimeInfo (0x82237DAC..0x82237DCC).
    Post(ShowtimeIntro(1), 146, 32);
    Post(JustBounced(6, 20, 1, 1), 144, 48);
    Post(Empty(), 145, 1);
    Post(StopMode(), 39, 24);
    Drain();
    Check(ShowTimeUntouchedAfterClear(lrGameState),
          "39 after 146/144/145 in the same drain: all nine ShowTimeInfo fields are zero");

    // 9. The arms write nothing else: unrelated neighbours keep their sentinels.
    lrGameState.mbDriveThruActive = true;
    lrGameState.meJunkyardState   = GameState::E_JY_CAR_SELECT;
    lrGameState.mfHowCloseToTotalled = 0.5f;
    Post(ImpactTimeStart(1.0f), 42, 8);
    Post(VehicleHit(10, 1, 0), 140, 36);
    Post(JustBounced(2, 3, 1, 1), 144, 48);
    Post(Empty(), 145, 1);
    Post(ShowtimeIntro(1), 146, 32);
    Post(Empty(), 43, 1);
    Drain();
    Check(lrGameState.mbDriveThruActive && lrGameState.meJunkyardState == GameState::E_JY_CAR_SELECT &&
          lrGameState.mfHowCloseToTotalled == 0.5f,
          "the six Showtime arms leave unrelated GameState fields alone");

    // ================= item 2: the rest of the console's arms + the tail =================
    // The player's published VehicleInfo (active index 0) is what case 218 compares against.
    gDirector.mAllVehicleData.mpPlayer = &gInput.maRaceCarInfo[0];
    gInput.maRaceCarInfo[0].mRaceCarState.mEntityId.muValue = 0x00012345u;

    // 10. RESET_PLAYER_CAR (0): the reset-camera byte (per-frame) and its arbitrator consumer.
    Post(ResetPlayerCar(1), 0, 80);
    Drain();
    Check(lrGameState.mbShouldResetPlayerCameraThisFrame, "0 @0x82238478: +0x151 = record +0x42 (1)");
    {
        SharedCameraContainerStandIn lContainer = { false, true };
        ArbSharedInfoStandIn lInfo = { &lrGameState };
        ArbitratorPrologueStandIn(lContainer, lInfo);
        Check(lContainer.mbUseGameplayExternal,
              "Arbitrator::Update 0x8226ADE0: +0x151 set -> SharedCameraContainer::mbUseGameplayExternal = 1");
    }
    Drain();
    Check(!lrGameState.mbShouldResetPlayerCameraThisFrame, "the next drain's prologue clears +0x151 (0x82237360)");
    {
        SharedCameraContainerStandIn lOff = { false, false };
        SharedCameraContainerStandIn lOn  = { true, false };
        ArbSharedInfoStandIn lInfo = { &lrGameState };
        ArbitratorPrologueStandIn(lOff, lInfo);
        ArbitratorPrologueStandIn(lOn, lInfo);
        Check(!lOff.mbUseGameplayExternal, "Arbitrator: +0x151 clear -> the container is not selected");
        Check(lOn.mbUseGameplayExternal, "Arbitrator: +0x151 clear never deselects (the console only stores 1)");
    }
    Post(ResetPlayerCar(0), 0, 80);
    Drain();
    Check(!lrGameState.mbShouldResetPlayerCameraThisFrame, "0: record +0x42 == 0 -> +0x151 stays 0");

    // 11. PLAYER_HIT_RIVAL (53) / RIVAL_HIT_PLAYER (54).
    Post(Empty(), 53, 12);
    Drain();
    Check(lrGameState.mbPlayerAndRivalImpactOccured && lrGameState.mbPlayerWonImpactAgainstRival,
          "53 @0x822384E8: +0x1B1 = 1, +0x1B2 = 1");
    Post(Empty(), 54, 12);
    Drain();
    Check(lrGameState.mbPlayerAndRivalImpactOccured && !lrGameState.mbPlayerWonImpactAgainstRival,
          "54 @0x82238504: +0x1B1 = 1, +0x1B2 = 0");
    Post(Empty(), 53, 12);
    Post(Empty(), 54, 12);
    Drain();
    Check(lrGameState.mbPlayerAndRivalImpactOccured && !lrGameState.mbPlayerWonImpactAgainstRival,
          "53 then 54 in one drain: the later arm's +0x1B2 stands");
    Drain();
    Check(!lrGameState.mbPlayerAndRivalImpactOccured && !lrGameState.mbPlayerWonImpactAgainstRival,
          "the prologue clears +0x1B1 / +0x1B2 (0x82237364 / 0x82237368)");

    // 12. ON_TRAFFIC_CHECKING (107).
    Post(Empty(), 107, 2);
    Drain();
    Check(lrGameState.mbPlayerCheckedTraffic, "107 @0x82238520: +0x1B3 = 1");
    Drain();
    Check(!lrGameState.mbPlayerCheckedTraffic, "the prologue clears +0x1B3 (0x8223736C)");

    // 13. SHUTDOWN (120): latched until the takedown camera's inactive post (case 6).
    Post(Empty(), 120, 24);
    Drain();
    Check(lrGameState.mbIsShutdown, "120 @0x82237E44: +0xDC mbIsShutdown = 1");
    Drain();
    Check(lrGameState.mbIsShutdown, "120: +0xDC is NOT per-frame");
    {
        WireRecord lTakedownOff;
        lTakedownOff.Word(0x00, -1);
        lTakedownOff.Byte(0x04, 0);
        lTakedownOff.Byte(0x05, 0);
        lTakedownOff.Byte(0x06, 0);
        Post(lTakedownOff, 6, 8);
    }
    Drain();
    Check(!lrGameState.mbIsShutdown, "case 6's inactive arm clears +0xDC (the shutdown latch's only clear)");

    // 14. HUD_MESSAGE_STUNT_PERFORMED (132): the stunt multiplier at +0xC.
    Post(StuntPerformed(2), 132, 24);
    Drain();
    Check(lrGameState.mbPlayerPerformedStunt, "132 @0x82238530: +0x1B5 = (multiplier 2 > 0)");
    Post(StuntPerformed(0), 132, 24);
    Drain();
    Check(!lrGameState.mbPlayerPerformedStunt, "132: multiplier 0 -> +0x1B5 = 0 (bgt, strict)");
    Post(StuntPerformed(-3), 132, 24);
    Drain();
    Check(!lrGameState.mbPlayerPerformedStunt, "132: signed -- multiplier -3 -> 0");
    Post(StuntPerformed(1), 132, 24);
    Drain();
    Drain();
    Check(!lrGameState.mbPlayerPerformedStunt, "the prologue clears +0x1B5 (0x82237378)");

    // 15. GAME_TRAINING_PAUSE (150) / UNPAUSE (151).
    Post(Empty(), 150, 1);
    Drain();
    Check(lrGameState.mbTrainingPause, "150 @0x82237E84: +0x1C2 mbTrainingPause = 1");
    Drain();
    Check(lrGameState.mbTrainingPause, "150: +0x1C2 is NOT per-frame");
    Post(Empty(), 151, 1);
    Drain();
    Check(!lrGameState.mbTrainingPause, "151 @0x82237E94: +0x1C2 = 0");

    // 16. PAYBACK_ACTIVATED (215) / PAYBACK_OVER (216).
    Post(PaybackActivated(1), 215, 12);
    Drain();
    Check(lrGameState.mbPaybackActive && static_cast<s32>(lrGameState.meActivePaybackType) == 1,
          "215 @0x82238488: +0xF0 = 1, +0xF4 = record +0x00 (1)");
    Drain();
    Check(lrGameState.mbPaybackActive, "215: +0xF0 is NOT per-frame");
    Post(Empty(), 216, 1);
    Drain();
    Check(!lrGameState.mbPaybackActive && static_cast<s32>(lrGameState.meActivePaybackType) == 3,
          "216 @0x822384C8: +0xF0 = 0, +0xF4 = 3 (li r11, 3)");

    // 17. SOUND_TRIGGER (218): the player in a tunnel.
    Post(SoundTrigger(0x00012345u, 1, 0x01), 218, 32);
    Drain();
    Check(lrGameState.mbPlayerInTunnel, "218 @0x82238550: player entity + AT_ENTITY + bit 0 -> +0xF8 = 1");
    Drain();
    Check(!lrGameState.mbPlayerInTunnel, "the prologue clears +0xF8 (0x82237370)");
    {
        bool lbAllBits = true;
        for (u32 luBit = 0; luBit < 6; ++luBit)
        {
            Post(SoundTrigger(0x00012345u, 1, 1u << luBit), 218, 32);
            Drain();
            lbAllBits = lbAllBits && lrGameState.mbPlayerInTunnel;
        }
        Check(lbAllBits, "218: each of bits 0..5 alone raises +0xF8 (0x8223857C..0x822385C0)");
    }
    Post(SoundTrigger(0x00012345u, 1, 0x40u | 0x80u | 0x100u), 218, 32);
    Drain();
    Check(!lrGameState.mbPlayerInTunnel, "218: bits above 5 raise nothing");
    Post(SoundTrigger(0x00054321u, 1, 0x3Fu), 218, 32);
    Drain();
    Check(!lrGameState.mbPlayerInTunnel, "218: another entity's trigger raises nothing (cmplw vs RaceCarState +0x3C8)");
    Post(SoundTrigger(0x00012345u, 2, 0x3Fu), 218, 32);
    Drain();
    Check(!lrGameState.mbPlayerInTunnel, "218: AHEAD_OF_ENTITY (type 2) raises nothing (cmpwi 1)");
    Post(SoundTrigger(0x00012345u, 1, 0x20u), 218, 32);
    Post(SoundTrigger(0x00054321u, 1, 0x3Fu), 218, 32);
    Drain();
    Check(lrGameState.mbPlayerInTunnel, "218: a later non-matching trigger does not clear an earlier match");

    // 18. CAR_ADDITION_PRESENTATION_END (224).
    lrGameState.mbNewCarAdded                       = true;
    lrGameState.mfCarAddedPresentationTimeRemaining = 2.5f;
    lrGameState.meAddedCarID                        = static_cast<EActiveRaceCarIndex>(2);
    Post(Empty(), 224, 1);
    Drain();
    Check(!lrGameState.mbNewCarAdded && lrGameState.mfCarAddedPresentationTimeRemaining == 0.0f,
          "224 @0x822382C0: +0x152 = 0, +0x154 = 0.0 (f31 == flt_82001CC0)");
    Check(static_cast<s32>(lrGameState.meAddedCarID) == 2, "224: +0x158 meAddedCarID untouched");

    // 19. The tail: the director's mbForceSloMoNotAllowed latch (flag tail +0x35431 = byte 0x01).
    lrGameState.mbCanUseSlomo = true;
    gDirector.maStateFlagTail[0x01] = 1;
    Drain();
    Check(!lrGameState.mbCanUseSlomo, "tail 0x8223886C: +0x35431 set -> +0x100 mbCanUseSlomo = 0");
    gDirector.maStateFlagTail[0x01] = 0;
    lrGameState.mbCanUseSlomo = true;
    Drain();
    Check(lrGameState.mbCanUseSlomo, "tail: +0x35431 clear -> +0x100 untouched");

    // 20. The two inactivity clocks (sim step 1/60) and the been-active latch.
    const f32 KF_STEP = 1.0f / 60.0f;
    gInput.mControllerInfo.mbAnyInput  = false;
    gInput.mbSimPaused                 = false;
    gInput.maRaceCarInfo[0].mbEngineOn = false;
    lrGameState.mfPadInactiveTime    = 0.0f;
    lrGameState.mfPlayerInactiveTime = 0.0f;
    lrGameState.mbPlayerBeenActive   = false;
    Drain();
    Drain();
    Drain();
    Check(std::fabs(lrGameState.mfPadInactiveTime - 3.0f * KF_STEP) < 1e-6f,
          "tail 0x82238964..0x8223897C: no pad input, running sim -> +0x148 += the sim step each drain");
    Check(std::fabs(lrGameState.mfPlayerInactiveTime - 3.0f * KF_STEP) < 1e-6f,
          "tail 0x822389D8..0x822389F8: engine off, no input -> +0x14C += the sim step");
    Check(!lrGameState.mbPlayerBeenActive, "tail: engine off -> +0x150 not raised");
    gInput.maRaceCarInfo[0].mbEngineOn = true;
    Drain();
    Check(std::fabs(lrGameState.mfPadInactiveTime - 4.0f * KF_STEP) < 1e-6f, "tail: the engine does not reset the PAD clock");
    Check(lrGameState.mfPlayerInactiveTime == 0.0f, "tail 0x822389B4: the player's mbEngineOn (+0x4E6) -> +0x14C = 0.0");
    Check(lrGameState.mbPlayerBeenActive, "tail 0x82238A28..0x82238A3C: mbEngineOn -> +0x150 = 1");
    gInput.maRaceCarInfo[0].mbEngineOn = false;
    gInput.mControllerInfo.mbAnyInput  = true;
    Drain();
    Check(lrGameState.mfPadInactiveTime == 0.0f && lrGameState.mfPlayerInactiveTime == 0.0f,
          "tail: ControllerInfo::mbAnyInput -> both clocks = 0.0 (0x8223894C / 0x822389C8)");
    Check(lrGameState.mbPlayerBeenActive, "tail: +0x150 is a latch (engine off again, still 1)");
    gInput.mControllerInfo.mbAnyInput = false;
    Drain();
    gInput.mbSimPaused = true;
    Drain();
    Check(lrGameState.mfPadInactiveTime == 0.0f && lrGameState.mfPlayerInactiveTime == 0.0f,
          "tail: a paused sim (input +0x7AC8) -> both clocks = 0.0 (0x82238958 / 0x822389D4)");
    gInput.mbSimPaused = false;

    Check(gAsserts == 0, "no assert fired");

    std::printf("FxDirectorInputQueue: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
