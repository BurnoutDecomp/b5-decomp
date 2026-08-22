#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
// ⭐ CgsPhysics::K_INVALID_RIGID_BODY_ID, the real one (X360 qword_82F2A3A8 in this TU). Was
// mirrored as a local u64 constant until 2026-08-04 because the RigidBodyId ODR fork made
// including this header a hard C2011; the fork is retired (task #141) and the mirror with it.
// Matches the two sibling vehicle-manager TUs (BrnVehicleManager.cpp:7,
// BrnVehicleManager_Construct.cpp:5), which have always included it directly.
#include "GameShared/GameClasses/Physics/CgsRigidBody.h"
// AddTrafficState @0x825EC390 only: the traffic body it projects and the wheel type it walks.
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"                // PhysicalTrafficVehicle
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"   // SimpleVehiclePhysics
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"                     // Wheel / Wheel::RoadContact
#include <cmath>                                                                        // std::fabs

#include <cstring>   // std::memcpy (models the Xbox XMemCpy block-copy intrinsic)
#include <cstddef>   // offsetof (VehicleOutputRequestInterface::_AssertLayout)

// BrnPhysics::Vehicle::VehicleOutputInterface + CrashingRaceCarInterface -- the bodied ledger
// functions homed by this group. Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// ⭐ THE FLAG THAT USED TO STAND HERE IS RETIRED (wave T3, physical traffic).
// VehicleOutputInterface::AddTrafficState @0x825EC390 IS BODIED at the tail of this file. The old
// note said its wheel internals were unhomed; they are not. Its per-wheel loop is instruction-for-
// instruction the SAME loop as the already-landed race-car twin UpdateRaceCarState @0x825EC808
// (source register at wheel+0x30, dest at WheelLite+0x44, strides 224/112, the same vrefp+2xNR
// reciprocal for mfSuspensionHeight), and every field it reads is a named member of Wheel /
// SimpleVehiclePhysics today. The one accessor that was genuinely missing -- the BASE's
// `const Wheel* GetWheel(EVehicleDrivenWheel) const` (DWARF BrnSimpleVehiclePhysics.h:214) -- was
// added to that header with this wave.

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

        // --- and the widths of the fields AddJoint reads by name (an offset gate alone is
        //     blind to a member retyped in place -- the TrafficPhysics tamper test proved that) ---
        static_assert(sizeof(InAddJoint::mu64Id) == 8,           "mId is an 8-byte handle (ld, not lwz)");
        static_assert(sizeof(InAddJoint::mu64ParentBodyId) == 8, "mParentBodyId is an 8-byte handle");
        static_assert(sizeof(InAddJoint::mu64ChildBodyId) == 8,  "mChildBodyId is an 8-byte handle");
        static_assert(offsetof(InAddJoint, mu64Id) == 0,            "AddJoint's `ld r10, 0(r29)`");
        static_assert(offsetof(InAddJoint, mu64ParentBodyId) == 8,  "AddJoint's `ld r10, 8(r29)`");
        static_assert(offsetof(InAddJoint, mu64ChildBodyId) == 16,  "AddJoint's `ld r10, 0x10(r29)`");
        // ⚠️ ADDED AFTER THE TAMPER TEST FOUND A HOLE: with only the sizeof==192 line above, shrinking
        // the tail 168 -> 160 was **SILENT** -- 24 + 160 == 184 and alignas(16) rounds it right
        // back to 192. That is the standing over-aligned-type trap (the same one that let a u32 be
        // added to ArticulatedJoint without moving its 80). Pinning the tail makes 192 an actual SUM.
        //
        // ⭐ 2026-08-04 (task #144): the tail is no longer opaque -- InAddJoint's payload was a
        // `u8 macOpaquePayload[168]` whose note claimed no DWARF/source existed, and the DWARF
        // names all six members. The two lines this replaces pinned that span's start and size;
        // these pin the SAME closure over the real members, so the trap they were guarding
        // against is still caught: every interior offset is a sum of the widths before it, and
        // shrinking or re-typing any of them moves a later one.
        static_assert(offsetof(InAddJoint, mJointFrames) == 32,  "mJointFrames @+0x20 (drain `addi r11,r26,0x20`)");
        static_assert(sizeof(InAddJoint::mJointFrames) == 80,    "JointFrames is 80B -- the FIVE lvx128 lanes");
        static_assert(offsetof(InAddJoint, mJointLimits) == offsetof(InAddJoint, mJointFrames) + sizeof(InAddJoint::mJointFrames),
                                                                 "mJointLimits follows mJointFrames (+0x70)");
        static_assert(sizeof(InAddJoint::mJointLimits) == 64,    "JointLimits is 64B -- the EIGHT ld/std pairs");
        static_assert(offsetof(InAddJoint, mbSpy) == offsetof(InAddJoint, mJointLimits) + sizeof(InAddJoint::mJointLimits),
                                                                 "mbSpy follows mJointLimits (+0xB0, the drain's `lbz 0xB0(event)`)");
        static_assert(offsetof(InAddJoint, mbSpy) == 176,        "176 + 1 -> 192 is the ONLY alignment slack in this record");

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
    // qword_82F2A3A8 == CgsPhysics::K_INVALID_RIGID_BODY_ID).
    // ⭐ 2026-08-04 (task #141): the rigid-body sentinel is now the REAL CgsRigidBody.h one, not a
    // local mirror. This note used to justify mirroring both with "CgsPhysics::RigidBodyId /
    // JointId are each declared twice in this tree (CgsRigidBody.h vs CgsPhysicsSimulationModule.h
    // -- an open ODR fork), so including either header here would turn a latent fork into a hard
    // C2011". Half of that was true and is now fixed; ⛔ THE OTHER HALF WAS NEVER TRUE:
    // CgsRigidBody.h declares no JointId whatsoever, so `CgsPhysics::JointId` was never forked --
    // the only other JointId in the tree is BrnPhysics::Vehicle::JointId (BrnArticulatedJoint.h:52),
    // a different namespace. KU64_INVALID_JOINT_ID stays a local constant only because
    // CgsPhysics::JointId genuinely has no sentinel of its own anywhere in the tree yet.
    // ---------------------------------------------------------------------------------------
    void VehicleOutputRequestInterface::AddJoint(const InAddJoint& lAddJointEvent)
    {
        CGS_ASSERT(lAddJointEvent.mu64Id != KU64_INVALID_JOINT_ID,
                   "!lAddJointEvent.mId.IsInvalid()");
        // X360 qword_82F2A3A8 == CgsPhysics::K_INVALID_RIGID_BODY_ID (CgsRigidBody.h:41, ~0ull);
        // the RigidBodyId -> u64 conversion is the class's own `operator u64() const` (:35), which
        // is what the console's bare `ld`/`cmpld` pair is.
        CGS_ASSERT(lAddJointEvent.mu64ParentBodyId != CgsPhysics::K_INVALID_RIGID_BODY_ID,
                   "lAddJointEvent.mParentBodyId != CgsPhysics::K_INVALID_RIGID_BODY_ID");
        CGS_ASSERT(lAddJointEvent.mu64ChildBodyId != CgsPhysics::K_INVALID_RIGID_BODY_ID,
                   "lAddJointEvent.mChildBodyId != CgsPhysics::K_INVALID_RIGID_BODY_ID");

        mAddJointQueue.AddEvent(lAddJointEvent);
    }

    // ---------------------------------------------------------------------------------------
    // VehicleOutputRequestInterface::Construct (DWARF :204)          NEW 2026-08-09 (conductor)
    //
    // X360-attested through CreateIOBuffer<VehicleManagerOutputBuffer> @0x8259DAF0, which runs
    // the buffer Construct after the stack alloc (the PS3 DecFIGS build keeps
    // VehicleManagerOutputBuffer::Construct out of line, and this is its payload): construct
    // all six request queues over their inline storage.
    // ---------------------------------------------------------------------------------------
    void VehicleOutputRequestInterface::Construct()
    {
        mRequiredRigidBodiesQueue.Construct();
        mRemoveRigidBodyQueue.Construct();
        mRequestFineLineQueue.Construct();
        mChangeRigidBodyInertiaQueue.Construct();
        mAddJointQueue.Construct();
        mRemoveJointQueue.Construct();
    }

    // ---------------------------------------------------------------------------------------
    // AggressiveDrivingFlags::Clear (DWARF :287)                   NEW 2026-08-10 (create path)
    //
    // X360-attested as the five `stb r31, 0x6C00..0x6C04(r29)` stores PhysicsModuleIO::
    // OutputBuffer::Construct @0x825ABB88..0x825ABBA0 emits over the VehicleOutputInterface
    // seat, r31 == 0. The struct is exactly five bools and the DWARF gives the class a Clear();
    // FLAG: that the five stores ARE this Clear() rather than five open-coded stores inside
    // VehicleOutputInterface::Construct is an inference from the DWARF declaring both -- the
    // emitted code is identical either way, so nothing downstream depends on the split.
    // ---------------------------------------------------------------------------------------
    void AggressiveDrivingFlags::Clear()
    {
        mbPlayerWonSlamThisFrame      = false;
        mbPlayerLostSlamThisFrame     = false;
        mbPlayerWonGrindingThisFrame  = false;
        mbPlayerLostGrindingThisFrame = false;
        mbRubbingThisFrame            = false;
    }

    // ---------------------------------------------------------------------------------------
    // VehicleOutputInterface::Construct (DWARF :312)               NEW 2026-08-10 (create path)
    //
    // The console keeps this one INLINE: PhysicsModuleIO::OutputBuffer::Construct @0x825ABB10
    // emits it over its own +44128 seat, with r29 == this. Read from the asm, in the console's
    // own emission order (which is NOT member order -- kept as shipped):
    //   0x825ABB58/70  addi r3, r29, 0x2620 ; bl PhysicalTrafficState,20>::Construct
    //   0x825ABB74/78  addi r3, r29, 0x2310 ; bl ImpactEvent,16>::Construct
    //   0x825ABB7C/80  addi r3, r29, 0x65F0 ; bl VariableEventQueue<1536,16>::Construct
    //   0x825ABB84     std  r31, 0(r29)                  <- mUsedRaceCars, ONE 8-byte zero
    //   0x825ABB88..A0 stb  r31, 0x6C00..0x6C04(r29)     <- mAggressiveDrivingFlags, five bools
    // The single `std` is the whole of BitArray<8u>: one 64-bit field (kuNumberOfBitFields == 1),
    // which is the same one-word image VehicleManager::ReadUpdatedBodies @0x82619A10 scans with
    // a single `ld`. Expressed as the container's own named UnSetAll(), not a raw store.
    // ---------------------------------------------------------------------------------------
    void VehicleOutputInterface::Construct()
    {
        // ⭐ THE OPAQUE SPAN IS GATED BEFORE IT IS CONSTRUCTED THROUGH. mGameEventQueue is
        // still modelled as a size-pinned storage array, and GetGameEventQueue() is this file's
        // sanctioned cast seam; running a Construct through that cast is only sound if the real
        // queue fits the span EXACTLY. It does, on the host as on the console:
        //   1 (mbIsConstructed) + 1536 (macData) + 3 pad + 4 + 4 + 4 == 1552 == 0x610.
        // If VariableEventQueue ever grows a pointer this fails to COMPILE instead of writing
        // past mGameEventQueueStorage into mAggressiveDrivingFlags.
        static_assert(sizeof(CgsModule::VariableEventQueue<1536, 16>) == 0x610,
                      "GameEventQueue must fit its size-pinned storage span exactly");

        mTrafficStateQueue.Construct();       // +0x2620
        mImpactEventQueue.Construct();        // +0x2310
        GetGameEventQueue()->Construct();     // +0x65F0
        mUsedRaceCars.UnSetAll();             // +0      (the single `std 0`)
        mAggressiveDrivingFlags.Clear();      // +0x6C00 (the five `stb 0`)
    }

    // ---------------------------------------------------------------------------------------
    // VehicleManagerOutputInterface::Construct (DWARF :86)         NEW 2026-08-10 (create path)
    //
    // X360 0x822E6790, 25 instructions, OUT OF LINE -- transcribed leg for leg:
    //   +0x6C0 CreateVehicleResult,8>      +0     TrafficCrashedEvent,20>
    //   +0x150 TrafficSlammedEvent,20>     +0x2F0 TrafficCrashedEvent,10>
    //   +0x3A0 RaceCarCrashEvent,8>        +0x5B0 RaceCarResetEvent,8>
    //   +0x750 short,32>                   +0x7A0 TrafficRemovedEvent,25>
    //   sth r11(=0), 0x79C   +  stb r11, 0x79E     == mVehicleGuiOutputMessages (3 bools)
    //   stfs f0, 0x874       +  stfs f0, 0x878     == mWheelFFSpring, f0 = flt_82001CC0
    //
    // ⭐ AS-SHIPPED ODDITY, reproduced rather than tidied: the console constructs
    // mCreateVehicleResultQueue (+0x6C0) FIRST, before the queue that sits at offset 0. The
    // remaining seven then run in member order. No `bl` is reordered here.
    //
    // ⭐ flt_82001CC0 == 0.0f, read out of the X360 image (x360rd.py, DELTA -1594), with the
    // reader self-tested in the same run against two constants this tree already names by other
    // means: flt_8208F83C -> 9.8100004196167 (KF_GRAVITY) and flt_82001D9C -> 2.0f (the
    // handbrake drift window). Its neighbours read as live data (1.875 / 176.0 / -176.0, then
    // the ASCII "Monitor already started: " four words later), so the zero is a pooled 0.0f
    // literal and not an unmapped read. Both spring lanes therefore start at zero.
    // ---------------------------------------------------------------------------------------
    void VehicleManagerOutputInterface::Construct()
    {
        mCreateVehicleResultQueue.Construct();      // +0x6C0  (console emits this first)
        mCrashedTrafficEventQueue.Construct();      // +0
        mSlammedTrafficEventQueue.Construct();      // +0x150
        mFineTrafficCrashedEventQueue.Construct();  // +0x2F0
        mRaceCarCrashEventQueue.Construct();        // +0x3A0
        mRaceCarResetEventQueue.Construct();        // +0x5B0
        mTrafficTypeRequestQueue.Construct();       // +0x750
        mRemovedTrafficEventQueue.Construct();      // +0x7A0

        mVehicleGuiOutputMessages.mbPlayerGrindingOther = false;   // +0x79C ) the `sth` covers
        mVehicleGuiOutputMessages.mbOtherGrindingPlayer = false;   // +0x79D ) these two
        mVehicleGuiOutputMessages.mbRubbing             = false;   // +0x79E (the `stb`)

        mWheelFFSpring.mfSpringCoefficient = 0.0f;  // +0x874
        mWheelFFSpring.mfSpringSaturation  = 0.0f;  // +0x878
    }

    // ---------------------------------------------------------------------------------------
    // VehicleOutputRequestInterface::Append (DWARF :201)             NEW 2026-08-09 (conductor)
    //
    // X360-attested INLINE in PhysicsModule::Update @0x825B0640 (0x825B2480..0x825B24C0):
    // merge the source interface's request queues onto this one, five appends in the
    // console's order. ⚠️ mChangeRigidBodyInertiaQueue is deliberately NOT appended --
    // Update drains that queue into the SIM input buffer through
    // BridgeVehicleManagerToSimulation_PostPhysics @0x825ADF60 instead; appending it here
    // too would double-apply every inertia change.
    // ---------------------------------------------------------------------------------------
    void VehicleOutputRequestInterface::Append(const VehicleOutputRequestInterface* lpSource)
    {
        mRequiredRigidBodiesQueue.Append(lpSource->mRequiredRigidBodiesQueue);   // @0x825A3898
        mRemoveRigidBodyQueue.Append(lpSource->mRemoveRigidBodyQueue);           // @0x825A3988
        mRequestFineLineQueue.Append(lpSource->mRequestFineLineQueue);           // @0x825AC068 (<13440,16>)
        mAddJointQueue.Append(lpSource->mAddJointQueue);                         // @0x825A3A68
        mRemoveJointQueue.Append(lpSource->mRemoveJointQueue);                   // @0x825A3B58
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

// =================================================================================================
// @0x825EC390  VehicleOutputInterface::AddTrafficState   (285 insns, DWARF :352)
//
// THE PRODUCER OF PhysicalTrafficState. One call per live physical-traffic slot per frame, from
// PhysicalTrafficManager::WriteOutVehicleStats @0x825F0308 (its only xref).
//
// SIGNATURE from the asm prologue: r3 this, r4 the EntityId, r5 the PhysicalTrafficVehicle.
// r31 is `*(r5 + 28)` == PhysicalTrafficVehicle::mpVehicleBody, so every physics read below goes
// through that body, whose static type is SimpleVehiclePhysics.
//
// THE SLOT. `_R30 = <queue>(this + 9760)` @0x825E5010 is BaseEventQueue<PhysicalTrafficState>::
// AddEvent() -- the no-arg "reserve the next slot and bump miLength" overload (its own body is the
// two CgsBaseEventQueue.h:360/:361 asserts, `816 * miLength + mpEvents`, `++miLength`). 9760 ==
// 0x2620 == mTrafficStateQueue.
//
// FIELD MAP, read off the asm (r30 == the new state, r31 == the SimpleVehiclePhysics):
//   state +800  mEntityID            <- the argument
//   state +448  mTransform  (4 rows) <- GetGraphicsVehicleTransform()       (@0x825BF158)
//   state +528  mLinearVelocity      <- physics +0x50
//   state +804  mfSpeed              <- physics +0x6C0 lane .x * flt_82F31928 (0.44704) == GetSpeed().x
//   state +808  mbFrozen             <- physics +0x70
//   state +809  mbIsDeforming        <- physics +0x712  (mbStartedDeforming)
//   state +810  mbIsFatallyCrashing  <- physics +0x711  (mbStartedFatallyCrashing)
//   state +812  mfSteering           <- the vtable slot-0 call == GetSteeringAngle()
//   state +544/608/672/736  maWheelTransforms[0..3] <- GetWheelsWorldTransfrom(i, false) (@0x825D8878)
//   state +0..447           maWheels[0..3]          <- the per-wheel loop
//   state +512  mvRoadTestNormal_HeightAboveRoad <- the above-ground rebase (below)
//
// THE WHEEL LOOP (0x825EC5B0..0x825EC79C) -- four iterations, source stride 224 == sizeof(Wheel),
// dest stride 112 == sizeof(WheelLite), identical register geometry to UpdateRaceCarState's loop
// (source r10 at wheel+0x30, dest r11 at WheelLite+0x44). It writes ELEVEN of WheelLite's fields
// and deliberately leaves four alone:
//   WARNING mfWheelLongSpeed / mfRoadLongSpeed / mfRoadLatSpeed / mbHasTraction are NOT written
//   here. There is no store to dest+84/+88/+92/+97 anywhere in the function (checked against the
//   RAW ASM, not the pseudocode); the race-car twin does write them. Reproduced as-is.
//
// ONE DIVERGENCE, REPRODUCED NOT "FIXED": the two ground bools are SWAPPED on the way in.
//     0x825EC740  lbz r6, var_138(r1)   ; var_138 == roadContact+40 == mbIsOnGround
//     0x825EC75C  stb r6, -0x1B(r11)    ; dest+41 == mbWasOnGroundLastUpdate
//     0x825EC750  lbz r9, -7(r10)       ; r10 == wheel+0x30, so wheel+0x29 == mbWasOnGroundLastUpdate
//     0x825EC768  stb r9, -0x1C(r11)    ; dest+40 == mbIsOnGround
// UpdateRaceCarState @0x825ECE78/0x825ECE80 stores the same two bytes straight through
// (+40 -> +40, +41 -> +41), so this is a real console asymmetry, not a decode error. Kept, and
// flagged for the verifier. (mbIsCloseToGround @+42 is copied by neither function.)
//
// THE ABOVE-GROUND REBASE (0x825EC7A0..0x825EC7F8). The result was captured against the PHYSICS
// pose; the published transform is the GRAPHICS pose, so the height is corrected by the .y of the
// difference:
//     delta = physics.GetPosition() - state.mTransform.wAxis
//     state.mvRoadTestNormal_HeightAboveRoad.xyz = aboveGround.mIntersectionNormal.xyz
//     state.mvRoadTestNormal_HeightAboveRoad.w   = aboveGround.mfVerticalDistance - delta.y
// (the console writes that .w twice -- once splicing in the slot's stale lane, once with the real
// value -- so only the second store survives; the dead first store is not reproduced.)
// =================================================================================================

// The wheel state byte the console compares against 2 (`lbz r3, 0xA7(r10)` == Wheel +0xD7 ==
// mu8State). 2 == detached; mbAttached is its negation. Same constant the race-car twin carries in
// its own file-local block.
static const u8 KU8_ADDTRAFFICSTATE_WHEEL_STATE_DETACHED = 2;

void VehicleOutputInterface::AddTrafficState(EntityId lEntityID,
                                             const PhysicalTrafficVehicle* lpPhysicalTrafficVehicle)
{
    // DIVERGENCE (named): the console has no null guards -- WriteOutVehicleStats only calls this
    // for a bit set in mUsedTrafficVehicles, whose body pointer is always seated. Kept as a
    // bring-up guard, same as the race-car twin above.
    CGS_ASSERT(lpPhysicalTrafficVehicle != 0, "lpPhysicalTrafficVehicle != NULL");
    if (lpPhysicalTrafficVehicle == 0 || lpPhysicalTrafficVehicle->mpVehicleBody == 0)
    {
        return;
    }

    const SimpleVehiclePhysics& lrPhysics = *lpPhysicalTrafficVehicle->mpVehicleBody;
    PhysicalTrafficState&       lrState   = mTrafficStateQueue.AddEvent();

    lrState.mEntityID           = lEntityID;
    lrState.mTransform          = lrPhysics.GetGraphicsVehicleTransform();
    lrState.mLinearVelocity     = lrPhysics.GetLinearVelocity();
    lrState.mfSpeed             = lrPhysics.GetSpeed().x;
    lrState.mbFrozen            = lrPhysics.IsFrozen();
    lrState.mbIsDeforming       = lrPhysics.HasStartedDeforming();
    lrState.mbIsFatallyCrashing = lrPhysics.IsFatallyCrashing();
    lrState.mfSteering          = lrPhysics.GetSteeringAngle().x;

    // The four wheel meshes' world matrices. lbHackDontReverseRightWheels is false at every
    // committed call site, this one included (`li r6, 0` before each bl).
    for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
    {
        lrState.maWheelTransforms[liWheel] =
            lrPhysics.GetWheelsWorldTransfrom(static_cast<EVehicleDrivenWheel>(liWheel), false);
    }

    const Matrix44Affine lPhysicsTransform = lrPhysics.GetTransform();

    for (s32 liWheel = 0; liWheel < eNumDrivenWheels; ++liWheel)
    {
        const Wheel& lrWheel     = *lrPhysics.GetWheel(static_cast<EVehicleDrivenWheel>(liWheel));
        WheelLite&   lrWheelLite = lrState.maWheels[liWheel];

        lrWheelLite.mVelocity          = lrWheel.mBodyPointVelocity;
        lrWheelLite.mfRadiansPerSecond = lrWheel.mIntegrationVariables.x;
        lrWheelLite.mfRadius           = lrWheel.mSlipVariables.w;
        lrWheelLite.mfRotation         = lrWheel.mIntegrationVariables.z;
        lrWheelLite.mfSkidFactor       = lrWheel.mSlipVariables.z;

        // The suspension height, normalised by whichever travel limit the wheel is inside. The
        // console emits vrefp + two Newton-Raphson steps + one vmulfp, i.e. a divide, and negates
        // the numerator on the below-rest branch (`vxor` against 0x80000000).
        // (`lvx128 v13, r10, 80` == wheel+0x80 == mPosition, lane .y;
        //  `lvx128 v0, r10, 48` == wheel+0x60 == mSuspensionAndInertiaVariables, lane .y or .x)
        const f32 lfWheelHeight = lrWheel.mPosition.y;
        if (lfWheelHeight >= 0.0f)
        {
            lrWheelLite.mfSuspensionHeight = lfWheelHeight / lrWheel.mSuspensionAndInertiaVariables.y;
        }
        else
        {
            lrWheelLite.mfSuspensionHeight = -lfWheelHeight / lrWheel.mSuspensionAndInertiaVariables.x;
        }

        // `lbz r3, 0xA7(r10)` == Wheel +0xD7 == mu8State; 2 == detached.
        lrWheelLite.mbAttached = (lrWheel.mu8State != KU8_ADDTRAFFICSTATE_WHEEL_STATE_DETACHED);

        lrWheelLite.mRoadContact.mNormal              = lrWheel.mRoadContact.mNormal;
        lrWheelLite.mRoadContact.mfLineDistanceToRoad = lrWheel.mRoadContact.mfLineDistanceToRoad;
        lrWheelLite.mRoadContact.mCollisionTag        = lrWheel.mRoadContact.mCollisionTag;
        lrWheelLite.mRoadContact.mbLineTestIsValid    = lrWheel.mRoadContact.mbLineTestIsValid;
        // THE SWAP -- see the banner. Do not "correct" it without re-reading 0x825EC738..0x825EC768.
        lrWheelLite.mRoadContact.mbWasOnGroundLastUpdate = lrWheel.mRoadContact.mbIsOnGround;
        lrWheelLite.mRoadContact.mbIsOnGround            = lrWheel.mRoadContact.mbWasOnGroundLastUpdate;

        // The world-space traction point: the vmaddfp chain row0*p.x + row1*p.y + row2*p.z + wAxis,
        // with p == maLocalTractionPoints[i] (`r3 = (i + 0x53) << 4` == +0x530 + 16*i).
        const Vector3 lvLocal = lrPhysics.GetLocalTractionPoint(static_cast<u8>(liWheel));
        lrWheelLite.mRoadContact.mPosition.x =
            lPhysicsTransform.xAxis.x * lvLocal.x + lPhysicsTransform.yAxis.x * lvLocal.y
          + lPhysicsTransform.zAxis.x * lvLocal.z + lPhysicsTransform.wAxis.x;
        lrWheelLite.mRoadContact.mPosition.y =
            lPhysicsTransform.xAxis.y * lvLocal.x + lPhysicsTransform.yAxis.y * lvLocal.y
          + lPhysicsTransform.zAxis.y * lvLocal.z + lPhysicsTransform.wAxis.y;
        lrWheelLite.mRoadContact.mPosition.z =
            lPhysicsTransform.xAxis.z * lvLocal.x + lPhysicsTransform.yAxis.z * lvLocal.y
          + lPhysicsTransform.zAxis.z * lvLocal.z + lPhysicsTransform.wAxis.z;
        lrWheelLite.mRoadContact.mPosition.w = 0.0f;
    }

    // ---- the above-ground rebase: physics pose -> the graphics pose just published -------------
    {
        const AboveGroundTestResult& lrAboveGround =
            *lrPhysics.GetAboveGroundTestResult();

        const f32 lfDeltaY = lPhysicsTransform.wAxis.y - lrState.mTransform.wAxis.y;

        lrState.mvRoadTestNormal_HeightAboveRoad.x = lrAboveGround.mIntersectionNormal.x;
        lrState.mvRoadTestNormal_HeightAboveRoad.y = lrAboveGround.mIntersectionNormal.y;
        lrState.mvRoadTestNormal_HeightAboveRoad.z = lrAboveGround.mIntersectionNormal.z;
        lrState.mvRoadTestNormal_HeightAboveRoad.w = lrAboveGround.mfVerticalDistance - lfDeltaY;
    }
}

}
}
