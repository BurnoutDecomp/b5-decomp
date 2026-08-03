// Layout check for the BrnPhysics::Vehicle::RaceCarPhysics OWN-MEMBER BLOCK (X360 +0x13F0..+0x1460).
//
// ⚠️⚠️ WHY THIS TU EXISTS -- READ BEFORE DELETING IT.
// RaceCarPhysics.h now carries a sixteen-member recovered block whose whole claim to being a
// DERIVATION rather than sixteen separate guesses is that the DWARF's member ORDER and the X360
// asm's member OFFSETS meet, with zero slack, on sizeof == 5216 (the asm-literal per-car stride in
// VehicleManager::Construct @0x8263B7C8). A claim like that is worth exactly as much as the gate
// that checks it -- and until this TU existed there was NO gate, because:
//
//   * RaceCarPhysics.cpp is NOT mounted in tools\build\build_game_exe.bat (grep it: zero hits),
//   * RaceCarPhysics_embed_check.cpp is NOT mounted either,
//   * so a static_assert placed in either of them would be compiled by nothing in the shipping
//     build. That is the same hole BrnVehicleManager_layout_check.cpp was created to close for
//     VehicleManager, and the same "VERIFY THE VERIFIER" failure this project has hit before: a
//     green build is not evidence about code the build never compiles.
//
// This TU is mounted, includes the header, and defines the never-called static member the header
// declares. Its body is nothing but static_asserts, which fire at COMPILE time -- so /OPT:REF
// discarding the (uncalled) function afterwards is irrelevant; unlike an LNK2019 witness, this one
// cannot be optimised away before it has done its job. Being a STATIC MEMBER is what gives it the
// access to private members that offsetof needs.
//
// ⭐ WHY THE ASSERTS ARE RELATIVE AND NOT ABSOLUTE. RaceCarPhysics is deliberately not byte-pinned:
// the ~0x13F0 bytes of ExternalPhysicsBody / SimpleVehiclePhysics / VehiclePhysics state ahead of
// the own block are NOT reproduced as padding (project rule -- parity by named members), so
// offsetof(RaceCarPhysics, mCrashNormal) is NOT 0x1440 on the host and must not be asserted to be.
// What IS reproducible, exactly, is every own member's offset RELATIVE to the first of them,
// because every member of the block is a fixed-width scalar or a 16-byte alignas(16) Vector3 --
// no pointer, no vptr, no ResourceHandle anywhere in it. So each assert below is
//
//        offsetof(member) - offsetof(mPropCollisionImpulseSum)  ==  <X360 offset> - <X360 base>
//
// with BOTH right-hand terms written as X360 LITERALS. Never `sizeof(T) - K`: an assert phrased
// against sizeof is self-fulfilling, it re-derives the answer it is checking.
//
// PROVENANCE of every X360 offset: recorded member-by-member in RaceCarPhysics.h's own block
// comment, each traced to a named X360 function and instruction address. Names are the DecFIGS
// DWARF's (references/DecFIGS/dwarfdump/.../RaceCarPhysics.h:198-250, in declaration order).

#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"

#include <cstddef>   // offsetof

namespace BrnPhysics
{
namespace Vehicle
{
    // The X360 seat of the FIRST own member. Everything below is expressed against it, so that the
    // console numbers stay visible in the source instead of being folded into deltas.
    static const std::size_t KU_X360_OWN_BLOCK_BASE = 0x13F0;
    // The X360 sizeof(RaceCarPhysics). NOT derived from the members below -- this is the per-car
    // stride VehicleManager::Construct walks with `addi r29, r29, 0x1460`, and BrnVehicleManager.h
    // already pins the same 5216 on its RaceCarVehicleRecord stand-in.
    static const std::size_t KU_X360_RACE_CAR_PHYSICS_SIZE = 0x1460;

