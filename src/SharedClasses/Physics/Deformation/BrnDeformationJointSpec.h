#pragma once

// ============================================================================
// SharedClasses/Physics/Deformation/BrnDeformationJointSpec.h
//
// BrnPhysics::Deformation::DeformationJointSpec -- the static, streamed
// description of ONE deformation joint that attaches a body part to its parent
// vehicle (the IK "rig" data the physical body part reads when it deforms, bends
// at its hinge, and finally breaks off). It carries the joint frame (position +
// rotation axis + default direction), the plastic/break tuning (max bend angle,
// detach stress threshold), and the joint kind (none / hinge / ball-and-socket).
//
// This is a LAYOUT-FIRST FROZEN HEADER: it declares the full DWARF member layout
// and every method, with NO function bodies (other agents author the .cpp). The
// member order/types are DWARF-authoritative
// (references/DecFIGS/dwarfdump/SharedClasses/Physics/Deformation/BrnIKBodyPartSpec.h,
// the DeformationJointSpec struct @ BrnIKBodyPartSpec.h:90). DeformationJointSpec
// lives in the larger BrnIKBodyPartSpec.h on the X360; this is the minimal slice
// of just the joint spec, carried separately so the physical-body-part family can
// read the joint's plastic/break fields without pulling the whole IK spec.
//
// This is the X360 ARTIST.XEX (big-endian) layout; pointers are 32-bit there but
// members here are pinned BY NAME + SEQUENCE. GROW additively if a future TU needs
// the joint spec's currently-unreferenced fields.
// ============================================================================

#include "types.hpp"           // s32, u8, f32
#include "BrnCommonTypes.h"    // Vector3

namespace BrnPhysics
{
namespace Deformation
{
    // DWARF BrnIKBodyPartSpec.h:37. The kind of joint a deformation body part hangs
    // off: a rigid (no) joint, a single-axis hinge, or a ball-and-socket. The
    // breakable/bendable paths in PhysicalBodyPart branch on this. Enumerator spellings
    // are the DWARF's engine-facing eCamelCase form (kept verbatim so the value set
    // matches the X360 exactly); the E_ project form is given as a comment.
    enum EDeformationJointType
    {
        eNone           = 0,   // E_DEFORMATION_JOINT_TYPE_NONE
        eHinge          = 1,   // E_DEFORMATION_JOINT_TYPE_HINGE
        eBallAndSocket  = 2,   // E_DEFORMATION_JOINT_TYPE_BALL_AND_SOCKET
        eJointTypeCount = 3    // E_DEFORMATION_JOINT_TYPE_COUNT
    };

    // DWARF BrnIKBodyPartSpec.h:90. The streamed joint description for one body part.
    struct DeformationJointSpec
    {
        // ---- members (DWARF-authoritative order + types) ----

        Vector3 mJointPosition;            // BrnIKBodyPartSpec.h:130 -- car-space joint anchor
        Vector3 mJointAxis;                // BrnIKBodyPartSpec.h:131 -- hinge rotation axis
        Vector3 mJointDefaultDirection;    // BrnIKBodyPartSpec.h:132 -- rest direction of the part
        f32     mfMaxJointAngle;           // BrnIKBodyPartSpec.h:133 -- plastic bend limit (radians)
        f32     mfJointDetachThreshold;    // BrnIKBodyPartSpec.h:134 -- stress at which the joint breaks
        EDeformationJointType meJointType; // BrnIKBodyPartSpec.h:135 -- none / hinge / ball-and-socket

        // ---- methods (declarations only; bodies authored elsewhere) ----

        // BrnIKBodyPartSpec.h:94. Zero-initialise the spec.
        void Construct();

        // BrnIKBodyPartSpec.h:97. The joint kind.
        EDeformationJointType GetType() const;

        // BrnIKBodyPartSpec.h:100. The car-space anchor position (mJointPosition).
        Vector3 GetCarSpacePosition() const;

        // BrnIKBodyPartSpec.h:103. The hinge rotation axis (mJointAxis).
        // ⭐ INLINED 2026-08-27 (detach-2 wave): DECLARE-ONLY until its first caller
        // (PhysicalBodyPart::TestJointForBreaking's arm 4a) turned it into an LNK2019. There is no
        // out-of-line X360 emission -- the console reads the member directly, `lvx128 v0, r3, r28`
        // with r28 == 0x10 @0x8260C344, i.e. the inlined accessor over mJointAxis at spec+0x10.
        // Same evidence pattern as GetMaxStress below.
        Vector3 GetRotationAxis() const { return mJointAxis; }

        // BrnIKBodyPartSpec.h:106. The rest/default direction (mJointDefaultDirection).
        Vector3 GetDefaultDirection() const;

        // BrnIKBodyPartSpec.h:109. The maximum plastic bend angle (mfMaxJointAngle).
        f32 GetMaxAngle() const;

        // BrnIKBodyPartSpec.h:112. The stress at which the joint detaches
        // (mfJointDetachThreshold). FLAG: DWARF names this getter GetMaxStress() but
        // it returns the detach-threshold member -- the "limit stress" the body part's
        // joint test compares against. Name kept from DWARF.
        f32 GetMaxStress() const { return mfJointDetachThreshold; }   // (inlined walls leg 4)

        // BrnIKBodyPartSpec.h:115 / :118. Stream fix-up/down (pointer relocation hooks).
        void FixUp(void* lpBase);
        void FixDown(void* lpBase);

        // BrnIKBodyPartSpec.h:121. Translate the joint anchor by an offset.
        void Translate(Vector3 lvOffset);

        // BrnIKBodyPartSpec.h:124. True when the joint can break (meJointType != eNone and
        // the detach threshold is finite).
        bool IsBreakable() const;
    };
}
}
