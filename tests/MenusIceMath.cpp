// OWNERLIST 2026-09-27, lane L5 MENUS: the ICE angle maths the pause / crash-nav camera's lens goes through, extracted from
// src/SDKs/Packages/ICE/ICEMath.cpp by run_menus_ice_math.py and compiled against the revision's own ICEMath.hpp.
//
// Checked against the ARTIST image and asm:
//   ICE::Angle::SetFromVecFloat @0x8252A948   x * flt_8207A908 (0x42652EE1 == 57.29578f), * flt_82004920 (0x3B360B61),
//                                             * flt_8207A934 (65536.0), + 65536 when 0 > x, fctidz, low 16 bits.
//                                             The PC spelled the first factor 180.0f / 3.1415927f == 0x42652EE0.
//   ICEMath::ATan @0x8252ABD8                 x = f1 (the divisor), y = f2: e = vrefp(x); t = -(e*x - 1); r = e*t + e;
//                                             q = y*r + 0; a = XMVectorATan(q) (0x821F0A70); x < 0: a = (pi | sign y) + a;
//                                             x == 0: a = pi/2 | sign y; SetFromVecFloat(a). The PC body was
//                                             std::atan2(first, second) -- the arguments the other way round.
//   ConvertLensLengthToFovAngle               UpdateLens @0x8252E548 expands it as ATan(lens, flt_8207B1F8 == 0x417F5C2A).
//   ICEMath::Angles::AngToDeg                 UpdateLens 0x8252E5DC..0x8252E630: 0 -> 0.0; else u16 * 2^-16 (0x37800000),
//                                             * 2pi (0x40C90FDB), * 0x42652EE1 -- three rounded multiplies.
// The expected values come from scratch/OWNERLIST_0927/L5/ice_model.py, an independent Python model of the same asm
// (every float32 operation rounded once, exact fused ops via fractions).
#include "types.hpp"
#include "SDKs/Packages/ICE/ICEMath.hpp"
#include "SDKs/XboxMath/XMVectorATan.h"
#include <cmath>
#include <cstdio>
#include <cstring>

#ifndef CGS_ASSERT
#define CGS_ASSERT(lbCondition, lpcMessage) ((void)0)
#endif

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static u32 Bits(f32 lfValue)
{
    u32 luBits;
    std::memcpy(&luBits, &lfValue, sizeof(luBits));
    return luBits;
}

static f32 FromBits(u32 luBits)
{
    f32 lfValue;
    std::memcpy(&lfValue, &luBits, sizeof(lfValue));
    return lfValue;
}

#include "menus_ice_math.inc"

static u16 AngleOf(f32 lfRadians)
{
    ICE::Angle leAngle;
    ICE::VecFloat lvfRadians;
    lvfRadians.x = lfRadians;
    lvfRadians.y = lvfRadians.z = lvfRadians.w = lfRadians;
    leAngle.SetFromVecFloat(lvfRadians);
    return static_cast<u16>(leAngle);
}

static void CheckAtan(f32 lfX, f32 lfY, u16 luExpected, const char* lpcName)
{
    const u16 luGot = static_cast<u16>(ICE::ICEMath::ATan(lfX, lfY));
    if (luGot != luExpected)
    {
        std::fprintf(stderr, "  %s: got %u, want %u\n", lpcName, static_cast<unsigned>(luGot),
                     static_cast<unsigned>(luExpected));
    }
    Check(luGot == luExpected, lpcName);
}

