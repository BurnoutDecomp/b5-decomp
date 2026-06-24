// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourAftertouchCrash slice this TU owns.
// SetParameters @0x821F4378 and Get* @0x821FA8F8 are defined inline in the header; this .cpp
// is the translation-unit anchor that pulls the header into the compile gate and forces
// out-of-line emission of both. The rest of the behaviour (Construct/Prepare/Update and the
// full rig) lands with its own TU.
//
// SetParameters is adopted by the crash-mode / takedown arbitrator states (Prepare) and the
// arbitrator testbed when they install an aftertouch-crash parameter block; Get* exposes the
// behaviour's embedded sub-object at +0x60 (gated by the +0x3C2 flag).
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h"

namespace BrnDirector
{
namespace Camera
{

// Out-of-line anchor: forces SetParameters to be emitted in this TU.
void BehaviourAftertouchCrash_SetParametersAnchor(
    BehaviourAftertouchCrash& lrBehaviour,
    const BehaviourAftertouchCrash::Parameters* lpParameters)
{
    lrBehaviour.SetParameters(lpParameters);
}

// Out-of-line anchor: forces Get* to be emitted in this TU.
BehaviourAftertouchCrash::GettableSubObject* BehaviourAftertouchCrash_GetAnchor(
    BehaviourAftertouchCrash& lrBehaviour)
{
    return lrBehaviour.Get();
}

} // namespace Camera
} // namespace BrnDirector
