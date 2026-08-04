#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT (InputBuffer::AppendXxxQueue<N> template bodies)
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
        typedef CgsModule::EventQueue<InAddRigidBody, 200>           InAddRigidBodyQueue;           // DWARF :330 (16 + 200*192 = 38416)
        // Capacities (the InputBuffer's OWN embedded queue capacities) are X360-attested from the
        // asm offset chain below. mAddJointQueue's capacity is PINNED at 36: 16 + 36*192 == 6928
        // fills exactly the gap to mRemoveJointQueue (0x2E360 -> 0x2FE70), the only value that fits
        // (matches the "capacity-36 in the input buffer" note in BaseEventQueue_InAddJoint.cpp).
        // The others take their attested Construct capacities; the byte OFFSET (not the capacity)
        // is the load-bearing fact for each AppendXxxQueue<N> body, and is pinned in _AssertLayout.
        typedef CgsModule::EventQueue<InChangeRigidBodyInertia, 200> InChangeRigidBodyInertiaQueue; // 16 + 200*80  = 16016
        typedef CgsModule::EventQueue<InRemoveRigidBody, 200>        InRemoveRigidBodyQueue;        // 16 + 200*16  = 3216
        typedef CgsModule::EventQueue<InAddJoint, 36>                InAddJointQueue;               // 16 + 36*192  = 6928 (capacity pinned by exact fit)
        typedef CgsModule::EventQueue<InRemoveJoint, 36>             InRemoveJointQueue;            // 16 + 36*8    = 304
        typedef CgsModule::EventQueue<InUpdateExternalBody, 200>     InUpdateExternalBodyQueue;     // 16 + 200*112 = 22416

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

        // X360 0x8289E408: the CONST overload -- read-lock (bit 4) guarded, then the same
        // `addi r3, r28, 0x10`. This is the one the drain side uses, because
        // PhysicsSimulationModule::ProcessAddRigidBodyQueue takes `const InputBuffer*`.
        // ⚠️ ABSENT FROM .ida-exports; recovered from BURNOUT_X360_ARTIST.XEX.i64 with headless
        // IDA 9.3 (task #140). Its assert text and source line are the binary's own:
        // "Not locked for reading\n", CgsPhysicsSimulationModuleIO.h:893.
        const InAddRigidBodyQueue* GetAddRigidBodyQueue() const;

        // AppendXxxQueue<N> -- bulk-append the caller's source EventQueue<Elem,N> onto the matching
        // embedded per-command queue. X360-attested member templates on the SOURCE queue capacity N;
        // each asserts write-locked + source non-null, then mXxxQueue.Append(*lpSourceQueue) (the
        // committed BaseEventQueue<Elem>::Append(const BaseEventQueue<Elem>&) memcpy-merge). The
        // destination member is fixed per method (offset pinned in _AssertLayout). Generic bodies
        // out-of-line below. Instances:
        //   AppendAddRigidBodyQueue<1>            @ 0x825A8298   AppendAddRigidBodyQueue<50>  @ 0x825A84C0
        //   AppendAddJointQueue<10>              @ 0x825A8598   AppendRemoveJointQueue<10>   @ 0x825A8678
        //   AppendRemoveRigidBodyQueue<50>       @ 0x825A83E0   AppendChangeRigidBodyInertiaQueue<200> @ 0x825AC2E8
        //   AppendUpdateExternalBodyQueue<60>    @ 0x825AC208
        template <s32 N> bool AppendAddRigidBodyQueue(CgsModule::EventQueue<InAddRigidBody, N>* lpSourceQueue);
        template <s32 N> bool AppendAddJointQueue(CgsModule::EventQueue<InAddJoint, N>* lpSourceQueue);
        template <s32 N> bool AppendChangeRigidBodyInertiaQueue(CgsModule::EventQueue<InChangeRigidBodyInertia, N>* lpSourceQueue);
        template <s32 N> bool AppendRemoveJointQueue(CgsModule::EventQueue<InRemoveJoint, N>* lpSourceQueue);
        template <s32 N> bool AppendRemoveRigidBodyQueue(CgsModule::EventQueue<InRemoveRigidBody, N>* lpSourceQueue);
        template <s32 N> bool AppendUpdateExternalBodyQueue(CgsModule::EventQueue<InUpdateExternalBody, N>* lpSourceQueue);

        // Byte-offset pins (never called).
        static void _AssertLayout();

    private:
        u8  maStatusPad[3];   // +0x0001..+0x0003 (force mfTimeStep to +0x0004 like the X360)
        f32 mfTimeStep;       // +0x0004
        u32 muMaxIterations;  // +0x0008  (pads to +0x0010 before the queue)
        InAddRigidBodyQueue mAddRigidBodyQueue;  // +0x0010 (16) -- first embedded queue
        // The full InputBuffer interleaves ~20 embedded per-command queues; only the destinations of
        // the reconstructed AppendXxxQueue methods are modelled BY NAME at their X360-attested byte
        // offsets (read off each method's `addi rD, this, off` -- see the AppendXxxQueue banner). The
        // unrecovered intervening queues are explicit padding (LAYOUT RECOVERY WITH PADDING); the
        // full offset map is InputBuffer::Construct @0x828A71B8 (out of this slice's scope). Each pad
        // is (targetOffset - endOfPreviousModelledMember); the _AssertLayout offsetof pins verify it.
        u8  maQueueGap0[46432];                                        // +0x9620..+0x14B80  (unrecovered queues)
        InChangeRigidBodyInertiaQueue mChangeRigidBodyInertiaQueue;    // +0x14B80 (84864)
        u8  maQueueGap1[3216];                                         // +0x18A10..+0x196A0
        InRemoveRigidBodyQueue        mRemoveRigidBodyQueue;           // +0x196A0 (104096)
        u8  maQueueGap2[81968];                                        // +0x1A330..+0x2E360
        InAddJointQueue               mAddJointQueue;                  // +0x2E360 (189280)
        InRemoveJointQueue            mRemoveJointQueue;               // +0x2FE70 (196208) -- immediately after mAddJointQueue
        u8  maQueueGap3[7344];                                         // +0x2FFA0..+0x31C50
        InUpdateExternalBodyQueue     mUpdateExternalBodyQueue;        // +0x31C50 (203856)
        // ... remaining input queues after this one omitted (incomplete slice) ...
    };

    // -------- InputBuffer::AppendXxxQueue<N> (generic bodies; per-N instances in the .cpp) --------
    // Every instance shares this shape (X360 asm): assert write-locked, assert the source queue is
    // non-null (a non-gating tripwire -- the append runs regardless, matching the binary), then
    // forward to the fixed destination member's BaseEventQueue<Elem>::Append (bulk memcpy-merge).
    template <s32 N>
    bool InputBuffer::AppendAddRigidBodyQueue(CgsModule::EventQueue<InAddRigidBody, N>* lpSourceQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lpSourceQueue != nullptr, "lpSourceQueue != NULL");
        return mAddRigidBodyQueue.Append(*lpSourceQueue);
    }

    template <s32 N>
    bool InputBuffer::AppendAddJointQueue(CgsModule::EventQueue<InAddJoint, N>* lpSourceQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lpSourceQueue != nullptr, "lpSourceQueue != NULL");
        return mAddJointQueue.Append(*lpSourceQueue);
    }

    template <s32 N>
    bool InputBuffer::AppendChangeRigidBodyInertiaQueue(CgsModule::EventQueue<InChangeRigidBodyInertia, N>* lpSourceQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lpSourceQueue != nullptr, "lpSourceQueue != NULL");
        return mChangeRigidBodyInertiaQueue.Append(*lpSourceQueue);
    }

    template <s32 N>
    bool InputBuffer::AppendRemoveJointQueue(CgsModule::EventQueue<InRemoveJoint, N>* lpSourceQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lpSourceQueue != nullptr, "lpSourceQueue != NULL");
        return mRemoveJointQueue.Append(*lpSourceQueue);
    }

    template <s32 N>
    bool InputBuffer::AppendRemoveRigidBodyQueue(CgsModule::EventQueue<InRemoveRigidBody, N>* lpSourceQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lpSourceQueue != nullptr, "lpSourceQueue != NULL");
        return mRemoveRigidBodyQueue.Append(*lpSourceQueue);
    }

    template <s32 N>
    bool InputBuffer::AppendUpdateExternalBodyQueue(CgsModule::EventQueue<InUpdateExternalBody, N>* lpSourceQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lpSourceQueue != nullptr, "lpSourceQueue != NULL");
        return mUpdateExternalBodyQueue.Append(*lpSourceQueue);
    }

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
