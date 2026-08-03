#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>   // std::memcpy (models the Xbox XMemCpy block-copy intrinsic)
#include <cstddef>   // offsetof (VehicleOutputRequestInterface::_AssertLayout)

// BrnPhysics::Vehicle::VehicleOutputInterface + CrashingRaceCarInterface -- the bodied ledger
// functions homed by this group. Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// FLAG -- VehicleOutputInterface::AddTrafficState @0x825EC390 is declared on the class but
// intentionally NOT bodied: its X360 body is a deep VMX128 per-wheel projection routine reaching
// SimpleVehiclePhysics wheel/contact-frame internals whose full layout is not homed in any
// committed header, and whose per-wheel reciprocal-magnitude Newton-Raphson normalization + lane
// splats cannot be reconstructed BY NAME without fabricating a large accessor surface. It remains
// declaration-only until the BrnSimpleVehiclePhysics wheel-state TU lands its own ledger.

namespace BrnPhysics
{
namespace Vehicle
{
    // Number of race-car slots (BitArray<8> width).
    static const s32 KI_NUM_RACE_CARS = 8;

    // @0x822B4860  VehicleOutputInterface::GetRaceCarState (non-const)
    //   (dossier 'GetRaceCar' is a stripped-name artifact; DWARF has the const/non-const
    //   GetRaceCarState overloads at :343/:347, both returning RaceCarState*.)
    RaceCarState* VehicleOutputInterface::GetRaceCarState(s32 liRaceCarIndex)
    {
        CGS_ASSERT(static_cast<u32>(liRaceCarIndex) < 8u, "invalid index : ");
        CGS_ASSERT(mUsedRaceCars.IsBitSet(static_cast<u32>(liRaceCarIndex)),
                   "mUsedRaceCars.IsBitSet( liRaceCarIndex )");
        return &maRaceCarStates[liRaceCarIndex];
    }

    // @0x825C08D0  VehicleOutputInterface::GetRaceCarState (const)
    //   The const overload (DWARF :343); the only difference from the non-const @0x822B4860 is the
    //   dropped FireAssert file/line args.
    const RaceCarState* VehicleOutputInterface::GetRaceCarState(s32 liRaceCarIndex) const
    {
        CGS_ASSERT(static_cast<u32>(liRaceCarIndex) < 8u, "invalid index : ");
        CGS_ASSERT(mUsedRaceCars.IsBitSet(static_cast<u32>(liRaceCarIndex)),
                   "mUsedRaceCars.IsBitSet( liRaceCarIndex )");
        return &maRaceCarStates[liRaceCarIndex];
    }

    // The X360 asm-called symbol distinct from GetRaceCarState: returns &maRaceCarStates[i] for the
    // CrashingRaceCarInterface's per-in-use-car walk.
    const RaceCarState* VehicleOutputInterface::GetRaceCar(u32 luRaceCarIndex) const
    {
        return &maRaceCarStates[luRaceCarIndex];
    }

    // @0x823C89C8  VehicleOutputInterface::operator=
    //   (dossier 'operat' is a truncated name.) The X360 body copies the fixed head byte-for-byte,
    //   re-merges the two bounded event queues, block-copies the game-event queue, and byte-copies
    //   the aggressive-driving flags. Returns *this. ADDITIVE GROW: a real ledger func not in the
    //   DWARF member set, no field reordered/retyped.
    VehicleOutputInterface& VehicleOutputInterface::operator=(const VehicleOutputInterface& lOther)
    {
        // @+0x00: BitArray<8> head.
        mUsedRaceCars = lOther.mUsedRaceCars;

        // @+0x10: 8 x RaceCarState (1120-byte stride) via the Xbox block-copy intrinsic (XMemCpy),
        //         modelled as std::memcpy (RaceCarState is trivially copyable).
        for (s32 liCar = 0; liCar < KI_NUM_RACE_CARS; ++liCar)
        {
            std::memcpy(&maRaceCarStates[liCar], &lOther.maRaceCarStates[liCar], sizeof(RaceCarState));
        }

        // @+0x2310 / @+0x2620: reset the live count then merge the source's live events.
        mImpactEventQueue.Clear();
        mImpactEventQueue.Append(lOther.mImpactEventQueue);

        mTrafficStateQueue.Clear();
        mTrafficStateQueue.Append(lOther.mTrafficStateQueue);

        // @+0x65F0: GameEventQueue (0x610 bytes) -- raw block copy.
        std::memcpy(&mGameEventQueueStorage, &lOther.mGameEventQueueStorage, sizeof(mGameEventQueueStorage));

        // @+0x6C00: AggressiveDrivingFlags (5 bytes, the bdnz-5 tail loop).
        mAggressiveDrivingFlags = lOther.mAggressiveDrivingFlags;

        return *this;
    }

