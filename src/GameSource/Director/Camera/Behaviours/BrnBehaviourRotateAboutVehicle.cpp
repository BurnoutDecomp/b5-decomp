// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourRotateAboutVehicle slice this TU owns.
// The single accessor @0x821FB410 (return this + 0x50) is defined inline in the header; this
// .cpp is the translation-unit anchor that pulls the header into the compile gate and forces an
// out-of-line emission of the accessor. The rest of the behaviour (Construct/Prepare/Update and
// the full rig) lands with its own TU.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.h"

namespace BrnDirector
{
namespace Camera
{

// Out-of-line anchor: forces the +0x50 accessor to be emitted in this TU.
BehaviourRotateAboutVehicle::EmbeddedSubObject*
BehaviourRotateAboutVehicle_GetEmbeddedSubObjectAnchor(BehaviourRotateAboutVehicle& lrBehaviour)
{
    return lrBehaviour.GetEmbeddedSubObject();   // addi r3, r3, 0x50 ; blr
}

} // namespace Camera
} // namespace BrnDirector
