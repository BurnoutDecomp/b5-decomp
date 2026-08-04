#pragma once

#include "types.hpp"                               // u32
#include "rw/physics/rigidbody.h"                  // rw::physics::RigidBody
#include "vendor/renderware/physics/JointFrames.hpp"
#include "vendor/renderware/physics/JointLimits.hpp"

// ===========================================================================
// rw::physics::Joint -- one entry of the simulation's circular joint list. It names the two
// bodies the constraint spans, the frame block that positions it on each of them, and the
// limit block that says how far it may travel.
//
// PROVENANCE
//   NAMES / TYPES / ORDER : DecFIGS DWARF joint.h:218..228 (rw::physics::Drive at
//     drive.h:224..234 is the SAME layout with DriveFrames/DriveDynamics in place of
//     JointFrames/JointLimits).
//   ATTESTED ON X360      : JointJacobian::Build @0x82BC42E8 prologue --
//     `lwz r26,0(r27)` m_skel, `lwz r28,4(r27)` m_limit, `lwz r29,0x10(r27)` m_bodyA,
//     `lwz r30,0x14(r27)` m_bodyB, `lwz r9,0x1C(r27)` m_spy; and
//     Simulation::JointBatchBuild @0x82BC6A30 walks m_right at +0x08.
//
// ⚠️⚠️ PLATFORM SPLIT, AND THE PC MUST FOLLOW X360. The DWARF types m_right / m_left /
// m_bodyA / m_bodyB as `int32_t` -- they are byte OFFSETS from the simulation's node pool on
// PS3, which is what AddressFromOffset() / OffsetFromAddress() and the SPU DriveRefs
// parameter exist for. On X360 they are RAW POINTERS: DriveBatchBuild does
// `lwz r11,0x14(r30)` and then `lwz r11,0x8C(r11)` with no base add anywhere. Modelled as
// pointers here, which is also what the x64 builds do.
//
// [INFERRED] the parent/child mapping. The DWARF exposes GetParent()/GetChild() but does not
// say which member each returns. The pairing below follows the frames: JointFrames names its
// FIRST quaternion mQuatA and its accessor GetChildAngularFrame(), and Build composes
// qA' = m_bodyA->mQuat (x) frames.mQuatA -- so m_bodyA is the CHILD. Flagged rather than
// asserted; nothing in the reconstruction depends on it (the builders use m_bodyA/m_bodyB).
// ===========================================================================

namespace rw
{
namespace physics
{
    struct JointJacobian;
    class Simulation;

    class Joint
    {
    public:
        JointFrames* GetFrames() const { return m_skel; }
        JointLimits* GetLimits() const { return m_limit; }
        RigidBody*   GetChild() const  { return m_bodyA; }   // [INFERRED] -- see the banner
        RigidBody*   GetParent() const { return m_bodyB; }   // [INFERRED]
        u32          GetTag() const    { return m_tag; }
        u32          GetSpy() const    { return m_spy; }
        Joint*       GetRight() const  { return m_right; }
        Joint*       GetLeft() const   { return m_left; }

        // Both writes are X360-attested inside PhysicsSimulationModule::ProcessAddJointQueue,
        // immediately after Simulation::AddJoint returns: `stw r28, 0x18(r3)` (the JointData
        // slot index, mirroring RigidBody::SetTag's role) and `stw r11, 0x1C(r3)` at
        // 0x828A4974/0x828A4978, the latter fed by `lbz 0xB0(event)` == InAddJoint::mbSpy.
        // ProcessSetJointSpyQueue @0x8289F950 writes m_spy the same way.
        // [INFERRED NAMES] -- the console inlines both; the spelling is taken from RigidBody's
        // attested pair, exactly as Drive.hpp's siblings are.
        //
        // ⚠️ SetSpy HERE IS A WHOLE-WORD STORE, unlike RigidBody::SetSpy. m_spy is its own u32
        // slot, so the console emits a plain `stw`; RigidBody's flag shares a word with its
        // state and therefore compiles to `ori 8` / `clrlwi ...,29`. Copying that shape into
        // this class would clobber three unrelated bits.
        void SetTag(u32 luTag)   { m_tag = luTag; }                     // +0x18
        void SetSpy(bool lbSpy)  { m_spy = static_cast<u32>(lbSpy); }   // +0x1C

    private:
        friend struct JointJacobian;
        friend class Simulation;

        JointFrames* m_skel;     // :218  +0x00
        JointLimits* m_limit;    // :219  +0x04
        Joint*       m_right;    // :220  +0x08  the circular `next` BatchBuild walks
        Joint*       m_left;     // :221  +0x0C
        RigidBody*   m_bodyA;    // :222  +0x10
        RigidBody*   m_bodyB;    // :223  +0x14
        u32          m_tag;      // :224  +0x18
        u32          m_spy;      // :225  +0x1C  RwBool -- stored raw into the jacobian's +0x4C
        int          m_offset;   // :227  +0x20  PS3 SPU addressing; unused on X360
        u32          m_pad[8];   // :228  +0x24
    };
}
}