    // ---------------------------------------------------------------------------------------
    // VehicleOutputRequestInterface::_AssertLayout   -- the gate for the derived six-queue layout.
    //
    // ⚠️ THIS IS **CONSOLE ARITHMETIC THAT THE HOST HAPPENS TO REPRODUCE**, not a lucky host
    // offsetof. It is legitimate here (and it is NOT the vacuous kind of gate) for one specific
    // reason: every one of the six members is pointer-free storage, and BaseEventQueue<T>'s single
    // pointer sits in a header that is padded to 16 on BOTH targets (X360 4+4+4 rounded up to the
    // 16-byte element alignment; host 8+4+4 == 16 exactly). So the X360 offsets ARE the host
    // offsets, member for member, and asserting them asserts the derivation.
    //
    // ⭐ WHAT MAKES IT A DERIVATION RATHER THAN SIX GUESSES: two of the offsets are read straight
    // off the asm, and the chain that produces them is built from strides attested elsewhere.
    //   PIN 1  mAddJointQueue @ 39904  -- AddJoint @0x825E7170: `addis r3,r28,1 ; addi r3,r3,-0x6420`
    //   PIN 2  mRemoveJointQueue @ 41840 -- SendCreateRemoveJointEvents @0x826013C0:
    //                                       `addis r26,r26,1 ; addi r26,r26,-0x5C90`
    // Walking the DWARF member order forward from 0 with the per-type strides derived in
    // CgsPhysicsSimulationIO_Events.h hits both, with zero slack, and ends at 41936.
    //
    // ⚠️ ITS BLIND SPOT, stated: nothing here proves mRequestFineLineQueue's own INTERNAL layout --
    // it proves only that 13456 bytes sit between mRemoveRigidBodyQueue and
    // mChangeRigidBodyInertiaQueue, which is what VariableEventQueue<13440,16> is. If that queue is
    // ever re-typed to the DWARF's InFineQueryQueue<13440> the number must not move (that class adds
    // no data members), and if it does move, PIN 1 will fail -- which is the point.
    // ---------------------------------------------------------------------------------------
    void VehicleOutputRequestInterface::_AssertLayout()
    {
        // --- the element strides the chain is built from (each derived in its own home) ------
        static_assert(sizeof(CgsPhysics::PhysicsSimulationIO::InAddRigidBody) == 192,           "InAddRigidBody stride 192");
        static_assert(sizeof(CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody) == 16,         "InRemoveRigidBody stride 16");
        static_assert(sizeof(CgsPhysics::PhysicsSimulationIO::InChangeRigidBodyInertia) == 80,  "InChangeRigidBodyInertia stride 80");
        static_assert(sizeof(CgsPhysics::PhysicsSimulationIO::InAddJoint) == 192,               "InAddJoint stride 192");
        static_assert(sizeof(CgsPhysics::PhysicsSimulationIO::InRemoveJoint) == 8,              "InRemoveJoint stride 8");

        // --- and the widths of the three fields AddJoint reads by name (an offset gate alone is
        //     blind to a member retyped in place -- the TrafficPhysics tamper test proved that) ---
        static_assert(sizeof(InAddJoint::mu64Id) == 8,           "mId is an 8-byte handle (ld, not lwz)");
        static_assert(sizeof(InAddJoint::mu64ParentBodyId) == 8, "mParentBodyId is an 8-byte handle");
        static_assert(sizeof(InAddJoint::mu64ChildBodyId) == 8,  "mChildBodyId is an 8-byte handle");
        static_assert(offsetof(InAddJoint, mu64Id) == 0,            "AddJoint's `ld r10, 0(r29)`");
        static_assert(offsetof(InAddJoint, mu64ParentBodyId) == 8,  "AddJoint's `ld r10, 8(r29)`");
        static_assert(offsetof(InAddJoint, mu64ChildBodyId) == 16,  "AddJoint's `ld r10, 0x10(r29)`");
        // ⚠️ ADDED AFTER THE TAMPER TEST FOUND A HOLE: with only the sizeof==192 line above, shrinking
        // the opaque tail 168 -> 160 was **SILENT** -- 24 + 160 == 184 and alignas(16) rounds it right
        // back to 192. That is the standing over-aligned-type trap (the same one that let a u32 be
        // added to ArticulatedJoint without moving its 80). Pinning the tail makes 192 an actual SUM.
        static_assert(offsetof(InAddJoint, macOpaquePayload) == 24, "the unrecovered tail starts after the three handles");
        static_assert(sizeof(InAddJoint::macOpaquePayload) == 168,  "24 + 168 == 192, with no alignment slack");

        // --- the six queue sizes, each = 16-byte BaseEventQueue header + N * stride -----------
        static_assert(sizeof(InAddRigidBodyQueue) == 9616,            "16 + 50*192");
        static_assert(sizeof(InRemoveRigidBodyQueue) == 816,          "16 + 50*16");
        static_assert(sizeof(OutFineQueryQueue) == 13456,             "VariableEventQueue<13440,16>: 1 + 13440 -> 13444 + 12");
        static_assert(sizeof(InChangeRigidBodyInertiaQueue) == 16016, "16 + 200*80");
        static_assert(sizeof(AddArticulatedJointQueue) == 1936,       "16 + 10*192");
        static_assert(sizeof(RemoveArticulatedJointQueue) == 96,      "16 + 10*8");

        // --- the chain, ending on the two asm pins -------------------------------------------
        static_assert(offsetof(VehicleOutputRequestInterface, mRequiredRigidBodiesQueue) == 0,        "DWARF :263 @0");
        static_assert(offsetof(VehicleOutputRequestInterface, mRemoveRigidBodyQueue) == 9616,         "DWARF :264");
        static_assert(offsetof(VehicleOutputRequestInterface, mRequestFineLineQueue) == 10432,        "DWARF :265");
        static_assert(offsetof(VehicleOutputRequestInterface, mChangeRigidBodyInertiaQueue) == 23888, "DWARF :268");
        static_assert(offsetof(VehicleOutputRequestInterface, mAddJointQueue) == 39904,
                      "PIN 1 -- AddJoint @0x825E7170 `addi r3,r3,-0x6420` (65536-0x6420 == 39904)");
        static_assert(offsetof(VehicleOutputRequestInterface, mRemoveJointQueue) == 41840,
                      "PIN 2 -- SendCreateRemoveJointEvents @0x826013C0 `addi r26,r26,-0x5C90` "
                      "(65536-0x5C90 == 41840)");
        static_assert(sizeof(VehicleOutputRequestInterface) == 41936,
                      "41840 + 96, 16-aligned -- the chain closes with zero slack");
    }

