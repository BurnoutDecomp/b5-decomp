// FX-DIRECTOR2 (crash parity 2026-09-25): Looker::Zoom's depth-of-field blurriness ramp.
//
// The runner (run_fxdirector2_looker_dof_ramp.py) lifts the revision's three KF_DOF_*_RATE constants into
// fxd2_dof_const.inc and Zoom's one blurriness write (`lrCamera.GetDepthOfField().SetBlurriness(...)`) into
// fxd2_dof_write.inc, and runs the write against a camera stand-in whose DOF records the value.
//
// Against the ARTIST body (Looker::Zoom @0x82222A78):
//   0x82222E30  lbz 0x1D (mbAssessingFOV) ; bne 0x82222F38 -> f13 = flt_82CDAD18 (0x3DCCCCCD, 0.1)
//   0x82222ED8  fcmpu FOV, target ; beq 0x82222F2C           -> f13 = flt_82CDAD1C (0x3DCCCCCD, 0.1)
//   0x82222F0C  fall-through (moving)                        -> f13 = flt_82CDAD20 (0x3E19999A, 0.15),
//               f12 = 1.0 (flt_82001C98) - blur (fsubs 0x82222F24)
//   0x82222F44  (the first two) f12 = fneg blur
//   0x82222F48  fmadds f0, f12, f13, f0 -- delta * rate + blur, ONE rounding (ROUNDING_RULE rule 3) -> stfs 0x134
// Each rate has exactly one reader (the lfs above) and no writer: findinit finds no other site, no CRT init thunk,
// no debug-menu registration, and no data word holds the address.
#include <cmath>
#include <cstdio>
#include <cstring>

typedef float        f32;
typedef unsigned int u32;

static unsigned gChecks = 0, gFailures = 0;

namespace
{
#include "fxd2_dof_const.inc"
}

struct FakeDepthOfField
{
    f32  mfBlurriness;
    void SetBlurriness(f32 lfBlurriness) { mfBlurriness = lfBlurriness; }
};
struct FakeCamera
{
    FakeDepthOfField mDepthOfField;
    FakeDepthOfField& GetDepthOfField() { return mDepthOfField; }
};

// Zoom's write, lifted, over one camera.
static f32 Write(f32 lfDofDelta, f32 lfDofRate, f32 lfBlur)
{
    FakeCamera lCamera;
    FakeCamera& lrCamera = lCamera;
    lrCamera.mDepthOfField.mfBlurriness = lfBlur;
#include "fxd2_dof_write.inc"
    return lrCamera.mDepthOfField.mfBlurriness;
}

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcName);
    }
}
static u32 Bits(f32 lf)
{
    u32 lu;
    std::memcpy(&lu, &lf, sizeof(lu));
    return lu;
}

int main()
{
    Check(Bits(KF_DOF_UNBLUR_RATE) == 0x3DCCCCCDu, "K1 the assessing rate is flt_82CDAD18 == 0x3DCCCCCD (0.1)");
    Check(Bits(KF_DOF_HOLD_RATE) == 0x3DCCCCCDu, "K2 the on-target rate is flt_82CDAD1C == 0x3DCCCCCD (0.1)");
    Check(Bits(KF_DOF_BLUR_RATE) == 0x3E19999Au, "K3 the moving rate is flt_82CDAD20 == 0x3E19999A (0.15)");

    // One rounding: every blurriness in [0, 1] on a 2^-16 grid, the three branches' deltas, against fmaf.
    u32 luMismatch = 0, luTwoRoundingsDiffer = 0;
    for (u32 luStep = 0; luStep <= 65536u; ++luStep)
    {
        const f32 lfBlur = static_cast<f32>(luStep) / 65536.0f;
        const f32 lfUp = 1.0f - lfBlur;   // the moving branch's own fsubs (rule 4)
        const f32 kaDelta[3] = { -lfBlur, -lfBlur, lfUp };
        const f32 kaRate[3] = { 0.1f, 0.1f, 0.15f };
        for (int liBranch = 0; liBranch < 3; ++liBranch)
        {
            const f32 lfFused = std::fmaf(kaDelta[liBranch], kaRate[liBranch], lfBlur);
            volatile f32 lfProduct = kaDelta[liBranch] * kaRate[liBranch];
            const f32 lfTwo = lfProduct + lfBlur;
            luTwoRoundingsDiffer += (Bits(lfTwo) != Bits(lfFused)) ? 1u : 0u;
            luMismatch += (Bits(Write(kaDelta[liBranch], kaRate[liBranch], lfBlur)) != Bits(lfFused)) ? 1u : 0u;
        }
    }
    Check(luMismatch == 0 && luTwoRoundingsDiffer > 0,
          "K4 the write is delta * rate + blur rounded ONCE (fmadds @0x82222F48) over 65537 blurriness values x 3 branches "
          "(a separate multiply and add differs on some of them)");
    if (luMismatch != 0)
        std::printf("      %u of %u writes differ from the fused result (%u differ between the two forms)\n",
                    luMismatch, 3u * 65537u, luTwoRoundingsDiffer);

    // The ramp moves: ten moving frames from 0.5 climb toward 1, ten assessing frames fall toward 0.
    f32 lfUpRamp = 0.5f, lfDownRamp = 0.5f, lfUpRef = 0.5f, lfDownRef = 0.5f;
    for (int liFrame = 0; liFrame < 10; ++liFrame)
    {
        lfUpRamp = Write(1.0f - lfUpRamp, KF_DOF_BLUR_RATE, lfUpRamp);
        lfDownRamp = Write(-lfDownRamp, KF_DOF_UNBLUR_RATE, lfDownRamp);
        lfUpRef = std::fmaf(1.0f - lfUpRef, 0.15f, lfUpRef);
        lfDownRef = std::fmaf(-lfDownRef, 0.1f, lfDownRef);
    }
    Check(lfUpRamp > 0.9f && lfUpRamp < 1.0f && Bits(lfUpRamp) == Bits(lfUpRef),
          "K5 ten moving frames take the blurriness from 0.5 toward 1 (0.5 + 0.5 * (1 - 0.85^10))");
    Check(lfDownRamp < 0.2f && lfDownRamp > 0.0f && Bits(lfDownRamp) == Bits(lfDownRef),
          "K6 ten assessing frames take it from 0.5 toward 0 (0.5 * 0.9^10)");

    std::printf("FxDirector2LookerDofRamp: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
