// FX-AINAN2 / crash parity CC-10 regression (2026-09-24): AIModule::OnRaceCarReachedCheckpoint
// @0x8278A658 and the three AICar members the console inlines into it (InvalidateRoute,
// OnReachedCheckpoint, IsOpponent), extracted verbatim from BrnAIModule_Events.cpp and
// BrnAICar_Update.cpp by run_fxainan2_checkpoint.py.
//
// Console, tools/re/ppcdis.py 0x8278A658 (an ARTIST export hole; 50 insns, r31 = action, r4 = car):
//   0x8278A674/0x8278A678  lpCar = GetAICar(this, lwz 4(action) == meGlobalRaceCarIndex)
//   0x8278A698  stfs flt_82001CC0 (0.0)                 -> car+0x14F4  mfWrongWayTime
//   0x8278A69C  stw 0                                   -> car+0x1408  Route::meStatus
//   0x8278A6A0  stw 0                                   -> car+0x1400  Route::miNodeCount
//   0x8278A6C0  stfs flt_82F302F4 (0x7F7FFFFF, FLT_MAX)  -> car+0x14F0  mfDistanceToCheckpoint
//   0x8278A6C8  sth lhz 0xC(action)                     -> car+0x1536  muDestinationSectionIndex
//   0x8278A6CC  stw lwz 8(action) + 1                   -> car+0x1520  miCurrentCheckpoint
//   0x8278A6D0  stb (car+0x14C0 == 1)                   -> car+0x153D  mbHasBlockCheckpoints
//   0x8278A6C4..0x8278A6F0  car+0x153A != 0xFF && car+0x1549 == 0 (IsOpponent), then
//   0x8278A6F4..0x8278A700  RaceBalancingManager::OnOpponentReachedCheckpoint(this + 0x3D9D0,
//                                                                            car, lwz 8(action))
// The old body was a named park that did nothing, so every store / call check fails on it; the
// "untouched" controls pass on both.
//
// Fixtures: AIModule::GetAICar answers a four-car table and records the index it was asked for;
// RaceBalancingManager::OnOpponentReachedCheckpoint records its calls. The module lives in raw
// storage (its constructor is in another TU); the body under test reads only its own members.
#define BRNAI_AIWAVE_A3_LANDED 1
#include "GameSource/World/AI/BrnAIModule.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/Route/BrnRoute.h"
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingManager.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>

static unsigned gAssertions = 0;
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace
{
    BrnAI::AICar gaCars[4];
    unsigned guLastCarIndex = 0xFFFFFFFFu;

    struct BalancingCall
    {
        const BrnAI::RaceBalancingManager* mpManager;
        const BrnAI::AICar* mpCar;
        s32 miCheckpointIndex;
    };
    BalancingCall gaCalls[4];
    unsigned guCalls = 0;
}

namespace BrnAI {
AICar* AIModule::GetAICar(u32 luIndex) const
{
    guLastCarIndex = luIndex;
    return luIndex < 4u ? &gaCars[luIndex] : nullptr;
}
void RaceBalancingManager::OnOpponentReachedCheckpoint(const AICar* lpAICar, s32 liCheckpointIndex)
{
    if (guCalls < 4u)
    {
        gaCalls[guCalls].mpManager = this;
        gaCalls[guCalls].mpCar = lpAICar;
        gaCalls[guCalls].miCheckpointIndex = liCheckpointIndex;
    }
    ++guCalls;
}
}

#include "restored_methods.inc"

