#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJointPool.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                // CGS_ASSERT
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManagerIO.h"        // ArticulatedJointCreateBuffer
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h" // VehicleOutputRequestInterface

#include <cstddef>       // offsetof (_AssertLayout)
#include <type_traits>   // std::is_floating_point (_AssertLayout -- see the tamper note there)

// BrnPhysics::Vehicle::ArticulatedJointPool -- the bodies. The class itself moved to
// BrnArticulatedJointPool.h this wave (2026-08-03, task #113); read that header's banner for WHY
// (it was a second, forked declaration of a class BrnPhysicalTrafficManager.h also declared, and
// the two were one symbol to the linker).
//
// ==================================================================================================
// ⭐⭐ ODR FORK #1 RETIRED 2026-08-03 (task #110). This TU used to declare, at namespace scope in
// BrnPhysics::Vehicle, its own
//         class ArticulatedJoint { public: int Construct(); private: u8 mPad0[80]; };
// while the real class lives in BrnArticulatedJoint.h.
//
// ⚠️⚠️ IT WAS NOT MERELY UNTIDY, IT WAS UNSATISFIABLE. The fork declared `int Construct()`; the
// DWARF (BrnArticulatedJoint.h:100) declares `void Construct()`. MSVC folds the return type into
// the mangled name, so the fork's call site demanded
//     ?Construct@ArticulatedJoint@Vehicle@BrnPhysics@@QEAAHXZ
// while any faithful body written against the real header defines
//     ?Construct@ArticulatedJoint@Vehicle@BrnPhysics@@QEAAXXZ
// -- two symbols, and no TU could ever have defined the one this file asked for. That is the
// standing "shadowing redeclaration" failure: invisible to every per-TU compile gate, visible only
// to a LINK (measured then as LNK2019 the moment this TU was first mounted).
//
// ⭐⭐ ODR FORK #2 RETIRED 2026-08-03 (task #113): the pool class ITSELF. See BrnArticulatedJointPool.h.
// The same return-type trap was live here too, in the opposite direction: this file declared
// `int Construct()` purely to agree with the fork in BrnPhysicalTrafficManager.h. It is now `void`,
// per the DWARF (BrnArticulatedJointPool.h:73), and the fork it was agreeing with is gone.
// ==================================================================================================

