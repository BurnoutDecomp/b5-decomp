#pragma once

// ============================================================================
// BrnPhysics::Vehicle::ArticulatedJointPool
//   GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJointPool.h
//   (DWARF home BrnArticulatedJointPool.h:69; members :145-153)
//
// The fixed pool of 10 articulation joints (cab<->trailer) plus its two use/broken bit-masks
// and the four swing/twist limit parameters. Reconstructed from BURNOUT_X360_ARTIST.XEX +
// the DecFIGS DWARF.
//
// ============================================================================
// WHY THIS HEADER EXISTS (2026-08-03, task #113). Until this wave the class was declared
// INSIDE BrnArticulatedJointPool.cpp, so it had no home a second TU could include -- and
// BrnPhysicalTrafficManager.h therefore carried its OWN
//     struct ArticulatedJointPool { int Construct(); void SendCreateRemoveJointEvents(const void*,
//                                   ArticulatedJointCreateBuffer*); u8 mOpaque[832]; };
// at namespace scope in BrnPhysics::Vehicle, and embedded THAT by value.
//
// THAT FORK WAS ALREADY LINKING SILENTLY, WHICH IS THE HAZARD RATHER THAN THE GOOD NEWS.
// The previous wave measured `ArticulatedJointPool::Construct` as RESOLVED when
// BrnPhysicalTrafficManager.cpp was trial-mounted -- resolved against the real class's body in
// BrnArticulatedJointPool.cpp, while the call site was the 832-byte opaque slice. The mangled
// name `?Construct@ArticulatedJointPool@Vehicle@BrnPhysics@@QEAA?A?@Z` encodes neither the
// class-key (the slice said `struct`, the .cpp said `class`) nor the member layout, so the two
// definitions were ONE symbol to the linker. It compiled, it linked, and any body that touched a
// member would have written host-laid-out fields into console-strided storage.
// Identical to the TrafficPhysics trap retired in `1d114be6` / `7843135d`.
//
// THE DE-FORK IS LAYOUT-NEUTRAL, MEASURED NOT ASSUMED. Every member is pointer-free:
//     10 * sizeof(ArticulatedJoint)==80  +  2 * BitArray<10>==8  +  4 * f32  ==  800 + 16 + 16 == 832
// on BOTH targets -- exactly the 832 the retired opaque asserted and exactly the span
// PhysicalTrafficManager::Construct @0x82636CA8 pins (`ArticulatedJointPool::Construct(this+103616)`
// then `stfsx` at this+104448; 104448-103616 == 832). So unlike the TrafficPhysics fold, this one
// moves NOTHING in PhysicalTrafficManager's own layout.
//
// `Construct` IS NARROWED int -> void HERE, and that is part of the de-fork, not a tidy-up.
// The DWARF (BrnArticulatedJointPool.h:73) declares `void Construct()`. The console @0x82600938
// ends `blr` with r3 still holding whatever the last ArticulatedJoint::Construct left there (its
// own `this`), which is why Hex-Rays types it `int` -- a return value NOBODY reads (its sole
// caller, PhysicalTrafficManager::Construct, discards it). The `int` in the retired .cpp
// declaration existed only to agree with the fork's `int Construct();`, because MSVC folds the
// return type into the mangled name. With the fork gone there is nothing left to agree with, and
// the DWARF shape wins. This is the exact mistake that made the ArticulatedJoint fork
// UNSATISFIABLE one wave earlier (`int Construct()` vs DWARF `void`), caught here before it could
// repeat.
// ============================================================================
//
// FLAG -- INCOMPLETE BY DESIGN, AND HERE IS THE FULL SCORE so the next wave does not re-hunt it.
// The image has NINE out-of-line ArticulatedJointPool bodies (headless IDA 9.3 function list):
//     0x825C29C8  IsJointInUse                       372 bytes   BODIED
//     0x825C2B40  GetJoint                           248 bytes   BODIED
//     0x82600938  Construct                          144 bytes   BODIED
//     0x826013C0  SendCreateRemoveJointEvents       1464 bytes   BODIED (this wave)
//     0x825D7DD8  ConstructArticulatedJoint         1132 bytes   not bodied
//     0x825D8248  RemoveJoint                        580 bytes   not bodied
//     0x825D8490  GetIndexOfOtherHalf                304 bytes   not bodied
//     0x826009C8  CreateJoint                       1632 bytes   not bodied  (also an export HOLE:
//                                                                absent from progress/identity.json)
//     0x82601028  RemoveBrokenJointsFromSimulation   920 bytes   not bodied
// The class declares NO virtuals (the DWARF lists none and the console never seats a vptr), so an
// unbodied method here cannot decay into a silent base default -- it is a hard LNK2019 the day
// something calls it. That is the difference between this and a hollow shell.
// The DWARF also lists Destruct/Prepare/Release/FlagJointToBeRemoved/GetSwingBreakAngleRadians/
// GetTwistBreakAngleRadians/GetJointId/GetFirstJointIndex, none of which has an X360 symbol
// (all inlined or absent); per the "gate each DWARF declaration on X360 attestation" rule they are
// NOT declared here.

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"     // CgsContainers::BitArray<N>
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnArticulatedJoint.h"  // the REAL ArticulatedJoint

