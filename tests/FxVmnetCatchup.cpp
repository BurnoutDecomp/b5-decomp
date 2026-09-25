// FX-VMNET (crash parity 2026-09-23, G44-D2): the PRODUCTION body of
// VehicleDriver::StartCatchupInterpolation @0x825FED30 (BrnVehicleDriver.cpp, with its file-scope
// constants), extracted by run_fxvmnet_catchup.py and compiled as a member of the REAL
// VehicleDriver (BrnVehicleDriver.h). That header only forward-declares VehiclePhysics, so this
// harness completes it with a fixture carrying exactly what the body touches: the
// ExternallySimulatedBody stores (transform, the two velocities, mbFrozen) and
// mSimpleAttribs.mCOMOffset.
//
// THE EXPECTED NUMBERS ARE THE CONSOLE'S. They were produced by EXECUTING the ARTIST machine code
// of 0x825FED30 on the FX-FX VMX emulator (extended for this job: vspltw128, vslw128, lvlx,
// lfs/stfs), on bit-identical inputs, with rw::math::vpu::SLerp and OrthoNormalize3x3 hooked so
// that the function's own instructions were what was measured:
//   A  snap flag      -> target = T with its translation moved onto the COM, vehicle = target,
//                        mSlerpTransform = {1,0,0,0}{0,1,0,0}{0,0,1,0}{0,0,0,0}, steps 0,
//                        snapped 1, frozen cleared, both velocities stored
//   B  |d|^2 = 100.01 -> the same snap arm (KVF_MAX_VEHICLE_INTERP_DIST_SQ = 100.0f, strict >)
//   C  |d|^2 = 100    -> NOT a snap: vehicle untouched, snapped byte untouched, steps 10,
//                        SLerp(from = identity/zero-w, to = target * inverse(current), 0.1)
//   D  interpolate    -> SLerp's `to` == the relative transform M = RotZ(0.3) + (1, 0.5, -2)
//                        the harness built target from (target = M * current) to 7e-6 -- i.e.
//                        target * inverse(current), NOT inverse(current) * target.
// For C and D the final mSlerpTransform is checked against the closed form of what the vendor
// SLerp + OrthoNormalize3x3 do to that `to` (both z axes agree, so SLerp takes its per-row lerp
// arm): rotation RotZ(atan2(0.1 sin 0.3, 0.9 + 0.1 cos 0.3)) and translation 0.1 * to.w.
#define _ALLOW_KEYWORD_MACROS 1
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnVehicleDriver.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned giAsserts = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int) { ++giAsserts; std::printf("ASSERT: %s\n", lpcMessage); return 0; }
    void* EndAssert() { return nullptr; }
}
}

namespace BrnPhysics
{
namespace Vehicle
{
    struct FixtureSimpleAttribs { Vector3 mCOMOffset; };

    // Completes the header's `class VehiclePhysics;` with the members the body reaches.
    class VehiclePhysics
    {
    public:
        const FixtureSimpleAttribs* GetSimpleAttribs() const { return &mSimpleAttribs; }
        const Matrix44Affine& GetTransform() const   { return mTransform; }
        void SetTransform(Matrix44Affine lTransform) { mTransform = lTransform; }
        void SetLinearVelocity(Vector3 lV)           { mLinearVelocity = lV; }
        void SetAngularVelocity(Vector3 lV)          { mAngularVelocity = lV; }
        bool IsFrozen() const                        { return mbFrozen; }
        void SetFrozen(bool lb)                      { mbFrozen = lb; }

        Matrix44Affine       mTransform;
        Vector3              mLinearVelocity;
        Vector3              mAngularVelocity;
        bool                 mbFrozen;
        FixtureSimpleAttribs mSimpleAttribs;
    };

    namespace vpu = rw::math::vpu;

    // The production constants + StartCatchupInterpolation (BrnVehicleDriver.cpp), pasted by the
    // runner -- or, for the pre-fix revision where no body existed, a body that does nothing.
#include "catchup_body.inc"
}
}