    // ---------------------------------------------------------------------------------------
    // @0x825E7170  VehicleOutputRequestInterface::AddJoint            NEW 2026-08-03
    //
    // ⚠️ RECOVERED FROM AN `.ida-exports` HOLE. This address has no 0x825E7170.json and no entry in
    // progress/identity.json, and the PS3 DecFIGS set has no twin either (PS3 inlines it) -- but the
    // caller's asm names the symbol, so it exists. Pulled with headless IDA 9.3: 53 instructions,
    // func range 0x825E7170..0x825E7244. Same "absent from JSON is not absent from image" rule that
    // recovered TrafficPhysics::Construct last wave.
    //
    // Three non-gating assert tripwires on the event's handles, then queue it:
    //   ld r10, 0(r29)    ; cmpld qword_82F2A3B0  -> "!lAddJointEvent.mId.IsInvalid()"        (:718)
    //   ld r10, 8(r29)    ; cmpld qword_82F2A3A8  -> "...mParentBodyId != K_INVALID_RIGID_BODY_ID" (:719)
    //   ld r10, 0x10(r29) ; cmpld qword_82F2A3A8  -> "...mChildBodyId  != K_INVALID_RIGID_BODY_ID" (:720)
    //   addis r3,r28,1 ; addi r3,r3,-0x6420 ; bl BaseEventQueue<InAddJoint>::AddEvent
    // The two sentinels are the module-global invalid handles (qword_82F2A3B0 == invalid JointId,
    // qword_82F2A3A8 == CgsPhysics::K_INVALID_RIGID_BODY_ID). Both are mirrored as local constants
    // rather than included: CgsPhysics::RigidBodyId / JointId are each declared twice in this tree
    // (CgsRigidBody.h vs CgsPhysicsSimulationModule.h -- an open ODR fork that has never met in one
    // TU), so including either header here would turn a latent fork into a hard C2011.
    // ---------------------------------------------------------------------------------------
    void VehicleOutputRequestInterface::AddJoint(const InAddJoint& lAddJointEvent)
    {
        // X360 qword_82F2A3A8 == CgsPhysics::K_INVALID_RIGID_BODY_ID (CgsRigidBody.h:41, ~0ull).
        static const u64 KU64_INVALID_RIGID_BODY_ID = 0xFFFFFFFFFFFFFFFFull;

        CGS_ASSERT(lAddJointEvent.mu64Id != KU64_INVALID_JOINT_ID,
                   "!lAddJointEvent.mId.IsInvalid()");
        CGS_ASSERT(lAddJointEvent.mu64ParentBodyId != KU64_INVALID_RIGID_BODY_ID,
                   "lAddJointEvent.mParentBodyId != CgsPhysics::K_INVALID_RIGID_BODY_ID");
        CGS_ASSERT(lAddJointEvent.mu64ChildBodyId != KU64_INVALID_RIGID_BODY_ID,
                   "lAddJointEvent.mChildBodyId != CgsPhysics::K_INVALID_RIGID_BODY_ID");

        mAddJointQueue.AddEvent(lAddJointEvent);
    }

    // @0x823625C0  CrashingRaceCarInterface::SetFromVehicleOutputInterface
    //   For every race-car slot in use (mUsedRaceCars bit set) copy that car's
    //   RaceCarState::mbResetCarTransform flag (byte @1098) into mabCrashingRaceCars[]. The
    //   per-iteration index<8 guard is the inlined CgsBitArray bounds assert (loop-bounded, never
    //   fires); GetUsedCarsBitArray() == the interface's first member.
    void CrashingRaceCarInterface::SetFromVehicleOutputInterface(const VehicleOutputInterface* lpOutput)
    {
        for (s32 liIndex = 0; liIndex < KI_NUM_RACE_CARS; ++liIndex)
        {
            CGS_ASSERT(static_cast<u32>(liIndex) < 8u, "invalid index : < 8");

            if (lpOutput->GetUsedCarsBitArray().IsBitSet(static_cast<u32>(liIndex)))
            {
                const RaceCarState* lpState = lpOutput->GetRaceCar(static_cast<u32>(liIndex));
                mabCrashingRaceCars[liIndex] = lpState->mbResetCarTransform;
            }
        }
    }
}
}
