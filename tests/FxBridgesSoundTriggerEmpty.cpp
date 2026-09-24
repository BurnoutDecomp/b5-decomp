// FX-BRIDGES (crash parity 2026-09-24): SoundTriggerAction::IsEmpty @0x82355178 -- the PRODUCTION body and its
// tolerance constant, extracted from src/GameSource/GameState/BrnGameActions.cpp by
// run_fxbridges_sound_trigger_empty.py and compiled against the real SoundTriggerAction.
//   0x82355178..0x823551B8  |mQueryPos| (sign bits cleared) compared lane-wise, STRICTLY greater, against the float
//                           at 0x82029BA4 (image-read 0x34000000 == FLT_EPSILON 1.1920929e-07); w is replaced by x
//                           (`vrlimi128 v13, v0, 1, 1`), so only x / y / z count;
//   0x823551BC..0x823551E0  then entity (+0x10), result type (+0x14) and active triggers (+0x18) must all be 0.
#include "GameSource/GameState/BrnGameActions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { return 0; }
    void* EndAssert() { return nullptr; }
}
}

#include "fxbridges_sound_trigger_epsilon.inc"

namespace BrnGameState
{
namespace GameStateModuleIO
{
#include "fxbridges_sound_trigger_empty.inc"
}
}

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("  FAIL %s\n", lpcName);
    }
    else
    {
        std::printf("  ok   %s\n", lpcName);
    }
}

using BrnGameState::GameStateModuleIO::SoundTriggerAction;

static bool EmptyAt(float x, float y, float z, float w)
{
    SoundTriggerAction lAction;
    std::memset(&lAction, 0, sizeof(lAction));
    lAction.mQueryPos = Vector3{ x, y, z, w };
    return lAction.IsEmpty();
}

int main()
{
    Check(EmptyAt(0.0f, 0.0f, 0.0f, 0.0f), "an all-zero record is empty");
    Check(!EmptyAt(5.0e-5f, 0.0f, 0.0f, 0.0f),
          "a 5e-5 lane is NOT empty: the tolerance is 0x82029BA4 == 1.1920929e-07 (FLT_EPSILON), not 1e-4");
    Check(!EmptyAt(0.0f, -2.0e-6f, 0.0f, 0.0f), "the sign bit is cleared first: |-2e-6| > FLT_EPSILON");
    Check(EmptyAt(0.0f, 0.0f, 1.0e-7f, 0.0f), "a lane at or under FLT_EPSILON still counts as zero (strict vcmpgtfp)");
    Check(EmptyAt(0.0f, 0.0f, 0.0f, 50.0f), "the w lane never counts (vrlimi128 puts x there)");
    SoundTriggerAction lAction;
    std::memset(&lAction, 0, sizeof(lAction));
    lAction.muActiveTriggers = 1u;
    Check(!lAction.IsEmpty(), "active triggers (+0x18) make it non-empty");
    std::printf("FxBridgesSoundTriggerEmpty: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
