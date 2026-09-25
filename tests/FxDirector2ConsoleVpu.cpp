// FX-DIRECTOR2 (crash parity 2026-09-25): Utils/BrnConsoleVpu.h against BehaviourRig::Update's own words.
//
// THE REFERENCE is the console: FxDirector2ConsoleVpuGolden.h holds what BehaviourRig::Update @0x822427C0's
// SDK-inline windows compute, run on a PPC/VMX128 emulator whose fused ops round exactly once (the generator,
// gen_rigvpu_golden.py, is FX-DIRECTOR2 scratch; its table is committed beside this file), over 32 random frames with
// world-sized positions:
//   0x82242D20..0x82242E38  InverseOfMatrixWithOrthonormal3x3(work), then the looked-at car x that inverse (Mult)
//   0x82242E3C..0x82242E60  the looked-at car's velocity through the inverse (TransformVector)
//   0x82242F90..0x8224300C  the camera = rig x work (Mult)
//   0x82242C0C..0x82242C38  the spring push, pos = At * length + pos (one vmaddfp128)
// Every x / y / z lane is compared BIT FOR BIT (the w lanes carry nothing the rig reads). The last four checks prove
// the table can tell the console's fused cascades from the vendor SDK's separately rounded forms: each vendor form
// must miss it somewhere.
#include <cstdio>
#include <cstring>
#include "types.hpp"
#include "GameSource/Director/Camera/Utils/BrnConsoleVpu.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "rw/math/vpu/vector3_operation.h"
#include "FxDirector2ConsoleVpuGolden.h"

using rw::math::vpu::Matrix44Affine;
using rw::math::vpu::Vector3;
namespace ConsoleVpu = BrnDirector::Camera::Utils::ConsoleVpu;

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcName);
    }
}

static unsigned int Bits(f32 lfValue)
{
    unsigned int luBits;
    std::memcpy(&luBits, &lfValue, sizeof(luBits));
    return luBits;
}

static f32 Float(unsigned int luBits)
{
    f32 lfValue;
    std::memcpy(&lfValue, &luBits, sizeof(lfValue));
    return lfValue;
}

static Vector3 Row(const unsigned int* lpuWords)
{
    return Vector3{ Float(lpuWords[0]), Float(lpuWords[1]), Float(lpuWords[2]), Float(lpuWords[3]) };
}

static Matrix44Affine Matrix(const unsigned int lauWords[16])
{
    Matrix44Affine lMatrix;
    lMatrix.xAxis = Row(lauWords + 0);
    lMatrix.yAxis = Row(lauWords + 4);
    lMatrix.zAxis = Row(lauWords + 8);
    lMatrix.wAxis = Row(lauWords + 12);
    return lMatrix;
}

static bool SameXYZ(const Vector3& lrValue, const unsigned int* lpuGolden)
{
    return Bits(lrValue.x) == lpuGolden[0] && Bits(lrValue.y) == lpuGolden[1] && Bits(lrValue.z) == lpuGolden[2];
}

static bool SameXYZ(const Matrix44Affine& lrValue, const unsigned int lauGolden[16])
{
    return SameXYZ(lrValue.xAxis, lauGolden + 0) && SameXYZ(lrValue.yAxis, lauGolden + 4)
        && SameXYZ(lrValue.zAxis, lauGolden + 8) && SameXYZ(lrValue.wAxis, lauGolden + 12);
}

int main()
{
    const int liCases = static_cast<int>(sizeof(kaConsoleVpuCases) / sizeof(kaConsoleVpuCases[0]));
    int liVendorLocalMisses = 0, liVendorVelocityMisses = 0, liVendorCameraMisses = 0, liVendorPushMisses = 0;
    for (int i = 0; i < liCases; ++i)
    {
        const ConsoleVpuCase& lrCase = kaConsoleVpuCases[i];
        const Matrix44Affine lWork     = Matrix(lrCase.mauWork);
        const Matrix44Affine lTarget   = Matrix(lrCase.mauTarget);
        const Matrix44Affine lRig      = Matrix(lrCase.mauRig);
        const Vector3        lVelocity = Row(lrCase.mauVelocity);
        const f32            lfLength  = Float(lrCase.muLength);

        const Matrix44Affine lInverse       = ConsoleVpu::InverseOfMatrixWithOrthonormal3x3(lWork);
        const Matrix44Affine lLocal         = ConsoleVpu::Mult(lTarget, lInverse);
        const Vector3        lLocalVelocity = ConsoleVpu::TransformVector(lInverse, lVelocity);
        const Matrix44Affine lCamera        = ConsoleVpu::Mult(lRig, lWork);
        const Vector3        lPushed        = ConsoleVpu::MultiplyAdd(lWork.zAxis, lfLength, lWork.wAxis);

        char lacName[160];
        std::snprintf(lacName, sizeof(lacName), "V%02d.1 the looked-at car in the working frame (0x82242D20..0x82242E34)", i);
        Check(SameXYZ(lLocal, lrCase.mauLocal), lacName);
        std::snprintf(lacName, sizeof(lacName), "V%02d.2 its velocity through the inverse (0x82242E48..0x82242E60)", i);
        Check(SameXYZ(lLocalVelocity, lrCase.mauLocalVelocity), lacName);
        std::snprintf(lacName, sizeof(lacName), "V%02d.3 the camera = rig x work (0x82242F90..0x8224300C)", i);
        Check(SameXYZ(lCamera, lrCase.mauCamera), lacName);
        std::snprintf(lacName, sizeof(lacName), "V%02d.4 the spring push At * length + pos (0x82242C34)", i);
        Check(SameXYZ(lPushed, lrCase.mauPushed), lacName);

        // The vendor SDK's forms, for the discrimination checks.
        const Matrix44Affine lVendorInverse = rw::math::vpu::InverseOfMatrixWithOrthonormal3x3(lWork);
        if (!SameXYZ(rw::math::vpu::Mult(lTarget, lVendorInverse), lrCase.mauLocal))
            ++liVendorLocalMisses;
        if (!SameXYZ(rw::math::vpu::TransformVector(lVendorInverse, lVelocity), lrCase.mauLocalVelocity))
            ++liVendorVelocityMisses;
        if (!SameXYZ(rw::math::vpu::Mult(lRig, lWork), lrCase.mauCamera))
            ++liVendorCameraMisses;
        if (!SameXYZ(lWork.zAxis * lfLength + lWork.wAxis, lrCase.mauPushed))
            ++liVendorPushMisses;
    }
    std::printf("the vendor forms miss the console on %d / %d / %d / %d of %d frames (local, velocity, camera, push)\n",
                liVendorLocalMisses, liVendorVelocityMisses, liVendorCameraMisses, liVendorPushMisses, liCases);
    Check(liVendorLocalMisses > 0, "D1 the table discriminates: the vendor inverse + Mult miss the looked-at car somewhere");
    Check(liVendorVelocityMisses > 0, "D2 the table discriminates: the vendor TransformVector misses the velocity somewhere");
    Check(liVendorCameraMisses > 0, "D3 the table discriminates: the vendor Mult misses the camera somewhere");
    Check(liVendorPushMisses > 0, "D4 the table discriminates: the unfused push misses somewhere");

    std::printf("FxDirector2ConsoleVpu: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
