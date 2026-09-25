// FX-GATE: rw::math::fpu::IsZero (vendor/renderware/include/rw/math/fpu/scalar_operation.h, compiled from
// the production header by run_fxgate_fpu_iszero.py) against the console's inlined predicate.
//
// rwmath 1.02.00 scalar.h:390 spells IsZero as `(value <= tolerance) && (value >= -tolerance)`. The X360
// compiler turns each `<=` / `>=` into ONE condition bit of an fcmpu, e.g. TrafficLaneTruck::Update:
//   0x82247C14  lfs   f0, flt_82001770            ; +2^-23 (0x34000000)
//   0x82247C18  fcmpu cr6, f30, f0
//   0x82247C1C  bgt   0x82247C34                  ; -> r11 = r25 (= 0, li r25,0 @0x82247C10): not zero
//   0x82247C24  lfs   f0, flt_82002514            ; -2^-23 (0xB4000000)
//   0x82247C28  li    r11, 1
//   0x82247C2C  fcmpu cr6, f30, f0
//   0x82247C30  bge   0x82247C38                  ; keep r11 = 1: zero
//   0x82247C34  mr    r11, r25                    ; 0
// bgt is bc 12,gt (taken only on an ORDERED greater); bge is bc 4,lt (taken unless the compare is an
// ORDERED less), so an unordered (NaN) value falls through the bgt and takes the bge: IsZero(NaN) is TRUE.
// The same shape at CrashPlayManager::UpdateMomentum 0x823022F8/0x82302300/0x8230230C and
// 0x82302348/0x8230234C/0x82302358, BrnLooker 0x82222D08/0x82222D0C/0x82222D20, VehiclePhysics
// 0x825FDC30/0x825FDC38/0x825FDC44 (flt_8208F620 = 0x34000000), BehaviourGameplayExternal
// 0x82241BE8/0x82241BEC/0x82241C00.
//
// The oracle below models the condition register, not a C++ spelling: fcmpu sets exactly one of
// LT/GT/EQ/UN, bgt branches on GT, bge branches on !LT. Inputs come from a volatile table so /O2 cannot fold
// the calls.
#include "rw/math/fpu/scalar_operation.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
    int giChecks   = 0;
    int giFailures = 0;

    float FloatFromBits(uint32_t luBits)
    {
        float lfValue;
        std::memcpy(&lfValue, &luBits, sizeof(lfValue));
        return lfValue;
    }

    uint32_t Bits(float lfValue)
    {
        uint32_t luBits;
        std::memcpy(&luBits, &lfValue, sizeof(luBits));
        return luBits;
    }

    // The four fcmpu result bits of (a, b).
    struct Cr { bool mbLt, mbGt, mbEq, mbUn; };

    Cr Fcmpu(float lfA, float lfB)
    {
        Cr lCr = { false, false, false, false };
        if (std::isnan(lfA) || std::isnan(lfB)) lCr.mbUn = true;
        else if (lfA < lfB)                     lCr.mbLt = true;
        else if (lfA > lfB)                     lCr.mbGt = true;
        else                                    lCr.mbEq = true;
        return lCr;
    }

    // 0x82247C14..0x82247C38 with the tolerances read from the image.
    bool ConsoleIsZero(float lfValue)
    {
        const float lfPlus  = FloatFromBits(0x34000000u);   // flt_82001770
        const float lfMinus = FloatFromBits(0xB4000000u);   // flt_82002514
        if (Fcmpu(lfValue, lfPlus).mbGt)                     // bgt 0x82247C34 -> 0
            return false;
        return !Fcmpu(lfValue, lfMinus).mbLt;               // bge 0x82247C38 keeps 1, else 0
    }
}

int main()
{
    // The header's tolerance is the image's constant (flt_82001770 / flt_82014460 / flt_8208F620 all
    // read 0x34000000, flt_82002514 reads 0xB4000000).
    ++giChecks;
    if (Bits(rw::math::fpu::KF_IS_ZERO_TOLERANCE) != 0x34000000u)
    {
        ++giFailures;
        std::printf("FAIL: KF_IS_ZERO_TOLERANCE bits 0x%08X, image flt_82001770 = 0x34000000\n",
                    Bits(rw::math::fpu::KF_IS_ZERO_TOLERANCE));
    }

    struct Case { uint32_t muBits; const char* mpcName; };
    static volatile const Case kaCases[] =
    {
        { 0x7FC00000u, "quiet NaN" },
        { 0xFFC00000u, "negative quiet NaN" },
        { 0x7FC12345u, "NaN with a payload" },
        { 0x7F800001u, "signalling NaN" },
        { 0x00000000u, "+0" },
        { 0x80000000u, "-0" },
        { 0x00000001u, "smallest +denormal" },
        { 0x80000001u, "smallest -denormal" },
        { 0x34000000u, "+tolerance (edge, zero)" },
        { 0xB4000000u, "-tolerance (edge, zero)" },
        { 0x34000001u, "next above +tolerance" },
        { 0xB4000001u, "next below -tolerance" },
        { 0x33FFFFFFu, "next below +tolerance" },
        { 0xB3FFFFFFu, "next above -tolerance" },
        { 0x3F800000u, "+1" },
        { 0xBF800000u, "-1" },
        { 0x7F800000u, "+inf" },
        { 0xFF800000u, "-inf" },
        { 0x7F7FFFFFu, "FLT_MAX" },
        { 0xFF7FFFFFu, "-FLT_MAX" },
    };

    for (const volatile Case& lrCase : kaCases)
    {
        const float lfValue    = FloatFromBits(lrCase.muBits);
        const bool  lbExpected = ConsoleIsZero(lfValue);
        const bool  lbActual   = rw::math::fpu::IsZero(lfValue);
        ++giChecks;
        if (lbActual != lbExpected)
        {
            ++giFailures;
            std::printf("FAIL: IsZero(%s, 0x%08X) = %d, console 0x82247C18..0x82247C34 answers %d\n",
                        lrCase.mpcName, lrCase.muBits, lbActual ? 1 : 0, lbExpected ? 1 : 0);
        }
    }

    // The oracle itself must say what the addresses say: a NaN IS zero, the edges are inclusive, the
    // first float beyond either edge is not zero.
    ++giChecks;
    if (!ConsoleIsZero(std::numeric_limits<float>::quiet_NaN()) || !ConsoleIsZero(FloatFromBits(0x34000000u))
        || !ConsoleIsZero(FloatFromBits(0xB4000000u)) || ConsoleIsZero(FloatFromBits(0x34000001u))
        || ConsoleIsZero(FloatFromBits(0xB4000001u)))
    {
        ++giFailures;
        std::printf("FAIL: the oracle does not model 0x82247C1C bgt / 0x82247C30 bge\n");
    }

    std::printf("%d/%d checks passed\n", giChecks - giFailures, giChecks);
    return giFailures == 0 ? 0 : 1;
}