using namespace BrnAI;
using BrnGameState::GameStateModuleIO::RaceCarReachedCheckpointAction;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        if (!lbPass) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
    }

    alignas(16) unsigned char gaModule[sizeof(AIModule)];
    AIModule& Module() { return *reinterpret_cast<AIModule*>(gaModule); }

    u32 Bits(f32 lfValue) { u32 luBits; std::memcpy(&luBits, &lfValue, 4); return luBits; }

    // A car mid-route: every field the console writes holds a stale, non-console value first.
    void Seat(AICar& lrCar, ERouteFindingStyle leStyle, s8 liOpponent, bool lbIsPlayer)
    {
        lrCar = AICar{};
        Route* const lpRoute = lrCar.GetRoute();
        lpRoute->meStatus = Route::E_STATUS_COMPLETE;
        lpRoute->miNodeCount = 9;
        lrCar.mfWrongWayTime = 5.0f;
        lrCar.mfDistanceToCheckpoint = 123.0f;
        lrCar.muDestinationSectionIndex = 11;
        lrCar.miCurrentCheckpoint = 1;
        lrCar.mbHasBlockCheckpoints = (leStyle == E_ROUTE_FINDING_RACE) ? 0 : 1;
        lrCar.meRouteFindingStyle = leStyle;
        lrCar.miOpponentIndex = liOpponent;
        lrCar.mbIsPlayer = lbIsPlayer;
        // Neighbours the console does NOT write.
        lrCar.miRouteTimeStamp = 42;
        lrCar.mfRaceTimer = 17.0f;
        lrCar.meSpeedSelectionMethod = 3;
        lrCar.muDefaultSectionIndex = 55;
    }

    void Send(EGlobalRaceCarIndex leGlobal, s32 liCheckpoint, u16 luNextSection)
    {
        RaceCarReachedCheckpointAction lAction;
        std::memset(&lAction, 0, sizeof(lAction));
        lAction.meActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(0);
        lAction.meGlobalRaceCarIndex = leGlobal;
        lAction.miCheckPointIndex = liCheckpoint;
        lAction.muNextCheckpointAISectionIndex = luNextSection;
        lAction.mbIsLocalPlayer = false;
        guCalls = 0;
        guLastCarIndex = 0xFFFFFFFFu;
        Module().OnRaceCarReachedCheckpoint(&lAction);
    }

    void CheckStores(const AICar& lrCar, s32 liExpectedCheckpoint, u16 luExpectedSection,
                     u8 luExpectedBlock, const char* lpcWho)
    {
        char lacLabel[160];
        const Route* const lpRoute = lrCar.GetRoute();
        std::snprintf(lacLabel, sizeof(lacLabel), "%s: mfWrongWayTime (+0x14F4) = 0.0 (0x8278A698)", lpcWho);
        Check(lrCar.mfWrongWayTime == 0.0f, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "%s: Route::meStatus (+0x1408) = 0 (0x8278A69C)", lpcWho);
        Check(lpRoute->meStatus == Route::E_STATUS_UNINITIALISED, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "%s: Route::miNodeCount (+0x1400) = 0 (0x8278A6A0)", lpcWho);
        Check(lpRoute->miNodeCount == 0, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "%s: mfDistanceToCheckpoint (+0x14F0) = flt_82F302F4 0x7F7FFFFF (0x8278A6C0)", lpcWho);
        Check(Bits(lrCar.mfDistanceToCheckpoint) == 0x7F7FFFFFu, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "%s: muDestinationSectionIndex (+0x1536) = next checkpoint section (0x8278A6C8)", lpcWho);
        Check(lrCar.muDestinationSectionIndex == luExpectedSection, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "%s: miCurrentCheckpoint (+0x1520) = checkpoint + 1 (0x8278A6CC)", lpcWho);
        Check(lrCar.miCurrentCheckpoint == liExpectedCheckpoint, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "%s: mbHasBlockCheckpoints (+0x153D) = (style == RACE) (0x8278A6D0)", lpcWho);
        Check(lrCar.mbHasBlockCheckpoints == luExpectedBlock, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "%s: control -- the fields the console does not write are untouched", lpcWho);
        Check(lrCar.miRouteTimeStamp == 42 && lrCar.mfRaceTimer == 17.0f &&
              lrCar.meSpeedSelectionMethod == 3 && lrCar.muDefaultSectionIndex == 55, lacLabel);
    }
}

int main()
{
    std::memset(gaModule, 0, sizeof(gaModule));

    // 1. A racing opponent (slot 2, not the player) passes checkpoint 3; the next one is section 77.
    Seat(gaCars[2], E_ROUTE_FINDING_RACE, 2, false);
    Send(static_cast<EGlobalRaceCarIndex>(2), 3, 77);
    Check(guLastCarIndex == 2u, "GetAICar is asked for the action's global index (lwz 4(action), 0x8278A674)");
    CheckStores(gaCars[2], 4, 77, 1, "opponent");
    Check(guCalls == 1u, "an opponent makes exactly one RaceBalancingManager::OnOpponentReachedCheckpoint call (0x8278A700)");
    Check(guCalls >= 1u && gaCalls[0].mpManager == &Module().mRaceBalancingManager,
          "... on this + 0x3D9D0 == &mRaceBalancingManager (0x8278A6F4/0x8278A6FC)");
    Check(guCalls >= 1u && gaCalls[0].mpCar == &gaCars[2], "... with the car");
    Check(guCalls >= 1u && gaCalls[0].miCheckpointIndex == 3,
          "... and the checkpoint index as posted, NOT + 1 (lwz 8(action) reloaded, 0x8278A6F8)");

    // 2. The player's own AICar (opponent slot 0, mbIsPlayer): the stores, but no rubber band.
    Seat(gaCars[0], E_ROUTE_FINDING_RACE, 0, true);
    Send(static_cast<EGlobalRaceCarIndex>(0), 2, 12);
    CheckStores(gaCars[0], 3, 12, 1, "player");
    Check(guCalls == 0u, "the player skips the rubber band (lbz 0x1549 ; bne 0x8278A6E0)");

    // 3. A car with no opponent slot (0xFF) that is not the player: the stores, no rubber band.
    Seat(gaCars[1], E_ROUTE_FINDING_FREE_ROAM, -1, false);
    Send(static_cast<EGlobalRaceCarIndex>(1), 5, 300);
    CheckStores(gaCars[1], 6, 300, 0, "no-slot free-roamer");
    Check(guCalls == 0u, "opponent index 0xFF skips the rubber band (cmplwi 0xFF ; beq 0x8278A6D4)");

    // 4. A marked-man opponent: IsOpponent holds, but only RACE blocks checkpoint sections.
    Seat(gaCars[3], E_ROUTE_FINDING_MARKED_MAN, 1, false);
    Send(static_cast<EGlobalRaceCarIndex>(3), 7, 0);
    CheckStores(gaCars[3], 8, 0, 0, "marked-man opponent");
    Check(guCalls == 1u && gaCalls[0].mpCar == &gaCars[3] && gaCalls[0].miCheckpointIndex == 7,
          "a non-race opponent still reaches the rubber band");

    Check(gAssertions == 0u, "control: no assert fires");

    std::printf("FxAinan2Checkpoint: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