    void RaceCarPhysics::_AssertOwnBlockLayout()
    {
        typedef RaceCarPhysics T;

        // ---- the block's own base. Everything is relative to this, so it is trivially 0 -- the
        //      assert that matters is that it is 16-ALIGNED, because a Vector3 sits on it and the
        //      whole block's internal padding depends on that. (If the compiler ever had to insert
        //      alignment padding BEFORE mPropCollisionImpulseSum that this model does not know
        //      about, the relative offsets would still hold; what would break is the sizeof
        //      closure at the bottom, which is why that one is here too.)
        static_assert(offsetof(T, mPropCollisionImpulseSum) % 16 == 0,
                      "RaceCarPhysics own block must start 16-aligned (a Vector3 sits on it)");
        static_assert(KU_X360_OWN_BLOCK_BASE % 16 == 0,
                      "the X360 seat of mPropCollisionImpulseSum is 16-aligned too");

        // ---- the sixteen members, in DWARF declaration order, each against its X360 seat. ----
        // Both terms of every right-hand side are X360 literals.
        static_assert(offsetof(T, mfTimeSinceTookDownPlayer) - offsetof(T, mPropCollisionImpulseSum)
                          == 0x1400 - 0x13F0,
                      "mfTimeSinceTookDownPlayer @X360 +0x1400 (Update @0x826418E0: lfs/fadds/stfs)");
        static_assert(offsetof(T, mfSlamSteering) - offsetof(T, mPropCollisionImpulseSum)
                          == 0x1404 - 0x13F0,
                      "mfSlamSteering @X360 +0x1404 (VehicleManager::CalculateSlamData @0x825C7678)");
        static_assert(offsetof(T, mfBeachedTime) - offsetof(T, mPropCollisionImpulseSum)
                          == 0x1408 - 0x13F0,
                      "mfBeachedTime @X360 +0x1408 (Update @0x826419C8; AddTractionPoint @0x825FFB0C)");
        static_assert(offsetof(T, mbPlayerCarInShowtime) - offsetof(T, mPropCollisionImpulseSum)
                          == 0x140C - 0x13F0,
                      "mbPlayerCarInShowtime @X360 +0x140C (IsPlayerVehicleActuallyInShowtime @0x827E42B0)");
        static_assert(offsetof(T, mbUsingAftertouch) - offsetof(T, mPropCollisionImpulseSum)
                          == 0x140D - 0x13F0,
                      "mbUsingAftertouch @X360 +0x140D (IsUsingAftertouch @0x825B8C88)");
        static_assert(offsetof(T, mu8StrengthStat) - offsetof(T, mPropCollisionImpulseSum)
                          == 0x140E - 0x13F0,
                      "mu8StrengthStat @X360 +0x140E (PhysicalTrafficVehicle::OnChecked @0x8261E3F0)");
        static_assert(offsetof(T, mInitialCrashVel) - offsetof(T, mPropCollisionImpulseSum)
                          == 0x1410 - 0x13F0,
                      "mInitialCrashVel @X360 +0x1410 (SetCrashing @0x825B8AD0, from this+0x50)");
        static_assert(offsetof(T, mInitialCrashAngVel) - offsetof(T, mPropCollisionImpulseSum)
                          == 0x1420 - 0x13F0,
                      "mInitialCrashAngVel @X360 +0x1420 (SetCrashing @0x825B8ACC, from this+0x60)");
        static_assert(offsetof(T, mfCrashTimer) - offsetof(T, mPropCollisionImpulseSum)
                          == 0x1430 - 0x13F0,
                      "mfCrashTimer @X360 +0x1430 (SetCrashing @0x825B8AC8; the AI crash slow-mo timer)");
        static_assert(offsetof(T, mbAISlowMo) - offsetof(T, mPropCollisionImpulseSum)
                          == 0x1434 - 0x13F0,
                      "mbAISlowMo @X360 +0x1434 (Update @0x82641698/9C/F4)");
        static_assert(offsetof(T, mbWroteIntoRWInSlowMo) - offsetof(T, mPropCollisionImpulseSum)
                          == 0x1435 - 0x13F0,
                      "mbWroteIntoRWInSlowMo @X360 +0x1435 (VehicleManager::GetUpdatedVehicleBodies)");
        static_assert(offsetof(T, mbDeformedBeyondDriveTimeLimitsInCrash)
                          - offsetof(T, mPropCollisionImpulseSum) == 0x1436 - 0x13F0,
                      "mbDeformedBeyondDriveTimeLimitsInCrash @X360 +0x1436 "
                      "(DeformableObject::UpdateDeformedBBox @0x825E0EBC)");
        static_assert(offsetof(T, mCrashNormal) - offsetof(T, mPropCollisionImpulseSum)
                          == 0x1440 - 0x13F0,
                      "mCrashNormal @X360 +0x1440 (GetNormalCausingCrash @0x825B3978)");
        static_assert(offsetof(T, mEntityCausingCrash) - offsetof(T, mPropCollisionImpulseSum)
                          == 0x1450 - 0x13F0,
                      "mEntityCausingCrash @X360 +0x1450 (VehicleManager::SetRaceCarCrashing @0x82635478)");
        static_assert(offsetof(T, mbDebugShowTargetAssist) - offsetof(T, mPropCollisionImpulseSum)
                          == 0x1454 - 0x13F0,
                      "mbDebugShowTargetAssist @X360 +0x1454 (UpdateTargetAssist @0x826202F4)");

        // ---- ⭐⭐ THE CLOSURE. This is the assert the whole recovery rests on.
        // The DWARF gave the member ORDER and the asm gave the OFFSETS; neither knows about the
        // stride. Lay the block out and its last data byte is a bool at +0x1454, so sizeof must
        // round up to the 16-byte alignment the four Vector3s force -- 0x1455 -> 0x1460 == 5216 --
        // which is the INDEPENDENTLY attested per-car stride. If a member is ever added, removed,
        // widened or re-ordered inside this block, this fires even when the relative asserts above
        // still pass (they only pin members against each other; this pins the END of the object).
        //
        // Phrased as (sizeof - offsetof(first own member)) so it stays valid while the ~0x13F0
        // bytes of base state ahead of the block remain unreproduced.
        static_assert(sizeof(T) - offsetof(T, mPropCollisionImpulseSum)
                          == KU_X360_RACE_CAR_PHYSICS_SIZE - KU_X360_OWN_BLOCK_BASE,
                      "the RaceCarPhysics own block must span exactly 0x1460-0x13F0 == 112 bytes: "
                      "the DWARF member order and the asm offsets close on the 5216 per-car stride");

        // ---- the two ingredients of the closure, pinned separately so a failure is diagnosable.
        static_assert(sizeof(Vector3) == 16 && alignof(Vector3) == 16,
                      "the block's four Vector3s are what force the class alignment to 16");
        static_assert(sizeof(EntityId) == 4,
                      "mEntityCausingCrash is a 4-byte packed handle, so mbDebugShowTargetAssist "
                      "lands at +0x1454 and the class ends at +0x1455 before padding");
        static_assert(alignof(T) == 16,
                      "class alignment 16 is what rounds the last data byte (+0x1455) up to 0x1460");
    }
}
}
