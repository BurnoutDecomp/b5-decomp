#ifndef GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_POSITION_LAG_H
#define GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_POSITION_LAG_H

#include "types.hpp"
#include "rw/math/vpu/types.h"                        // rw::math::vpu::Vector3 / Matrix44Affine
#include "GameShared/GameClasses/Core/CgsAssert.h"    // CGS_ASSERT (the Update mbConstructed assert)

// ============================================================================
// GameSource/Director/Camera/Utils/BrnPositionLag.h
//
// BrnDirector::Camera::Utils::PositionLag -- a per-axis, frame-rate-aware position smoother a
// camera rig drops in to make the camera "lag" behind a target transform. It remembers the
// target's last local offset and last world position, and each frame eases the new offset
// toward the previous one with independent X/Y/Z response rates and an overall smoothing
// factor, writing the lagged result back into the transform's translation. HOME for the
// PositionLag class and its Parameters.
//
// ----------------------------------------------------------------------------
// LAYOUT (pinned from BrnDirector::Camera::Utils::PositionLag::Update @0x821F8F08):
//
//   PositionLag:
//     +0x00  Vector3 mLastLocalOffset      (the eased local offset carried frame to frame)
//     +0x10  Vector3 mLastWorldPosition    (the target's world position last frame)
//     +0x20  bool    mbFirstFrame          (seeds the carried state on the first Update)
//     +0x21  bool    mbConstructed         (Construct sets it; Update asserts it)
//
//   PositionLag::Parameters:
//     +0x00  u32 muVersion                 (serialised version tag)
//     +0x04  f32 mfXResponse               (per-axis ease rates: how fast the lag closes)
//     +0x08  f32 mfYResponse
//     +0x0C  f32 mfZResponse
//     +0x10  f32 mfSmoothing               (overall blend, used as 1 - smoothing per frame)
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

class PositionLag
{
public:

    // The tunable response/smoothing block (one per rig, serialised with a version tag).
    class Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` (camera-tunings TextFile{Read,Write}Serialiser).
        // Per-instance body is a separate TU.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        // Reset to defaults. Body in BrnPositionLag.cpp.
        void Construct();

        u32 muVersion;     // +0x00
        f32 mfXResponse;   // +0x04
        f32 mfYResponse;   // +0x08
        f32 mfZResponse;   // +0x0C
        f32 mfSmoothing;   // +0x10
    };

    // Reset the carried state (first-frame + constructed flags). Body in BrnPositionLag.cpp.
    void Construct();

    // Ease the transform's translation toward its lagged position this frame. @0x821F8F08.
    // The hand-vectorised VMX cascade is reconstructed in the .cpp as the rw::math::vpu
    // matrix/vector ops it computes (inverse-affine -> TransformPoint -> per-axis Lerp ->
    // response-scale -> TransformVector -> Pos accumulate), verified lane-for-lane vs the asm.
    void Update(const Parameters& lrParameters, f32 lfTimestep, rw::math::vpu::Matrix44Affine& lrTransform);

private:

    rw::math::vpu::Vector3 mLastLocalOffset;    // +0x00
    rw::math::vpu::Vector3 mLastWorldPosition;  // +0x10
    bool                   mbFirstFrame;        // +0x20
    bool                   mbConstructed;       // +0x21
};

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_UTILS_BRN_POSITION_LAG_H
