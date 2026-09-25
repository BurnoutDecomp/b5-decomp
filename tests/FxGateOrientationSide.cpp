// FX-GATE: BrnSound::Logic::Collision MapPositionToOrientationUsingBox (X360 0x8269ED18), the body and
// KV_DIRECTION_BIAS EXTRACTED from src/GameSource/Sound/Collision/BrnCollisionStateManager.cpp by
// run_fxgate_orientation_side.py.
//
// The side distance is `vminfp v10, v7(left), v8(right)` at 0x8269EF10. vminfp returns a NaN when
// EITHER operand is NaN (vA's first) and orders -0 below +0; std::min(left, right) hands back `left`
// when only `right` is NaN. After that the Roof / Bottom tests are `vcmpgtfp.` all-lanes (0x8269EF18 +
// beq 0x8269EF28, and the Bottom one after it), which a NaN nearest distance FAILS -- so on the console
// a NaN right distance leaves the contact on Side, where the PC went on to Roof or Bottom.
#include "types.hpp"
#include "rw/math/vpu/types.h"
#include "GameSource/AttribSys/Enums/eOrientation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace fxgate
{
    using rw::math::vpu::Vector3;
    using rw::math::vpu::Matrix44Affine;
#include "extracted.inc"
}

namespace
{
    int giChecks   = 0;
    int giFailures = 0;

    using AttribSys::Enums::eOrientation::eOrientation;
    using rw::math::vpu::Vector3;
    using rw::math::vpu::Matrix44Affine;

    const char* Name(int liOrientation)
    {
        switch (liOrientation)
        {
            case 1: return "Front";
            case 2: return "Side";
            case 4: return "Rear";
            case 8: return "Roof";
            case 16: return "Bottom";
            default: return "?";
        }
    }

    Matrix44Affine Identity()
    {
        Matrix44Affine lMatrix;
        lMatrix.SetIdentity();
        return lMatrix;
    }

    void Expect(Vector3 lPosition, Vector3 lMin, Vector3 lMax, int liExpected, const char* lpcWhat)
    {
        const Vector3 lZero = { 0.0f, 0.0f, 0.0f, 0.0f };
        const int liActual = static_cast<int>(fxgate::MapPositionToOrientationUsingBox(
            lPosition, lZero, Identity(), lZero, lZero, lMin, lMax));
        ++giChecks;
        if (liActual != liExpected)
        {
            ++giFailures;
            std::printf("FAIL: %s: %s, console %s\n", lpcWhat, Name(liActual), Name(liExpected));
        }
    }
}

int main()
{
    const float lfNaN = std::numeric_limits<float>::quiet_NaN();
    const Vector3 lMin = { -1.0f, -0.5f, -2.0f, 0.0f };
    const Vector3 lMax = {  1.0f,  0.5f,  2.0f, 0.0f };

    // Ordered: each face wins where it is nearest.
    Expect({ 0.0f, 0.0f,  1.9f, 0.0f }, lMin, lMax, 1,  "near the front face");
    Expect({ 0.0f, 0.0f, -1.9f, 0.0f }, lMin, lMax, 4,  "near the rear face");
    Expect({ 0.9f, 0.0f,  0.0f, 0.0f }, lMin, lMax, 2,  "near the right side");
    Expect({-0.9f, 0.0f,  0.0f, 0.0f }, lMin, lMax, 2,  "near the left side");
    Expect({ 0.0f, 0.45f, 0.0f, 0.0f }, lMin, lMax, 8,  "near the roof");
    Expect({ 0.0f,-0.45f, 0.0f, 0.0f }, lMin, lMax, 16, "near the bottom");

    // A NaN right face (lMax.x): left |x - (-1)| = 0.2 wins over front/rear, the side branch runs, and
    // vminfp 0x8269EF10 makes the nearest distance NaN, so the roof (0.1) and bottom (0.9) tests fail:
    // Side. std::min kept left = 0.2 and the roof test then took Roof.
    const Vector3 lMaxNaNRight = { lfNaN, 0.5f, 2.0f, 0.0f };
    Expect({-0.8f, 0.4f, 0.0f, 0.0f }, lMin, lMaxNaNRight, 2, "NaN right distance, roof nearer than left (vminfp -> NaN -> Side)");
    Expect({-0.8f,-0.4f, 0.0f, 0.0f }, lMin, lMaxNaNRight, 2, "NaN right distance, bottom nearer than left (vminfp -> NaN -> Side)");

    // A NaN left face: std::min(NaN, right) already returned the NaN.
    const Vector3 lMinNaNLeft = { lfNaN, -0.5f, -2.0f, 0.0f };
    Expect({ 0.8f, 0.4f, 0.0f, 0.0f }, lMinNaNLeft, lMax, 2, "NaN left distance, roof nearer than right");

    std::printf("%d/%d checks passed\n", giChecks - giFailures, giChecks);
    return giFailures == 0 ? 0 : 1;
}