int main()
{
    const f32 KF_LENS_BASE = FromBits(0x417F5C2Au);   // flt_8207B1F8

    // ---- ATan: the lens half-angle, atan(base / lens) (x = the lens, the DIVISOR) ----
    CheckAtan(5.0f,   KF_LENS_BASE, 13217, "ATan(5, base) == 13217");
    CheckAtan(12.0f,  KF_LENS_BASE,  9659, "ATan(12, base) == 9659");
    CheckAtan(15.96f, KF_LENS_BASE,  8192, "ATan(15.96, base) == 8192");
    CheckAtan(24.0f,  KF_LENS_BASE,  6121, "ATan(24, base) == 6121");
    CheckAtan(35.0f,  KF_LENS_BASE,  4462, "ATan(35, base) == 4462");
    CheckAtan(50.0f,  KF_LENS_BASE,  3222, "ATan(50, base) == 3222");
    CheckAtan(85.0f,  KF_LENS_BASE,  1935, "ATan(85, base) == 1935");
    CheckAtan(200.0f, KF_LENS_BASE,   830, "ATan(200, base) == 830");
    CheckAtan(500.0f, KF_LENS_BASE,   332, "ATan(500, base) == 332");

    // ---- ATan: the quadrant fix (x < 0: + (pi | sign y); x == 0: pi/2 | sign y) ----
    CheckAtan(-50.0f, KF_LENS_BASE,  29545, "ATan(-50, base) == 29545 (x < 0: pi + a)");
    CheckAtan(0.0f,   KF_LENS_BASE,  16384, "ATan(0, base) == 16384 (x == 0: pi/2)");
    CheckAtan(0.0f,  -KF_LENS_BASE,  49152, "ATan(0, -base) == 49152 (x == 0: -pi/2, wrapped)");
    CheckAtan(50.0f, -KF_LENS_BASE,  62313, "ATan(50, -base) == 62313 (negative, wrapped)");
    CheckAtan(-50.0f, -KF_LENS_BASE, 35990, "ATan(-50, -base) == 35990 (x < 0, y < 0: -pi + a)");
    CheckAtan(1.0f,   1.0f,           8192, "ATan(1, 1) == 8192");
    CheckAtan(1.0f,  -1.0f,          57344, "ATan(1, -1) == 57344");
    CheckAtan(-1.0f,  1.0f,          24576, "ATan(-1, 1) == 24576");
    CheckAtan(3.0f,   4.0f,           9672, "ATan(3, 4) == 9672 (atan(4/3))");

    // ---- SetFromVecFloat: the degrees factor is the image's 0x42652EE1 ----
    Check(AngleOf(FromBits(0x3F00A22Cu)) == 5241,  "SetFromVecFloat(0x3F00A22C) == 5241 (0x42652EE0 gives 5240)");
    Check(AngleOf(FromBits(0x3F80A22Cu)) == 10482, "SetFromVecFloat(0x3F80A22C) == 10482 (0x42652EE0 gives 10481)");
    Check(AngleOf(FromBits(0x3FCA080Au)) == 16463, "SetFromVecFloat(0x3FCA080A) == 16463 (0x42652EE0 gives 16462)");
    Check(AngleOf(FromBits(0x40000061u)) == 20861, "SetFromVecFloat(0x40000061) == 20861 (0x42652EE0 gives 20860)");
    Check(AngleOf(FromBits(0x40409CDEu)) == 31391, "SetFromVecFloat(0x40409CDE) == 31391 (0x42652EE0 gives 31390)");
    Check(AngleOf(FromBits(0xBF00A22Cu)) == 60295, "SetFromVecFloat(-0x3F00A22C) == 60295 (negative: + 65536)");

    // ---- ConvertLensLengthToFovAngle: ATan(lens, 0x417F5C2A) ----
    Check(static_cast<u16>(ICE::ICEMath::ConvertLensLengthToFovAngle(5.0f)) == 13217,
          "ConvertLensLengthToFovAngle(5) == 13217");
    Check(static_cast<u16>(ICE::ICEMath::ConvertLensLengthToFovAngle(50.0f)) == 3222,
          "ConvertLensLengthToFovAngle(50) == 3222");
    Check(static_cast<u16>(ICE::ICEMath::ConvertLensLengthToFovAngle(500.0f)) == 332,
          "ConvertLensLengthToFovAngle(500) == 332");

    // ---- AngToDeg: three rounded multiplies ----
    Check(Bits(ICE::ICEMath::Angles::AngToDeg(ICE::Angle(static_cast<u16>(0)))) == 0x00000000u, "AngToDeg(0) == 0.0");
    Check(Bits(ICE::ICEMath::Angles::AngToDeg(ICE::Angle(static_cast<u16>(332)))) == 0x3FE97001u,
          "AngToDeg(332) == 0x3FE97001");
    Check(Bits(ICE::ICEMath::Angles::AngToDeg(ICE::Angle(static_cast<u16>(3222)))) == 0x418D9780u,
          "AngToDeg(3222) == 0x418D9780 (17.698975)");
    Check(Bits(ICE::ICEMath::Angles::AngToDeg(ICE::Angle(static_cast<u16>(8192)))) == 0x42340000u,
          "AngToDeg(8192) == 45.0");
    Check(Bits(ICE::ICEMath::Angles::AngToDeg(ICE::Angle(static_cast<u16>(13217)))) == 0x429134D0u,
          "AngToDeg(13217) == 0x429134D0 (72.60315)");
    Check(Bits(ICE::ICEMath::Angles::AngToDeg(ICE::Angle(static_cast<u16>(16384)))) == 0x42B40000u,
          "AngToDeg(16384) == 90.0");

    std::printf("MenusIceMath: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
