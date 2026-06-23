#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_ATTACHMENT_TRUCK_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_ATTACHMENT_TRUCK_H

#include "types.hpp"
#include "rw/math/vpu/types.h"                        // rw::math::vpu::Vector3 (the 16-byte position)
#include "GameShared/GameClasses/Core/CgsAssert.h"    // CGS_ASSERT (the !mbFirstFrame guard)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnAttachmentTruck.h
//
// BrnDirector::Camera::AttachmentTruck -- the gyro-cam "attachment truck": a small helper
// that eases a camera attachment point along a world-space track (the truck) frame to frame.
// It carries a cached world position (a Vector3) and a "first frame" guard flag. HOME for the
// AttachmentTruck class slice this TU bodies (the GetPosition accessor).
//
// ----------------------------------------------------------------------------
// The ONLY function homed here is GetPosition @0x821F3FF8: it asserts the truck has been
// stepped at least once (!mbFirstFrame) and copies the cached world position (a single 16-byte
// aligned vector load/store) into the caller-supplied output vector, returning it. Called by
// BehaviourGyroCam::Update. The truck's full step/ease logic lands with the gyro-cam rig TU;
// this header models only the slice GetPosition needs, BY NAME.
//
// Note: the X360 GetPosition assert string is attributed to the gyro-cam behaviour header
// because the truck is an inline member of the gyro-cam rig and GetPosition is emitted from
// that header's inline.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

class AttachmentTruck
{
public:

    // Return (by value) the truck's cached world position. Asserts the truck has been
    // stepped (!mbFirstFrame): reading the position before the first step is a logic error.
    // @0x821F3FF8. The X360 returns the Vector3 BY VALUE (sret): r3 = the hidden return slot
    // (destination), r4 = this (source) -- so this is a by-value getter, NOT an out-param ref
    // (which would put this in r3 / out in r4, inverting the observed register convention).
    //   lbz  r11, 0x40(this)    ; mbFirstFrame
    //   cmplwi r11, 0           ; assert !mbFirstFrame
    //   lvx128 v0, r0, this(r4) ; load 16-byte mPosition (offset 0) from this
    //   stvx128 v0, r0, ret(r3) ; store into the sret return slot
    rw::math::vpu::Vector3 GetPosition() const;

private:

    // FLAG: only the members GetPosition reads are modelled at their asm-attested offsets; the
    //   rest of the truck rig (the ease/step tunables and working state) lands with the gyro-cam
    //   rig TU. The cached world position is the 16-byte aligned vector at +0x00 (the lvx128
    //   source); mbFirstFrame is the byte at +0x40 (the lbz source). A reserved span places the
    //   flag at its attested offset.
    rw::math::vpu::Vector3 mPosition;                  // +0x00  cached world position (lvx128 source)
    u8                     maReserved10[0x40 - 0x10];  // +0x10 .. +0x3F (truck rig state not modelled here)
    u8                     mbFirstFrame;               // +0x40  set until the truck has been stepped once
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::AttachmentTruck::GetPosition @0x821F3FF8
// A single aligned 16-byte vector copy from the cached position into the output vector; not a
// multi-stage VMX pipeline, so reproduced as a plain by-name Vector3 assignment.
// ----------------------------------------------------------------------------
inline rw::math::vpu::Vector3
AttachmentTruck::GetPosition() const
{
    CGS_ASSERT(!mbFirstFrame, "!mbFirstFrame");
    return mPosition;                                  // lvx128 v0,(this=r4); stvx128 v0,(sret=r3)
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_ATTACHMENT_TRUCK_H
