#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"             // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsBitArray.h"     // CgsContainers::BitArray<N>

// BrnPhysics::Vehicle::ArticulatedJointPool -- the fixed pool of 10 articulation joints plus its
// two use/broken bit-masks and the four swing/twist limit parameters. Member names/order/types
// verbatim from the DecFIGS DWARF (BrnArticulatedJointPool.h:145-153). Reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// This is the coherent home for the whole class: Construct() keeps its original by-offset stores
// (the DWARF member layout lands the two BitArrays at +800/+808 and the four floats at
// +816/+820/+824/+828), and GetJoint / IsJointInUse are bodied here against the named members.

namespace BrnPhysics::Vehicle
{
class ArticulatedJoint
{
public:
    int Construct();

private:
    u8 mPad0[80];   // sizeof-compatible with the real BrnArticulatedJoint.h layout (0x48 -> 0x50)
};

class ArticulatedJointPool
{
public:
    static const s32 KI_NUM_JOINTS = 10;   // maJoints capacity
    static const u32 KU_NUM_JOINTS = 10u;  // BitArray<10> NUMBITS

    int               Construct();
    ArticulatedJoint* GetJoint(s32 liJointIndex);           // @0x825C2B40
    bool              IsJointInUse(s32 liJointIndex) const;  // @0x825C29C8

private:
    ArticulatedJoint                       maJoints[KI_NUM_JOINTS];   // @0    (stride 80)
    CgsContainers::BitArray<KU_NUM_JOINTS> mUsedJoints;               // @800
    CgsContainers::BitArray<KU_NUM_JOINTS> mJointsBrokenThisFrame;    // @808
    float                                  mfSwingAngleDegrees;       // @816
    float                                  mfMaxSwingVelocityDegrees; // @820
    float                                  mfTwistAngleDegrees;       // @824
    float                                  mfMaxTwistVelocityDegrees; // @828
};

int ArticulatedJointPool::Construct()
{
    // The two masks cleared @800/@808 (BitArray<10>::UnSetAll() emits the same `std 0` as the
    // prior mu64=0), then each joint Construct()ed, then the four limit floats keep their VALUES
    // by offset (@816=30, @820=10, @824=15, @828=10 -- the asm reuses the same loaded register/
    // value for @820 and @828).
    mUsedJoints.UnSetAll();
    mJointsBrokenThisFrame.UnSetAll();

    int liResult = 0;
    for (ArticulatedJoint& lJoint : maJoints)
        liResult = lJoint.Construct();

    mfSwingAngleDegrees       = 30.0f;   // @816
    mfMaxSwingVelocityDegrees = 10.0f;   // @820
    mfTwistAngleDegrees       = 15.0f;   // @824
    mfMaxTwistVelocityDegrees = 10.0f;   // @828 (same f0 reg/value reused from @820 in the asm)
    return liResult;
}

// GetJoint @ 0x825C2B40 : bounds-/in-use-asserted accessor; returns &maJoints[liJointIndex]
// (the tail slwi/add/slwi computes this + liJointIndex*80). Both asserts are non-gating tripwires.
ArticulatedJoint* ArticulatedJointPool::GetJoint(s32 liJointIndex)
{
    CGS_ASSERT(liJointIndex >= 0 && liJointIndex < KI_NUM_JOINTS, "Invalid joint index: ");
    CGS_ASSERT(IsJointInUse(liJointIndex), "IsJointInUse( liJointIndex )");
    return &maJoints[liJointIndex];
}

// IsJointInUse @ 0x825C29C8 (const) : reads the u64 mUsedJoints word at +800 and bit-tests
// (1 << (index & 63)). Two bounds asserts precede it (signed [0,10) 'Invalid joint index: ';
// inlined CgsBitArray bound 'invalid index : ') -- both non-gating.
bool ArticulatedJointPool::IsJointInUse(s32 liJointIndex) const
{
    CGS_ASSERT(liJointIndex >= 0 && liJointIndex < KI_NUM_JOINTS, "Invalid joint index: ");
    CGS_ASSERT(static_cast<u32>(liJointIndex) < KU_NUM_JOINTS, "invalid index : ");
    return mUsedJoints.IsBitSet(static_cast<u32>(liJointIndex));
}
}
