// FX-DIRECTOR2 (crash parity 2026-09-25): CameraRig::Construct @0x8220B0E8 and the twenty rig presets.
//
// The runner (run_fxdirector2_camera_rig.py) writes the revision's BrnCameraRigConstruct.cpp and
// BrnCameraRigParams.cpp into fxdirector2_camera_rig.inc; a revision without them does not build, and every numeric
// check then counts as failed.
//
// THE REFERENCE is the console itself: FxDirector2CameraRigGolden.h holds CameraRig::Construct's outputs as the
// ARTIST code produced them, run instruction by instruction on a PPC/VMX128 emulator over the raw words (the
// generator, gen_camrig_golden.py, is FX-DIRECTOR2 scratch; its table is committed beside this file):
//   * every preset against two car boxes, mirrored and not (80 cases), and 24 pseudo-random parameter sets;
//   * the preset words themselves, read out of the image (the CRT-thunk vectors and the static scalars).
// The console evaluates sin / cos with its inlined minimax and fuses its multiply-adds; the PC uses the vendor's
// std::sin / std::cos and separate mul + add, so the transform is compared to within 1e-5 (the measured gap is
// ~2e-7). The FOV and the presets are compared bit for bit.
#include <cmath>
#include <cstdio>
#include <cstring>
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"

#include "fxdirector2_camera_rig.inc"
#include "FxDirector2CameraRigGolden.h"

using BrnDirector::Camera::Utils::CameraRig;

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

static u32 Bits(f32 lfValue)
{
    u32 luBits;
    std::memcpy(&luBits, &lfValue, sizeof(luBits));
    return luBits;
}

static f32 Float(u32 luBits)
{
    f32 lfValue;
    std::memcpy(&lfValue, &luBits, sizeof(lfValue));
    return lfValue;
}

static Vector3 V3(const u32 lauBits[3])
{
    Vector3 lVector;
    lVector.x = Float(lauBits[0]); lVector.y = Float(lauBits[1]); lVector.z = Float(lauBits[2]); lVector.w = 0.0f;
    return lVector;
}

