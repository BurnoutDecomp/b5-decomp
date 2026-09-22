// FX-AIDRV regression G01-D2 (crash parity 2026-09-22): the production AICar::Reset, extracted
// verbatim from src/GameSource/World/AI/BrnAICar_Update.cpp by run_aidrv_car_reset.py, checked
// against the ARTIST assembly @0x82792800.
//
// Console, inside the !lbKeepTransform block (`clrlwi r10,r29,24 ; bne 0x827928BC` @0x82792898):
//   0x82792848 li r30,0 ; 0x8279284C li r11,0x7FFF ; 0x82792850 li r9,1   (no later writes)
//   0x827928A8 sth r11,0x1530   muResetOnTrackSectionIndex = 0x7FFF
//   0x827928AC stb r30,0x1538   muResetOnTrackStartPortal  = 0
//   0x827928B0 stb r9,0x1539    muResetOnTrackEndPortal    = 1
//   0x827928B4 stw r8(0x14),0x14D0  meResetSpeedType = 20 (E_RESET_SPEED_TYPE_COUNT)
//   0x827928B8 stvx128 v0(vspltisw 0),r31,0x1430   mPosition = 0
// (The PS3 DecFIGS Reset @0x9C68EC stores the same pair: +5433 = 1, +5432 = 0.) With
// lbKeepTransform the block is skipped and all five keep their values.
// The fixtures are the two console callees whose bodies are not under test (SetDirection /
// SetRight store their argument) and the five Aggressiveness setters (plain stores).
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAICar_Constants.h"
#include "GameSource/World/AI/Route/BrnRoute.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>

static unsigned gAssertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace BrnAI {
void AICar::SetDirection(Vector3 lDirection) { mDirection = lDirection; }
void AICar::SetRight(Vector3 lRight) { mRight = lRight; }
void Aggressiveness::SetAggression(f32 lfAggression) { mfAggressionLevel = lfAggression; mbAggressionLevelSet = true; }
void Aggressiveness::SetProximityToSpeedMatch(f32 lfValue) { mfProximitySpeedMatch = lfValue; }
void Aggressiveness::SetTimeForSpeedMatch(f32 lfValue) { mfTimeForSpeedMatch = lfValue; }
void Aggressiveness::SetRelativeSpeedForMatch(f32 lfValue) { mfRelativeSpeedForSpeedMatch = lfValue; }
void Aggressiveness::SetAcclerationRateForSpeedMatch(f32 lfValue) { mfAcclerationRateForSpeedMatch = lfValue; }
}

#include "restored_methods.inc"

using namespace BrnAI;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        if (!lbPass) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
    }
    bool AllLanesZero(const Vector3& lrV) { return lrV.x == 0.0f && lrV.y == 0.0f && lrV.z == 0.0f && lrV.w == 0.0f; }

    // A car mid-race: a reset section/portal pair and a position recorded by
    // UpdateResetOnTrackSection / UpdateInRangeData.
    void Dirty(AICar& lrCar)
    {
        lrCar.muResetOnTrackSectionIndex = 0x0123;
        lrCar.muResetOnTrackStartPortal  = 7;
        lrCar.muResetOnTrackEndPortal    = 9;
        lrCar.meResetSpeedType           = static_cast<EResetSpeedType>(3);
        lrCar.mPosition                  = Vector3{ 10.0f, 20.0f, 30.0f, 0.0f };
    }
}

int main()
{
    static AICar sCar{};

    // ---- lbKeepTransform == false: the console's five stores ---------------------------------
    Dirty(sCar);
    sCar.Reset(static_cast<EPersonalityType>(0), false);
    Check(sCar.muResetOnTrackSectionIndex == 0x7FFF, "section index -> 0x7FFF (sth @0x827928A8)");
    Check(sCar.muResetOnTrackStartPortal == 0, "start portal -> 0 (stb r30 @0x827928AC)");
    Check(sCar.muResetOnTrackEndPortal == 1, "end portal -> 1 (stb r9 @0x827928B0)");
    Check(static_cast<s32>(sCar.meResetSpeedType) == 20, "reset speed type -> 20 (stw 0x14 @0x827928B4)");
    Check(AllLanesZero(sCar.mPosition), "position -> 0 (stvx128 @0x827928B8)");

    // ---- lbKeepTransform == true: the block is skipped ---------------------------------------
    Dirty(sCar);
    sCar.Reset(static_cast<EPersonalityType>(1), true);
    Check(sCar.muResetOnTrackSectionIndex == 0x0123, "keep transform: section index kept");
    Check(sCar.muResetOnTrackStartPortal == 7, "keep transform: start portal kept");
    Check(sCar.muResetOnTrackEndPortal == 9, "keep transform: end portal kept");
    Check(static_cast<s32>(sCar.meResetSpeedType) == 3, "keep transform: reset speed type kept");
    Check(sCar.mPosition.x == 10.0f && sCar.mPosition.y == 20.0f && sCar.mPosition.z == 30.0f,
          "keep transform: position kept");

    Check(gAssertions == 0, "no assertion fired");
    std::printf("AIDrvCarReset: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
