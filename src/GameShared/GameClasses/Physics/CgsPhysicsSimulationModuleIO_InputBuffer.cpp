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

        //                                                                   X360 offset      attested by
        static_assert(offsetof(InputBuffer, mAddRigidBodyQueue)           ==     16, "mAddRigidBodyQueue @0x10       (GetAddRigidBodyQueue const @0x8289E408  addi r3,r28,0x10)");
        static_assert(offsetof(InputBuffer, mUpdateRigidBodyQueue)        ==  38432, "mUpdateRigidBodyQueue @0x9620  (accessor @0x8289E4B0  addis 1 / addi -0x69E0)");
        static_assert(offsetof(InputBuffer, mApplyForceQueue)             ==  76848, "mApplyForceQueue @0x12C30      (accessor @0x8289E558  addis 1 / addi  0x2C30)");
        static_assert(offsetof(InputBuffer, mChangeRigidBodyInertiaQueue) ==  84864, "mChangeRigidBodyInertiaQueue @0x14B80 (no const accessor; chain + AppendChangeRigidBodyInertiaQueue @0x825AC2E8)");
        static_assert(offsetof(InputBuffer, mSetRigidBodySpyQueue)        == 100880, "mSetRigidBodySpyQueue @0x18A10 (accessor @0x8289E600  addis 2 / addi -0x75F0)");
        static_assert(offsetof(InputBuffer, mRemoveRigidBodyQueue)        == 104096, "mRemoveRigidBodyQueue @0x196A0 (accessor @0x8289E6A8  addis 2 / addi -0x6960 -- export HOLE, headless)");
        static_assert(offsetof(InputBuffer, mRemoveAllRigidBodiesQueue)   == 107312, "mRemoveAllRigidBodiesQueue @0x1A330 (accessor @0x8289E750  addis 2 / addi -0x5CD0)");
        static_assert(offsetof(InputBuffer, mAddContactQueue)             == 107344, "mAddContactQueue @0x1A350      (accessor @0x8289E7F8  addis 2 / addi -0x5CB0)");
        static_assert(offsetof(InputBuffer, mAddJointQueue)               == 189280, "mAddJointQueue @0x2E360        (accessor @0x8289E8A0  addis 3 / addi -0x1CA0)");
        static_assert(offsetof(InputBuffer, mRemoveJointQueue)            == 196208, "mRemoveJointQueue @0x2FE70     (accessor @0x8289E948  addis 3 / addi -0x190)");
        static_assert(offsetof(InputBuffer, mUpdateJointFramesQueue)      == 196512, "mUpdateJointFramesQueue @0x2FFA0 (accessor @0x8289E9F0  addis 3 / addi -0x60)");
        static_assert(offsetof(InputBuffer, mUpdateJointLimitsQueue)      == 199984, "mUpdateJointLimitsQueue @0x30D30 (accessor @0x8289EA98  addis 3 / addi  0xD30)");
        static_assert(offsetof(InputBuffer, mSetJointSpyQueue)            == 202880, "mSetJointSpyQueue @0x31880     (accessor @0x8289EB40  addis 3 / addi  0x1880)");
        static_assert(offsetof(InputBuffer, mAddDriveQueue)               == 203472, "mAddDriveQueue @0x31AD0        (accessor @0x8289EBE8  addis 3 / addi  0x1AD0)");
        static_assert(offsetof(InputBuffer, mRemoveDriveQueue)            == 203632, "mRemoveDriveQueue @0x31B70     (accessor @0x8289EC90  addis 3 / addi  0x1B70)");
        static_assert(offsetof(InputBuffer, mUpdateDriveFramesQueue)      == 203664, "mUpdateDriveFramesQueue @0x31B90 (accessor @0x8289ED38  addis 3 / addi  0x1B90)");
        static_assert(offsetof(InputBuffer, mUpdateDriveDynamicsQueue)    == 203760, "mUpdateDriveDynamicsQueue @0x31BF0 (accessor @0x8289EDE0  addis 3 / addi  0x1BF0)");
        static_assert(offsetof(InputBuffer, mSetDriveSpyQueue)            == 203824, "mSetDriveSpyQueue @0x31C30     (accessor @0x8289EE88  addis 3 / addi  0x1C30 -- export HOLE, headless)");
        static_assert(offsetof(InputBuffer, mUpdateExternalBodyQueue)     == 203856, "mUpdateExternalBodyQueue @0x31C50 (accessor @0x8289EF30  addis 3 / addi  0x1C50; AppendUpdateExternalBodyQueue @0x825AC208)");

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
    template bool InputBuffer::AppendRemoveJointQueue<10>(CgsModule::EventQueue<InRemoveJoint, 10>*);                    // @0x825A8678
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
    // stored time step is positive before returning it in f1.
    f32 InputBuffer::GetTimeStep() const
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
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
}
}
