// FX-FPUMAX call-site group A (run_fxfpumax_order_sites.py): the shipped statements, extracted into
// fxfpumax_order_sites.inl and compiled against the shipped rw/math/fpu/scalar_operation.h, against the
// console's own forms. The oracle builds every console Min / Max from the PowerPC fsel read off the
// DIFFERENCE's bit pattern (fsel frD,frA,frB,frC = frA >= 0 ? frB : frC; NaN -> frC; -0 counts as >= 0),
// so it shares no spelling with the header. Comparisons are bit-exact (zero sign and NaN included).
#include "rw/math/fpu/scalar_operation.h"
#include "fxfpumax_order_sites.inl"

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

    bool Same(float a, float b) { return Bits(a) == Bits(b); }

    float Fsel(float lfTest, float lfSecond, float lfThird)
    {
        const uint32_t luBits = Bits(lfTest);
        if ((luBits & 0x7F800000u) == 0x7F800000u && (luBits & 0x007FFFFFu) != 0u)
            return lfThird;                                                  // NaN
        if ((luBits & 0x80000000u) != 0u && (luBits & 0x7FFFFFFFu) != 0u)
            return lfThird;                                                  // negative, non-zero
        return lfSecond;                                                     // +0 / -0 / positive
    }
    float CMin(float a, float b) { return Fsel(a - b, b, a); }             // rwmath scalar.h:141-167
    float CMax(float a, float b) { return Fsel(a - b, a, b); }             // rwmath scalar.h:208-235

    volatile float gfNaN      = std::numeric_limits<float>::quiet_NaN();
    volatile float gfZero     = 0.0f;
    float NaN()     { return gfNaN; }
    float NegZero() { return -gfZero; }
    float V(float lfValue) { volatile float lfOpaque = lfValue; return lfOpaque; }

    // ---- the console forms --------------------------------------------------------------------
    // BehaviourGameplayExternal::Update .cpp:440: 0x82241C4C fsel(-t, 0.0, t) ; 0x82241C54 fsel(1-v, v, 1.0)
    float ConsoleGrounded(float t) { return 1.0f - CMin(1.0f, CMax(0.0f, t)); }
    // 0x82241F40 fsel(-s, 0, s) ; 0x82241F4C fsel(0.8-v, v, 0.8) (0.8 @0x820054C8) ; 0x82241F58 fsel(f-v, f, v)
    float ConsoleImpact(float f, float s) { return CMax(f, CMin(0.8f, CMax(0.0f, s))); }
    // Looker::Zoom 0x82222EE4 fcmpu F,t ; 0x82222EF0 ble -> 0x82222F08 fsel(vd-(t-F), t-F, vd)
    //                                     else 0x82222EFC fsel(-vd-(t-F), -vd, t-F)
    float ConsoleLookerStep(float F, float t, float vel, float dt)
    {
        const float lfDelta = t - F;
        const float lfMax   = vel * dt;
        const bool lbGreater = F > t;                                        // ble taken on <= and unordered
        return lbGreater ? CMax(-lfMax, lfDelta) : CMin(lfMax, lfDelta);
    }
    // KeyAnimController::Update 0x8223D190 fcmpu played, 1.0 ; 0x8223D194 bge keeps 1.0 ; else played
    float ConsoleParameter(float timer, float length)
    {
        const float lfPlayed = timer / length;
        float lfResult = 1.0f;
        if (lfPlayed < 1.0f)                                                 // bge not taken: ordered and <
            lfResult = lfPlayed;
        return lfResult;
    }
    // KeyAnimController::UpdateFocus 0x821F7FF0..0x821F8024 (MIN 0.001 @0x82001B08)
    void ConsoleBand(float I, float n, float f, float fm, float* out)
    {
        const float K = 0.001f;
        out[0] = CMin(1.0f, I);
        out[1] = CMax(0.0f, n - fm);
        out[2] = CMax(out[1] + K, n);
        out[3] = CMax(out[2], f);
        out[4] = CMax(out[3] + K, fm + f);
    }
    // WheelStateMachine::Update 0x82294028 (6.7041669 @0x82013278) ; 0x82294050 (* 0.14916097 @0x82013274) ;
    // 0x82294058 fsel(-x, 0.0, x)
    float ConsoleReverseThrust(float s) { return CMax(0.0f, (6.7041669f - s) * 0.14916097f); }
    // InputPads::FillRawData 0x828E7484 fsel(-1 - s, -1, s) ; 0x828E748C fsel(1 - v, v, 1.0)
    float ConsoleClampAxis(float s) { return CMin(1.0f, CMax(-1.0f, s)); }
}

