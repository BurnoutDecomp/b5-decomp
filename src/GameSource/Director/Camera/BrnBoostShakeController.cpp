// BrnDirector::BoostShakeController -- the camera boost-shake intensity driver.
// Reconstructed from BURNOUT_X360_ARTIST.XEX @0x8220E548, semantic-parity (not byte-matching).
//
// Bodied here (1 ledger function):
//   BoostShakeController::Update   @0x8220E548
//
// ⛔⛔ THIS TU IS NOT ON THE BUILD LIST, AND IT HAS A REAL CONSOLE CALLER (found 2026-08-02,
// final-camera wave). BehaviourGameplayExternal::Update @0x82240828 calls
// BrnDirector::BoostShakeController::Update directly at 0x822422B0. So the body below is
// the fifth member of this cluster's "body exists, nothing links it" family -- the same
// shape that hid Camera::SetRequestedTimeDilation for months: the link stays green only
// because nothing on the build list reaches it, and it will become an unresolved external
// the moment BehaviourGameplayExternal::Update is transcribed.
// ⭐ MOUNTING IT COSTS NOTHING: this TU includes only its own header, and that header
// includes only types.hpp and <cstddef>, so there is NO cascade -- measured, not assumed.
// Mount it WITH Update (a body with no caller is as dead as no body), not before.
//
// The X360 body is pure scalar FP (fcmpu / fdivs / fsubs / fmuls + four fsel branchless
// clamps); it is reproduced step-for-step below. The two literals the asm pulls from
// rodata are 0.0 (flt_82001CC0, also the divide-by-zero guard constant and the f12
// store-when-zero value) and 1.0 (flt_82001C98) -- the canonical clamp01 pair, which the
// Hex-Rays decode already spells as `1.0 - x`.

#include "GameSource/Director/Camera/BrnBoostShakeController.h"

namespace BrnDirector
{
    namespace
    {
        // PowerPC fsel: result = (lfA >= 0.0f) ? lfB : lfC. The selection compares against
        // +0.0 (so -0.0 selects lfB), matching the hardware the asm relies on.
        inline f32 FSel(f32 lfA, f32 lfB, f32 lfC)
        {
            return (lfA >= 0.0f) ? lfB : lfC;
        }

        // clamp01 the X360 way: low edge via fsel(-x, 0, x) (x<=0 -> 0), then high edge via
        // fsel(1 - x, x, 1) (x>=1 -> 1).
        inline f32 Clamp01(f32 lfX)
        {
            lfX = FSel(-lfX, 0.0f, lfX);
            return FSel(1.0f - lfX, lfX, 1.0f);
        }
    }

    // @0x8220E548.
    BoostShakeOutput* BoostShakeController::Update(BoostShakeOutput* lpOutput,
                                                   const BoostShakeCameraState* lpState,
                                                   const BoostShakeParamsRef* lpParamsRef) const
    {
        const f32 lfDuration = lpState->mfBoostDuration;   // *(state + 0x3D4)

        // Divide-by-zero guard: a zero duration yields no shake (store 0.0, return).
        if (lfDuration == 0.0f)
        {
            lpOutput->mfShakeAmplitude = 0.0f;             // *(output + 0x114) = 0.0
            return lpOutput;
        }

        const BoostShakeParams* lpParams = lpParamsRef->mpParams;   // *(params + 4)
        const f32 lfRampStart = lpParams->mfRampStart;     // *(params + 0x1C)
        const f32 lfRampEnd   = lpParams->mfRampEnd;       // *(params + 0x24)
        const f32 lfSpan      = lfRampEnd - lfRampStart;   // fsubs f9, f13, f11

        // Normalized boost progress, clamped to [0,1].
        f32 lfWeight = lpState->mfBoostElapsed / lfDuration;   // fdivs f0, f10, f0
        lfWeight = Clamp01(lfWeight);

        // Remap into the curve's [rampStart, rampEnd] window, clamp to [0,1].
        lfWeight = (lfWeight - lfRampStart) / lfSpan;          // fsubs/fdivs
        lfWeight = Clamp01(lfWeight);

        lpOutput->mfShakeAmplitude = lfWeight * lpParams->mfAmplitude;   // *(output+0x114) = f0 * *(params+0x20)
        lpOutput->mbShakeFlag =
            static_cast<unsigned char>(lpParams->miStyleFlag);          // stb *(output+0x11C) = *(params+0x18)

        return lpOutput;
    }
}