using namespace BrnPhysics::Vehicle;
using rw::math::vpu::Matrix44Affine;
using rw::math::vpu::Vector3;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}

static bool Near(float a, float b, float lfTol) { return std::fabs(a - b) <= lfTol; }
static bool RowNear(const Vector3& r, float x, float y, float z, float w, float lfTol)
{
    return Near(r.x, x, lfTol) && Near(r.y, y, lfTol) && Near(r.z, z, lfTol) && Near(r.w, w, lfTol);
}
static bool RowExact(const Vector3& r, float x, float y, float z, float w)
{
    return r.x == x && r.y == y && r.z == z && r.w == w;
}
static bool IsIdentityZeroW(const Matrix44Affine& m)
{
    return RowExact(m.xAxis, 1, 0, 0, 0) && RowExact(m.yAxis, 0, 1, 0, 0)
        && RowExact(m.zAxis, 0, 0, 1, 0) && RowExact(m.wAxis, 0, 0, 0, 0);
}

static Matrix44Affine Rows(Vector3 x, Vector3 y, Vector3 z, Vector3 w)
{
    Matrix44Affine m; m.xAxis = x; m.yAxis = y; m.zAxis = z; m.wAxis = w; return m;
}

static const unsigned char KU8_FILL = 0xA5;
static VehicleDriver  gDriver;    // static storage: 16-byte aligned rows
static VehiclePhysics gVehicle;

static void Setup(const Matrix44Affine& lrCurrent, Vector3 lvCom, bool lbFrozen)
{
    std::memset(&gDriver, KU8_FILL, sizeof(gDriver));
    gVehicle.mTransform       = lrCurrent;
    gVehicle.mLinearVelocity  = Vector3{ 9, 9, 9, 9 };
    gVehicle.mAngularVelocity = Vector3{ 8, 8, 8, 8 };
    gVehicle.mbFrozen         = lbFrozen;
    gVehicle.mSimpleAttribs.mCOMOffset = lvCom;
}

static unsigned char SnappedByte() { unsigned char b; std::memcpy(&b, &gDriver.mbSnappedThisFrame, 1); return b; }

