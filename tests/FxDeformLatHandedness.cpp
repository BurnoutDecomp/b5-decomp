// Harness for run_fxdeformlat_handedness.py (crash parity G16-D1; prepared by FX-DEFORM-LAT, landed by FX-XLANE).
//
// BodyPartBBoxSpec::HackCheckHandedness @0x825E6EA0 and BBoxPointSkinData::HackSwapHandedness
// @0x825E6DB8, extracted from the real sources, run on real BodyPartBBoxSpec records.
//   check: T = dot3(cross(row0,row1),row2) ; mirror iff !(T >= 0) (left-handed or NaN)
//   mirror: each skin point P -> T0 + R0*(-L.x) + R1*L.y + R2*L.z with L_i = dot3(R_i, P - T0)
//           (4 lanes); then row0 = -row0 (all four lanes), AFTER the points (original basis).
#include "SharedClasses/Physics/Deformation/BrnBodyPartBBoxSpec.h"
#include <cmath>
#include <cstdio>
#include <cstring>

#include "methods.inc"

using namespace BrnPhysics::Deformation;
static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}
static unsigned Bits(float f) { unsigned u; std::memcpy(&u, &f, 4); return u; }
static void SetPoint(BBoxPointSkinData& lr, float x, float y, float z, float w)
{
    lr.mVertex = Vector3{ x, y, z, w };
    lr.mafWeights[0] = 0.25f; lr.mafWeights[1] = 0.5f; lr.mafWeights[2] = 0.25f;
    lr.mauBoneIndices[0] = 7; lr.mauBoneIndices[1] = 8; lr.mauBoneIndices[2] = 9;
}
static bool SkinUntouched(const BBoxPointSkinData& lr)
{
    return lr.mafWeights[0] == 0.25f && lr.mafWeights[1] == 0.5f && lr.mafWeights[2] == 0.25f
        && lr.mauBoneIndices[0] == 7 && lr.mauBoneIndices[1] == 8 && lr.mauBoneIndices[2] == 9;
}
// Double-precision reference of the console mirror for one point.
static void Reference(const Matrix44Affine& m, const Vector3& p, double out[4])
{
    const double R[3][4] = { { m.xAxis.x, m.xAxis.y, m.xAxis.z, m.xAxis.w },
                             { m.yAxis.x, m.yAxis.y, m.yAxis.z, m.yAxis.w },
                             { m.zAxis.x, m.zAxis.y, m.zAxis.z, m.zAxis.w } };
    const double T[4] = { m.wAxis.x, m.wAxis.y, m.wAxis.z, m.wAxis.w };
    const double d[3] = { p.x - T[0], p.y - T[1], p.z - T[2] };
    double L[3];
    for (int i = 0; i < 3; ++i) L[i] = R[i][0] * d[0] + R[i][1] * d[1] + R[i][2] * d[2];
    L[0] = -L[0];
    for (int c = 0; c < 4; ++c) out[c] = T[c] + R[0][c] * L[0] + R[1][c] * L[1] + R[2][c] * L[2];
}

