// Tiny embed/compile check for the rw::collision::CapsuleVolume home: includes
// the header and touches each reconstructed entry point so the gate exercises
// the full TU.
#include "vendor/renderware/collision/CapsuleVolume.hpp"
#include "vendor/renderware/collision/GPInstance.hpp"

namespace
{
void CapsuleVolumeEmbedCheck()
{
    using rw::collision::CapsuleVolume;
    using rw::collision::AABBox;
    using rw::collision::Feature;
    using rw::collision::GPInstance;
    using rw::collision::Vec4;

    CapsuleVolume lVolume;

    rw::math::vpu::Vector3 lDiag = lVolume.GetBBoxDiag();
    (void)lDiag;

    Vec4 lDir{1.0f, 0.0f, 0.0f, 0.0f};
    Feature lFeature;
    lVolume.GetMaximumFeature(1, lDir, lFeature);

    AABBox lBox;
    Vec4 lTransform[4] = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };
    (void)lVolume.GetBBox(lTransform, 1, lBox);
    (void)lVolume.GetBBox(nullptr, 0, lBox);

    GPInstance lInstance;
    (void)lVolume.CreateGPInstance(lInstance, lTransform);
    (void)lVolume.CreateGPInstance(lInstance, nullptr);
}
} // namespace
