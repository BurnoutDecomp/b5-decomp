// FX-GATE (crash parity 2026-09-25): BehaviourAftertouchCrash::Update's three rotation products -- the orbit
// TransformVector, the pitch Mult and the roll Mult -- as statements EXTRACTED from the production source (with the
// file-local rotation builders) by run_fxgate_aftertouch_products.py, against each whole block of the console's words
// run on emu64 (FxGateAftertouchProductsData.h).
#include "types.hpp"
#include "rw/math/vpu/types.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/vector4_operation.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "GameSource/Director/Camera/Utils/BrnConsoleVpu.h"
#include "SDKs/XboxMath/XMVectorSinCos.h"
#include "SDKs/XboxMath/XMScalarSinCos.h"

#include <cmath>
#include <cstdio>
#include <cstring>

typedef rw::math::vpu::Vector3 Vector3;
typedef rw::math::vpu::Matrix44Affine Matrix44Affine;

namespace
{
#include "fxgate_aftertouch_products_builders.inc"   // RotationX / RotationZ / XMMatrixRotationY + the constants
}

namespace BrnDirector
{
namespace Camera
{
#include "fxgate_aftertouch_products_kernels.inc"    // K_Orbit / K_Pitch / K_Roll
}
}

#include "FxGateAftertouchProductsData.h"

namespace
{
    unsigned Bits(float lf) { unsigned lu; std::memcpy(&lu, &lf, 4); return lu; }
    float Float(unsigned lu) { float lf; std::memcpy(&lf, &lu, 4); return lf; }
    bool IsNaNBits(unsigned lu) { return (lu & 0x7F800000u) == 0x7F800000u && (lu & 0x007FFFFFu) != 0u; }
    bool Same(unsigned luA, unsigned luB) { return (IsNaNBits(luA) && IsNaNBits(luB)) || luA == luB; }

    Vector3 Vec(const unsigned* lpu) { Vector3 lv; lv.x = Float(lpu[0]); lv.y = Float(lpu[1]); lv.z = Float(lpu[2]); lv.w = Float(lpu[3]); return lv; }
    Matrix44Affine Mat(const unsigned* lpu)
    {
        Matrix44Affine lm;
        lm.xAxis = Vec(lpu); lm.yAxis = Vec(lpu + 4); lm.zAxis = Vec(lpu + 8); lm.wAxis = Vec(lpu + 12);
        return lm;
    }
    bool MatchVec(const Vector3& lv, const unsigned* lpu)
    {
        return Same(Bits(lv.x), lpu[0]) && Same(Bits(lv.y), lpu[1]) && Same(Bits(lv.z), lpu[2]) && Same(Bits(lv.w), lpu[3]);
    }
    bool MatchMat(const Matrix44Affine& lm, const unsigned* lpu)
    {
        return MatchVec(lm.xAxis, lpu) && MatchVec(lm.yAxis, lpu + 4) && MatchVec(lm.zAxis, lpu + 8) && MatchVec(lm.wAxis, lpu + 12);
    }
}

int main()
{
    unsigned luChecks = 0, luFailures = 0, luPrinted = 0;
    if (Bits(KF_CAMERA_X_ROTATION_SPEED) != 0xBD4CCCCDu)
    {
        ++luFailures;
        std::printf("FAIL  KF_CAMERA_X_ROTATION_SPEED 0x%08X, the image has 0xBD4CCCCD @0x82CDA718\n",
                    Bits(KF_CAMERA_X_ROTATION_SPEED));
    }
    ++luChecks;
    for (const OrbitRow& lrRow : kaOrbitRows)
    {
        const Vector3 lvOut = BrnDirector::Camera::K_Orbit(Float(lrRow.mauIn[0]), Vec(lrRow.mauIn + 1));
        ++luChecks;
        if (!MatchVec(lvOut, lrRow.mauOut))
        {
            ++luFailures;
            if (luPrinted++ < 3)
                std::printf("FAIL  orbit stick %.9g: (%08X %08X %08X %08X), console (%08X %08X %08X %08X)\n",
                            Float(lrRow.mauIn[0]), Bits(lvOut.x), Bits(lvOut.y), Bits(lvOut.z), Bits(lvOut.w),
                            lrRow.mauOut[0], lrRow.mauOut[1], lrRow.mauOut[2], lrRow.mauOut[3]);
        }
    }
    luPrinted = 0;
    for (const PitchRow& lrRow : kaPitchRows)
    {
        const Matrix44Affine lmOut =
            BrnDirector::Camera::K_Pitch(Float(lrRow.mauIn[0]), Float(lrRow.mauIn[1]), Mat(lrRow.mauIn + 2));
        ++luChecks;
        if (!MatchMat(lmOut, lrRow.mauOut))
        {
            ++luFailures;
            if (luPrinted++ < 3)
                std::printf("FAIL  pitch %.9g deg, height %.9g: the frame differs from the console's\n",
                            Float(lrRow.mauIn[0]), Float(lrRow.mauIn[1]));
        }
    }
    luPrinted = 0;
    for (const RollRow& lrRow : kaRollRows)
    {
        const Matrix44Affine lmOut = BrnDirector::Camera::K_Roll(Float(lrRow.mauIn[0]), Mat(lrRow.mauIn + 1));
        ++luChecks;
        if (!MatchMat(lmOut, lrRow.mauOut))
        {
            ++luFailures;
            if (luPrinted++ < 3)
                std::printf("FAIL  roll %.9g rad: the transform differs from the console's\n", Float(lrRow.mauIn[0]));
        }
    }
    std::printf("FxGateAftertouchProducts: %u checks, %u failures\n", luChecks, luFailures);
    return luFailures == 0 ? 0 : 1;
}
