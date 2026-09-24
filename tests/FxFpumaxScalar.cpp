// FX-FPUMAX: rw::math::fpu::Min / Max / Clamp (vendor/renderware/include/rw/math/fpu/scalar_operation.h,
// compiled from the production header by run_fxfpumax_scalar.py) against an fsel oracle.
//
// The original rwmath 1.02.00 include/rw/math/fpu/scalar.h
// (references/Feb-2007/BrnEntityModuleUnity/EARenderWare/stable/external/rwmath/1.02.00/...):
//   Min<float>  :141-167  test = a - b ; fsel(test, b, a)
//   Max<float>  :208-235  test = a - b ; fsel(test, a, b)
//   Min/Max<double> :169-196 / :237-264  the same with `float test = a - b`
//   generic T   :136-139 / :203-206  a < b ? a : b / a > b ? a : b
//   Clamp       :335-338  Min(max, Max(min, value))
// The console inlines them as that fsel: Min at 0x822B21F8/0x822B21FC `fsubs f13,a,b ; fsel f0,f13,b,a`
// (CheckVehicleForPowerPark), Max at 0x8271F574 `fsel f0,d,d,0.0` (Max(d, 0.0f)).
// PowerPC fsel frD,frA,frB,frC = (frA >= 0.0) ? frB : frC -- a NaN frA selects frC and -0 counts as >= 0.
// The oracle below reads that from the BIT PATTERN of the difference, so it shares no spelling with the
// header. Inputs come from a volatile table so /O2 cannot fold the calls. Every comparison is bit-exact:
// the selected operand's bits, NaN payload and zero sign included.
#include "rw/math/fpu/scalar_operation.h"

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

    uint64_t Bits(double ldValue)
    {
        uint64_t luBits;
        std::memcpy(&luBits, &ldValue, sizeof(luBits));
        return luBits;
    }

    float FloatFromBits(uint32_t luBits)
    {
        float lfValue;
        std::memcpy(&lfValue, &luBits, sizeof(lfValue));
        return lfValue;
    }

    double DoubleFromBits(uint64_t luBits)
    {
        double ldValue;
        std::memcpy(&ldValue, &luBits, sizeof(ldValue));
        return ldValue;
    }

    // fsel's test, from the bits: NaN -> false; +0 / -0 / positive -> true; negative non-zero -> false.
    bool FselTakesSecond(float lfTest)
    {
        const uint32_t luBits = Bits(lfTest);
        const bool lbNaN      = (luBits & 0x7F800000u) == 0x7F800000u && (luBits & 0x007FFFFFu) != 0u;
        if (lbNaN)
        {
            return false;
        }
        const bool lbNegativeNonZero = (luBits & 0x80000000u) != 0u && (luBits & 0x7FFFFFFFu) != 0u;
        return !lbNegativeNonZero;
    }

    template <typename T> T Fsel(float lfTest, T lSecond, T lThird) { return FselTakesSecond(lfTest) ? lSecond : lThird; }

    float  OracleMin(float a, float b)    { return Fsel(a - b, b, a); }
    float  OracleMax(float a, float b)    { return Fsel(a - b, a, b); }
    double OracleMin(double a, double b)  { return Fsel(static_cast<float>(a - b), b, a); }
    double OracleMax(double a, double b)  { return Fsel(static_cast<float>(a - b), a, b); }
    template <typename T> T OracleClamp(T v, T lo, T hi) { return OracleMin(hi, OracleMax(lo, v)); }

    bool Same(float a, float b)   { return Bits(a) == Bits(b); }
    bool Same(double a, double b) { return Bits(a) == Bits(b); }

    bool IsNaNBits(float lfValue)
    {
        const uint32_t luBits = Bits(lfValue);
        return (luBits & 0x7F800000u) == 0x7F800000u && (luBits & 0x007FFFFFu) != 0u;
    }

    // Distinct NaNs (sign and payload differ) so a check can tell WHICH operand came back.
    const uint32_t KU_NAN_A = 0x7FC00001u;
    const uint32_t KU_NAN_B = 0xFFC00002u;

    volatile float gafValues[] =
    {
        0.0f, -0.0f, 1.0f, -1.0f, 0.5f, -0.5f, 2.0f, 3.0f, 1.0e30f, -1.0e30f,
        std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(),
        std::numeric_limits<float>::min(), std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
        0.0f, 0.0f,   // the two NaNs, filled in by main (a volatile initialiser cannot take a bit pattern)
    };
    const int KI_NUM_VALUES = static_cast<int>(sizeof(gafValues) / sizeof(gafValues[0]));

    volatile double gadValues[] =
    {
        0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 2.0, 3.0, 1.0e30, -1.0e30,
        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
        0.0, 0.0,     // the two NaNs
    };
    const int KI_NUM_DOUBLE_VALUES = static_cast<int>(sizeof(gadValues) / sizeof(gadValues[0]));

    float  V(int i)  { return gafValues[i]; }
    double DV(int i) { return gadValues[i]; }

    // Opaque single values (volatile reads) for the named cases.
    volatile float gfZero = 0.0f, gfOne = 1.0f, gfTwo = 2.0f, gfHalf = 0.5f, gfMinusOne = -1.0f, gfThree = 3.0f;
    volatile float gfNaNA = 0.0f, gfNaNB = 0.0f;
    float F(volatile float& lrValue) { return lrValue; }
    float NegZero() { return -F(gfZero); }
}

