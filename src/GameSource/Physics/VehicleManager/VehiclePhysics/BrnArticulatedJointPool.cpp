#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"             // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsBitArray.h"     // CgsContainers::BitArray<N>
// ⭐ 2026-08-03 (task #110): the REAL ArticulatedJoint, not a local opaque re-declaration.
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJoint.h"

// BrnPhysics::Vehicle::ArticulatedJointPool -- the fixed pool of 10 articulation joints plus its
// two use/broken bit-masks and the four swing/twist limit parameters. Member names/order/types
// verbatim from the DecFIGS DWARF (BrnArticulatedJointPool.h:145-153). Reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// This is the coherent home for the whole class: Construct() keeps its original by-offset stores
// (the DWARF member layout lands the two BitArrays at +800/+808 and the four floats at
// +816/+820/+824/+828), and GetJoint / IsJointInUse are bodied here against the named members.

// ==================================================================================================
// ⭐⭐ ODR FORK RETIRED 2026-08-03 (task #110). This TU used to declare, at namespace scope in
// BrnPhysics::Vehicle, its own
//         class ArticulatedJoint { public: int Construct(); private: u8 mPad0[80]; };
// while the real class lives in BrnArticulatedJoint.h -- whose own banner asked for exactly this
// unification ("the two should be unified onto this header by the maintainer").
//
// ⚠️⚠️ IT WAS NOT MERELY UNTIDY, IT WAS UNSATISFIABLE. The fork declared `int Construct()`; the
// DWARF (BrnArticulatedJoint.h:100) declares `void Construct()`. MSVC folds the return type into
// the mangled name, so the fork's call site demanded
//     ?Construct@ArticulatedJoint@Vehicle@BrnPhysics@@QEAAHXZ
// while any faithful body written against the real header defines
//     ?Construct@ArticulatedJoint@Vehicle@BrnPhysics@@QEAAXXZ
// -- two symbols, and no TU could ever have defined the one this file asked for. That is the
// standing "shadowing redeclaration" failure: invisible to every per-TU compile gate, visible only
// to a LINK (measured here as LNK2019 the moment this TU was first mounted).
//
// The de-fork is layout-neutral: the real ArticulatedJoint is Matrix44Affine(64) +
// ArticulatedJointId(8) = 72, and the type is alignas(16) via Matrix44Affine, so sizeof == 80 --
// exactly the stride the console's `slwi/add/slwi` index math uses (i*80) and exactly the size the
// retired opaque asserted. The static_assert below is the gate that keeps it that way.
// ==================================================================================================
namespace BrnPhysics::Vehicle
{
// ⚠️ THIS IS THE WEAK HALF OF THE GATE and is kept only because the STRIDE is this TU's concern.
// A sizeof assert cannot catch a member being displaced: ArticulatedJoint is 64 + 8 == 72 with
// alignas(16), so eight bytes of tail padding absorb any small addition and 80 never moves
// (tamper-verified 2026-08-03 -- adding a u32 did not fail this line). The line that actually
// pins the layout is ArticulatedJoint::_AssertLayout() in BrnArticulatedJoint.cpp, whose
// offsetof(mJointId) == 0x40 does fail on the same tamper. Do not treat this one as coverage.
static_assert(sizeof(ArticulatedJoint) == 80,
              "ArticulatedJointPool::Construct @0x82600938 indexes maJoints with i*80 "
              "(slwi r11,r31,2 ; add r11,r31,r11 ; slwi r11,r11,4), and the embedded pool must "
              "stay 832 bytes for BrnPhysicalTrafficManager.h's +103616..+104448 span");

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

    for (ArticulatedJoint& lJoint : maJoints)
        lJoint.Construct();

    mfSwingAngleDegrees       = 30.0f;   // @816
    mfMaxSwingVelocityDegrees = 10.0f;   // @820
    mfTwistAngleDegrees       = 15.0f;   // @824
    mfMaxTwistVelocityDegrees = 10.0f;   // @828 (same f0 reg/value reused from @820 in the asm)

    // ⚠️ FLAG (task #110): THE CONSOLE RETURNS NOTHING. @0x82600938 ends `blr` with r3 still
    // holding whatever the last ArticulatedJoint::Construct left there (its own `this`), which is
    // why Hex-Rays types both functions `int`. The `int` here is NOT recovered semantics -- it is
    // the shape BrnPhysicalTrafficManager.h's ArticulatedJointPool slice was committed against
    // (that header says so at :376-379), and MSVC encodes the return type in the mangled name, so
    // narrowing it to `void` is part of retiring THAT fork, not this one. This value is read by
    // nobody: the sole caller, PhysicalTrafficManager::Construct @0x82636CA8, discards it.
    return 0;
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