namespace BrnPhysics
{
namespace Vehicle
{
    // Forward decls: SendCreateRemoveJointEvents only takes pointers to these two, and pulling
    // either header in here would drag the whole physics-IO event vocabulary into every TU that
    // embeds the pool (BrnPhysicalTrafficManager.h is one). Their real homes are
    // SharedIO/BrnVehicleOutputInterface.h and BrnPhysicalTrafficManagerIO.h, and the pool's own
    // TU includes both.
    struct VehicleOutputRequestInterface;
    class  ArticulatedJointCreateBuffer;

    // DWARF BrnArticulatedJointPool.h:69. `struct` matches the DWARF's class-key.
    struct ArticulatedJointPool
    {
        static const s32 KI_NUM_JOINTS = 10;   // maJoints capacity
        static const u32 KU_NUM_JOINTS = 10u;  // BitArray<10> NUMBITS

        typedef CgsContainers::BitArray<KU_NUM_JOINTS> ArticulatedJointBitArray;  // DWARF :57

        void              Construct();                                // @0x82600938 (DWARF :73)
        ArticulatedJoint* GetJoint(s32 liJointIndex);                 // @0x825C2B40 (DWARF :136)
        bool              IsJointInUse(s32 liJointIndex) const;       // @0x825C29C8 (DWARF :119)

        // @0x826013C0 (DWARF :104). Drain one frame's batched joint create/remove requests out of
        // the working buffer and onto the simulation request interface.
        void SendCreateRemoveJointEvents(VehicleOutputRequestInterface* lpRequestInterface,
                                         const ArticulatedJointCreateBuffer* lpJointWorkingBuffer);

        // Never called; bodied in BrnArticulatedJointPool.cpp (a MOUNTED TU) and nothing but
        // static_asserts. Static so it can see the private block through offsetof.
        static void _AssertLayout();

    private:
        ArticulatedJoint         maJoints[KI_NUM_JOINTS];   // @0   (stride 80)  DWARF :145
        ArticulatedJointBitArray mUsedJoints;               // @800               DWARF :146
        ArticulatedJointBitArray mJointsBrokenThisFrame;    // @808               DWARF :147
        f32                      mfSwingAngleDegrees;       // @816               DWARF :150
        f32                      mfMaxSwingVelocityDegrees; // @820               DWARF :151
        f32                      mfTwistAngleDegrees;       // @824               DWARF :152
        f32                      mfMaxTwistVelocityDegrees; // @828               DWARF :153
    };
}
}
