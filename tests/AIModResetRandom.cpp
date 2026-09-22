// FX-AIMOD (crash parity 2026-09-22), G07-D6: ResetOnTrackManager::ResetAwayFromPlayer @0x82784148
// draws from the ONE CgsNumeric::Random at .data 0x8300D5D0 that AICar::Construct / AICar::Reset
// re-seed (findinit: readers ResetAwayFromPlayer 0x827841BC, ResetAheadFromSideTurnings 0x82790A60,
// ScanForwardsAndAlongJunction 0x82785314; writers AICar::Construct / AICar::Reset).
// The runner extracts the PRODUCTION ResetAwayFromPlayer + its helper namespace (Strategies.cpp),
// GetAICar (BrnResetOnTrackManager.cpp) and AICar::Reset + GetRandomNumber + the stream
// (BrnAICar_Update.cpp). Section geometry is a fixture.
//
// After any Reset the draws are 0.0, 0.78315, 0.19289, 0.70422 ... With 100 sections, section 19 and
// section 70 far (> 2000 m) and every other section at the player, a post-Reset call probes 0, 78,
// then accepts 19 -- on EVERY call that follows a Reset, as on the console.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAICar_Constants.h"
#include "GameSource/World/AI/Route/BrnRoute.h"
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#undef protected
#undef private
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned gAssertions = 0, gChecks = 0, gFailures = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcText, const char*, int) { ++gAssertions; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsResource {
BaseResourcePtr::BaseResourcePtr() {}
BaseResourcePtr::~BaseResourcePtr() {}
}

static Vector3 gaMiddles[100];
namespace BrnAI {
void AICar::SetDirection(Vector3 lDirection) { mDirection = lDirection; }
void AICar::SetRight(Vector3 lRight) { mRight = lRight; }
void Aggressiveness::SetAggression(f32 lfAggression) { mfAggressionLevel = lfAggression; mbAggressionLevelSet = true; }
void Aggressiveness::SetProximityToSpeedMatch(f32 lfValue) { mfProximitySpeedMatch = lfValue; }
void Aggressiveness::SetTimeForSpeedMatch(f32 lfValue) { mfTimeForSpeedMatch = lfValue; }
void Aggressiveness::SetRelativeSpeedForMatch(f32 lfValue) { mfRelativeSpeedForSpeedMatch = lfValue; }
void Aggressiveness::SetAcclerationRateForSpeedMatch(f32 lfValue) { mfAcclerationRateForSpeedMatch = lfValue; }
Vector3 AICar::GetPosition() const { return mPosition; }
const AISection* AISectionsData::GetAISection(u32 luIndex) const { return &mpaSections[luIndex]; }
Vector3 AISection::GetMiddle() const { return gaMiddles[mId]; }
}

#include "restored_methods.inc"

using namespace BrnAI;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass) { ++gFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

int main()
{
    static AISection saSections[100];
    for (u32 luSection = 0; luSection < 100; ++luSection)
    {
        saSections[luSection].mId = luSection;
        gaMiddles[luSection] = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
    }
    gaMiddles[19] = Vector3{ 5000.0f, 0.0f, 0.0f, 0.0f };
    gaMiddles[70] = Vector3{ 0.0f, 0.0f, 4000.0f, 0.0f };
    static AISectionsData sData;
    sData.mpaSections = saSections;
    sData.muNumSections = 100;

    static AICar saCars[2];
    static ResetOnTrackManager sManager;
    sManager.mpAISectionData.mpResourceMemory = &sData;
    sManager.mpaAICars = saCars;
    sManager.mePlayerGlobalRaceCarIndex = E_GLOBAL_RACE_CAR_INDEX_0;
    saCars[0].mPosition = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };

    ResetOnTrackManager::ResetOnTrackCoords lCoords{};

    // A car attaches (HandleManagementEvents case 0 -> AICar::Reset re-seeds the stream).
    saCars[1].Reset(static_cast<EPersonalityType>(0), false);
    const bool lbFirst = sManager.ResetAwayFromPlayer(&lCoords);
    Check(lbFirst && lCoords.mpAISection == &saSections[19], "after a Reset: draws 0.0, 0.783, 0.193 -> section 19");

    // Another attach re-seeds again: the console probes the same 0, 78, 19.
    saCars[1].Reset(static_cast<EPersonalityType>(0), false);
    const bool lbSecond = sManager.ResetAwayFromPlayer(&lCoords);
    Check(lbSecond && lCoords.mpAISection == &saSections[19], "after the next Reset: the same stream -> section 19 again");

    // Without a Reset the stream continues: 0.704 -> section 70.
    const bool lbThird = sManager.ResetAwayFromPlayer(&lCoords);
    Check(lbThird && lCoords.mpAISection == &saSections[70], "no Reset in between: the stream continues -> section 70");
    Check(lbThird && lCoords.mDirection.x == 1.0f && lCoords.mDirection.z == 0.0f, "accepted pose faces +X (0x82784334..0x82784368)");

    Check(gAssertions == 0, "valid fixtures raise no assertion");
    std::printf("AIModResetRandom: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
