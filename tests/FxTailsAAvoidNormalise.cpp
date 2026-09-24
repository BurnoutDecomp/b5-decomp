// FX-TAILS-A item 6 (crash parity 2026-09-24): ResetOnTrackManager::AvoidObstacles @0x827941E0 normalises the
// flattened reset direction the console's way -- NO zero guard. run_fxtailsa_avoid_normalise.py extracts the
// production AvoidObstacles / TestRecentResets / GetAICar and the TU helper namespace; TestSectionHNG and
// TestCarHNG are observed fixtures here (TestCarHNG records the direction it is handed).
//
// Console, 0x827942A0..0x827942FC: v0 = v13 * v13 (v13 = the flattened direction, lanes x, z), lane 0 + lane 1
// (vspltw / vaddfp), vrsqrtefp, two Newton-Raphson steps (`vnmsubfp` e = 1 - lenSq*y*y, `vmaddfp` y' = (y*0.5)*e + y,
// both fused; 1.0 / 0.5 from vcfsx 1,0 / 1,1), v125 = v13 * y2 -- and no vcmpeqfp/vsel. For a zero flattened
// direction lenSq = +0, vrsqrtefp gives +inf, 0 * inf = NaN, and v125 -- the direction TestCarHNG gets at
// 0x82794368 -- is NaN in both lanes. The old body kept it (0, 0) behind an invented `lenSq != 0` test.
// A zero flattened reset direction needs a vertical or zero mDirection, which no strategy produces
// (FX-AIRESET), so this is unreachable with the shipped data.
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#include "GameSource/Math/BrnMathUtils.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

static unsigned gAssertions = 0;
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsResource {
BaseResourcePtr::BaseResourcePtr() {}
BaseResourcePtr::~BaseResourcePtr() {}
}

// ---- observed fixtures ------------------------------------------------------------------------------------------
static int     gCarHNGCalls = 0;
static Vector2 gFirstCarHNGDirection{};
namespace BrnAI {
bool ResetOnTrackManager::TestSectionHNG(const AISection*, Vector2, Vector2) { return false; }
bool ResetOnTrackManager::TestCarHNG(const AISection*, const NearbyVehicles*, Vector2, Vector2 lDirection, f32)
{
    if (gCarHNGCalls++ == 0)
    {
        gFirstCarHNGDirection = lDirection;
    }
    return false;
}
}

#include "restored_methods.inc"

using namespace BrnAI;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcLabel);
        if (!lbPass) { ++guFailures; }
    }
    bool IsNaN(f32 lfValue) { return lfValue != lfValue; }
    bool Same(f32 lfA, f32 lfB) { return std::memcmp(&lfA, &lfB, sizeof(f32)) == 0; }

    struct Scene
    {
        ResetOnTrackManager mManager;
        AICar               mCar;
        AISection           mSection;
        ResetOnTrackManager::ResetOnTrackCoords mCoords;
        AIModuleIO::ResetOnTrackRequest         mRequest;

        explicit Scene(Vector3 lDirection)
            : mManager(), mCar(), mSection(), mCoords(), mRequest()
        {
            mManager.mRecentResets.Construct();
            mManager.mpaAICars = &mCar;
            mManager.miResetCount = 0;
            mCar.meCarState = E_AI_CAR_STATE_IN_RANGE;
            mRequest.meGlobalRaceCarIndex = E_GLOBAL_RACE_CAR_INDEX_0;
            mRequest.mfResetSpeed = 20.0f;
            mRequest.mfResetDistance = 0.0f;
            mRequest.meResetType = E_RESET_TYPE_STANDARD;
            mCoords.mpAISection = &mSection;
            mCoords.mPosition = Vector3{ 16.0f, 0.0f, 32.0f, 0.0f };
            mCoords.mDirection = lDirection;
            gCarHNGCalls = 0;
            gFirstCarHNGDirection = Vector2{ -7.0f, -7.0f, 0.0f, 0.0f };
            gAssertions = 0;
        }
        bool Run() { return mManager.AvoidObstacles(&mRequest, &mCoords); }
    };
}

int main()
{
    // ---- a zero flattened direction ---------------------------------------------------------------------------------
    {
        Scene s(Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });
        const bool lbOk = s.Run();
        Check(gCarHNGCalls >= 1, "zero direction: TestCarHNG is reached (0x82794368)");
        Check(IsNaN(gFirstCarHNGDirection.x) && IsNaN(gFirstCarHNGDirection.y),
              "zero direction: TestCarHNG gets v125 = (NaN, NaN), the console's unguarded rsqrt (was (0, 0))");
        Check(lbOk && Same(s.mCoords.mPosition.x, 16.0f) && Same(s.mCoords.mPosition.z, 32.0f),
              "zero direction: nothing in the way -> accepted where it stands (control)");
    }
    {
        Scene s(Vector3{ 0.0f, 1.0f, 0.0f, 0.0f });   // straight up: Flatten -> (0, 0)
        s.Run();
        Check(IsNaN(gFirstCarHNGDirection.x) && IsNaN(gFirstCarHNGDirection.y),
              "vertical direction: flattened (0, 0) -> TestCarHNG gets (NaN, NaN) (was (0, 0))");
    }

    // ---- ordinary directions are unchanged -------------------------------------------------------------------------
    {
        Scene s(Vector3{ 0.0f, 0.0f, 2.0f, 0.0f });
        s.Run();
        Check(Same(gFirstCarHNGDirection.x, 0.0f) && Same(gFirstCarHNGDirection.y, 1.0f),
              "control: (0, 0, 2) -> TestCarHNG gets (0, 1)");
    }
    {
        Scene s(Vector3{ 3.0f, 7.0f, -4.0f, 0.0f });
        s.Run();
        const f32 lfInv = 1.0f / sqrtf(3.0f * 3.0f + (-4.0f) * (-4.0f));   // the pre-fix nonzero arm
        Check(Same(gFirstCarHNGDirection.x, 3.0f * lfInv) && Same(gFirstCarHNGDirection.y, -4.0f * lfInv),
              "control: (3, 7, -4) -> the flattened (3, -4) * (1 / sqrtf(25)), bit for bit as before");
    }

    std::printf("FxTailsAAvoidNormalise: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