int main()
{
    using namespace fxfpumax_sites;

    // ---------------------------------------------------------------- GameplayExternal .cpp:440
    Check(Same(Grounded(NaN()), ConsoleGrounded(NaN())) && Same(Grounded(NaN()), 0.0f),
          "GameplayExternal: a NaN time in air clamps to 1 (Min(1, Max(0, NaN)) = 1), so the boost shake factor is 0");
    Check(Same(Grounded(V(0.25f)), 0.75f) && Same(Grounded(V(2.0f)), 0.0f) && Same(Grounded(V(-1.0f)), 1.0f)
          && Same(Grounded(NegZero()), ConsoleGrounded(NegZero())),
          "GameplayExternal: ordinary / above / below / -0 times in air");

    // ---------------------------------------------------------------- GameplayExternal impact shake
    Check(Same(ImpactShake(V(0.3f), NaN()), ConsoleImpact(0.3f, NaN())) && Same(ImpactShake(V(0.3f), NaN()), 0.8f),
          "GameplayExternal: a NaN impact shake clamps to 0.8 and latches over 0.3");
    Check(Same(ImpactShake(V(0.3f), V(0.5f)), 0.5f) && Same(ImpactShake(V(0.3f), V(2.0f)), 0.8f)
          && Same(ImpactShake(V(0.3f), V(-1.0f)), 0.3f) && Same(ImpactShake(V(0.9f), V(0.5f)), 0.9f),
          "GameplayExternal: ordinary impact shakes (clamp to [0, 0.8], then latch the larger)");
    Check(Same(ImpactShake(NegZero(), NegZero()), ConsoleImpact(NegZero(), NegZero())),
          "GameplayExternal: -0 factor and -0 shake: Max(-0, Clamp(-0, 0, 0.8) = +0) keeps -0 (equal: the first)");

    // ---------------------------------------------------------------- Looker::Zoom step
    {
        LookerFixture lLooker;
        float lfFOV = 0.0f;
        lLooker.mfMaxFOVVelocity = V(10.0f);
        lLooker.mfTargetFOV = V(60.0f);
        Check(Same(lLooker.Step(V(50.0f), V(0.5f), &lfFOV), 5.0f) && Same(lfFOV, 55.0f),
              "Looker: below the target the FOV steps up by min(v dt, gap)");
        lLooker.mfTargetFOV = V(50.0f);
        Check(Same(lLooker.Step(V(60.0f), V(0.5f), &lfFOV), -5.0f) && Same(lfFOV, 55.0f),
              "Looker: above the target the FOV steps down by min(v dt, gap)");
        Check(Same(lLooker.Step(V(51.0f), V(0.5f), &lfFOV), -1.0f) && Same(lfFOV, 50.0f),
              "Looker: above the target within one step it lands on the target");
        lLooker.mfMaxFOVVelocity = NaN();
        Check(Same(lLooker.Step(V(60.0f), V(0.5f), &lfFOV), ConsoleLookerStep(60.0f, 50.0f, NaN(), 0.5f))
              && Same(lfFOV, 50.0f),
              "Looker: above the target with a NaN velocity band, Max(-NaN, t - F) keeps t - F: the FOV snaps to the target");
        lLooker.mfMaxFOVVelocity = V(10.0f);
        lLooker.mfTargetFOV = NaN();
        Check(Same(lLooker.Step(V(50.0f), V(0.5f), &lfFOV), ConsoleLookerStep(50.0f, NaN(), 10.0f, 0.5f))
              && Same(ConsoleLookerStep(50.0f, NaN(), 10.0f, 0.5f), 5.0f),
              "Looker: a NaN target takes the Min arm (0x82222EF0 ble is taken unordered): step +v dt, not -v dt");
        lLooker.mfTargetFOV = V(50.0f);
        Check(Same(lLooker.Step(NaN(), V(0.5f), &lfFOV), ConsoleLookerStep(NaN(), 50.0f, 10.0f, 0.5f)),
              "Looker: a NaN FOV takes the Min arm too: step +v dt");
    }

    // ---------------------------------------------------------------- KeyAnimController::Update parameter
    {
        KeyAnimFixture lKeyAnim;
        lKeyAnim.mfPlaybackTimer = V(0.0f);
        lKeyAnim.mfLength = V(0.0f);
        Check(Same(lKeyAnim.Parameter(), ConsoleParameter(0.0f, 0.0f)) && Same(lKeyAnim.Parameter(), 1.0f),
              "KeyAnim: a NaN played fraction (0 / 0) seeks to 1.0 (0x8223D194 bge taken unordered)");
        lKeyAnim.mfPlaybackTimer = V(2.0f);
        lKeyAnim.mfLength = V(4.0f);
        const bool lbHalf = Same(lKeyAnim.Parameter(), 0.5f);
        lKeyAnim.mfPlaybackTimer = V(5.0f);
        Check(lbHalf && Same(lKeyAnim.Parameter(), 1.0f), "KeyAnim: ordinary fractions pass, past the end caps at 1.0");
    }

    // ---------------------------------------------------------------- KeyAnimController::UpdateFocus band
    {
        float lafShipped[5], lafConsole[5];
        FocusBand(V(0.5f), V(10.0f), V(20.0f), V(2.0f), lafShipped);
        ConsoleBand(0.5f, 10.0f, 20.0f, 2.0f, lafConsole);
        bool lbSame = true;
        for (int i = 0; i < 5; ++i) lbSame = lbSame && Same(lafShipped[i], lafConsole[i]);
        Check(lbSame, "UpdateFocus: an ordinary band equals the console's five selects");

        FocusBand(V(0.5f), NaN(), V(20.0f), V(2.0f), lafShipped);
        ConsoleBand(0.5f, NaN(), 20.0f, 2.0f, lafConsole);
        lbSame = true;
        for (int i = 0; i < 5; ++i) lbSame = lbSame && Same(lafShipped[i], lafConsole[i]);
        Check(lbSame && Same(lafShipped[1], lafConsole[1]) && lafShipped[1] != lafShipped[1],
              "UpdateFocus: a NaN near focus keeps the focus start NaN (0x821F8000 Max(0, NaN) selects the NaN)");

        FocusBand(V(0.5f), NegZero(), V(20.0f), V(0.0f), lafShipped);
        ConsoleBand(0.5f, NegZero(), 20.0f, 0.0f, lafConsole);
        Check(Same(lafShipped[1], lafConsole[1]) && Same(lafShipped[1], 0.0f),
              "UpdateFocus: a -0 start edge becomes +0 (Max(0, -0) selects the zero first operand)");

        FocusBand(V(2.0f), V(10.0f), V(20.0f), V(2.0f), lafShipped);
        Check(Same(lafShipped[0], 1.0f), "UpdateFocus: blurriness caps at 1.0");
    }

    // ---------------------------------------------------------------- WheelStateMachine reverse thrust
    Check(Same(ReverseThrustScale(NaN()), ConsoleReverseThrust(NaN())) && ReverseThrustScale(NaN()) != ReverseThrustScale(NaN()),
          "Wheel: a NaN wheel speed keeps the reverse-thrust ramp NaN (Max(0, NaN) selects the NaN), not 0");
    Check(Same(ReverseThrustScale(V(0.0f)), ConsoleReverseThrust(0.0f)) && Same(ReverseThrustScale(V(10.0f)), 0.0f)
          && Same(ReverseThrustScale(V(6.7041669f)), ConsoleReverseThrust(6.7041669f)),
          "Wheel: ordinary speeds (ramp below the cutoff, 0 above, +0 at it)");

    // ---------------------------------------------------------------- CgsInputPadsPC ClampAxis
    Check(Same(ClampAxis(NaN()), ConsoleClampAxis(NaN())) && Same(ClampAxis(NaN()), 1.0f),
          "ClampAxis: a NaN axis reads +1 (FillRawData's Min(1, Max(-1, NaN)))");
    Check(Same(ClampAxis(V(2.0f)), 1.0f) && Same(ClampAxis(V(-2.0f)), -1.0f) && Same(ClampAxis(V(0.25f)), 0.25f)
          && Same(ClampAxis(NegZero()), ConsoleClampAxis(NegZero())),
          "ClampAxis: above / below / inside / -0");

    std::printf("FxFpumaxOrderSites: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
