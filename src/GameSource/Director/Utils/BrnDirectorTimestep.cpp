// ============================================================================
// GameSource/Director/Utils/BrnDirectorTimestep.cpp
//
// Compilation home for BrnDirector::Timestep. The class and its three methods
// (Get @0x821F2908, GetVecFloat @0x821F2978, Set) are defined inline in the header;
// this .cpp is the translation-unit anchor that pulls the header into the compile
// gate and forces out-of-line emission of Get and GetVecFloat so the bodied
// functions have an emitted home. No additional definitions live here.
// ============================================================================

#include "GameSource/Director/Utils/BrnDirectorTimestep.h"

namespace BrnDirector
{

// Out-of-line anchor: forces Get to be emitted in this TU. The director camera
// behaviours call Get(eType) once per frame to scale their vector math by the
// frame delta; this is the same scalar the inline returns.
f32 Timestep_GetAnchor(const Timestep& lrTimestep, Timestep::EType leType)
{
    return lrTimestep.Get(leType);
}

// Out-of-line anchor: forces GetVecFloat to be emitted in this TU. @0x821F2978
//   bounds-asserts leType, then copies the 16-byte broadcast VecFloat register
//   maVecFloatTimestep[leType] (the asm `lvx128 v0,16*leType(this); stvx128 v0,result`
//   -- a whole 16-byte SIMD-register load/store at element stride 16). The inline
//   `return maVecFloatTimestep[leType];` is that same 16-byte register copy.
// BehaviourSpirallingDeathcam::Update is the caller that scales its vector math by this.
VecFloat Timestep_GetVecFloatAnchor(const Timestep& lrTimestep, Timestep::EType leType)
{
    return lrTimestep.GetVecFloat(leType);
}

} // namespace BrnDirector