int main()
{
    gafValues[KI_NUM_VALUES - 2] = FloatFromBits(KU_NAN_A);
    gafValues[KI_NUM_VALUES - 1] = FloatFromBits(KU_NAN_B);
    gadValues[KI_NUM_DOUBLE_VALUES - 2] = DoubleFromBits(0x7FF8000000000001ull);
    gadValues[KI_NUM_DOUBLE_VALUES - 1] = DoubleFromBits(0xFFF8000000000002ull);
    gfNaNA = FloatFromBits(KU_NAN_A);
    gfNaNB = FloatFromBits(KU_NAN_B);

    namespace fpu = rw::math::fpu;
    const float lfNaNA = F(gfNaNA);
    const float lfNaNB = F(gfNaNB);
    const float lfZero = F(gfZero);
    const float lfNegZero = NegZero();
    const float lfOne = F(gfOne);
    const float lfTwo = F(gfTwo);
    const float lfHalf = F(gfHalf);
    const float lfMinusOne = F(gfMinusOne);
    const float lfThree = F(gfThree);

    // ---------------------------------------------------------------- Min<float>
    {
        bool lbFirstNaN = true, lbSecondNaN = true;
        for (int i = 0; i < KI_NUM_VALUES; ++i)
        {
            lbFirstNaN  = lbFirstNaN  && Same(fpu::Min(lfNaNA, V(i)), lfNaNA);
            lbSecondNaN = lbSecondNaN && Same(fpu::Min(V(i), lfNaNA), V(i));
        }
        Check(lbFirstNaN,  "Min<float>(NaN, x) is the NaN: fsel on a NaN difference selects frC = a");
        Check(lbSecondNaN, "Min<float>(x, NaN) is x: fsel on a NaN difference selects frC = a");
        Check(Same(fpu::Min(lfNaNA, lfNaNB), lfNaNA), "Min<float>(NaN_A, NaN_B) is NaN_A (the first)");
        Check(Same(fpu::Min(lfZero, lfNegZero), lfNegZero), "Min<float>(+0, -0) is -0: +0 - -0 = +0 >= 0 selects b");
        Check(Same(fpu::Min(lfNegZero, lfZero), lfZero), "Min<float>(-0, +0) is +0: -0 - +0 = -0 >= 0 selects b");
        Check(Same(fpu::Min(lfOne, lfTwo), lfOne) && Same(fpu::Min(lfTwo, lfOne), lfOne)
              && Same(fpu::Min(lfMinusOne, lfHalf), lfMinusOne), "Min<float> of ordinary values is the smaller");
        bool lbMatrix = true;
        for (int i = 0; i < KI_NUM_VALUES; ++i)
            for (int j = 0; j < KI_NUM_VALUES; ++j)
                lbMatrix = lbMatrix && Same(fpu::Min(V(i), V(j)), OracleMin(V(i), V(j)));
        Check(lbMatrix, "Min<float> equals fsel(a - b, b, a) bit-exactly over the 18 x 18 value matrix");
    }

    // ---------------------------------------------------------------- Max<float>
    {
        bool lbFirstNaN = true, lbSecondNaN = true;
        for (int i = 0; i < KI_NUM_VALUES; ++i)
        {
            lbFirstNaN  = lbFirstNaN  && Same(fpu::Max(lfNaNA, V(i)), V(i));
            lbSecondNaN = lbSecondNaN && Same(fpu::Max(V(i), lfNaNA), lfNaNA);
        }
        Check(lbFirstNaN,  "Max<float>(NaN, x) is x: fsel on a NaN difference selects frC = b (0x8271F574 gives 0.0)");
        Check(lbSecondNaN, "Max<float>(x, NaN) is the NaN: fsel on a NaN difference selects frC = b");
        Check(Same(fpu::Max(lfNaNA, lfNaNB), lfNaNB), "Max<float>(NaN_A, NaN_B) is NaN_B (the second)");
        Check(Same(fpu::Max(lfZero, lfNegZero), lfZero), "Max<float>(+0, -0) is +0: equal operands select a");
        Check(Same(fpu::Max(lfNegZero, lfZero), lfNegZero), "Max<float>(-0, +0) is -0: -0 - +0 = -0 >= 0 selects a");
        Check(Same(fpu::Max(lfOne, lfTwo), lfTwo) && Same(fpu::Max(lfTwo, lfOne), lfTwo)
              && Same(fpu::Max(lfMinusOne, lfHalf), lfHalf), "Max<float> of ordinary values is the larger");
        bool lbMatrix = true;
        for (int i = 0; i < KI_NUM_VALUES; ++i)
            for (int j = 0; j < KI_NUM_VALUES; ++j)
                lbMatrix = lbMatrix && Same(fpu::Max(V(i), V(j)), OracleMax(V(i), V(j)));
        Check(lbMatrix, "Max<float> equals fsel(a - b, a, b) bit-exactly over the 18 x 18 value matrix");
    }

    // ---------------------------------------------------------------- Clamp<float>
    {
        Check(Same(fpu::Clamp(lfNaNA, lfZero, lfOne), lfOne),
              "Clamp<float>(NaN, 0, 1) is 1: Max(0, NaN) keeps the NaN, Min(1, NaN) selects the 1");
        Check(Same(fpu::Clamp(lfNaNA, lfMinusOne, lfThree), lfThree), "Clamp<float>(NaN, -1, 3) is 3 (max)");
        Check(Same(fpu::Clamp(lfHalf, lfNaNA, lfOne), lfHalf) && Same(fpu::Clamp(lfTwo, lfNaNA, lfOne), lfOne),
              "Clamp<float>(v, NaN, hi) is Min(hi, v): a NaN min is ignored");
        Check(Same(fpu::Clamp(lfHalf, lfZero, lfNaNA), lfNaNA), "Clamp<float>(v, lo, NaN) is the NaN max");
        Check(Same(fpu::Clamp(lfNegZero, lfZero, lfOne), lfZero),
              "Clamp<float>(-0, +0, 1) is +0: Max(+0, -0) selects the +0 min");
        Check(Same(fpu::Clamp(lfHalf, lfOne, lfZero), lfZero),
              "Clamp<float> with an inverted range (min 1 > max 0) is the max: Min is applied last");
        Check(Same(fpu::Clamp(lfHalf, lfZero, lfOne), lfHalf) && Same(fpu::Clamp(lfMinusOne, lfZero, lfOne), lfZero)
              && Same(fpu::Clamp(lfTwo, lfZero, lfOne), lfOne), "Clamp<float> inside / below / above an ordinary range");
        bool lbMatrix = true;
        for (int i = 0; i < KI_NUM_VALUES; ++i)
            for (int j = 0; j < KI_NUM_VALUES; ++j)
                for (int k = 0; k < KI_NUM_VALUES; ++k)
                    lbMatrix = lbMatrix && Same(fpu::Clamp(V(i), V(j), V(k)), OracleClamp(V(i), V(j), V(k)));
        Check(lbMatrix, "Clamp<float> equals Min(max, Max(min, value)) of the fsel oracle over all 18^3 triples");
    }

    // ---------------------------------------------------------------- double
    {
        const double ldNaN = DV(KI_NUM_DOUBLE_VALUES - 2);
        const double ldOne = DV(2), ldZero = DV(0), ldNegZero = DV(1), ldTwo = DV(6);
        Check(Same(fpu::Max(ldNaN, ldOne), ldOne) && Same(fpu::Max(ldOne, ldNaN), ldNaN),
              "Max<double>: a NaN in either operand returns the second");
        Check(Same(fpu::Min(ldNaN, ldOne), ldNaN) && Same(fpu::Min(ldOne, ldNaN), ldOne),
              "Min<double>: a NaN in either operand returns the first");
        Check(Same(fpu::Min(ldZero, ldNegZero), ldNegZero) && Same(fpu::Max(ldNegZero, ldZero), ldNegZero),
              "Min<double>(+0, -0) is -0 and Max<double>(-0, +0) is -0");
        Check(Same(fpu::Clamp(ldNaN, ldZero, ldTwo), ldTwo), "Clamp<double>(NaN, 0, 2) is 2 (max)");
        bool lbMatrix = true;
        for (int i = 0; i < KI_NUM_DOUBLE_VALUES; ++i)
            for (int j = 0; j < KI_NUM_DOUBLE_VALUES; ++j)
                lbMatrix = lbMatrix && Same(fpu::Min(DV(i), DV(j)), OracleMin(DV(i), DV(j)))
                                    && Same(fpu::Max(DV(i), DV(j)), OracleMax(DV(i), DV(j)));
        Check(lbMatrix, "Min/Max<double> equal the fsel oracle (difference held in a float) over the 14 x 14 matrix");
    }

    // ---------------------------------------------------------------- generic T (integral): values unchanged
    {
        volatile int giThree = 3, giFive = 5, giSeven = 7, giMinusTwo = -2;
        const int liThree = giThree, liFive = giFive, liSeven = giSeven, liMinusTwo = giMinusTwo;
        Check(fpu::Min(liThree, liFive) == 3 && fpu::Min(liFive, liThree) == 3
              && fpu::Max(liThree, liFive) == 5 && fpu::Max(liFive, liThree) == 5,
              "Min/Max<int>: the generic ternaries (scalar.h:136-139 / :203-206)");
        Check(fpu::Clamp(liSeven, 0, liFive) == 5 && fpu::Clamp(liMinusTwo, 0, liFive) == 0
              && fpu::Clamp(liThree, 0, liFive) == 3, "Clamp<int> above / below / inside");
        volatile unsigned guBig = 0xFFFFFFF0u;
        Check(fpu::Max(static_cast<unsigned>(guBig), 1u) == 0xFFFFFFF0u && fpu::Min(static_cast<unsigned>(guBig), 1u) == 1u,
              "Min/Max<unsigned> compare unsigned");
    }

    Check(IsNaNBits(lfNaNA) && IsNaNBits(lfNaNB) && !Same(lfNaNA, lfNaNB), "fixture: the two NaNs are NaN and distinct");

    std::printf("FxFpumaxScalar: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
