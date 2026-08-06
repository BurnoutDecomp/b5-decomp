#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsPhysics::PhysicsSimulationIO::OutputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. The X360-baked CgsPhysicsSimulationModuleIO.h file/line args are
// discarded per project convention; the assert strings are X360 rodata, reproduced verbatim
// (including the trailing newline on the lock-status messages).
//
//   GetTimeStepUsed()       const  @ 0x825BD0A8
//       -> read-lock (bit 4) guard, return mfTimeStepUsed (this+4)
//   SetMaxIterationsUsed(s32)      @ 0x8289F088
//       -> write-lock (bit 3) guard, store muMaxIterationsUsed (this+8)
//   SetTimeStepUsed(f32)           @ 0x8289EFD8
//       -> write-lock (bit 3) guard, store mfTimeStepUsed (this+4)
//
// X360 lock-bit notes (faithful to asm):
//   - read lock  : lbz 0(this); extrwi r11,r11,1,27 -> MSB0 bit 27 == LSB bit 4 == eStatusLockedForRead.
//   - write lock : lbz 0(this); extrwi r11,r11,1,28 -> MSB0 bit 28 == LSB bit 3 == eStatusLockedForWrite.

namespace CgsPhysics
{
namespace PhysicsSimulationIO
{
    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, mfTimeStepUsed)        == 0x0004,  "mfTimeStepUsed @ +0x0004");
        static_assert(offsetof(OutputBuffer, muMaxIterationsUsed)   == 0x0008,  "muMaxIterationsUsed @ +0x0008");
        static_assert(offsetof(OutputBuffer, mUpdateRigidBodyQueue) == 0x0010,  "mUpdateRigidBodyQueue @ +0x0010");
        // ⚠️ ADJACENCY PINS from here on (2026-08-06): OutUpdateRigidBody carries a full
        // rw::physics::RigidBody whose five pointer lanes widen on x64, so the queues after it
        // sit at console +0x9620/+0x1F430/+0x20040 ONLY on a 4-byte-pointer target. Each pin
        // states the console chain in host-tracking form; a transposed pair still fails.
        static_assert(offsetof(OutputBuffer, mContactSpyQueue) ==
                      offsetof(OutputBuffer, mUpdateRigidBodyQueue) + sizeof(OutUpdateRigidBodyQueue),
                      "mContactSpyQueue right after mUpdateRigidBodyQueue (console +0x9620 == 0x10 + 16+200*192)");
        static_assert(offsetof(OutputBuffer, mJointSpyQueue) ==
                      offsetof(OutputBuffer, mContactSpyQueue) + sizeof(OutContactSpyQueue),
                      "mJointSpyQueue right after mContactSpyQueue (console +0x1F430 == 0x9620 + 16+800*112; GetJointSpyQueue @0x8259F1C8)");
        static_assert(offsetof(OutputBuffer, mDriveSpyQueue) ==
                      offsetof(OutputBuffer, mJointSpyQueue) + sizeof(OutJointSpyQueue),
                      "mDriveSpyQueue right after mJointSpyQueue (console +0x20040 == 0x1F430 + 16+64*48; GetDriveSpyQueue @0x8259F270)");
    }

    // X360 0x825BD0A8. Read-lock guarded; returns the post-step time-step-used scalar (this+4).
    f32 OutputBuffer::GetTimeStepUsed() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return mfTimeStepUsed;
    }

    // X360 0x8289F088. Write-lock guarded setter for muMaxIterationsUsed (this+8).
    void OutputBuffer::SetMaxIterationsUsed(s32 luMaxIterationsUsed)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        muMaxIterationsUsed = luMaxIterationsUsed;
    }

    // X360 0x8289EFD8. Write-lock guarded setter for mfTimeStepUsed (this+4).
    void OutputBuffer::SetTimeStepUsed(f32 fTimeStepUsed)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mfTimeStepUsed = fTimeStepUsed;
    }

    // X360 0x8259F120. Write-lock guarded (bit 3; faithful to the asm) accessor returning the
    // embedded contact-spy output queue (this + 0x9620 == +38432, the second embedded queue,
    // sitting right after mUpdateRigidBodyQueue == EventQueue<OutUpdateRigidBody,200> which
    // occupies [16, 16+16+200*192)=[16,38432)). No bounds check. Consumed by PhysicsModule::Update
    // / DeformationSensor::OutputContactSpy / AddContactSpiesToOutputQueue.
    OutputBuffer::OutContactSpyQueue* OutputBuffer::GetContactSpyQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mContactSpyQueue;
    }

    // X360 0x8259F078. READ-lock (bit 4) guarded const twin of the accessor above -- it sits
    // one 0xA8 slot BEFORE 0x8259F120 in the uniform spy-accessor block and returns the same
    // &mContactSpyQueue (`return a1 + 38432`), with the read-side "Not locked for reading\n"
    // tripwire (CgsPhysicsSimulationModuleIO.h:1366). ADDED 2026-08-06 (bridge de-facade wave):
    // its one caller is PhysicsModule::BridgeSimulationToOutput @0x825B0540, which drains the
    // spy queue out of the CONST sim-module output buffer.
    const OutputBuffer::OutContactSpyQueue* OutputBuffer::GetContactSpyQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mContactSpyQueue;
    }

    // X360 0x8289F130. Write-lock (bit 3) guarded accessor returning the first embedded
    // output queue, &mUpdateRigidBodyQueue (+0x10). Both OutUpdateRigidBody emitters
    // (AddActiveBodiesToOutputQueue @0x828A6CDC, ActivateAndFreezeAsNeeded @0x828A6DE8)
    // open with `bl sub_8289F130`.
    OutputBuffer::OutUpdateRigidBodyQueue* OutputBuffer::GetUpdateRigidBodyQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mUpdateRigidBodyQueue;
    }

    // X360 0x8259F1C8. Write-lock (bit 3) guarded accessor returning &mJointSpyQueue
    // (console +0x1F430 -- the middle entry of the 0x8259F120 + k*0xA8 spy-accessor block).
    // Sole in-scope caller: AddJointSpiesToOutputQueue @0x828A5900.
    OutputBuffer::OutJointSpyQueue* OutputBuffer::GetJointSpyQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mJointSpyQueue;
    }

    // X360 0x8259F270. Write-lock (bit 3) guarded accessor returning &mDriveSpyQueue
    // (console +0x20040). ⭐ RETYPED 2026-08-06 from the raw-byte-offset GetOutDriveSpyQueue
    // -- see the header note; the console offset stopped being the host offset the moment
    // OutUpdateRigidBody's payload widened, and the adjacency pin in _AssertLayout now
    // carries what the 0x20040 literal used to claim. (Sibling OutputBuffer::Destruct clears
    // console +131144 == this queue's miLength, confirming the base.)
    OutputBuffer::OutDriveSpyQueue* OutputBuffer::GetDriveSpyQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mDriveSpyQueue;
    }
}
}
