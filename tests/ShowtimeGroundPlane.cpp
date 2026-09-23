// Harness for run_showtime_ground_plane.py (crash parity G37-D2 / G37-D3). Two shipped blocks of
// RaceCarPhysics.cpp are pasted in through extracted.inc:
//   * UpdateAftertouch's camera-axis block -> CameraAxes(): the console zeroes lane y of camera X
//     and Z BEFORE the assert and the normalise (0x8262EC70 / 0x8262ED14);
//   * UpdateTargetAssist's candidate loop -> PickTarget(): the console zeroes lane y of
//     (target - car) before distance, unit and the 0.766 dot gate (0x82620078).
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>

#define CGS_ASSERT(c, m) do { (void)sizeof(c); } while (0)

namespace vpu = rw::math::vpu;

struct TargetId { u32 muValue; };
struct Params
{
    Vector3  maTargetPositions[8];
    TargetId maTargetIds[8];
    s32      miNumTargets;
    s32      miCurrentTargetId;
};
static Params MS;

#include "extracted.inc"

int main()
{
    int liChecks = 0, liFailures = 0;
    auto Check = [&](bool lbPass, const char* lpcName) {
        ++liChecks;
        if (!lbPass) { ++liFailures; std::printf("FAIL: %s\n", lpcName); }
    };
    auto Near = [](float a, float b) { return std::fabs(a - b) < 1e-4f; };

    // A chase camera pitched 25 degrees down, looking along +z.
    const float s = std::sin(0.4363323f), c = std::cos(0.4363323f);
    Matrix44Affine lCamera;
    lCamera.xAxis = { 1.0f, 0.0f, 0.0f, 0.0f };
    lCamera.yAxis = { 0.0f, c, s, 0.0f };
    lCamera.zAxis = { 0.0f, -s, c, 0.0f };
    lCamera.wAxis = { 0.0f, 0.0f, 0.0f, 1.0f };
    Vector3 lvX, lvZ;
    CameraAxes(&lCamera, lvX, lvZ);
    Check(Near(lvZ.y, 0.0f), "camera Z is flattened (no vertical aftertouch push)");
    Check(Near(lvZ.z, 1.0f), "flattened camera Z is re-normalised to unit length");
    Check(Near(lvX.x, 1.0f) && Near(lvX.y, 0.0f), "camera X stays the unit right axis");

    // The car is airborne 10 m above a target 10 m ahead, aiming horizontally along +x. On the
    // ground plane the target is dead ahead (dot 1.0 > 0.766); in 3D it is 45 degrees down
    // (dot 0.707 < 0.766) and would be rejected.
    MS.miNumTargets = 1;
    MS.miCurrentTargetId = -1;
    MS.maTargetIds[0].muValue = 7u;
    MS.maTargetPositions[0] = { 10.0f, 0.0f, 0.0f, 0.0f };
    const Vector3 lvCar = { 0.0f, 10.0f, 0.0f, 0.0f };
    const Vector3 lvAim = { 1.0f, 0.0f, 0.0f, 0.0f };
    Check(PickTarget(lvCar, lvAim) == 0, "a target below and ahead passes the ground-plane dot gate");

    std::printf("ShowtimeGroundPlane: %d checks, %d failures\n", liChecks, liFailures);
    return liFailures ? 1 : 0;
}
