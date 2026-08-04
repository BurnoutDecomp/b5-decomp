#pragma once

#include "types.hpp"                                 // u32
#include "rw/physics/rigidbody.h"                    // rw::physics::RigidBody
#include "vendor/renderware/physics/DriveFrames.hpp"
#include "vendor/renderware/physics/DriveDynamics.hpp"

// ===========================================================================
// rw::physics::Drive -- one entry of the simulation's circular drive list. Layout-identical
// to rw::physics::Joint (see Joint.hpp for the platform-split and parent/child notes, which
// apply here word for word); only the two payload pointers differ.
//
// PROVENANCE
//   NAMES / TYPES / ORDER : DecFIGS DWARF drive.h:224..234.
//   ATTESTED ON X360      : DriveJacobian::Build @0x82BC5590 -- `lwz r27,0(r26)` m_skel,
//     `lwz r28,4(r26)` m_crtl, `lwz r30,0x10(r26)` m_bodyA, `lwz r29,0x14(r26)` m_bodyB,
//     `lwz r3,0x1C(r26)` m_spy; Simulation::DriveBatchBuild @0x82BC6AB8 walks m_right at +0x08
//     and reads both bodies' state through `lwz r11,0x8C(r11)` with NO base add -- which is
//     what proves the four `int32_t` slots hold raw pointers on this platform.
// ===========================================================================

namespace rw
{
namespace physics
{
    struct DriveJacobian;
    class Simulation;

    class Drive
    {
    public:
        DriveFrames*   GetFrames() const   { return m_skel; }
        DriveDynamics* GetDynamics() const { return m_crtl; }
        RigidBody*     GetChild() const    { return m_bodyA; }   // [INFERRED] -- see Joint.hpp
        RigidBody*     GetParent() const   { return m_bodyB; }   // [INFERRED]
        u32            GetTag() const      { return m_tag; }
        u32            GetSpy() const      { return m_spy; }
        Drive*         GetRight() const    { return m_right; }
        Drive*         GetLeft() const     { return m_left; }

    private:
        friend struct DriveJacobian;
        friend class Simulation;

        DriveFrames*   m_skel;     // :224  +0x00
        DriveDynamics* m_crtl;     // :225  +0x04
        Drive*         m_right;    // :226  +0x08
        Drive*         m_left;     // :227  +0x0C
        RigidBody*     m_bodyA;    // :228  +0x10
        RigidBody*     m_bodyB;    // :229  +0x14
        u32            m_tag;      // :230  +0x18
        u32            m_spy;      // :231  +0x1C  RwBool
        int            m_offset;   // :233  +0x20  PS3 SPU addressing; unused on X360
        u32            m_pad[8];   // :234  +0x24
    };
}
}
