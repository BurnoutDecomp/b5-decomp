#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer (1-byte FlagSet status base)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                  // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h" // In/Out event payloads

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
    //
    // GROWN in place (ODR-safe, as this header's own banner anticipates): the first embedded
    // event queue after the scalar controls is mAddRigidBodyQueue == EventQueue<InAddRigidBody,200>
    // (DWARF :330). alignas(16) on the Event payload pads the 12-byte BaseEventQueue base to 16,
    // forcing muMaxIterations(+8) to pad up to the +0x10 queue start; EventQueue<T,N> == 16 + N*
    // sizeof(T). The remaining input queues are omitted (incomplete slice).
    struct InputBuffer : public CgsModule::IOBuffer
    {
        typedef CgsModule::EventQueue<InAddRigidBody, 200> InAddRigidBodyQueue;   // DWARF :330 (16 + 200*192)

        // X360 0x8289E338: read-lock (bit 4) guarded; asserts muMaxIterations > 0, returns it.
        int  GetMaxIterations() const;
        // X360 0x8259ECD8: write-lock (bit 3) guarded (NOTE: a Get that tests the WRITE bit --
        // faithful to the asm, intentionally NOT "fixed"); asserts mfTimeStep > 0, returns it.
        f32  GetTimeStep() const;
        // X360 0x8259EDB0: write-lock (bit 3) guarded; asserts the arg > 0, stores muMaxIterations.
        void SetMaxIterations(int luMaxIterations);
        // X360 0x8259EBF8: write-lock (bit 3) guarded; asserts the arg > 0, stores mfTimeStep.
        void SetTimeStep(f32 lfTimeStep);

        // X360 0x825BCE08: write-lock (bit 3) guarded; returns &mAddRigidBodyQueue (this+0x10).
        InAddRigidBodyQueue* GetAddRigidBodyQueue();

        // Byte-offset pins (never called).
        static void _AssertLayout();

    private:
        u8  maStatusPad[3];   // +0x0001..+0x0003 (force mfTimeStep to +0x0004 like the X360)
        f32 mfTimeStep;       // +0x0004
        u32 muMaxIterations;  // +0x0008  (pads to +0x0010 before the queue)
        InAddRigidBodyQueue mAddRigidBodyQueue;  // +0x0010 (first embedded queue)
        // ... remaining input queues omitted (incomplete slice) ...
    };

    // Minimal leading slice of the physics-simulation output buffer.
    //
    // GROWN in place (ODR-safe): mUpdateRigidBodyQueue == EventQueue<OutUpdateRigidBody,200>
    // (DWARF :713, [16 .. 38432)) is the first embedded queue at +0x10, then mContactSpyQueue ==
    // EventQueue<OutContactSpy,800> (DWARF :720) at +0x9620 (+38432). Remaining output queues omitted.
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        typedef CgsModule::EventQueue<OutUpdateRigidBody, 200> OutUpdateRigidBodyQueue; // DWARF :713 (16 + 200*192 == 38416)
        typedef CgsModule::EventQueue<OutContactSpy,      800> OutContactSpyQueue;      // DWARF :720 (+0x9620 / +38432)

        // X360 0x825BD0A8: read-lock (bit 4) guarded; returns the post-step time-step-used scalar.
        f32  GetTimeStepUsed() const;
        // X360 0x8289F088: write-lock (bit 3) guarded; stores muMaxIterationsUsed.
        void SetMaxIterationsUsed(s32 luMaxIterationsUsed);
        // X360 0x8289EFD8: write-lock (bit 3) guarded; stores mfTimeStepUsed.
        void SetTimeStepUsed(f32 fTimeStepUsed);

        // X360 0x8259F120: write-lock (bit 3) guarded; returns &mContactSpyQueue (this+0x9620).
        OutContactSpyQueue* GetContactSpyQueue();

        // X360 0x8259F270: write-lock (bit 3) guarded; returns the embedded OutDriveSpy queue
        // slice at +0x20040 (131136). The queue interior is owned by its callers, so a raw byte
        // pointer to the attested offset is returned.
        void* GetOutDriveSpyQueue();

        // Byte-offset pins (never called).
        static void _AssertLayout();

    private:
        u8  maStatusPad[3];        // +0x0001..+0x0003 (force mfTimeStepUsed to +0x0004 like the X360)
        f32 mfTimeStepUsed;        // +0x0004
        s32 muMaxIterationsUsed;   // +0x0008  (pads to +0x0010 before the queues)
        OutUpdateRigidBodyQueue mUpdateRigidBodyQueue;  // +0x0010 [16 .. 38432)
        OutContactSpyQueue      mContactSpyQueue;       // +0x9620 (+38432)
        // ... remaining output queues omitted (incomplete slice) ...
    };
}
}