int main()
{
    // ---- the twenty presets, in the console's storage order (BrnCameraRigParams.cpp's definition order) ----
    const CameraRig::Params* const lapPresets[20] = {
        &CameraRig::ParamsRearLongFlat,     &CameraRig::ParamsFrontQuarterLong,  &CameraRig::ParamsFrontQuarterClose,
        &CameraRig::ParamsFrontQuarterCloseDeep, &CameraRig::ParamsHighSideFlat, &CameraRig::ParamsSideFlat,
        &CameraRig::ParamsBonnetHigh,       &CameraRig::ParamsBootHigh,          &CameraRig::ParamsBonnetLow,
        &CameraRig::ParamsFrontQCuFwd,      &CameraRig::ParamsSideLookingForwards, &CameraRig::ParamsRearQFwd,
        &CameraRig::ParamsRigFrontQBwd,     &CameraRig::ParamsFrontRearview,     &CameraRig::ParamsBootViewFwd,
        &CameraRig::ParamsFrontQLowBwd,     &CameraRig::ParamsRoofFwd,           &CameraRig::ParamsBootFwd,
        &CameraRig::ParamsFrontQCuFwd2,     &CameraRig::ParamsUnderbelly };
    for (int i = 0; i < 20; ++i)
    {
        const CameraRig::Params& lrPreset = *lapPresets[i];
        const PresetWords& lrWords = kaPresetWords[i];
        const bool lbMatch =
            Bits(lrPreset.mOffsetFromTarget.x) == lrWords.mauTarget[0]
            && Bits(lrPreset.mOffsetFromTarget.y) == lrWords.mauTarget[1]
            && Bits(lrPreset.mOffsetFromTarget.z) == lrWords.mauTarget[2]
            && Bits(lrPreset.mOffsetFromTarget.w) == 0u
            && Bits(lrPreset.mOffsetFromRotationCentre.x) == lrWords.mauCentre[0]
            && Bits(lrPreset.mOffsetFromRotationCentre.y) == lrWords.mauCentre[1]
            && Bits(lrPreset.mOffsetFromRotationCentre.z) == lrWords.mauCentre[2]
            && Bits(lrPreset.mOffsetFromRotationCentre.w) == 0u
            && Bits(lrPreset.mfFOV) == lrWords.muFov && Bits(lrPreset.mfRoll) == lrWords.muRoll
            && Bits(lrPreset.mfPitch) == lrWords.muPitch && Bits(lrPreset.mfYaw) == lrWords.muYaw
            && static_cast<u8>(lrPreset.mbWidescreenOnly ? 1 : 0) == lrWords.mu8Wide;
        char lacName[160];
        std::snprintf(lacName, sizeof(lacName), "P%02d CameraRig::Params%s is the image's 0x%08X block, word for word",
                      i, lrWords.mpcName, lrWords.muAddress);
        Check(lbMatch, lacName);
    }

    // ---- CameraRig::Construct against the console: bit for bit (the campaign rounding rule) ----
    f32 lfWorst = 0.0f;
    int liLaneMismatches = 0;
    const int liCases = static_cast<int>(sizeof(kaRigCases) / sizeof(kaRigCases[0]));
    for (int i = 0; i < liCases; ++i)
    {
        const RigCase& lrCase = kaRigCases[i];
        CameraRig::Params lParams;
        std::memset(static_cast<void*>(&lParams), 0, sizeof(lParams));
        lParams.mOffsetFromTarget         = V3(lrCase.mauTarget);
        lParams.mOffsetFromRotationCentre = V3(lrCase.mauCentre);
        lParams.mfFOV   = Float(lrCase.muFov);
        lParams.mfRoll  = Float(lrCase.muRoll);
        lParams.mfPitch = Float(lrCase.muPitch);
        lParams.mfYaw   = Float(lrCase.muYaw);
        BrnDirector::Camera::AABBox lBox;
        lBox.mMin = V3(lrCase.mauMin);
        lBox.mMax = V3(lrCase.mauMax);

        CameraRig lRig;
        std::memset(static_cast<void*>(&lRig), 0xCD, sizeof(lRig));   // pool garbage: Construct writes it all
        lRig.Construct(lParams, lBox, lrCase.mu8Reverse != 0);

        const Matrix44Affine& lrOut = lRig.GetRigTransform();
        const f32 lafOut[16] = { lrOut.xAxis.x, lrOut.xAxis.y, lrOut.xAxis.z, lrOut.xAxis.w,
                                 lrOut.yAxis.x, lrOut.yAxis.y, lrOut.yAxis.z, lrOut.yAxis.w,
                                 lrOut.zAxis.x, lrOut.zAxis.y, lrOut.zAxis.z, lrOut.zAxis.w,
                                 lrOut.wAxis.x, lrOut.wAxis.y, lrOut.wAxis.z, lrOut.wAxis.w };
        bool lbMatch = Bits(lRig.GetFOV()) == lrCase.muOutFov;
        for (int k = 0; k < 16; ++k)
        {
            const f32 lfDelta = std::fabs(lafOut[k] - Float(lrCase.mauRows[k]));
            if (Bits(lafOut[k]) != lrCase.mauRows[k])
            {
                lbMatch = false;
                ++liLaneMismatches;
            }
            if (lfDelta > lfWorst)
                lfWorst = lfDelta;
        }
        char lacName[160];
        std::snprintf(lacName, sizeof(lacName), "R%03d Construct(%s) == the console's transform and FOV, bit for bit",
                      i, lrCase.mpcLabel);
        Check(lbMatch, lacName);
    }
    std::printf("transform lanes off the console: %d; worst delta %g\n", liLaneMismatches,
                static_cast<double>(lfWorst));

    std::printf("FxDirector2CameraRig: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
