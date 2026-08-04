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
    void InputBuffer::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer, mfTimeStep)         == 0x0004, "mfTimeStep @ +0x0004");
        static_assert(offsetof(InputBuffer, muMaxIterations)    == 0x0008, "muMaxIterations @ +0x0008");
        static_assert(offsetof(InputBuffer, mAddRigidBodyQueue) == 0x0010, "mAddRigidBodyQueue @ +0x0010");
        // AppendXxxQueue destination offsets, each read off the method's `addi rD, this, off` in the
        // X360 asm (self-validating the padding chain -- a wrong pad or dest capacity fails here).
        static_assert(offsetof(InputBuffer, mChangeRigidBodyInertiaQueue) == 84864,  "mChangeRigidBodyInertiaQueue @ 0x14B80 (AppendChangeRigidBodyInertiaQueue @0x825AC2E8)");
        static_assert(offsetof(InputBuffer, mRemoveRigidBodyQueue)        == 104096, "mRemoveRigidBodyQueue @ 0x196A0 (AppendRemoveRigidBodyQueue @0x825A83E0)");
        static_assert(offsetof(InputBuffer, mAddJointQueue)              == 189280, "mAddJointQueue @ 0x2E360 (AppendAddJointQueue @0x825A8598)");
        static_assert(offsetof(InputBuffer, mRemoveJointQueue)           == 196208, "mRemoveJointQueue @ 0x2FE70 (AppendRemoveJointQueue @0x825A8678)");
        static_assert(offsetof(InputBuffer, mUpdateExternalBodyQueue)    == 203856, "mUpdateExternalBodyQueue @ 0x31C50 (AppendUpdateExternalBodyQueue @0x825AC208)");
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
}
}
