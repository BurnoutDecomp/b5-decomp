// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnAttachmentTruck.cpp
//
// Compilation home for the BrnDirector::Camera::AttachmentTruck slice this TU owns.
// GetPosition @0x821F3FF8 is defined inline in the header; this .cpp is the translation-unit
// anchor that pulls the header into the compile gate and forces an out-of-line emission of
// GetPosition. The full truck step/ease logic lands with the gyro-cam rig TU.
//
// GetPosition is read by BehaviourGyroCam::Update to fetch the truck's eased world position.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnAttachmentTruck.h"

// The truck Parameters::Serialise<S> visitor drives serialiser S by name; each S supplies the
// scalar f32 field helper. Pull in the two serialisers this TU instantiates the visitor over
// (DebugMenuSerialiser + TextFileWriteSerialiser -- the only two S the X360 emits for this block;
// the read serialiser is NOT emitted here).
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"

namespace BrnDirector
{
namespace Camera
{

// Out-of-line anchor: forces AttachmentTruck::GetPosition to be emitted in this TU.
rw::math::vpu::Vector3 AttachmentTruck_GetPositionAnchor(const AttachmentTruck& lrTruck)
{
    return lrTruck.GetPosition();
}

// ----------------------------------------------------------------------------
// AttachmentTruck::Parameters::Serialise<S> -- the ONE truck field-walk visitor body.
//
// Walks the block's two f32 tunables, handing each to the serialiser S by name. S supplies the
// per-field direction: TextFileWriteSerialiser writes each as a "<label> : <value>\n" line
// (FormatName + fprintf, inlined -> @0x82216470); DebugMenuSerialiser registers each as a
// debug-menu float with a 0.01 adjust step (Process<float> + CgsDev::DebugComponent::SetStep,
// inlined -> @0x82215A38). The field sequence + labels are identical across both instances' asm;
// only S's inlined scalar helper differs, so ONE uniform body serves both.
//
// Field/label order (from the write+debug asm -- Convergence Time Secs first, then Initial offset):
//   +0x04 mfConvergenceTimeSecs  "Convergence Time Secs"
//   +0x00 mfInitialOffsetDist    "Initial offset dist."
// ----------------------------------------------------------------------------
template<class TSerialiser>
void AttachmentTruck::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Convergence Time Secs", mfConvergenceTimeSecs);
    lrSerialiser.Serialise("Initial offset dist.", mfInitialOffsetDist);
}

// Explicit instantiations -- one per serialiser S the X360 emits this block's field-walk over.
// (Only DebugMenuSerialiser @0x82215A38 and TextFileWriteSerialiser @0x82216470 are present in
// this TU; the read serialiser instance is not emitted for the truck block.)
template void AttachmentTruck::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);
template void AttachmentTruck::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);

} // namespace Camera
} // namespace BrnDirector
