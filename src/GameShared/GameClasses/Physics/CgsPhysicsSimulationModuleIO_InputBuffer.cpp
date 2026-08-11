#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsPhysics::PhysicsSimulationIO::InputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. The X360-baked CgsPhysicsSimulationModuleIO.h file/line args are
// discarded per project convention; the assert strings are X360 rodata, reproduced verbatim
// (including the trailing newline on the lock-status messages).
//
//   GetMaxIterations() const  @ 0x8289E338
//       -> read-lock (bit 4) guard, assert muMaxIterations > 0, return muMaxIterations (this+8)
//   GetTimeStep()      const  @ 0x8259ECD8
//       -> write-lock (bit 3) guard, assert mfTimeStep > 0, return mfTimeStep (this+4)
//   SetMaxIterations(int)     @ 0x8259EDB0
//       -> write-lock (bit 3) guard, assert arg > 0, store muMaxIterations (this+8)
//   SetTimeStep(f32)          @ 0x8259EBF8
//       -> write-lock (bit 3) guard, assert arg > 0, store mfTimeStep (this+4)
//
// X360 lock-bit notes (faithful to asm; the WRITE-bit check on GetTimeStep is intentional and
// is NOT "corrected" to a read-bit check):
//   - read lock  : lbz 0(this); extrwi r11,r11,1,27 -> PPC MSB0 bit 27 == LSB bit 4 ==
//                  eStatusLockedForRead  -> IsBufferLockedForReading().
//   - write lock : lbz 0(this); extrwi r11,r11,1,28 -> PPC MSB0 bit 28 == LSB bit 3 ==
//                  eStatusLockedForWrite -> IsBufferLockedForWriting().

