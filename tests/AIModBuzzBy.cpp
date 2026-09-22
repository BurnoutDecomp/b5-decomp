// FX-AIMOD (crash parity 2026-09-22), G05-D1: BuzzBy::Prepare, inlined in AIModule::Prepare @0x82798070
// stage 4 (0x827982EC..0x82798380). The runner extracts the PRODUCTION Prepare, ChooseAheadOrBehind,
// the file-static mRandom and the tuning constants from BrnAIBuzzBy.cpp; CgsRandom.cpp is compiled
// in whole (RandomFloat). ResetOnTrackRequest::Construct is an observed fixture.
//
// Console expectation: after Prepare the 0x8300D5A0 Random holds Random::Construct()'s folded state,
// so ChooseAheadOrBehind (@0x827718B8, the only reader) draws 0.0 first -> BEHIND (type 3), then
// 0.783 (> 0.5 -> AHEAD) and 0.193 (< 0.8 -> FROM_TURNINGS, type 5).
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/World/AI/BrnAIBuzzBy.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h"
#undef protected
#undef private
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned gAssertions = 0, gChecks = 0, gFailures = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcText, const char*, int) { ++gAssertions; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
void* EndAssert() { return nullptr; }
} }

namespace BrnAI { namespace AIModuleIO {
void ResetOnTrackRequest::Construct(EGlobalRaceCarIndex leGlobalRaceCarIndex, f32 lfResetSpeed,
                                    f32 lfResetDistance, BrnAI::EResetType leResetType)
{
    meGlobalRaceCarIndex = leGlobalRaceCarIndex;
    mfResetSpeed = lfResetSpeed;
    mfResetDistance = lfResetDistance;
    meResetType = leResetType;
}
} }

#include "restored_methods.inc"

using namespace BrnAI;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass) { ++gFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

int main()
{
    static BuzzBy sBuzzBy;
    std::memset(&sBuzzBy, 0xCD, sizeof(sBuzzBy));   // Prepare must not rely on zeroed storage
    std::memset(&mRandom, 0, sizeof(mRandom));      // the file-static's .bss state before Prepare

    AICar* const lpCars = reinterpret_cast<AICar*>(0x1000);
    ResetOnTrackManager* const lpManager = reinterpret_cast<ResetOnTrackManager*>(0x2000);
    sBuzzBy.Prepare(lpCars, lpManager);

    Check(sBuzzBy.mpGlobalRaceCars == lpCars && sBuzzBy.mpResetOnTrackManager == lpManager, "Prepare stores both pointers (0x82798374/78)");
    Check(sBuzzBy.mfTimeInFreeRoam == 0.0f, "Prepare zeroes mfTimeInFreeRoam (0x827982EC)");
    Check(!sBuzzBy.mbIsInGameMode && !sBuzzBy.mbIsInJunkyard, "Prepare clears the in-mode / in-junkyard flags (0x827982F0/F4)");
    Check(sBuzzBy.miNumActiveCars == 0, "Prepare empties the active list (0x827982FC)");
    Check(sBuzzBy.mbResetBuzzTimers, "Prepare requests a buzz-timer reset (0x8279837C stb 1)");
    Check(sBuzzBy.miCarsAwaitingCollection == 0, "Prepare clears the cars awaiting collection (0x82798380)");
    Check(mRandom.muSeed == 0xB5E330D02EC654DAull && mRandom.mauIntegerBuffer[1] == 0x3FE43E6Cu && mRandom.muOldestBufferIndex == 0u,
          "Prepare Constructs the file-static Random (0x82798304..0x82798370)");

    AIModuleIO::ResetOnTrackRequest lRequest{};
    sBuzzBy.ChooseAheadOrBehind(&lRequest, 20.0f, static_cast<EGlobalRaceCarIndex>(4));
    Check(lRequest.meResetType == E_RESET_TYPE_BEHIND_PLAYER_ROAD_RAGE && lRequest.mfResetDistance == -60.0f,
          "first buzz-by after Prepare: draw 0.0 -> BEHIND (type 3)");
    sBuzzBy.ChooseAheadOrBehind(&lRequest, 20.0f, static_cast<EGlobalRaceCarIndex>(5));
    Check(lRequest.meResetType == E_RESET_TYPE_FROM_TURNINGS_ROAD_RAGE,
          "second buzz-by: draws 0.783 then 0.193 -> AHEAD from side turnings (type 5)");

    Check(gAssertions == 0, "valid calls raise no assertion");
    std::printf("AIModBuzzBy: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
