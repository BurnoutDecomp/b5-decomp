// FX-FPUMAX call-site group B (run_fxfpumax_vecfloat_sites.py): BrnBehaviourGameplayExternal.cpp's VecFloat
// Min / Max / Clamp sites. The console computes them on the VMX unit (vmaxfp / vminfp -- rwmath 1.02.00
// vpu/detail/scalar_operation_inline.h:89-120, Clamp = Min(max, Max(min, value)) :189-192), NOT with the fsel
// rw::math::fpu forms. AltiVec PEM: vmaxfp / vminfp give a NaN when either operand is NaN (vA's when both are)
// and order the zeros -0 < +0. The oracle below derives both from sign-magnitude integer KEYS of the bit
// patterns (no float compare), so it shares no spelling with the helpers under test. Bit-exact comparisons.
#include "rw/math/fpu/scalar_operation.h"
#include "fxfpumax_vecfloat_sites.inl"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace
{
    int giChecks   = 0;
    int giFailures = 0;

    void Check(bool lbOk, const char* lpcWhat)
    {
        ++giChecks;
        if (!lbOk)
        {
            ++giFailures;
            std::printf("FAIL: %s\n", lpcWhat);
        }
    }

    uint32_t Bits(float lfValue)
    {
        uint32_t luBits;
        std::memcpy(&luBits, &lfValue, sizeof(luBits));
        return luBits;
    }
    float FromBits(uint32_t luBits)
    {
        float lfValue;
        std::memcpy(&lfValue, &luBits, sizeof(lfValue));
        return lfValue;
    }
    bool Same(float a, float b) { return Bits(a) == Bits(b); }
    bool IsNaN(float a) { const uint32_t u = Bits(a); return (u & 0x7F800000u) == 0x7F800000u && (u & 0x007FFFFFu) != 0u; }

    // Total order of the non-NaN floats, -0 below +0.
    uint32_t Key(float a) { const uint32_t u = Bits(a); return (u & 0x80000000u) ? ~u : (u | 0x80000000u); }
    float Vmaxfp(float a, float b)
    {
        if (IsNaN(a)) return a;
        if (IsNaN(b)) return b;
        return Key(a) >= Key(b) ? a : b;
    }
    float Vminfp(float a, float b)
    {
        if (IsNaN(a)) return a;
        if (IsNaN(b)) return b;
        return Key(a) <= Key(b) ? a : b;
    }
    float VClamp(float v, float lo, float hi) { return Vminfp(hi, Vmaxfp(lo, v)); }

    volatile float gafValues[] =
    {
        0.0f, -0.0f, 1.0f, -1.0f, 0.5f, -0.5f, 3.0f, 1.0e30f, -1.0e30f,
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::denorm_min(), 0.0f, 0.0f,   // the last two become distinct NaNs
    };
    const int KI_NUM_VALUES = static_cast<int>(sizeof(gafValues) / sizeof(gafValues[0]));
    float Value(int i) { return gafValues[i]; }

    volatile float gfZero = 0.0f;
    float NaN()     { return Value(KI_NUM_VALUES - 2); }
    float NegZero() { return -gfZero; }
    float V(float lfValue) { volatile float lfOpaque = lfValue; return lfOpaque; }
}