namespace CgsPhysics
{
namespace PhysicsSimulationIO
{
    // ⭐⭐ THE FULL NINETEEN-QUEUE LAYOUT GATE (task #142, 2026-08-04).
    //
    // This TU is MOUNTED (build_game_exe.bat), so these fire on every build -- not at
    // `work submit` time. That matters: [[gates-are-stale-not-dead]] counted 1,543 of 2,862
    // gates in this tree as submit-time only, and a layout claim nobody compiles is a comment.
    //
    // EVERY right-hand side is an X360 constant lifted from a specific instruction pair, never
    // a restatement of the header's own types. Sixteen come from an
    // `InputBuffer::GetXxxQueue() const` accessor's closing `addis r3,r28,H`/`addi r3,r3,L`;
    // two more (#6, #18) from the same instructions in accessors that had to be pulled headless
    // out of the .i64 because they are missing from .ida-exports; one (#4) has no const accessor
    // and is fixed by the chain, and was already committed at that value before this wave.
    //
    // These pin in a CASCADE and that is deliberate. Because the class now carries no padding
    // members at all, a wrong element stride anywhere does not fail one assert -- it fails that
    // queue's pin and every pin after it. A gate that can only ever fail in one place is a gate
    // that has not been tested; this one was tamper-tested (see the wave log) and the tamper
    // produced exactly the cascade described.
    void InputBuffer::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer, mfTimeStep)      == 0x0004, "mfTimeStep @ +0x0004");
        static_assert(offsetof(InputBuffer, muMaxIterations) == 0x0008, "muMaxIterations @ +0x0008");

        // ⚠️⚠️ RE-SPELLED 2026-08-05 (the rigid-body drain group). Until then all nineteen
        // pins were ABSOLUTE X360 byte offsets, and they held because every event type was
        // pointer-free -- the host InputBuffer was byte-for-byte the console one. Promoting
        // InUpdateRigidBody's payload to the real rw::physics::RigidBody (BY VALUE, per its
        // DWARF) makes queue #2's element legitimately WIDER on x64 (the body's five pointer
        // lanes widen), which shifts every later member by 200 * (sizeof(RigidBody) - 176).
        // This is a RUNTIME IO buffer, sizeof-driven end to end -- NOT a serialized record,
        // so [[serialized slots stay 32-bit]] does not apply and forcing the console offsets
        // onto the host would be pinning the WRONG layout in place (the exact
        // console-offset-embed_check trap, in reverse).
        //
        // The gate keeps its strength in three parts:
        //   * the two members BEFORE the widened queue keep their ABSOLUTE X360 pins (their
        //     host offsets still equal the console's);
        //   * every element stride is pinned against its own X360 constant in
        //     CgsPhysicsSimulationIO_Events.h (InUpdateRigidBody's as the adjacency form);
        //   * the FULL eighteen-seam gapless chain below enforces order + no padding, which
        //     together with the stride pins reproduces every console constant (quoted in the
        //     messages) on a 4-byte-pointer target.
        // ⚠️ The DIVISION OF LABOUR is exact, and was tamper-MEASURED, not assumed: widening
        // one element (InSetRigidBodySpy 16 -> 32) fails ONLY its Events.h stride pin (35
        // C2338s) -- the chain re-lays out consistently and stays green, BY DESIGN. What the
        // chain alone catches is member REORDERING and inserted padding (tamper: swapping two
        // queue members fails the seams around them). Neither gate subsumes the other.
        static_assert(offsetof(InputBuffer, mAddRigidBodyQueue)    ==    16, "mAddRigidBodyQueue @0x10      (GetAddRigidBodyQueue const @0x8289E408  addi r3,r28,0x10)");
        static_assert(offsetof(InputBuffer, mUpdateRigidBodyQueue) == 38432, "mUpdateRigidBodyQueue @0x9620 (accessor @0x8289E4B0  addis 1 / addi -0x69E0; host == console, InAddRigidBody is pointer-free)");

        static_assert(offsetof(InputBuffer, mApplyForceQueue)             == offsetof(InputBuffer, mUpdateRigidBodyQueue)        + sizeof(InputBuffer::mUpdateRigidBodyQueue),        "update-rb -> apply-force gapless        (X360 @0x12C30, accessor @0x8289E558  addis 1 / addi  0x2C30)");
        static_assert(offsetof(InputBuffer, mChangeRigidBodyInertiaQueue) == offsetof(InputBuffer, mApplyForceQueue)             + sizeof(InputBuffer::mApplyForceQueue),             "apply-force -> change-inertia gapless   (X360 @0x14B80, accessor @0x8259EE80 -- the retracted one)");
        static_assert(offsetof(InputBuffer, mSetRigidBodySpyQueue)        == offsetof(InputBuffer, mChangeRigidBodyInertiaQueue) + sizeof(InputBuffer::mChangeRigidBodyInertiaQueue), "change-inertia -> set-rb-spy gapless    (X360 @0x18A10, accessor @0x8289E600  addis 2 / addi -0x75F0)");
        static_assert(offsetof(InputBuffer, mRemoveRigidBodyQueue)        == offsetof(InputBuffer, mSetRigidBodySpyQueue)        + sizeof(InputBuffer::mSetRigidBodySpyQueue),        "set-rb-spy -> remove-rb gapless         (X360 @0x196A0, accessor @0x8289E6A8 -- export HOLE, headless)");
        static_assert(offsetof(InputBuffer, mRemoveAllRigidBodiesQueue)   == offsetof(InputBuffer, mRemoveRigidBodyQueue)        + sizeof(InputBuffer::mRemoveRigidBodyQueue),        "remove-rb -> remove-all gapless         (X360 @0x1A330, accessor @0x8289E750  addis 2 / addi -0x5CD0)");
        // ⚠️ The ONE seam with alignment slack, on BOTH targets: the remove-all queue ends
        // 16-unaligned (its element is a bare u8) and the next queue is 16-aligned. Console:
        // 107312 + 20 -> pad to 107344. The rounded form is the exact claim; a gapless form
        // here would be inventing a layout neither target has.
        static_assert(offsetof(InputBuffer, mAddContactQueue)             == ((offsetof(InputBuffer, mRemoveAllRigidBodiesQueue) + sizeof(InputBuffer::mRemoveAllRigidBodiesQueue) + 15) / 16) * 16,
                                                                                                                                                                                      "remove-all -> add-contact, padded to 16 (X360 @0x1A350, accessor @0x8289E7F8  addis 2 / addi -0x5CB0)");
        static_assert(offsetof(InputBuffer, mAddJointQueue)               == offsetof(InputBuffer, mAddContactQueue)             + sizeof(InputBuffer::mAddContactQueue),             "add-contact -> add-joint gapless        (X360 @0x2E360, accessor @0x8289E8A0  addis 3 / addi -0x1CA0)");
        static_assert(offsetof(InputBuffer, mRemoveJointQueue)            == offsetof(InputBuffer, mAddJointQueue)               + sizeof(InputBuffer::mAddJointQueue),               "add-joint -> remove-joint gapless       (X360 @0x2FE70, accessor @0x8289E948  addis 3 / addi -0x190)");
        static_assert(offsetof(InputBuffer, mUpdateJointFramesQueue)      == offsetof(InputBuffer, mRemoveJointQueue)            + sizeof(InputBuffer::mRemoveJointQueue),            "remove-joint -> joint-frames gapless    (X360 @0x2FFA0, accessor @0x8289E9F0  addis 3 / addi -0x60)");
        static_assert(offsetof(InputBuffer, mUpdateJointLimitsQueue)      == offsetof(InputBuffer, mUpdateJointFramesQueue)      + sizeof(InputBuffer::mUpdateJointFramesQueue),      "joint-frames -> joint-limits gapless    (X360 @0x30D30, accessor @0x8289EA98  addis 3 / addi  0xD30)");
        static_assert(offsetof(InputBuffer, mSetJointSpyQueue)            == offsetof(InputBuffer, mUpdateJointLimitsQueue)      + sizeof(InputBuffer::mUpdateJointLimitsQueue),      "joint-limits -> joint-spy gapless       (X360 @0x31880, accessor @0x8289EB40  addis 3 / addi  0x1880)");
        static_assert(offsetof(InputBuffer, mAddDriveQueue)               == offsetof(InputBuffer, mSetJointSpyQueue)            + sizeof(InputBuffer::mSetJointSpyQueue),            "joint-spy -> add-drive gapless          (X360 @0x31AD0, accessor @0x8289EBE8  addis 3 / addi  0x1AD0)");
        static_assert(offsetof(InputBuffer, mRemoveDriveQueue)            == offsetof(InputBuffer, mAddDriveQueue)               + sizeof(InputBuffer::mAddDriveQueue),               "add-drive -> remove-drive gapless       (X360 @0x31B70, accessor @0x8289EC90  addis 3 / addi  0x1B70)");
        static_assert(offsetof(InputBuffer, mUpdateDriveFramesQueue)      == offsetof(InputBuffer, mRemoveDriveQueue)            + sizeof(InputBuffer::mRemoveDriveQueue),            "remove-drive -> drive-frames gapless    (X360 @0x31B90, accessor @0x8289ED38  addis 3 / addi  0x1B90)");
        static_assert(offsetof(InputBuffer, mUpdateDriveDynamicsQueue)    == offsetof(InputBuffer, mUpdateDriveFramesQueue)      + sizeof(InputBuffer::mUpdateDriveFramesQueue),      "drive-frames -> drive-dynamics gapless  (X360 @0x31BF0, accessor @0x8289EDE0  addis 3 / addi  0x1BF0)");
        static_assert(offsetof(InputBuffer, mSetDriveSpyQueue)            == offsetof(InputBuffer, mUpdateDriveDynamicsQueue)    + sizeof(InputBuffer::mUpdateDriveDynamicsQueue),    "drive-dynamics -> drive-spy gapless     (X360 @0x31C30, accessor @0x8289EE88 -- export HOLE, headless)");
        static_assert(offsetof(InputBuffer, mUpdateExternalBodyQueue)     == offsetof(InputBuffer, mSetDriveSpyQueue)            + sizeof(InputBuffer::mSetDriveSpyQueue),            "drive-spy -> external-body gapless      (X360 @0x31C50, accessor @0x8289EF30  addis 3 / addi  0x1C50)");

        // The widened queue itself, stated in the only form that is true on both targets:
        // 16-byte queue header + 200 elements of (16-byte id slot + the RigidBody). On the
        // console that evaluates to 16 + 200*192 == 38416 (the 0x9620->0x12C30 gap).
        static_assert(sizeof(InputBuffer::mUpdateRigidBodyQueue) == 16 + 200 * (16 + sizeof(rw::physics::RigidBody)),
                      "update-rb queue = 16 + 200*(16 + sizeof(RigidBody))  (X360: 16 + 200*192)");

        // ADJACENCY, spelled as `sizeof(InputBuffer::member)` and NOT as `sizeof(SomeQueueType)`.
        // The difference is the whole point: a type-spelled gate is invariant under re-typing the
        // MEMBER and would pass with the member pointing at the wrong queue type entirely. These
        // four are the queues whose element strides were wrong until this wave, so they are the
        // ones most worth stating twice.
        static_assert(sizeof(InputBuffer::mUpdateJointFramesQueue)   == 3472, "16 + 36*96 -- the 96 is (199984-196512-16)/36");
        static_assert(sizeof(InputBuffer::mUpdateJointLimitsQueue)   == 2896, "16 + 36*80 -- the 80 is (202880-199984-16)/36");
        static_assert(sizeof(InputBuffer::mUpdateDriveFramesQueue)   ==   96, "16 +  1*80 -- the 80 is (203760-203664-16)/1");
        static_assert(sizeof(InputBuffer::mUpdateDriveDynamicsQueue) ==   64, "16 +  1*48 -- the 48 is (203824-203760-16)/1");
        // ... and the same four spelled a THIRD way: each member's offset MUST equal the previous
        // member's offset plus that previous member's own size. This is the form that catches a
        // compensating pair of errors, which absolute pins alone cannot.
        static_assert(offsetof(InputBuffer, mUpdateJointLimitsQueue)   == offsetof(InputBuffer, mUpdateJointFramesQueue)   + sizeof(InputBuffer::mUpdateJointFramesQueue),   "joint frames -> joint limits is gapless");
        static_assert(offsetof(InputBuffer, mSetJointSpyQueue)         == offsetof(InputBuffer, mUpdateJointLimitsQueue)   + sizeof(InputBuffer::mUpdateJointLimitsQueue),   "joint limits -> joint spy is gapless");
        static_assert(offsetof(InputBuffer, mUpdateDriveDynamicsQueue) == offsetof(InputBuffer, mUpdateDriveFramesQueue)   + sizeof(InputBuffer::mUpdateDriveFramesQueue),   "drive frames -> drive dynamics is gapless");
        static_assert(offsetof(InputBuffer, mSetDriveSpyQueue)         == offsetof(InputBuffer, mUpdateDriveDynamicsQueue) + sizeof(InputBuffer::mUpdateDriveDynamicsQueue), "drive dynamics -> drive spy is gapless");
    }

    // -------- explicit instantiations: InputBuffer::AppendXxxQueue<N> --------
    // Force the out-of-line emission the X360 ARTIST build produced for each source-queue capacity
    // (generic bodies live in CgsPhysicsSimulationModuleIO.h). Templated on the SOURCE queue's N.
    template bool InputBuffer::AppendAddJointQueue<10>(CgsModule::EventQueue<InAddJoint, 10>*);                          // @0x825A8598
    template bool InputBuffer::AppendAddRigidBodyQueue<1>(CgsModule::EventQueue<InAddRigidBody, 1>*);                    // @0x825A8298
    template bool InputBuffer::AppendAddRigidBodyQueue<50>(CgsModule::EventQueue<InAddRigidBody, 50>*);                  // @0x825A84C0
    template bool InputBuffer::AppendChangeRigidBodyInertiaQueue<200>(CgsModule::EventQueue<InChangeRigidBodyInertia, 200>*); // @0x825AC2E8
    template bool InputBuffer::AppendRemoveJointQueue<10>(const CgsModule::EventQueue<InRemoveJoint, 10>*);   // const src per the console `PBV` mangling                    // @0x825A8678
    template bool InputBuffer::AppendRemoveRigidBodyQueue<50>(CgsModule::EventQueue<InRemoveRigidBody, 50>*);            // @0x825A83E0
    template bool InputBuffer::AppendUpdateExternalBodyQueue<60>(CgsModule::EventQueue<InUpdateExternalBody, 60>*);      // @0x825AC208

    // X360 0x8289E338. Read-lock guard, then asserts muMaxIterations > 0 before returning it.
    int InputBuffer::GetMaxIterations() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        CGS_ASSERT(muMaxIterations > 0, "muMaxIterations > 0");
        return muMaxIterations;
    }

    // X360 0x8259ECD8. Write-lock guard (faithful to the asm's bit-3 test), then asserts the
    // stored time step is positive before returning it in f1. NON-const since 2026-08-05 --
    // see the overload note in CgsPhysicsSimulationModuleIO.h.
    f32 InputBuffer::GetTimeStep()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(mfTimeStep > 0.0f, "mfTimeStep > 0.0f");
        return mfTimeStep;
    }

    // X360 0x8289E260 (54 instructions) -- the READ-lock const twin (`lbz 0(r27)` +
    // `extrwi r11,r11,1,27` == LSB bit 4 == eStatusLockedForRead; both asserts cite
    // d:\p4 CgsPhysicsSimulationModuleIO.h:833/:834 in the image). This is the flavour
    // PhysicsSimulationModule::Update @0x828A74D0 calls between LockForRead/UnlockForRead
    // to fetch the step it feeds rw::physics::Simulation::SimulationUpdate.
    f32 InputBuffer::GetTimeStep() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        CGS_ASSERT(mfTimeStep > 0.0f, "mfTimeStep > 0.0f");
        return mfTimeStep;
    }

    // X360 0x8259EDB0. Write-lock guarded setter for muMaxIterations.
    void InputBuffer::SetMaxIterations(int luMaxIterations)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(luMaxIterations > 0, "luMaxIterations > 0");
        muMaxIterations = luMaxIterations;
    }

    // X360 0x8259EBF8. Write-lock guarded setter for mfTimeStep.
    void InputBuffer::SetTimeStep(f32 lfTimeStep)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lfTimeStep > 0.0f, "lfTimeStep > 0.0f");
        mfTimeStep = lfTimeStep;
    }

    // X360 0x825BCE08. Write-lock guarded (bit 3; faithful to the asm) accessor returning the
    // embedded add-rigid-body request queue (this + 0x10, the first embedded queue member). No
    // bounds check. Consumed by Deformation/Props AddToSim / AddPropToSim / CreatePart.
    InputBuffer::InAddRigidBodyQueue* InputBuffer::GetAddRigidBodyQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mAddRigidBodyQueue;
    }

    // X360 0x8289E408 (41 instructions) -- the CONST overload, and an .ida-exports HOLE
    // recovered headless out of BURNOUT_X360_ARTIST.XEX.i64 (task #140). Note the lock bit
    // DIFFERS from the non-const twin above: `lbz 0(r28)` + `extrwi r11,r11,1,27` is
    // MSB0 bit 27 == LSB bit 4 == eStatusLockedForRead, and the rodata string it fires is
    // "Not locked for reading\n" (CgsPhysicsSimulationModuleIO.h:893). Then the identical
    // `addi r3, r28, 0x10`. Consumed by ProcessAddRigidBodyQueue @0x828A2708.
    const InputBuffer::InAddRigidBodyQueue* InputBuffer::GetAddRigidBodyQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mAddRigidBodyQueue;
    }

    // -------- the other eighteen const queue accessors (task #142) --------
    // One per input queue, in the image's own address order. Each is the identical
    // 42-instruction body: read-lock guard firing "Not locked for reading\n", then return the
    // member's address. Sixteen were read out of .ida-exports; @0x8289E6A8 and @0x8289EE88 are
    // holes and were pulled headless from BURNOUT_X360_ARTIST.XEX.i64.
    //
    // ⚠️ These are the drains' entry points and nothing calls them YET -- ProcessInputBuffers
    // and eighteen of the nineteen drains are still unwritten. /OPT:REF will strip every one of
    // them out of the exe until a drain lands. That is stated plainly rather than implied: this
    // is link-level completeness, not execution.
    const InputBuffer::InUpdateRigidBodyQueue* InputBuffer::GetUpdateRigidBodyQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mUpdateRigidBodyQueue; }

    const InputBuffer::InApplyForceQueue* InputBuffer::GetApplyForceQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mApplyForceQueue; }

    const InputBuffer::InSetRigidBodySpyQueue* InputBuffer::GetSetRigidBodySpyQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mSetRigidBodySpyQueue; }

    const InputBuffer::InRemoveRigidBodyQueue* InputBuffer::GetRemoveRigidBodyQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mRemoveRigidBodyQueue; }

    const InputBuffer::InRemoveAllRigidBodiesQueue* InputBuffer::GetRemoveAllRigidBodiesQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mRemoveAllRigidBodiesQueue; }

    const InputBuffer::InAddContactQueue* InputBuffer::GetAddContactQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mAddContactQueue; }

    // ⭐ ADDED 2026-08-06 (BridgeContactsToSimulation wave). The WRITE-side accessor
    // @0x8259EF28: same 42-instruction accessor shape as the const block above but guarded on
    // the WRITE lock bit -- the PS3 DecFIGS twin's assert path bakes "Not locked for writing\n"
    // + CgsPhysicsSimulationModuleIO.h:1073, and the X360 body's `extrwi` picks status bit 3.
    InputBuffer::InAddContactQueue* InputBuffer::GetAddContactQueue()
    { CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n"); return &mAddContactQueue; }

    const InputBuffer::InAddJointQueue* InputBuffer::GetAddJointQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mAddJointQueue; }

    const InputBuffer::InRemoveJointQueue* InputBuffer::GetRemoveJointQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mRemoveJointQueue; }

    const InputBuffer::InUpdateJointFramesQueue* InputBuffer::GetUpdateJointFramesQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mUpdateJointFramesQueue; }

    const InputBuffer::InUpdateJointLimitsQueue* InputBuffer::GetUpdateJointLimitsQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mUpdateJointLimitsQueue; }

    const InputBuffer::InSetJointSpyQueue* InputBuffer::GetSetJointSpyQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mSetJointSpyQueue; }

    const InputBuffer::InAddDriveQueue* InputBuffer::GetAddDriveQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mAddDriveQueue; }

    const InputBuffer::InRemoveDriveQueue* InputBuffer::GetRemoveDriveQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mRemoveDriveQueue; }

    const InputBuffer::InUpdateDriveFramesQueue* InputBuffer::GetUpdateDriveFramesQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mUpdateDriveFramesQueue; }

    const InputBuffer::InUpdateDriveDynamicsQueue* InputBuffer::GetUpdateDriveDynamicsQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mUpdateDriveDynamicsQueue; }

    const InputBuffer::InSetDriveSpyQueue* InputBuffer::GetSetDriveSpyQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mSetDriveSpyQueue; }

    const InputBuffer::InUpdateExternalBodyQueue* InputBuffer::GetUpdateExternalBodyQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mUpdateExternalBodyQueue; }

    // ⭐ 2026-08-05 (the rigid-body drain group): the NINETEENTH const accessor, whose
    // committed nonexistence claim was retracted -- see the header. Its body @0x8259EE80 is
    // the same 42-instruction shape as the eighteen above (read-lock guard, this header's
    // line 914, then `addis r3,r28,1 / addi r3,r3,0x4B80` == &mChangeRigidBodyInertiaQueue);
    // it merely was not EMITTED inside the uniform 0x8289E408+k*0xA8 block.
    const InputBuffer::InChangeRigidBodyInertiaQueue* InputBuffer::GetChangeRigidBodyInertiaQueue() const
    { CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n"); return &mChangeRigidBodyInertiaQueue; }

    // ---------------------------------------------------------------------------------------------
    // Construct @0x828A71B8 -- LANDED 2026-08-11 (create-drain wave, conductor). See the header
    // banner for why this was missing and what it cost (a NULL-mpEvents memcpy AV on the first
    // rigid-body append the create drain ever produced). The console body verbatim: status byte
    // = 1, the nineteen queue Constructs at their pinned offsets (each wires the base
    // BaseEventQueue::mpEvents at its inline maEvents and zeroes the length), both scalars,
    // then the Clear() tail. Console offsets cited per line; every one matches the
    // _AssertLayout pin for that member.
    // ---------------------------------------------------------------------------------------------
    void InputBuffer::Construct()
    {
        IOBuffer::Construct();                        // *this = 1 (clear flags + eStatusConstructed)

        mAddRigidBodyQueue.Construct();               // +16
        mUpdateRigidBodyQueue.Construct();            // +38432
        mApplyForceQueue.Construct();                 // +76848
        mChangeRigidBodyInertiaQueue.Construct();     // +84864
        mSetRigidBodySpyQueue.Construct();            // +100880
        mRemoveRigidBodyQueue.Construct();            // +104096
        mRemoveAllRigidBodiesQueue.Construct();       // +107312
        mAddContactQueue.Construct();                 // +107344 (InAddPotentialContact_1024)
        mAddJointQueue.Construct();                   // +189280
        mRemoveJointQueue.Construct();                // +196208
        mUpdateJointFramesQueue.Construct();          // +196512
        mUpdateJointLimitsQueue.Construct();          // +199984
        mSetJointSpyQueue.Construct();                // +202880
        mAddDriveQueue.Construct();                   // +203472
        mRemoveDriveQueue.Construct();                // +203632
        mUpdateDriveFramesQueue.Construct();          // +203664
        mUpdateDriveDynamicsQueue.Construct();        // +203760
        mSetDriveSpyQueue.Construct();                // +203824
        mUpdateExternalBodyQueue.Construct();         // +203856

        mfTimeStep      = 0.0f;                       // *(a1+4)  = 0.0
        muMaxIterations = 0;                          // *(a1+8)  = 0

        Clear();                                      // the console's tail call
    }

    // ---------------------------------------------------------------------------------------------
    // Clear @0x828A1A88 -- mfTimeStep = 0 and every queue length = 0 (each console store lands at
    // queueOffset+8 == BaseEventQueue::miLength). ⚠️ muMaxIterations is NOT touched -- console.
    // ---------------------------------------------------------------------------------------------
    void InputBuffer::Clear()
    {
        mfTimeStep = 0.0f;                            // *(a1+4) = 0.0

        mAddRigidBodyQueue.Clear();                   // +24     (16+8)
        mChangeRigidBodyInertiaQueue.Clear();         // +84872
        mUpdateRigidBodyQueue.Clear();                // +38440
        mApplyForceQueue.Clear();                     // +76856
        mSetRigidBodySpyQueue.Clear();                // +100888
        mRemoveAllRigidBodiesQueue.Clear();           // +107320
        mRemoveRigidBodyQueue.Clear();                // +104104
        mAddContactQueue.Clear();                     // +107352
        mAddJointQueue.Clear();                       // +189288
        mRemoveJointQueue.Clear();                    // +196216
        mUpdateJointFramesQueue.Clear();              // +196520
        mUpdateJointLimitsQueue.Clear();              // +199992
        mSetJointSpyQueue.Clear();                    // +202888
        mAddDriveQueue.Clear();                       // +203480
        mRemoveDriveQueue.Clear();                    // +203640
        mUpdateDriveFramesQueue.Clear();              // +203672
        mUpdateDriveDynamicsQueue.Clear();            // +203768
        mSetDriveSpyQueue.Clear();                    // +203832
        mUpdateExternalBodyQueue.Clear();             // +203864
    }
}
}
