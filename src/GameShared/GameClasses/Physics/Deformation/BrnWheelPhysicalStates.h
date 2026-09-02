#ifndef BRN_PHYSICS_DEFORMATION_BRNWHEELPHYSICALSTATES_H
#define BRN_PHYSICS_DEFORMATION_BRNWHEELPHYSICALSTATES_H
#include "types.hpp"
#include "BrnCommonTypes.h"   // Matrix44Affine, Vector3
// =============================================================================
// BrnWheelPhysicalStates.h  (OWNING HEADER for
//   BrnPhysics::Deformation::WheelPhysicalStates)
//
// Home for the deformation wheel physical-state block that DeformableObject::OutputWheelData
// @0x82608E28 assembles and the two entity-module readbacks consume
// (RaceCarEntityModule::ReadUpdatedActiveRaceCarDataFromPhysics leg L3 and
// TrafficEntityModule::ProcessDeformationData @0x8271DEB0).
//
// LAYOUT -- DWARF (BrnDeformationEvents.h:216-:220 + the enclosing struct), attested by the
// X360 walks on both sides:
//   +0x000  maStates[4]         WheelPhysicalState { Matrix44Affine @+0, Vector3 @+0x40,
//                               Vector3 @+0x50 } -- 96-byte stride (`addi r31, r31, 0x60` in
//                               the producer; `addi r29, r29, 0x60` in the traffic reader,
//                               which copies the four transform rows).
//   +0x180  mabWheelExists[4]   producer live arm `stb 1, -4(r11)` with r11 = block+0x184+w
//                               (attached-wheel arm and detached-wheel arm both write 1);
//                               traffic reader `lbzx r11, r24, r27` with r24 = entry+0x180.
//   +0x184  mabWheelAttached[4] producer live arm `stb 1, 0(r11)`, detached arm `stb 0`;
//                               traffic reader `lbz r11, 0x274(r23+w)` == entry+0x184+w, the
//                               "exists but not attached => fatal" test.
//
// The console sizeof is 0x188 (392) and its copy-assignment @0x825C0A00 copies exactly that
// span (24 quadwords + 8 bytes). On the host the Matrix44Affine member gives the struct
// 16-byte alignment, so sizeof rounds to 400 -- which is ALSO the stride the entity-module
// output interface advances its maWheelStates cursor by (0x190; see
// BrnDeformationOutputInterface.h), so the by-name struct and the interface slot agree.
//
// 2026-09-02 (traffic-deformation wave): promoted from the opaque
// {u8[0x180]; u8[8]} home. The old home hid a producer defect: OutputWheelData's live arm
// wrote the exists byte at +0x180 through a raw offset and never wrote +0x184 (attached),
// which the traffic consumer reads as "wheel torn off" and marks the car fatally crashing.
// =============================================================================
namespace BrnPhysics
{
namespace Deformation
{
struct alignas(16) WheelPhysicalStates
{
    // BrnDeformationEvents.h:216
    struct alignas(16) WheelPhysicalState
    {
        Matrix44Affine mWorldSpaceTransform;        // :218  +0x00
        Vector3        mWorldSpaceVelocity;         // :219  +0x40
        Vector3        mWorldSpaceAngularVelocity;  // :220  +0x50
    };

    static const u32 KU_NUM_WHEELS = 4;

    WheelPhysicalState maStates[KU_NUM_WHEELS];     // +0x000 (stride 0x60)
    bool               mabWheelExists[KU_NUM_WHEELS];    // +0x180
    bool               mabWheelAttached[KU_NUM_WHEELS];  // +0x184

    // WheelPhysicalStates::operator=  @ 0x825C0A00. Field copy of the whole block (24
    // quadwords over maStates, then the 8 flag bytes); returns *this.
    WheelPhysicalStates& operator=(const WheelPhysicalStates& lkrSource);
};
} // namespace Deformation
} // namespace BrnPhysics
#endif // BRN_PHYSICS_DEFORMATION_BRNWHEELPHYSICALSTATES_H
