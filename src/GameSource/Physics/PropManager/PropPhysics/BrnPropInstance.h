// GameSource/Physics/PropManager/PropPhysics/BrnPropInstance.h
#pragma once

// BrnPhysics::Props::PropInstance -- a single physical prop instance owned by the
// PropManager. This header is authored from the DecFIGS DWARF layout for this exact
// source path (references/DecFIGS/dwarfdump/GameSource/Physics/PropManager/PropPhysics/
// BrnPropInstance.h), with every member OFFSET pinned against the X360 ARTIST asm of the
// out-of-line accessors in this batch:
//   GetJointIndex      @ 0x825B96B0  reads u8 @ +0x6C  (mu8JointIndex)
//   SetAngularVelocity @ 0x825DE5F8  stores 16B @ +0x50 (mAngularVelocity)
//   SetTransform       @ 0x825DE370  loads/stores four 16B rows @ +0x00..+0x30 (mWorldTransform)
//
// LAYOUT (DWARF member order + asm-attested offsets):
//   +0x00  Matrix44Affine mWorldTransform  (64B: four Vector3 rows, alignas(16))
//   +0x40  Vector3        mLinearVelocity  (16B)
//   +0x50  Vector3        mAngularVelocity (16B)
//   +0x60  PropEntityID   mEntityId        (4B, one packed EntityId word)
//   +0x64  uint32_t       muTypeId
//   +0x68  uint32_t       muInstanceId
//   +0x6C  uint8_t        mu8JointIndex    (KU_NOT_JOINTED == 255 => not jointed)
//   +0x6D  bool           mbIsStatic
//   +0x6E  uint8_t        muMovementState  (EPropMovementState)
//   +0x6F  uint8_t        mu8Flags         (KU_ADDED_THIS_FRAME_FLAG / KU_HAS_EXTRA_COM_OFFSET_FLAG)
//
// Only the three accessors reconstructed in this batch are DEFINED (in the sibling .cpp);
// the remaining DWARF-declared trivial accessors are declared for shape but left for a
// later batch (their bodies are inline field pokes not yet grounded against asm here).

#include "types.hpp"                                     // u8/u32/s32 primitives
#include "BrnCommonTypes.h"                              // Vector3, Matrix44Affine
#include "SharedClasses/Physics/Props/BrnPropEntityID.h" // BrnWorld::PropEntityID
#include <cstddef>                                       // offsetof

namespace BrnPhysics
{
namespace Props
{
    // BrnPropInstance.h:37 (DWARF) -- movement state stored in muMovementState.
    enum EPropMovementState
    {
        E_PROP_MOVESTATE_STATIONARY = 0,
        E_PROP_MOVESTATE_JUST_MOVED = 1,
        E_PROP_MOVESTATE_MOVING     = 2,
    };

    // The prop-entity handle type is BrnWorld::PropEntityID (SharedClasses home).
    typedef BrnWorld::PropEntityID PropEntityID;

    struct PropInstance
    {
        // --- file-scope constants (DWARF BrnPropInstance.h:151-153) ---
        static const uint8_t KU_NOT_JOINTED             = 255; // sentinel: no joint
        static const uint8_t KU_ADDED_THIS_FRAME_FLAG   = 1;   // mu8Flags bit 0
        static const uint8_t KU_HAS_EXTRA_COM_OFFSET_FLAG = 2; // mu8Flags bit 1

        // --- accessors reconstructed in this batch (bodies in BrnPropInstance.cpp) ---

        // 0x825DE370 -- validate + store the world transform (all four rows).
        void SetTransform(Matrix44Affine lTransform);

        // 0x825DE5F8 -- validate + store the angular velocity.
        void SetAngularVelocity(Vector3 lAngularVelocity);

        // 0x825B96B0 -- assert IsJointed() then return the joint index.
        int32_t GetJointIndex() const;

        // Inline tripwire predicate used by GetJointIndex (DWARF :131). The X360 folds
        // this into the caller as (mu8JointIndex != KU_NOT_JOINTED).
        bool IsJointed() const { return mu8JointIndex != KU_NOT_JOINTED; }

        // ------------------------------------------------------------------
        // Members (DWARF order; offsets asm-pinned -- see banner).
        // ------------------------------------------------------------------
        Matrix44Affine mWorldTransform;   // +0x00  (64B)
        Vector3        mLinearVelocity;   // +0x40  (16B)
        Vector3        mAngularVelocity;  // +0x50  (16B)
        PropEntityID   mEntityId;         // +0x60  (4B)
        uint32_t       muTypeId;          // +0x64
        uint32_t       muInstanceId;      // +0x68
        uint8_t        mu8JointIndex;     // +0x6C
        bool           mbIsStatic;        // +0x6D
        uint8_t        muMovementState;   // +0x6E  (EPropMovementState)
        uint8_t        mu8Flags;          // +0x6F
    };

    // Pointer-free sub-structs / offset pins. mWorldTransform is the first member after
    // the (vtable-less) object head, and mu8JointIndex..mu8Flags are POD bytes with no
    // pointer widening on the 64-bit host, so these offsets are host-stable and assertable.
    static_assert(offsetof(PropInstance, mWorldTransform)  == 0x00, "mWorldTransform @0x00");
    static_assert(offsetof(PropInstance, mLinearVelocity)  == 0x40, "mLinearVelocity @0x40");
    static_assert(offsetof(PropInstance, mAngularVelocity) == 0x50, "mAngularVelocity @0x50");
    static_assert(offsetof(PropInstance, mu8JointIndex)    == 0x6C, "mu8JointIndex @0x6C");
    static_assert(offsetof(PropInstance, mbIsStatic)       == 0x6D, "mbIsStatic @0x6D");
    static_assert(offsetof(PropInstance, muMovementState)  == 0x6E, "muMovementState @0x6E");
    static_assert(offsetof(PropInstance, mu8Flags)         == 0x6F, "mu8Flags @0x6F");
}
}
