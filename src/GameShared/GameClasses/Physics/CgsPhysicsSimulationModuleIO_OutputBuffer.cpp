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
        static_assert(offsetof(OutputBuffer, mfTimeStepUsed)      == 0x0004, "mfTimeStepUsed @ +0x0004");
        static_assert(offsetof(OutputBuffer, muMaxIterationsUsed) == 0x0008, "muMaxIterationsUsed @ +0x0008");
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
}
}