namespace BrnPhysics
{
namespace Vehicle
{

// ---------------------------------------------------------------------------------------
// ArticulatedJointPool::_AssertLayout   -- the gate that keeps the de-fork honest.
//
// ⚠️ A `sizeof(ArticulatedJoint) == 80` line CANNOT carry this gate and used to be all there was.
// ArticulatedJoint is Matrix44Affine(64) + ArticulatedJointId(8) == 72 and alignas(16) via the
// matrix, so eight bytes of tail padding absorb any small addition and 80 never moves
// (tamper-verified 2026-08-03: adding a u32 did NOT fail that line). ⭐ ON AN OVER-ALIGNED TYPE,
// GATE OFFSETS, NOT SIZE. Every line below is an OFFSET into this class, plus the three
// sizeof(member) lines that catch a member being retyped in place.
// ---------------------------------------------------------------------------------------
void ArticulatedJointPool::_AssertLayout()
{
    // --- the element stride the console's index math uses -------------------------------
    // Construct @0x82600938 walks maJoints with `slwi r11,r31,2 ; add r11,r31,r11 ; slwi r11,r11,4`
    // == i*80, and GetJoint @0x825C2B40 ends with the same sequence.
    static_assert(sizeof(ArticulatedJoint) == 80,
                  "maJoints is strided i*80 by Construct @0x82600938 and GetJoint @0x825C2B40");

    // --- the four member offsets PhysicalTrafficManager::Construct's neighbours pin ------
    // The pool sits at PhysicalTrafficManager+103616 and the next member it writes is at
    // +104448, so the pool is exactly 832 bytes; inside it, ArticulatedJointPool::Construct
    // clears two 64-bit masks and then stores four floats at the offsets below.
    static_assert(offsetof(ArticulatedJointPool, maJoints) == 0,   "maJoints @0");
    static_assert(offsetof(ArticulatedJointPool, mUsedJoints) == 800,
                  "mUsedJoints @800 -- Construct's first `std 0`, and the word IsJointInUse "
                  "@0x825C29C8 bit-tests");
    static_assert(offsetof(ArticulatedJointPool, mJointsBrokenThisFrame) == 808,
                  "mJointsBrokenThisFrame @808 -- Construct's second `std 0`");
    static_assert(offsetof(ArticulatedJointPool, mfSwingAngleDegrees) == 816,       "@816 = 30.0f");
    static_assert(offsetof(ArticulatedJointPool, mfMaxSwingVelocityDegrees) == 820, "@820 = 10.0f");
    static_assert(offsetof(ArticulatedJointPool, mfTwistAngleDegrees) == 824,       "@824 = 15.0f");
    static_assert(offsetof(ArticulatedJointPool, mfMaxTwistVelocityDegrees) == 828, "@828 = 10.0f");

    // --- widths, because an offset gate alone is blind to a member retyped in place ------
    // (the TrafficPhysics tamper test proved this: u8 -> u32 in a 3-byte pad hole is SILENT to
    //  every offset and size assert around it)
    static_assert(sizeof(ArticulatedJointPool::ArticulatedJointBitArray) == 8, "BitArray<10> is one u64 field");
    // ⚠️ ADDED AFTER THE TAMPER TEST FOUND A HOLE. `sizeof(f32) == 4` is a tautology, and with only
    // that line case 6 (`mfSwingAngleDegrees` f32 -> u32) was **SILENT**: same width, same offset,
    // same class size -- and semantically wrong, because the console stores these four with `stfsx`,
    // a FLOAT store. The type check is what actually catches it.
    static_assert(std::is_floating_point<decltype(ArticulatedJointPool::mfSwingAngleDegrees)>::value,
                  "mfSwingAngleDegrees is written by `stfsx` -- a float, not an integer word");
    static_assert(std::is_floating_point<decltype(ArticulatedJointPool::mfMaxSwingVelocityDegrees)>::value,
                  "mfMaxSwingVelocityDegrees is written by `stfsx`");
    static_assert(std::is_floating_point<decltype(ArticulatedJointPool::mfTwistAngleDegrees)>::value,
                  "mfTwistAngleDegrees is written by `stfsx`");
    static_assert(std::is_floating_point<decltype(ArticulatedJointPool::mfMaxTwistVelocityDegrees)>::value,
                  "mfMaxTwistVelocityDegrees is written by `stfsx`");
    static_assert(sizeof(decltype(ArticulatedJointPool::mfSwingAngleDegrees)) == 4,
                  "...and a SINGLE-precision one (stfsx, not stfdx)");

    // --- and the total, which is what BrnPhysicalTrafficManager.h embeds ------------------
    // 10*80 + 8 + 8 + 4*4 == 832, with zero slack: 828 + 4 == 832 and the class is 16-aligned
    // via ArticulatedJoint, 832 % 16 == 0, so there is no tail pad hiding a mistake here.
    static_assert(sizeof(ArticulatedJointPool) == 832,
                  "PhysicalTrafficManager::Construct @0x82636CA8 pins the pool at this+103616 and "
                  "its successor at this+104448");
}

// ---------------------------------------------------------------------------------------
// ArticulatedJointPool::Construct   @ 0x82600938  (144 bytes)
//
// The two masks cleared @800/@808 (BitArray<10>::UnSetAll() emits the same `std 0` as the
// prior mu64=0), then each joint Construct()ed, then the four limit floats keep their VALUES
// by offset (@816=30, @820=10, @824=15, @828=10 -- the asm reuses the same loaded register/
// value for @820 and @828).
//
// ⚠️ RETURN TYPE: `void`, per the DWARF (BrnArticulatedJointPool.h:73). The console ends `blr`
// with r3 still holding the last ArticulatedJoint::Construct's `this`, which is why Hex-Rays
// types it `int`; the value is read by nobody (PhysicalTrafficManager::Construct @0x82636CA8,
// the sole caller, discards it). See the header banner.
// ---------------------------------------------------------------------------------------
void ArticulatedJointPool::Construct()
{
    mUsedJoints.UnSetAll();
    mJointsBrokenThisFrame.UnSetAll();

    for (ArticulatedJoint& lJoint : maJoints)
        lJoint.Construct();

    mfSwingAngleDegrees       = 30.0f;   // @816
    mfMaxSwingVelocityDegrees = 10.0f;   // @820
    mfTwistAngleDegrees       = 15.0f;   // @824
    mfMaxTwistVelocityDegrees = 10.0f;   // @828 (same f0 reg/value reused from @820 in the asm)
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

// ---------------------------------------------------------------------------------------
// ArticulatedJointPool::SendCreateRemoveJointEvents   @ 0x826013C0  (1464 bytes)   NEW 2026-08-03
//
// One frame's batched articulation-joint requests, drained out of the working IO buffer and onto
// the simulation request interface: every slot flagged for creation becomes an AddJoint, every slot
// flagged for removal becomes a RemoveJoint. The asserts are at BrnArticulatedJointPool.cpp:431/432.
//
// ⭐ THIS FUNCTION NEVER DEREFERENCES `this`. The X360 prologue keeps r4/r5 (the two arguments) and
// simply drops r3; nothing in the 1464 bytes reads a pool member. It is a member only because the
// DWARF declares it as one (BrnArticulatedJointPool.h:104) -- which is also why the retired fork in
// BrnPhysicalTrafficManager.h could carry a WRONG `this` layout and still not crash. What the fork
// got wrong was the SIGNATURE: it declared `(const void*, ArticulatedJointCreateBuffer*)` where the
// DWARF has `(VehicleOutputRequestInterface*, const ArticulatedJointCreateBuffer*)`, so the symbol
// its call site demanded could never have been defined by a faithful body.
//
// BOTH LOOPS ARE THE INLINED CgsBitArray FIRST/NEXT-SET-BIT WALK, not hand-rolled index math:
//   lwz/ld field ; branch if zero ; v - ((v-1) & v) ; cntlzd ; 64*field - clz + 63
// is GetFirstNonZeroBit(), and the `(idx & ~63) + 64 clipped to 10` inner scan with the
// CgsBitArray.h:203 "invalid index : " bound assert is GetNextNonZeroBit(idx). The console then
// re-checks `0 <= idx < 10` at each loop head, which is exactly the -1/out-of-range contract those
// two helpers already carry, so the `>= 0` test below is the whole of it.
//
// The two event accessors are the asserted ones (GetCreateJointEvent @0x825C26F0 /
// GetRemoveJointEvent @0x825C2860, both already bodied in BrnPhysicalTrafficManagerIO.cpp; IDA
// truncates their names to `ArticulatedJointCreateBu` / `ArticulatedJointCreat`, which is name
// truncation, not an export hole).
//
// The create side calls the out-of-line VehicleOutputRequestInterface::AddJoint @0x825E7170; the
// remove side is the INLINED RemoveJoint (its assert carries file SharedIO/BrnVehicleOutputInterface.h
// line 730 and it lands on the queue at interface+41840). Both are modelled as the named methods.
// ---------------------------------------------------------------------------------------
void ArticulatedJointPool::SendCreateRemoveJointEvents(
        VehicleOutputRequestInterface* lpRequestInterface,
        const ArticulatedJointCreateBuffer* lpJointWorkingBuffer)
{
    CGS_ASSERT(lpRequestInterface != nullptr, "lpRequestInterface != NULL");        // :431
    CGS_ASSERT(lpJointWorkingBuffer != nullptr, "lpJointWorkingBuffer != NULL");    // :432

    const ArticulatedJointCreateBuffer::CreatedJointBitArray* lpCreatedJoints =
        lpJointWorkingBuffer->GetCreatedJointBitArray();

    for (s32 liJointIndex = lpCreatedJoints->GetFirstNonZeroBit();
         liJointIndex >= 0;
         liJointIndex = lpCreatedJoints->GetNextNonZeroBit(liJointIndex))
    {
        lpRequestInterface->AddJoint(lpJointWorkingBuffer->GetCreateJointEvent(liJointIndex));
    }

    const ArticulatedJointCreateBuffer::RemovedJointBitArray* lpRemovedJoints =
        lpJointWorkingBuffer->GetRemovedJointBitArray();

    for (s32 liJointIndex = lpRemovedJoints->GetFirstNonZeroBit();
         liJointIndex >= 0;
         liJointIndex = lpRemovedJoints->GetNextNonZeroBit(liJointIndex))
    {
        lpRequestInterface->RemoveJoint(lpJointWorkingBuffer->GetRemoveJointEvent(liJointIndex));
    }
}

}
}