int main()
{
    using namespace fxfpumax_vecfloat;
    gafValues[KI_NUM_VALUES - 2] = FromBits(0x7FC00001u);
    gafValues[KI_NUM_VALUES - 1] = FromBits(0xFFC00002u);

    // ---------------------------------------------------------------- the VecFloat helpers
#if defined(FXFPUMAX_HAVE_VECFLOAT_HELPERS)
    {
        bool lbMax = true, lbMin = true, lbClamp = true;
        for (int i = 0; i < KI_NUM_VALUES; ++i)
            for (int j = 0; j < KI_NUM_VALUES; ++j)
            {
                lbMax = lbMax && Same(VecFloatMax(Value(i), Value(j)), Vmaxfp(Value(i), Value(j)));
                lbMin = lbMin && Same(VecFloatMin(Value(i), Value(j)), Vminfp(Value(i), Value(j)));
                for (int k = 0; k < KI_NUM_VALUES; ++k)
                    lbClamp = lbClamp && Same(VecFloatClamp(Value(i), Value(j), Value(k)),
                                              VClamp(Value(i), Value(j), Value(k)));
            }
        Check(lbMax, "VecFloatMax equals vmaxfp bit-exactly over the 14 x 14 matrix");
        Check(lbMin, "VecFloatMin equals vminfp bit-exactly over the 14 x 14 matrix");
        Check(lbClamp, "VecFloatClamp equals vminfp(max, vmaxfp(min, v)) over all 14^3 triples");
        Check(IsNaN(VecFloatClamp(NaN(), V(0.0f), V(1.0f))) && IsNaN(VecFloatClamp(V(0.5f), NaN(), V(1.0f)))
              && IsNaN(VecFloatClamp(V(0.5f), V(0.0f), NaN())),
              "VecFloatClamp: a NaN value, min or max gives NaN (the fsel fpu::Clamp gives max / ignores min)");
        Check(Same(VecFloatMax(V(0.0f), NegZero()), 0.0f) && Same(VecFloatMax(NegZero(), V(0.0f)), 0.0f)
              && Same(VecFloatMin(V(0.0f), NegZero()), -0.0f) && Same(VecFloatMin(NegZero(), V(0.0f)), -0.0f),
              "VecFloatMax(+-0) is +0 and VecFloatMin(+-0) is -0 in either order");
    }
#else
    Check(false, "VecFloatMax / VecFloatMin / VecFloatClamp exist in BrnBehaviourGameplayExternal.cpp");
    Check(false, "(VecFloatMin: absent)");
    Check(false, "(VecFloatClamp: absent)");
    Check(false, "(VecFloatClamp NaN: absent)");
    Check(false, "(VecFloat zeros: absent)");
#endif

    // ---------------------------------------------------------------- ModifyTargetAngles
    {
        const float lfPitch = V(0.3926991f), lfRoll = V(0.5f);
        Vec4 lAngles = { NaN(), 7.0f, NaN(), 9.0f };
        ModifyTargetAngleClamps(lAngles, lfPitch, lfRoll);
        Check(IsNaN(lAngles.x) && IsNaN(lAngles.z) && Same(lAngles.y, 7.0f) && Same(lAngles.w, 9.0f),
              "ModifyTargetAngles: NaN pitch / roll stay NaN through vmaxfp / vminfp (fsel Clamp would give +bound)");
        Vec4 lOrdinary = { V(1.0f), 7.0f, V(-1.0f), 9.0f };
        ModifyTargetAngleClamps(lOrdinary, lfPitch, lfRoll);
        Check(Same(lOrdinary.x, lfPitch) && Same(lOrdinary.z, -lfRoll) && Same(lOrdinary.y, 7.0f),
              "ModifyTargetAngles: out-of-band angles clamp to +pitch / -roll, lanes y / w untouched");
        Vec4 lInside = { V(0.25f), 0.0f, NegZero(), 0.0f };
        ModifyTargetAngleClamps(lInside, lfPitch, lfRoll);
        Check(Same(lInside.x, 0.25f) && Same(lInside.z, VClamp(-0.0f, -0.5f, 0.5f)),
              "ModifyTargetAngles: in-band angles pass (a -0 roll stays -0)");
    }

    // ---------------------------------------------------------------- InterpolateLastPlayerTransform
    {
        Check(IsNaN(ExcessSpeed(NaN())), ".cpp:584 a NaN car speed keeps the excess speed NaN (vmaxfp; fsel Max gives 0)");
        Check(Same(ExcessSpeed(V(5.0f)), 2.0f) && Same(ExcessSpeed(V(1.0f)), 0.0f) && Same(ExcessSpeed(V(3.0f)), 0.0f),
              ".cpp:584 ordinary speeds: above / below / at SPEED_TO_INTERP_MPS");

        float lafOut[3];
        RateLimit(V(0.2f), NaN(), lafOut);
        Check(IsNaN(lafOut[0]) && IsNaN(lafOut[1]) && IsNaN(lafOut[2]),
              ".cpp:589/:590 a NaN speed mod keeps the rotate angle, its clamp and the blend NaN");
        RateLimit(NaN(), V(0.5f), lafOut);
        Check(IsNaN(lafOut[2]), ".cpp:591 a NaN out-angle keeps the blend NaN (vmaxfp against MIN_DIV_ANGLE)");
        RateLimit(V(0.2f), V(0.5f), lafOut);
        Check(Same(lafOut[0], 0.1f) && Same(lafOut[1], 0.05f) && Same(lafOut[2], 0.05f / 0.2f),
              ".cpp:589..:591 ordinary: 0.2 * 0.5 = 0.1 rad, rate-limited to MAX_INTERP 0.05, blend 0.25");
        RateLimit(V(0.0f), V(0.5f), lafOut);
        Check(Same(lafOut[1], 0.01f) && Same(lafOut[2], 0.01f / 0.00001f),
              ".cpp:590/:591 a zero out-angle: the angle floors at MIN_INTERP and divides by MIN_DIV_ANGLE");
    }

    // ---------------------------------------------------------------- Update .cpp:214
    Check(IsNaN(PitchLimitReduction(NaN())) && Same(PitchLimitReduction(V(0.0f)), 0.0f)
          && Same(PitchLimitReduction(V(1000.0f)), 5.0f),
          ".cpp:214 the pitch-limit reduction: NaN / standing / past 100 MPH");

    // ---------------------------------------------------------------- structural (SLerp / acos arguments)
    for (const StructuralCheck& lrCheck : kaStructural)
        Check(lrCheck.mbOk, lrCheck.mpcWhat);

    std::printf("FxFpumaxVecFloatSites: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
