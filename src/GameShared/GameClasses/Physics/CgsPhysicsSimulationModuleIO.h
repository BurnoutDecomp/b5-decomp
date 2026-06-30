#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer (1-byte FlagSet status base)

// CgsPhysics::PhysicsSimulationIO - the per-frame IO buffers the physics-simulation module
// exchanges with the game. This header homes MINIMAL leading slices of the InputBuffer and
// OutputBuffer aggregates: just the scalar control fields the X360-attested getter/setter
// bodies touch (the two body .cpps next to this header), at their recovered byte offsets.
// Both derive CgsModule::IOBuffer (a 1-byte FlagSet status base: bit 3 == locked-for-write,
// bit 4 == locked-for-read), mirroring the committed CgsGui::ModelIO buffers.
//
// These are deliberately INCOMPLETE class slices. The full InputBuffer / OutputBuffer carry a
// large fan of embedded event queues after these fields (see CgsPhysicsSimulationIO_Events.h
// and the EventQueue_*/_Construct TUs); a future InputBuffer/OutputBuffer::Construct TU will
// GROW this header in place with those queue members at their own offsets -- it MUST NOT
// redefine these scalars (ODR). The scalar offsets below are pinned with offsetof
// static_asserts in never-called _AssertLayout() members.
//
// InputBuffer  (derives CgsModule::IOBuffer):
//   +0x0000  CgsModule::IOBuffer status (1-byte FlagSet; +0x1..+0x3 pad)
//   +0x0004  mfTimeStep        f32  (GetTimeStep @0x8259ECD8 lfs 4; SetTimeStep @0x8259EBF8 stfs 4)
//   +0x0008  muMaxIterations   u32  (GetMaxIterations @0x8289E338 lwz 8; SetMaxIterations @0x8259EDB0 stw 8)
// OutputBuffer (derives CgsModule::IOBuffer):
//   +0x0000  CgsModule::IOBuffer status (1-byte FlagSet; +0x1..+0x3 pad)
//   +0x0004  mfTimeStepUsed       f32  (GetTimeStepUsed @0x825BD0A8 lfs 4; SetTimeStepUsed @0x8289EFD8 stfs 4)
//   +0x0008  muMaxIterationsUsed  s32  (SetMaxIterationsUsed @0x8289F088 stw 8)
namespace CgsPhysics
{
namespace PhysicsSimulationIO
{
    // Minimal leading slice of the physics-simulation input buffer.
    struct InputBuffer : public CgsModule::IOBuffer
    {
        // X360 0x8289E338: read-lock (bit 4) guarded; asserts muMaxIterations > 0, returns it.
        int  GetMaxIterations() const;
        // X360 0x8259ECD8: write-lock (bit 3) guarded (NOTE: a Get that tests the WRITE bit --
        // faithful to the asm, intentionally NOT "fixed"); asserts mfTimeStep > 0, returns it.
        f32  GetTimeStep() const;
        // X360 0x8259EDB0: write-lock (bit 3) guarded; asserts the arg > 0, stores muMaxIterations.
        void SetMaxIterations(int luMaxIterations);
        // X360 0x8259EBF8: write-lock (bit 3) guarded; asserts the arg > 0, stores mfTimeStep.
        void SetTimeStep(f32 lfTimeStep);

        // Byte-offset pins (never called).
        static void _AssertLayout();

    private:
        u8  maStatusPad[3];   // +0x0001..+0x0003 (force mfTimeStep to +0x0004 like the X360)
        f32 mfTimeStep;       // +0x0004
        u32 muMaxIterations;  // +0x0008
    };

    // Minimal leading slice of the physics-simulation output buffer.
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // X360 0x825BD0A8: read-lock (bit 4) guarded; returns the post-step time-step-used scalar.
        f32  GetTimeStepUsed() const;
        // X360 0x8289F088: write-lock (bit 3) guarded; stores muMaxIterationsUsed.
        void SetMaxIterationsUsed(s32 luMaxIterationsUsed);
        // X360 0x8289EFD8: write-lock (bit 3) guarded; stores mfTimeStepUsed.
        void SetTimeStepUsed(f32 fTimeStepUsed);

        // Byte-offset pins (never called).
        static void _AssertLayout();

    private:
        u8  maStatusPad[3];        // +0x0001..+0x0003 (force mfTimeStepUsed to +0x0004 like the X360)
        f32 mfTimeStepUsed;        // +0x0004
        s32 muMaxIterationsUsed;   // +0x0008
    };
}
}
