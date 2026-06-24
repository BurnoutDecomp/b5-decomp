#pragma once

// Slot tables for the X360 CgsPhysics::PhysicsSimulationModule (DWARF
// CgsPhysicsSimulationModule.h). The module owns three fixed-capacity slot
// arrays -- RigidBodyData (200), JointData (36) and DriveData (1) -- each a
// struct-of-arrays whose entries are indexed by slot. This header declares the
// JointData and DriveData slot tables with the DWARF-attested member layout so
// their out-of-line accessors / constructor (CgsPhysicsSimulationModule.cpp)
// have a complete type.
//
// The rw::physics element payloads (Joint / JointFrames / Drive / DriveFrames /
// DriveDynamics) are modelled here only as correctly-sized, alignment-faithful
// opaque spans: every byte offset / array stride is X360-attested (off the slot
// accessors' asm and the DWARF field map), but the elements' interior field
// layout is NOT consumed by the functions homed here, so it is intentionally
// not invented. JointLimits IS modelled field-by-field because JointData's
// constructor (X360 @0x827DB798) default-initialises each maLimits[] entry by
// member.

#include "types.hpp"

namespace CgsPhysics
{
    // ---- rw::physics element payloads -------------------------------------
    // Sizes are X360-attested array strides (DriveData/JointData accessor asm +
    // the DWARF field map): JointFrames 80B, DriveFrames 64B, DriveDynamics 32B.
    // Interior layout not consumed here -> opaque sized spans (declared-only
    // element interiors, flagged).

    // 5 quaternion/vector slots: Quaternion(16)+Vector3(16)+Quaternion(16)+
    // Vector3(16)+Quaternion(16) == 80 bytes (DWARF jointframes.h).
    struct alignas(16) JointFrames { u8 macOpaque[80]; };

    // Quaternion(16)+Vector3(16)+Quaternion(16)+Vector3(16) == 64 bytes
    // (DWARF driveframes.h).
    struct alignas(16) DriveFrames { u8 macOpaque[64]; };

    // 2 x Params{f32 mSpring,mDamping,mStrength; DriveType mType} == 2*16 == 32
    // bytes (DWARF drivedynamics.h).
    struct alignas(16) DriveDynamics { u8 macOpaque[32]; };

    // Opaque handle tables -- single u64 ids / single pointers; sized to the
    // DWARF (JointId/DriveId == uint64_t).
    struct JointId { u64 muId; };
    struct DriveId { u64 muId; };

    namespace rw_physics
    {
        struct Joint;   // opaque rw::physics::Joint (maRWJoints holds Joint*)
        struct Drive;   // opaque rw::physics::Drive (maRWDrives holds Drive*)
    }

    // JointLimits IS modelled field-by-field: JointData's constructor writes its
    // members by name. 64-byte record (DWARF jointlimits.h). The two trailing
    // enum slots default to 0 (SWING_LOCKED / TWIST_LOCKED).
    enum E_SwingType : s32 { E_SWING_LOCKED = 0, E_SWING_CONE = 1, E_SWING_HINGE = 2, E_SWING_AXLE = 3, E_SWING_FREE = 4 };
    enum E_TwistType : s32 { E_TWIST_LOCKED = 0, E_TWIST_ARC = 1, E_TWIST_FREE = 2 };

    struct alignas(16) JointLimits
    {
        f32         mafPprism[4];   // Vector3 mPprism  (@+0x00, 16B incl. pad)
        f32         mafVprism[4];   // Vector3 mVprism  (@+0x10, 16B incl. pad)
        f32         mfVtwist;       // @+0x20
        f32         mfVswing;       // @+0x24
        f32         mfSwinga;       // @+0x28
        f32         mfTwista;       // @+0x2C
        f32         mfSwingc;       // @+0x30
        f32         mfTwistc;       // @+0x34
        E_SwingType meSwingf;       // @+0x38
        E_TwistType meTwistf;       // @+0x3C
    };

    // ---- JointData (DWARF CgsPhysicsSimulationModule.h:132) ----------------
    // knSize == 36. Struct-of-arrays slot table; the accessors index it by slot.
    class JointData
    {
    public:
        static const s32 KI_SIZE = 36;

        // Default-construct every maLimits[] entry (X360 @0x827DB798): a 36-pass
        // loop over &maLimits[0] (== this+0xB40) that, per 64-byte record, zeroes
        // mPprism, mVprism, mfVtwist..mfTwista and the two enum slots, and sets
        // mfSwingc/mfTwistc to 1.0f. No other slot array is touched.
        JointData();

        // Checked slot accessor (X360 @0x8289D168): asserts liIndex < knSize and
        // mabUsedSlot[liIndex], then returns maRWJoints[liIndex].
        rw_physics::Joint* GetJoint(s32 liIndex);

        // Offsets below are X360-ABI (4-byte pointer) byte offsets; on the x64
        // host gate the pointer-array slots widen, but member access is by name.
    private:
        JointFrames        maFrames[36];     // X360 @+0x0000 (stride 80)
        JointLimits        maLimits[36];     // X360 @+0x0B40 (stride 64)
        rw_physics::Joint* maRWJoints[36];   // X360 @+0x1440 (4B ptr)  -> 5184
        JointId            maGameIDs[36];    // X360 @+0x14D0 (stride 8)
        bool               mabUsedSlot[36];  // X360 @+0x15F0            -> 5616
    };

    // ---- DriveData (DWARF CgsPhysicsSimulationModule.h:197) ----------------
    // knSize == 1. Struct-of-arrays slot table; the accessors index it by slot.
    class DriveData
    {
    public:
        static const s32 KI_SIZE = 1;

        // Checked slot accessor (X360 @0x8289D1E8): asserts liIndex < knSize and
        // mabUsedSlot[liIndex], then returns &maFrames[liIndex].
        DriveFrames* GetDriveFrames(s32 liIndex);

        // Checked slot accessor (X360 @0x8289D268): asserts liIndex < knSize and
        // mabUsedSlot[liIndex], then returns &maScaledDynamics[liIndex].
        DriveDynamics* GetScaledDriveDynamics(s32 liIndex);

        // Offsets below are X360-ABI (4-byte pointer) byte offsets; on the x64
        // host gate the pointer-array slot widens, but member access is by name.
    private:
        DriveFrames        maFrames[1];          // X360 @+0x00 (stride 64)
        DriveDynamics      maDynamics[1];        // X360 @+0x40 (stride 32)
        DriveDynamics      maScaledDynamics[1];  // X360 @+0x60 (stride 32) -> 96
        rw_physics::Drive* maRWDrives[1];        // X360 @+0x80 (4B ptr)
        DriveId            maGameIDs[1];         // X360 @+0x88 (8-aligned)
        bool               mabUsedSlot[1];       // X360 @+0x90            -> 144
    };
}