int main()
{
    static BodyPartBBoxSpec s;

    // (a) left-handed axis flip: R0 = (-1,0,0,0), T0 = (1,2,3,1).
    std::memset(&s, 0, sizeof(s));
    s.mOrientation.xAxis = Vector3{ -1.0f, 0.0f, 0.0f, 0.0f };
    s.mOrientation.yAxis = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
    s.mOrientation.zAxis = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
    s.mOrientation.wAxis = Vector3{ 1.0f, 2.0f, 3.0f, 1.0f };
    for (int k = 0; k < 8; ++k) SetPoint(s.maCornerSkinData[k], 4.0f + k, 6.0f, 8.0f, 0.0f);
    SetPoint(s.mCentreSkinData, 4.0f, 6.0f, 8.0f, 0.0f);
    SetPoint(s.mJointSkinData, 5.0f, 6.0f, 8.0f, 0.0f);
    s.HackCheckHandedness();
    bool lbCorners = true;
    for (int k = 0; k < 8; ++k)
    {
        const Vector3& v = s.maCornerSkinData[k].mVertex;
        lbCorners = lbCorners && v.x == -2.0f - k && v.y == 6.0f && v.z == 8.0f && v.w == 1.0f && SkinUntouched(s.maCornerSkinData[k]);
    }
    Check(lbCorners, "(a) corners mirrored to (-2-k, 6, 8, T.w) with the ORIGINAL basis; skin untouched");
    Check(s.mCentreSkinData.mVertex.x == -2.0f && s.mJointSkinData.mVertex.x == -3.0f, "(a) centre and joint mirrored");
    Check(Bits(s.mOrientation.xAxis.x) == 0x3F800000u && Bits(s.mOrientation.xAxis.y) == 0x80000000u
          && Bits(s.mOrientation.xAxis.z) == 0x80000000u && Bits(s.mOrientation.xAxis.w) == 0x80000000u,
          "(a) row0 sign-flipped on all four lanes, after the points");
    Check(s.mOrientation.yAxis.y == 1.0f && s.mOrientation.wAxis.x == 1.0f, "(a) rows 1..3 untouched");

    // (b) an oblique left-handed frame (a reflected 30-degree yaw) with a non-zero origin.
    std::memset(&s, 0, sizeof(s));
    const float c = std::cos(0.5235988f), sn = std::sin(0.5235988f);
    s.mOrientation.xAxis = Vector3{ -c, -sn, 0.0f, 0.0f };   // reflected row 0
    s.mOrientation.yAxis = Vector3{ -sn, c, 0.0f, 0.0f };
    s.mOrientation.zAxis = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
    s.mOrientation.wAxis = Vector3{ 0.5f, -1.5f, 2.0f, 1.0f };
    const Matrix44Affine lFrame = s.mOrientation;
    SetPoint(s.maCornerSkinData[3], 1.25f, 0.75f, -0.5f, 0.0f);
    const Vector3 lP = s.maCornerSkinData[3].mVertex;
    s.HackCheckHandedness();
    double laRef[4]; Reference(lFrame, lP, laRef);
    const Vector3& lv = s.maCornerSkinData[3].mVertex;
    Check(std::fabs(lv.x - laRef[0]) < 1e-5 && std::fabs(lv.y - laRef[1]) < 1e-5 && std::fabs(lv.z - laRef[2]) < 1e-5
          && std::fabs(lv.w - laRef[3]) < 1e-5, "(b) oblique frame: L = R*(P-T), not R^T (1e-5 of the double reference)");

    // (c) identity basis: right-handed, nothing moves.
    std::memset(&s, 0, sizeof(s));
    s.mOrientation.SetIdentity();
    SetPoint(s.mCentreSkinData, 1.0f, 2.0f, 3.0f, 0.0f);
    s.HackCheckHandedness();
    Check(s.mCentreSkinData.mVertex.x == 1.0f && Bits(s.mOrientation.xAxis.y) == 0u, "(c) identity basis unchanged (control)");

    // (d) the all-zero placeholder part: T = +0 >= 0, nothing moves.
    std::memset(&s, 0, sizeof(s));
    SetPoint(s.mJointSkinData, 1.0f, 2.0f, 3.0f, 0.0f);
    s.HackCheckHandedness();
    Check(s.mJointSkinData.mVertex.x == 1.0f && Bits(s.mOrientation.xAxis.x) == 0u, "(d) all-zero placeholder unchanged (control)");

    // (e) NaN in row0: T is NaN, !(T >= 0) holds, so the mirror arm runs and row0 flips sign.
    std::memset(&s, 0, sizeof(s));
    s.mOrientation.SetIdentity();
    s.mOrientation.xAxis.x = std::nanf("");
    s.HackCheckHandedness();
    Check((Bits(s.mOrientation.xAxis.y) & 0x80000000u) != 0u && (Bits(s.mOrientation.xAxis.x) & 0x80000000u) != 0u,
          "(e) NaN basis takes the mirror arm (row0 sign bits flip)");

    std::printf("FxDeformLatHandedness: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
