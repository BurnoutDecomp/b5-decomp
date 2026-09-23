// FX-VEHPHYS (crash parity 2026-09-23, G52-D1): BrnPhysics::Vehicle::VehiclePhysics::
// UpdateSuspensionPostSimulation @0x825F6BB0 -- the first wheel loop (0x825F6C74..0x825F7130),
// the above-plane wheel repair and its suspension-reach gate.
//
// run_fxvehphys_suspension_reach.py EXTRACTS the production loop text from VehiclePhysics.cpp
// (from its "@0x825F6C74..7130" banner up to the "@0x825F7134..729C" banner of the next loop) and
// compiles it into ReachFixture::ReachLoop. The fixture carries maWheels / mTransform /
// mPreviousControls with their real types; the [wsus-rep] probe helpers are disarmed.
//
// Console facts checked (ARTIST asm):
//   0x825F6E5C..6E70 W = TransformPoint(mTransform, mPosition) ; 0x825F6E90 f0 = dot(n,contact) - dot(n,W)
//   0x825F6E78..6EAC vrefp(dot(n,-n)) + 2 Newton ; 0x825F6EB0 t = recip * f0 ; 0x825F6EB8 f31 = t
//   0x825F6EE0 v126 = W - n*t (the wheel contact point on the road plane)
//   0x825F7048/7060 v13 = (p.y - susp.x) + r ; 0x825F7058/705C v4 = splat(t)
//   0x825F7064 vcmpgefp. v13,v13,v4 -> REACH GATE: (p.y - susp.x + r) >= t
//   0x825F7098..70B8 dot(Up, n) > 0.5 (f29) ; 0x825F70E0..70FC d = dot(Up, v126 - (W - n*r)) > 0 (f30)
//   0x825F7104..7120 p.y = max(p.y + d, susp.x)
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

static std::string gLog;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int) { std::printf("ASSERT: %s\n", lpcMessage); return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Message { u64 gxMessageFilterFlags = 1; }
namespace Log
{
    StrStreamBase& DebugPrint::operator<<(const char* lpcText) { gLog += lpcText; return *this; }
    void WriteToLog(const char*) {}
    static DebugPrint sCapture;
    DebugPrint* gpDebugPrint = &sCapture;
}
}

#include "GameShared/GameClasses/Development/CgsStrStream.cpp"

namespace BrnPhysics
{
namespace Vehicle
{
    namespace vpu = rw::math::vpu;

    // The production [wsus-rep] witness helpers, disarmed (not console state).
    inline bool WheelSusProbeArmed() { return false; }
    inline bool WheelSusTakeLine() { return false; }
    inline u32& WheelSusFrameFor(const void*) { static u32 su = 0u; return su; }

    struct ReachFixture
    {
        decltype(VehiclePhysics::maWheels)          maWheels{};
        decltype(VehiclePhysics::mTransform)        mTransform{};
        decltype(VehiclePhysics::mPreviousControls) mPreviousControls;

        void ReachLoop();
    };

    void ReachFixture::ReachLoop()
    {
#include "suspension_reach_loop.inc"
    }
}
}

using namespace BrnPhysics::Vehicle;

static int giChecks = 0, giFailures = 0;

// One grounded wheel (index 2), identity body transform, flat road n = (0,1,0), radius 0.35.
// Every other wheel is airborne and must be left alone.
static void Case(const char* lpcName, f32 lfWheelY, f32 lfMinSuspension, f32 lfContactY, f32 lfExpectedY)
{
    ReachFixture lCar;
    lCar.mTransform.SetIdentity();
    for (auto& lrWheel : lCar.maWheels)
    {
        lrWheel.mRoadContact.mbIsOnGround = false;
        lrWheel.mu8State = 0;
        lrWheel.mPosition = Vector3{ 0.8f, 0.123f, -1.4f, 0.0f };
    }
    Wheel& lrWheel = lCar.maWheels[2];
    lrWheel.mRoadContact.mbIsOnGround = true;
    lrWheel.mRoadContact.mNormal   = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
    lrWheel.mRoadContact.mPosition = Vector3{ -0.8f, lfContactY, 1.4f, 0.0f };
    lrWheel.mPosition = Vector3{ -0.8f, lfWheelY, 1.4f, 0.0f };
    lrWheel.mSlipVariables.w = 0.35f;
    lrWheel.mSuspensionAndInertiaVariables.x = lfMinSuspension;

    lCar.ReachLoop();

    bool lbPass = std::fabs(lCar.maWheels[2].mPosition.y - lfExpectedY) <= 1.0e-6f;
    for (int i : { 0, 1, 3 })
        lbPass = lbPass && lCar.maWheels[i].mPosition.y == 0.123f;
    ++giChecks;
    std::printf("%s: wheel y %.6f (want %.6f)\n", lpcName, lCar.maWheels[2].mPosition.y, lfExpectedY);
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}

int main()
{
    // (A) t = 0.30, reach = 0.0 - 0.10 + 0.35 = 0.25 < t: the console SKIPS the repair.
    //     (The >= 0 gate let it in: d = r - t = 0.05, p.y = max(0.05, 0.10) = 0.10.)
    Case("(A) reach 0.25 < t 0.30: no repair", 0.0f, 0.10f, -0.30f, 0.0f);
    // (B) p.y below its down stop: t = -0.80, reach = -0.65 >= t: the console REPAIRS,
    //     d = r - t = 1.15, p.y = max(-1.0 + 1.15, 0.0) = 0.15. (The >= 0 gate kept -1.0.)
    Case("(B) reach -0.65 >= t -0.80: repaired to 0.15", -1.0f, 0.0f, -0.20f, 0.15f);
    // (C) control: t = 0.30, reach = 0.45: both gates repair, p.y = max(0.2 + 0.05, 0.1) = 0.25.
    Case("(C) reach 0.45 >= t 0.30: repaired to 0.25", 0.2f, 0.10f, -0.10f, 0.25f);

    std::printf("FxVehphysSuspensionReach: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
