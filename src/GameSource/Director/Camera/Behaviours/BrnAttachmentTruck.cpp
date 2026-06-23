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

namespace BrnDirector
{
namespace Camera
{

// Out-of-line anchor: forces AttachmentTruck::GetPosition to be emitted in this TU.
rw::math::vpu::Vector3 AttachmentTruck_GetPositionAnchor(const AttachmentTruck& lrTruck)
{
    return lrTruck.GetPosition();
}

} // namespace Camera
} // namespace BrnDirector
