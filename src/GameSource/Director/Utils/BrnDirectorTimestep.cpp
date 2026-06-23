// ============================================================================
// GameSource/Director/Utils/BrnDirectorTimestep.cpp
//
// Compilation home for BrnDirector::Timestep. The class and its three methods
// (Get @0x821F2908, GetVecFloat, Set) are defined inline in the header; this .cpp
// is the translation-unit anchor that pulls the header into the compile gate and
// forces an out-of-line instantiation of Get so the bodied function has an emitted
// home. No additional definitions live here.
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

} // namespace BrnDirector
