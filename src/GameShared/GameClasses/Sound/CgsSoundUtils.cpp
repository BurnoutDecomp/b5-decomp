#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"

#include <cmath>

// CgsSound::Utils::Slope::Slope(const SlopeParams&) @ 0x826A1F70.
//
// Reconstructed from the X360 pseudocode/asm (member access by name -- no offset
// cast). The constructor copies the four range floats out of the source params,
// then nudges mfMaxInput away from mfMinInput by a tiny epsilon when the input span
// is (near) zero, guaranteeing a non-zero denominator for the later GetValue scale
// division.
//
//   0x826A1F70  lfs  f0, flt_82001CC0   ; 0.0f
//   ...         stfs f0, 0/4/8/0xC(r3)  ; zero mParams (the SlopeParams ctor)
//   0x826A1F88  lwz/stw 0(r4)->0(r3)    ; mfMinInput  = params.mfMinInput
//   0x826A1F94  lwz/stw 4(r4)->4(r3)    ; mfMaxInput  = params.mfMaxInput
//   0x826A1FA0  lwz/stw 8(r4)->8(r3)    ; mfMinOutput = params.mfMinOutput
//   0x826A1FAC  lwz/stw 0xC(r4)->0xC(r3); mfMaxOutput = params.mfMaxOutput
//   0x826A1FA4  fsubs f13, mfMaxInput, mfMinInput
//   0x826A1FB0  fabs  f12, f13
//   0x826A1FC0  fcmpu f12, flt_820AD47C ; 0.000001f
//   0x826A1FC4  bgelr                   ; if |span| >= 1e-6 return
//   0x826A1FC8  fadds mfMaxInput += 1e-6

namespace CgsSound
{
namespace Utils
{

// flt_820AD47C -- the degenerate-span epsilon used by this ctor (1e-6f).
static const f32 KF_MIN_INPUT_SPAN = 0.000001f;

Slope::Slope(const SlopeParams& params)
    : mParams(params)
{
    if (std::fabs(mParams.mfMaxInput - mParams.mfMinInput) < KF_MIN_INPUT_SPAN)
    {
        mParams.mfMaxInput += KF_MIN_INPUT_SPAN;
    }
}

}
}
