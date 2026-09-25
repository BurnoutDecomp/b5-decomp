// crash parity FX-NETCRASH (2026-09-25): the PRODUCTION body of VehicleDriver::StartCatchupInterpolation
// @0x825FED30 (BrnVehicleDriver.cpp, with its file-scope constants), extracted by
// run_fxnetcrash_catchup_com_fma.py and compiled as a member of the REAL VehicleDriver. The header only
// forward-declares VehiclePhysics, so the fixture below completes it with exactly what the body touches
// (as tests/FxVmnetCatchup.cpp does).
//
// Under test: the centre-of-mass move 0x825FF0C0..0x825FF0EC, all four lanes,
//     wAxis + fma(zAxis, com.z, fma(xAxis, com.x, RN(yAxis * com.y)))
// on the SNAP arm (lbSnap = true), where the target is also written into the vehicle transform. The
// expected bit patterns come from com_cases.inc: exact rational arithmetic, one rounding per console
// instruction, on inputs where the twice-rounded form differs in every lane. Every comparison is BITWISE.
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

#include "catchup_body.inc"
}
}

#include "com_cases.inc"

using namespace BrnPhysics::Vehicle;
using rw::math::vpu::Matrix44Affine;
using rw::math::vpu::Vector3;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName, int liCase)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s (case %d)\n", lpcName, liCase); }
}

static unsigned Bits(float lf) { unsigned lu; std::memcpy(&lu, &lf, 4); return lu; }
static float FromBits(unsigned lu) { float lf; std::memcpy(&lf, &lu, 4); return lf; }
static Vector3 Row(const unsigned* lpu) { return Vector3{ FromBits(lpu[0]), FromBits(lpu[1]), FromBits(lpu[2]), FromBits(lpu[3]) }; }
static bool SameBits(const Vector3& lr, const unsigned* lpu)
{
    const bool lbSame = Bits(lr.x) == lpu[0] && Bits(lr.y) == lpu[1] && Bits(lr.z) == lpu[2] && Bits(lr.w) == lpu[3];
    if (!lbSame)
    {
        std::printf("  got %08X %08X %08X %08X want %08X %08X %08X %08X\n", Bits(lr.x), Bits(lr.y), Bits(lr.z), Bits(lr.w),
                    lpu[0], lpu[1], lpu[2], lpu[3]);
    }
    return lbSame;
}

static VehicleDriver  gDriver;    // static storage: 16-byte aligned rows
static VehiclePhysics gVehicle;

int main()
{
    int liCase = 0;
    for (const ComCase& lrCase : KA_COM_CASES)
    {
        std::memset(&gDriver, 0xA5, sizeof(gDriver));
        gVehicle.mTransform.SetIdentity();
        gVehicle.mLinearVelocity  = Vector3{ 9, 9, 9, 9 };
        gVehicle.mAngularVelocity = Vector3{ 8, 8, 8, 8 };
        gVehicle.mbFrozen         = false;
        gVehicle.mSimpleAttribs.mCOMOffset = Vector3{ FromBits(lrCase.com[0]), FromBits(lrCase.com[1]),
                                                      FromBits(lrCase.com[2]), 0.0f };

        Matrix44Affine lTarget;
        lTarget.xAxis = Row(lrCase.rows[0]);
        lTarget.yAxis = Row(lrCase.rows[1]);
        lTarget.zAxis = Row(lrCase.rows[2]);
        lTarget.wAxis = Row(lrCase.w);

        gDriver.StartCatchupInterpolation(&gVehicle, lTarget, Vector3{ 1, 2, 3, 0 }, Vector3{ 0.1f, 0.2f, 0.3f, 0 }, true);

        Check(SameBits(gDriver.mCatchupTargetTransform.wAxis, lrCase.want),
              "mCatchupTargetTransform.wAxis == w + fma(z, com.z, fma(x, com.x, RN(y * com.y))) in all four lanes -- bitwise",
              liCase);
        Check(SameBits(gVehicle.mTransform.wAxis, lrCase.want),
              "the snap writes that same translation into the vehicle transform -- bitwise", liCase);
        ++liCase;
    }

    std::printf("FxNetcrashCatchupComFma: %d checks, %d failures (%u asserts)\n", giChecks, giFailures, giAsserts);
    return giFailures ? 1 : 0;
}
