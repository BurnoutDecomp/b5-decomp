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
        static_assert(offsetof(InputBuffer, mfTimeStep)      == 0x0004, "mfTimeStep @ +0x0004");
        static_assert(offsetof(InputBuffer, muMaxIterations) == 0x0008, "muMaxIterations @ +0x0008");
    }

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
}
}