int main()
{
    const Matrix44Affine lIdentityAt0 = Rows(Vector3{ 1, 0, 0, 0 }, Vector3{ 0, 1, 0, 0 },
                                             Vector3{ 0, -0.0f, 1, 0 }, Vector3{ 0, 0, 0, 0 });
    const Vector3 lvCom{ 0.1f, 0.35f, -0.2f, 0.0f };

    // ---- A: snap flag ------------------------------------------------------------------------------
    {
        const Matrix44Affine lT = Rows(Vector3{ 0.9553365f, 0.29552022f, 0, 0 },
                                       Vector3{ -0.29552022f, 0.9553365f, 0, 0 },
                                       Vector3{ 0, 0, 1, 0 }, Vector3{ 10, 20, 30, 0 });
        Setup(lIdentityAt0, lvCom, true);
        gDriver.StartCatchupInterpolation(&gVehicle, lT, Vector3{ 1, 2, 3, 0 }, Vector3{ 0.1f, 0.2f, 0.3f, 0 }, true);
        // console: target.w = (9.992102, 20.36392, 29.8)
        Check(RowNear(gDriver.mCatchupTargetTransform.wAxis, 9.992102f, 20.36392f, 29.8f, 0.0f, 2e-6f),
              "A: target translation moved onto the COM (console 9.992102 20.36392 29.8)");
        Check(RowExact(gDriver.mCatchupTargetTransform.xAxis, 0.9553365f, 0.29552022f, 0, 0),
              "A: target rotation rows copied");
        Check(RowNear(gVehicle.mTransform.wAxis, 9.992102f, 20.36392f, 29.8f, 0.0f, 2e-6f)
              && RowExact(gVehicle.mTransform.yAxis, -0.29552022f, 0.9553365f, 0, 0),
              "A: snap writes the target into the vehicle transform");
        Check(IsIdentityZeroW(gDriver.mSlerpTransform), "A: snap seats mSlerpTransform = identity rows, zero w");
        Check(gDriver.mi8NumOfInterpSteps == 0, "A: snap sets mi8NumOfInterpSteps = 0");
        Check(SnappedByte() == 1, "A: snap sets mbSnappedThisFrame = 1 (stb @+0xD5)");
        Check(!gVehicle.mbFrozen, "A: a frozen body is thawed (lbz/stb 0 @+0x70)");
        Check(RowExact(gVehicle.mLinearVelocity, 1, 2, 3, 0) && RowExact(gVehicle.mAngularVelocity, 0.1f, 0.2f, 0.3f, 0),
              "A: both velocities stored (+0x50 / +0x60)");
    }

    // ---- B: snap by distance, |d|^2 = 100.01 ------------------------------------------------------
    {
        const Matrix44Affine lT = Rows(Vector3{ 1, 0, 0, 0 }, Vector3{ 0, 1, 0, 0 }, Vector3{ 0, -0.0f, 1, 0 },
                                       Vector3{ 10.0005f, 0, 0, 0 });
        Setup(lIdentityAt0, Vector3{ 0, 0, 0, 0 }, false);
        gDriver.StartCatchupInterpolation(&gVehicle, lT, Vector3{ 4, 5, 6, 0 }, Vector3{ 0.4f, 0.5f, 0.6f, 0 }, false);
        Check(gDriver.mi8NumOfInterpSteps == 0 && SnappedByte() == 1 && gVehicle.mTransform.wAxis.x == 10.0005f,
              "B: |d|^2 = 100.01 > KVF_MAX_VEHICLE_INTERP_DIST_SQ (100) snaps without the flag");
        Check(!gVehicle.mbFrozen, "B: an unfrozen body stays unfrozen");
    }

    // ---- C: |d|^2 == 100 exactly -> interpolate --------------------------------------------------
    {
        const Matrix44Affine lT = Rows(Vector3{ 1, 0, 0, 0 }, Vector3{ 0, 1, 0, 0 }, Vector3{ 0, -0.0f, 1, 0 },
                                       Vector3{ 6, 8, 0, 0 });
        Setup(lIdentityAt0, Vector3{ 0, 0, 0, 0 }, false);
        gDriver.StartCatchupInterpolation(&gVehicle, lT, Vector3{ 0, 0, 0, 0 }, Vector3{ 0, 0, 0, 0 }, false);
        Check(gDriver.mi8NumOfInterpSteps == 10, "C: |d|^2 == 100 is NOT a snap (strict >): steps = ki8NumNetworkSlerpSteps");
        Check(SnappedByte() == KU8_FILL, "C: the interpolate arm does not write mbSnappedThisFrame");
        Check(RowExact(gVehicle.mTransform.wAxis, 0, 0, 0, 0), "C: the interpolate arm leaves the vehicle transform alone");
        Check(RowNear(gDriver.mSlerpTransform.xAxis, 1, 0, 0, 0, 1e-6f) && RowNear(gDriver.mSlerpTransform.wAxis, 0.6f, 0.8f, 0, 0, 1e-6f),
              "C: mSlerpTransform = one tenth of the way from identity to (6,8,0)");
    }

    // ---- D: interpolate with a real rotation --------------------------------------------------------
    {
        const Matrix44Affine lCurrent = Rows(Vector3{ 1, 0, 0, 0 }, Vector3{ 0, 0.921061f, 0.38941833f, 0 },
                                             Vector3{ 0, -0.38941833f, 0.921061f, 0 }, Vector3{ 100, 5, -50, 0 });
        const Matrix44Affine lT = Rows(Vector3{ 0.9553365f, 0.27219212f, 0.11508099f, 0 },
                                       Vector3{ -0.29552022f, 0.87992316f, 0.37202555f, 0 },
                                       Vector3{ 0, -0.38941833f, 0.921061f, 0 },
                                       Vector3{ 101.0079f, 5.826291f, -51.60492f, 0 });
        Setup(lCurrent, lvCom, true);
        gDriver.StartCatchupInterpolation(&gVehicle, lT, Vector3{ 7, 8, 9, 0 }, Vector3{ 0.7f, 0.8f, 0.9f, 0 }, false);
        // console target.w = (101.0, 6.239367, -51.647415)
        Check(RowNear(gDriver.mCatchupTargetTransform.wAxis, 101.0f, 6.239367f, -51.647415f, 0.0f, 1e-4f),
              "D: target translation moved onto the COM (console 101.0 6.239367 -51.647415)");
        Check(gDriver.mi8NumOfInterpSteps == 10 && SnappedByte() == KU8_FILL, "D: interpolate arm (steps 10, no snap flag)");
        Check(RowExact(gVehicle.mTransform.wAxis, 100, 5, -50, 0) && RowExact(gVehicle.mTransform.yAxis, 0, 0.921061f, 0.38941833f, 0),
              "D: the vehicle transform is untouched");
        Check(!gVehicle.mbFrozen, "D: thawed");
        // SLerp(identity, M, 0.1) + OrthoNormalize3x3 for M = RotZ(0.3) + (1, 0.5, -2), as the CONSOLE runs it
        // (FX-GATE, crash parity 2026-09-25, b5 8c12ed5a). SLerp @0x82216858 picks its arm from the RELATIVE rotation's
        // angle -- its axis / angle query sub_82216510 on transpose(from) * to -- not from the angle between the two z
        // axes: here rel = M, 0.3 rad > 2 degrees, so it takes the arc, the rotation by 0.1 * 0.3 about z. (This check
        // used to expect the old vendor SLerp's per-row lerp arm, RotZ(atan2(0.1 sin 0.3, 0.9 + 0.1 cos 0.3)) =
        // RotZ(0.029676), because that body tested the z axes.) The console's words run on emu64 for this SLerp give
        // rows (0.99955004, 0.0299955, 0, w 0.99955004) / (-0.0299955, 0.99955004, 0, w -0.0299955) / (0, 0, 1, 0),
        // translation (0.1, 0.05, -0.2, 0) and angle out 0.27. OrthoNormalize3x3 then pivots on z for this input (the
        // relative transform the body computes is M to ~7e-6, so |x.y| is not exactly 0) and rebuilds x and y by cross
        // product: every w lane 0.
        const float lfC = 0.99955004f, lfS = 0.0299955f;   // cos / sin of 0.1 * 0.3
        Check(RowNear(gDriver.mSlerpTransform.xAxis, lfC, lfS, 0, 0, 2e-5f)
              && RowNear(gDriver.mSlerpTransform.yAxis, -lfS, lfC, 0, 0, 2e-5f)
              && RowNear(gDriver.mSlerpTransform.zAxis, 0, 0, 1, 0, 2e-5f),
              "D: mSlerpTransform rotation == RotZ(0.03) (the console's arc arm) -- target * inverse(current), not inverse(current) * target");
        if (giFailures != 0)
        {
            const Vector3* lapRows[3] = { &gDriver.mSlerpTransform.xAxis, &gDriver.mSlerpTransform.yAxis, &gDriver.mSlerpTransform.zAxis };
            for (int liRow = 0; liRow < 3; ++liRow)
                std::printf("      D: mSlerpTransform row %d = (%.8f, %.8f, %.8f, w %.8f)\n", liRow, lapRows[liRow]->x,
                            lapRows[liRow]->y, lapRows[liRow]->z, lapRows[liRow]->w);
        }
        Check(RowNear(gDriver.mSlerpTransform.wAxis, 0.1f, 0.05f, -0.2f, 0, 2e-5f),
              "D: mSlerpTransform translation == 0.1 * M.w = (0.1, 0.05, -0.2)");
    }

    Check(giAsserts == 0, "no console assert fires on these valid inputs (incl. the :328/:329 tripwires)");

    std::printf("FxVmnetCatchup: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
