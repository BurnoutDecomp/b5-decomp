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
#include "GameSource/Director/DirectorModule/BrnDirectorGameState.h"
#include "GameSource/GameState/BrnGameActions.h"
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
    alignas(16) u8 mau8Bytes[64];
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
    Check(gAsserts == 0, "no assert fired");

    std::printf("FxDirectorInputQueue: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
