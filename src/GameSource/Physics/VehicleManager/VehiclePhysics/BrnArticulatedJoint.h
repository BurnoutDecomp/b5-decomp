#pragma once

// ============================================================================
// BrnPhysics::Vehicle::ArticulatedJoint / ArticulatedJointId
//   GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJoint.h
//   (DWARF home: BrnArticulatedJoint.h:48 (ArticulatedJointId), :96 (ArticulatedJoint))
//
// The trailer/cab articulation joint: a parent->joint transform plus a packed
// joint identity. Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// LAYOUT (X360 asm authoritative -- from ArticulatedJoint::Set @0x825D7BC0):
//   @0x00  Matrix44Affine     mParentToJointTransform   (4 rows, 4x lvx128/stvx128 @ +0/+0x10/+0x20/+0x30)
//   @0x40  ArticulatedJointId mJointId                  (u64; `std r26, 0x40(r28)`)
//   sizeof == 0x48 -> rounds to 0x50 (Matrix44Affine is alignas(16))
//
// FLAG: the sibling TU BrnArticulatedJointPool.cpp declares a *local*, opaque
// `ArticulatedJoint { u8 mPad0[80]; int Construct(); }` (no shared header). That
// 80-byte opaque is sizeof-compatible with this real layout (0x48 padded to 0x50);
// the two should be unified onto this header by the maintainer. This TU does not
// touch the pool's local declaration (it bodies only ArticulatedJoint::Set, a
// distinct external symbol).
//
// ArticulatedJointId is a 64-bit packed handle (DWARF: ArticulatedJointId : JointId,
// GetRawId() const -> uint64_t). Only the raw-u64 storage is load-bearing for Set
// (the X360 stores the whole 8-byte register at +0x40); the bit-field accessors
// (GetCabEntityId / GetCabVehicleIndex / ...) are declared for the type vocabulary
// and bodied in their own TUs.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                           // EntityId
#include "rw/math/vpu/types.h"                        // rw::math::vpu::Matrix44Affine
// KU_ENTITYTYPE_TRAFFIC_VEHICLE moved OUT of this header and into
// BrnVehicleConstants.h. It was defined identically here and in BrnPhysicalTrafficManager.h, and
// the ArticulatedJointPool de-fork made those two headers meet for the first time -- turning a
// duplicate that had never mattered into a hard C2374. See that header for the note.
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"  // KU_ENTITYTYPE_TRAFFIC_VEHICLE

namespace BrnPhysics
{
namespace Vehicle
{
    // The invalid-EntityId sentinel (X360 dword_82F2A3A4). Local constant mirroring the
    // same-named value in CgsEntityId.h, which declares it as a PRIVATE static of
    // CgsSceneManager::EntityId and therefore cannot expose it to this TU; the same mirroring
    // is already committed at BrnPhysicalTrafficManager.cpp:239, which spells it as a literal.
    const u32 KU_INVALID_ENTITY_ID = 0xFFFFFFFFu;

    // DWARF BrnArticulatedJoint.h:40 -- the base packed-id handle. Minimal: only the
    // raw u64 storage is needed here; the engine JointId base supplies the bit layout
    // in its own home. Modelled as a u64-carrying base so ArticulatedJointId is a u64.
    struct JointId
    {
        u64 mu64RawId;
    };

    // DWARF BrnArticulatedJoint.h:48 -- ArticulatedJointId : JointId.
    struct ArticulatedJointId : public JointId
    {
        void               Construct();                                   // :52
        void               Set(u16 luVehicleIndex, EntityId lCab, EntityId lTrailer); // :58
        void               Set(JointId lBase);                            // :62
        u64                GetRawId() const;                              // :65
        JointId            GetBaseJointID() const;                        // :68
        EntityId           GetCabEntityId() const;                        // :74
        u16                GetCabVehicleIndex() const;                    // :77
        u16                GetTrailerVehicleIndex() const;                // :80
        u16                GetJointPoolIndex() const;                     // :83
    };

    // DWARF BrnArticulatedJoint.h:96.
    struct ArticulatedJoint
    {
        void                            Construct();                      // :100
        // @0x825D7BC0 -- validate the transform (RwMathVPU::IsValid) then store both
        // the parent->joint transform (4 rows) and the joint id (u64).
        void                            Set(const rw::math::vpu::Matrix44Affine& lrParentToJointTransform,
                                            ArticulatedJointId lJointId);  // :105
        rw::math::vpu::Matrix44Affine   GetParentToJointTransform() const; // :108
        ArticulatedJointId              GetJointId() const;               // :111

        // Never called; bodied in BrnArticulatedJoint.cpp (a MOUNTED TU) and nothing but
        // static_asserts. Static so it can see the private block through offsetof.
        static void _AssertLayout();

    private:
        rw::math::vpu::Matrix44Affine   mParentToJointTransform;          // +0x00  :115
        ArticulatedJointId              mJointId;                         // +0x40  :116
    };
}
}
