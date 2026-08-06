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
        // ============================================================================
        // ⭐⭐ ALL NINETEEN INPUT QUEUES, 2026-08-04 (task #142). The four `maQueueGap`
        // byte spans that used to stand for "unrecovered queues" (138,960 bytes of
        // padding) are RETIRED -- every queue is now a named member at an offset read
        // out of the X360 image.
        //
        // NAMES, ORDER AND CAPACITIES: DWARF CgsPhysicsSimulationModuleIO.h:330..:459,
        // which declares all nineteen in memory order. (The committed claim that this
        // layout was "not in scope" / needed InputBuffer::Construct @0x828A71B8 was
        // wrong on both counts -- see the OFFSETS note.)
        //
        // OFFSETS: every `InputBuffer::GetXxxQueue() const` accessor is a 42-instruction
        // read-lock-guarded return whose last two instructions are literally the member
        // offset. They form a uniform block at 0x8289E408 + k*0xA8:
        //     #  member                        accessor    addis/addi      offset
        //     1  mAddRigidBodyQueue            0x8289E408  addi 0x10           16
        //     2  mUpdateRigidBodyQueue         0x8289E4B0  1 / -0x69E0      38432
        //     3  mApplyForceQueue              0x8289E558  1 /  0x2C30      76848
        //     4  mChangeRigidBodyInertiaQueue  -- no const accessor --      84864
        //     5  mSetRigidBodySpyQueue         0x8289E600  2 / -0x75F0     100880
        //     6  mRemoveRigidBodyQueue         0x8289E6A8  2 / -0x6960     104096  ⚠️ export HOLE
        //     7  mRemoveAllRigidBodiesQueue    0x8289E750  2 / -0x5CD0     107312
        //     8  mAddContactQueue              0x8289E7F8  2 / -0x5CB0     107344
        //     9  mAddJointQueue                0x8289E8A0  3 / -0x1CA0     189280
        //    10  mRemoveJointQueue             0x8289E948  3 / -0x190      196208
        //    11  mUpdateJointFramesQueue       0x8289E9F0  3 / -0x60       196512
        //    12  mUpdateJointLimitsQueue       0x8289EA98  3 /  0xD30      199984
        //    13  mSetJointSpyQueue             0x8289EB40  3 /  0x1880     202880
        //    14  mAddDriveQueue                0x8289EBE8  3 /  0x1AD0     203472
        //    15  mRemoveDriveQueue             0x8289EC90  3 /  0x1B70     203632
        //    16  mUpdateDriveFramesQueue       0x8289ED38  3 /  0x1B90     203664
        //    17  mUpdateDriveDynamicsQueue     0x8289EDE0  3 /  0x1BF0     203760
        //    18  mSetDriveSpyQueue             0x8289EE88  3 /  0x1C30     203824  ⚠️ export HOLE
        //    19  mUpdateExternalBodyQueue      0x8289EF30  3 /  0x1C50     203856
        // ⚠️ #6 and #18 are absent from .ida-exports and were recovered headless from
        // BURNOUT_X360_ARTIST.XEX.i64 with IDA 9.3 (`idat.exe`, there is no idat64.exe) --
        // holes #12 and #13 in this campaign. See [[ida-export-set-has-holes]].
        // #4 has no const accessor at all (its drain reaches the queue differently); its
        // offset is the one value the chain fixes rather than the image, and it was
        // already committed at 84864 before this wave, which is the control that passed.
        //
        // ⭐ THE CONTROL THAT MAKES THE OTHER THIRTEEN CREDIBLE: six of these offsets
        // (#1 #4 #6 #9 #10 #19) were already in the tree from earlier waves, derived by a
        // completely different route. All six reproduce EXACTLY.
        //
        // ⭐⭐ AND THE CHAIN CLOSES WITH NO PADDING. Laying the nineteen out in DWARF
        // order at their natural C++ sizes lands every single one on its X360 offset,
        // with exactly one 8-byte alignment bump (#7 -> #8, where a 1-byte-element queue
        // is followed by a 16-aligned one). No explicit padding member survives anywhere
        // in this class. That is only possible if all nineteen element strides are right,
        // which is why four of them had to be corrected in the events header first
        // (InUpdateJointFrames/JointLimits/DriveFrames/DriveDynamics were 16 bytes each).
        // ============================================================================
        typedef CgsModule::EventQueue<InAddRigidBody, 200>           InAddRigidBodyQueue;           // DWARF :330 (16 + 200*192 = 38416)
        typedef CgsModule::EventQueue<InUpdateRigidBody, 200>        InUpdateRigidBodyQueue;        // DWARF :337 (16 + 200*192 = 38416)
        typedef CgsModule::EventQueue<InApplyForce, 250>             InApplyForceQueue;             // DWARF :344 (16 + 250*32  =  8016)
        typedef CgsModule::EventQueue<InChangeRigidBodyInertia, 200> InChangeRigidBodyInertiaQueue; // DWARF :351 (16 + 200*80  = 16016)
        typedef CgsModule::EventQueue<InSetRigidBodySpy, 200>        InSetRigidBodySpyQueue;        // DWARF :358 (16 + 200*16  =  3216)
        typedef CgsModule::EventQueue<InRemoveRigidBody, 200>        InRemoveRigidBodyQueue;        // DWARF :365 (16 + 200*16  =  3216)
        typedef CgsModule::EventQueue<InRemoveAllRigidBodies, 8>     InRemoveAllRigidBodiesQueue;   // DWARF :372 (16 +   8*1   =    24)
        typedef CgsModule::EventQueue<InAddPotentialContact, 1024>   InAddContactQueue;             // DWARF :379 (16 +1024*80  = 81936)
        typedef CgsModule::EventQueue<InAddJoint, 36>                InAddJointQueue;               // DWARF :386 (16 +  36*192 =  6928)
        typedef CgsModule::EventQueue<InRemoveJoint, 36>             InRemoveJointQueue;            // DWARF :393 (16 +  36*8   =   304)
        typedef CgsModule::EventQueue<InUpdateJointFrames, 36>       InUpdateJointFramesQueue;      // DWARF :400 (16 +  36*96  =  3472)
        typedef CgsModule::EventQueue<InUpdateJointLimits, 36>       InUpdateJointLimitsQueue;      // DWARF :407 (16 +  36*80  =  2896)
        typedef CgsModule::EventQueue<InSetJointSpy, 36>             InSetJointSpyQueue;            // DWARF :414 (16 +  36*16  =   592)
        typedef CgsModule::EventQueue<InAddDrive, 1>                 InAddDriveQueue;               // DWARF :421 (16 +   1*144 =   160)
        typedef CgsModule::EventQueue<InRemoveDrive, 1>              InRemoveDriveQueue;            // DWARF :428 (16 +   1*16  =    32)
        typedef CgsModule::EventQueue<InUpdateDriveFrames, 1>        InUpdateDriveFramesQueue;      // DWARF :435 (16 +   1*80  =    96)
        typedef CgsModule::EventQueue<InUpdateDriveDynamics, 1>      InUpdateDriveDynamicsQueue;    // DWARF :442 (16 +   1*48  =    64)
        typedef CgsModule::EventQueue<InSetDriveSpy, 1>              InSetDriveSpyQueue;            // DWARF :449 (16 +   1*16  =    32)
        typedef CgsModule::EventQueue<InUpdateExternalBody, 200>     InUpdateExternalBodyQueue;     // DWARF :456 (16 + 200*112 = 22416)

        // X360 0x8289E338: read-lock (bit 4) guarded; asserts muMaxIterations > 0, returns it.
        int  GetMaxIterations() const;
        // X360 0x8259ECD8: write-lock (bit 3) guarded (NOTE: a Get that tests the WRITE bit --
        // faithful to the asm, intentionally NOT "fixed"); asserts mfTimeStep > 0, returns it.
        // ⚠️ 2026-08-05: NON-const now. The console carries TWO GetTimeStep bodies with opposite
        // lock guards, the same split GetAddRigidBodyQueue has: this write-lock one (producer
        // side) and a READ-lock const one @0x8289E260 (its asserts cite IO.h:833/:834), which is
        // what PhysicsSimulationModule::Update @0x828A74D0 calls under LockForRead ("lfNewStep").
        // Until this split the committed const body asserted the WRITE bit, so the first real
        // Update tick would have fired "Not locked for writing" every frame -- a false assert
        // armed in a Get. Overload resolution: the drain/Update side holds `const InputBuffer*`,
        // so it binds the const/read one; producers hold a non-const pointer and bind this.
        f32  GetTimeStep();
        // X360 0x8289E260 (54 insn, exports 0x8289E260.json): read-lock (bit 4) guarded const
        // twin; asserts mfTimeStep > 0 ("mfTimeStep > 0.0f", IO.h:834), returns it.
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

        // -----------------------------------------------------------------------------------
        // ⭐ THE OTHER EIGHTEEN CONST QUEUE ACCESSORS (task #142, 2026-08-04).
        // DWARF :487..:547 declares a const getter per queue. In the image they are a uniform
        // block of 42-instruction bodies at 0x8289E408 + k*0xA8, and every one has the same
        // shape as the AddRigidBody twin above, verified instruction-for-instruction on all
        // sixteen that .ida-exports carries plus the two pulled headless:
        //     read-lock guard (`lbz 0(this)`; `extrwi r11,r11,1,27` == LSB bit 4) firing the
        //     rodata string "Not locked for reading\n", then `addis r3,r28,H`/`addi r3,r3,L`
        //     == &mXxxQueue. No bounds check, no other work.
        // They are landed together because they are the entry point EVERY input drain uses:
        // each `ProcessXxxQueue(const InputBuffer*)` opens with a call to its own accessor.
        // ⚠️ Their addresses are the offset evidence for the member map above -- so these
        // bodies and those offsetof pins are the same fact stated twice, deliberately.
        const InUpdateRigidBodyQueue*        GetUpdateRigidBodyQueue()        const;  // @0x8289E4B0
        const InApplyForceQueue*             GetApplyForceQueue()             const;  // @0x8289E558
        const InSetRigidBodySpyQueue*        GetSetRigidBodySpyQueue()        const;  // @0x8289E600
        const InRemoveRigidBodyQueue*        GetRemoveRigidBodyQueue()        const;  // @0x8289E6A8  ⚠️ export HOLE
        const InRemoveAllRigidBodiesQueue*   GetRemoveAllRigidBodiesQueue()   const;  // @0x8289E750
        const InAddContactQueue*             GetAddContactQueue()             const;  // @0x8289E7F8
        // ⭐ ADDED 2026-08-06 (BridgeContactsToSimulation wave): the WRITE-side twin
        // @0x8259EF28 -- write-lock guard ("Not locked for writing\n", DWARF decl :1073 per the
        // PS3 DecFIGS assert path) then &mAddContactQueue. It is what the bridge calls before
        // every CheckContactQueueSize/AddEventSafe pair and before the two BridgeSimpleTraffic*
        // forwards.
        InAddContactQueue*                   GetAddContactQueue();                    // @0x8259EF28
        const InAddJointQueue*               GetAddJointQueue()               const;  // @0x8289E8A0
        const InRemoveJointQueue*            GetRemoveJointQueue()            const;  // @0x8289E948
        const InUpdateJointFramesQueue*      GetUpdateJointFramesQueue()      const;  // @0x8289E9F0
        const InUpdateJointLimitsQueue*      GetUpdateJointLimitsQueue()      const;  // @0x8289EA98
        const InSetJointSpyQueue*            GetSetJointSpyQueue()            const;  // @0x8289EB40
        const InAddDriveQueue*               GetAddDriveQueue()               const;  // @0x8289EBE8
        const InRemoveDriveQueue*            GetRemoveDriveQueue()            const;  // @0x8289EC90
        const InUpdateDriveFramesQueue*      GetUpdateDriveFramesQueue()      const;  // @0x8289ED38
        const InUpdateDriveDynamicsQueue*    GetUpdateDriveDynamicsQueue()    const;  // @0x8289EDE0
        const InSetDriveSpyQueue*            GetSetDriveSpyQueue()            const;  // @0x8289EE88  ⚠️ export HOLE
        const InUpdateExternalBodyQueue*     GetUpdateExternalBodyQueue()     const;  // @0x8289EF30

        // ⛔⛔ RETRACTED 2026-08-05 (the rigid-body drain group). The note that stood here said
        // a const GetChangeRigidBodyInertiaQueue() does NOT exist in the image ("no room in
        // the accessor block ... the drain reaches its queue without calling one"). BOTH
        // halves were false, and the second was checkable all along: ProcessChangeRigidBody-
        // InertiaQueue @0x828A4A90 opens with `bl 0x8259EE80`, and THAT function is the
        // accessor -- the identical 42-instruction read-lock body ("Not locked for reading\n",
        // this header's line 914) ending `addis r3,r28,1 / addi r3,r3,0x4B80` == +84864 ==
        // mChangeRigidBodyInertiaQueue, exactly the offset pinned in the member table. It
        // simply does not LIVE in the uniform 0x8289E408+k*0xA8 block (the compiler emitted it
        // at 0x8259EE80, next to GetTimeStep/SetTimeStep); "not in the block" was read as
        // "not in the image". The NINTH false nonexistence claim in this subsystem --
        // re-verify every remaining one before acting on it.
        const InChangeRigidBodyInertiaQueue* GetChangeRigidBodyInertiaQueue() const;  // @0x8259EE80 (NOT in the k*0xA8 block)
        // -----------------------------------------------------------------------------------

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
        template <s32 N> bool AppendRemoveJointQueue(const CgsModule::EventQueue<InRemoveJoint, N>* lpSourceQueue);   // const src: console `PBV` mangling, see the body
        template <s32 N> bool AppendRemoveRigidBodyQueue(CgsModule::EventQueue<InRemoveRigidBody, N>* lpSourceQueue);
        template <s32 N> bool AppendUpdateExternalBodyQueue(CgsModule::EventQueue<InUpdateExternalBody, N>* lpSourceQueue);

        // Byte-offset pins (never called).
        static void _AssertLayout();

    private:
        u8  maStatusPad[3];   // +0x0001..+0x0003 (force mfTimeStep to +0x0004 like the X360)
        f32 mfTimeStep;       // +0x0004
        u32 muMaxIterations;  // +0x0008  (pads to +0x0010 before the queue)
        // The nineteen embedded per-command queues, in DWARF declaration order. Offsets are the
        // X360 accessor table in the banner above; sizes are 16 + capacity*sizeof(element).
        // NO PADDING MEMBERS -- the natural C++ layout reproduces every attested offset. Each
        // one is pinned by an offsetof static_assert in _AssertLayout().
        InAddRigidBodyQueue           mAddRigidBodyQueue;            // +0x00010 (     16)  first embedded queue
        InUpdateRigidBodyQueue        mUpdateRigidBodyQueue;         // +0x09620 (  38432)
        InApplyForceQueue             mApplyForceQueue;              // +0x12C30 (  76848)
        InChangeRigidBodyInertiaQueue mChangeRigidBodyInertiaQueue;  // +0x14B80 (  84864)
        InSetRigidBodySpyQueue        mSetRigidBodySpyQueue;         // +0x18A10 ( 100880)
        InRemoveRigidBodyQueue        mRemoveRigidBodyQueue;         // +0x196A0 ( 104096)
        InRemoveAllRigidBodiesQueue   mRemoveAllRigidBodiesQueue;    // +0x1A330 ( 107312)  1-byte element; align 8
        InAddContactQueue             mAddContactQueue;              // +0x1A350 ( 107344)  <- the one alignment bump (+8)
        InAddJointQueue               mAddJointQueue;                // +0x2E360 ( 189280)
        InRemoveJointQueue            mRemoveJointQueue;             // +0x2FE70 ( 196208)
        InUpdateJointFramesQueue      mUpdateJointFramesQueue;       // +0x2FFA0 ( 196512)
        InUpdateJointLimitsQueue      mUpdateJointLimitsQueue;       // +0x30D30 ( 199984)
        InSetJointSpyQueue            mSetJointSpyQueue;             // +0x31880 ( 202880)
        InAddDriveQueue               mAddDriveQueue;                // +0x31AD0 ( 203472)
        InRemoveDriveQueue            mRemoveDriveQueue;             // +0x31B70 ( 203632)
        InUpdateDriveFramesQueue      mUpdateDriveFramesQueue;       // +0x31B90 ( 203664)
        InUpdateDriveDynamicsQueue    mUpdateDriveDynamicsQueue;     // +0x31BF0 ( 203760)
        InSetDriveSpyQueue            mSetDriveSpyQueue;             // +0x31C30 ( 203824)
        InUpdateExternalBodyQueue     mUpdateExternalBodyQueue;      // +0x31C50 ( 203856)  last; ends at 226272
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

    // ⭐ Source-queue param is CONST (retyped 2026-08-06): the console's own mangling proves it
    // -- the AppendRemoveJointQueue<10> emission @0x825A8678 mangles `...QAAXPBV$EventQueue...`
    // (`PBV` = pointer to CONST class), and its bridge caller @0x825ABA08 feeds a queue obtained
    // from a `const VehicleOutputRequestInterface*`. Append() only reads the source, so the
    // non-const spelling was an over-strict host invention. (Existing non-const callers still
    // convert implicitly.)
    template <s32 N>
    bool InputBuffer::AppendRemoveJointQueue(const CgsModule::EventQueue<InRemoveJoint, N>* lpSourceQueue)
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

    // The physics-simulation output buffer -- COMPLETE as of 2026-08-06 (the spy wave): all
    // FOUR embedded output queues are named members (DWARF :743..:746), and the console
    // offset chain closes exactly:
    //     +0x00010 mUpdateRigidBodyQueue  16 + 200*192 == 38416
    //     +0x09620 mContactSpyQueue       16 + 800*112 == 89616   (GetContactSpyQueue @0x8259F120)
    //     +0x1F430 mJointSpyQueue         16 +  64*48  ==  3088   (GetJointSpyQueue   @0x8259F1C8)
    //     +0x20040 mDriveSpyQueue         16 +   1*64  ==    80   (GetDriveSpyQueue   @0x8259F270)
    // The three spy-queue accessors are a uniform write-lock-guarded block at 0x8259F120 +
    // k*0xA8, exactly the InputBuffer accessor-block shape; each closing addi IS the member
    // offset. ⚠️ The HOST offsets differ (OutUpdateRigidBody carries a full RigidBody whose
    // pointer lanes widen), so the pins in the .cpp are the ADJACENCY form and the old raw
    // `this + 0x20040` return in GetOutDriveSpyQueue is RETIRED with the typed member.
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        typedef CgsModule::EventQueue<OutUpdateRigidBody, 200> OutUpdateRigidBodyQueue; // DWARF :713 (console 16 + 200*192)
        typedef CgsModule::EventQueue<OutContactSpy,      800> OutContactSpyQueue;      // DWARF :720 (+0x9620)
        typedef CgsModule::EventQueue<OutJointSpy,         64> OutJointSpyQueue;        // DWARF :727 (+0x1F430)
        typedef CgsModule::EventQueue<OutDriveSpy,          1> OutDriveSpyQueue;        // DWARF :734 (+0x20040)

        // X360 0x828A5F38 (DWARF :693): zero the two scalar controls, Clear() all four
        // queues, chain to IOBuffer::Destruct (which this non-virtual member HIDES, as the
        // console's does). Declared 2026-08-06 so its body TU
        // (CgsPhysicsSimulationModuleIO.cpp) could retire its local slice-fork of this class
        // -- see the rewrite banner there.
        void Destruct();

        // X360 0x825BD0A8: read-lock (bit 4) guarded; returns the post-step time-step-used scalar.
        f32  GetTimeStepUsed() const;
        // X360 0x8289F088: write-lock (bit 3) guarded; stores muMaxIterationsUsed.
        void SetMaxIterationsUsed(s32 luMaxIterationsUsed);
        // X360 0x8289EFD8: write-lock (bit 3) guarded; stores mfTimeStepUsed.
        void SetTimeStepUsed(f32 fTimeStepUsed);

        // X360 0x8289F130: write-lock (bit 3) guarded; returns &mUpdateRigidBodyQueue (+0x10).
        // (DWARF :728. The non-const producer-side accessor AddActiveBodiesToOutputQueue /
        // ActivateAndFreezeAsNeeded open with -- `bl sub_8289F130` in both emitters.)
        OutUpdateRigidBodyQueue* GetUpdateRigidBodyQueue();

        // X360 0x8259F120: write-lock (bit 3) guarded; returns &mContactSpyQueue (console +0x9620).
        OutContactSpyQueue* GetContactSpyQueue();

        // X360 0x8259F078: READ-lock (bit 4) guarded const twin (one 0xA8 slot before the
        // write trio in the uniform accessor block; "Not locked for reading\n", header :1366).
        // ADDED 2026-08-06 -- called by PhysicsModule::BridgeSimulationToOutput @0x825B0540 on
        // the const sim-module output buffer.
        const OutContactSpyQueue* GetContactSpyQueue() const;

        // X360 0x8259F1C8: write-lock (bit 3) guarded; returns &mJointSpyQueue (console +0x1F430).
        // (DWARF :734... the :731/:734/:737 non-const trio; AddJointSpiesToOutputQueue opens
        // with `bl sub_8259F1C8`.)
        OutJointSpyQueue* GetJointSpyQueue();

        // X360 0x8259F270: write-lock (bit 3) guarded; returns &mDriveSpyQueue (console +0x20040).
        // ⭐ RETYPED 2026-08-06: this was `void* GetOutDriveSpyQueue()` returning a raw
        // `this + 0x20040` byte pointer -- a CONSOLE offset that would have silently pointed
        // into the middle of the widened mContactSpyQueue the moment OutUpdateRigidBody's
        // payload grew (which this wave does). The DWARF name (:737 GetDriveSpyQueue) and the
        // typed member replace it; no other caller existed.
        OutDriveSpyQueue* GetDriveSpyQueue();

        // Byte-offset pins (never called).
        static void _AssertLayout();

    private:
        u8  maStatusPad[3];        // +0x0001..+0x0003 (force mfTimeStepUsed to +0x0004 like the X360)
        f32 mfTimeStepUsed;        // +0x0004
        s32 muMaxIterationsUsed;   // +0x0008  (pads to +0x0010 before the queues)
        OutUpdateRigidBodyQueue mUpdateRigidBodyQueue;  // console +0x00010 (host: wider -- RigidBody payload)
        OutContactSpyQueue      mContactSpyQueue;       // console +0x09620
        OutJointSpyQueue        mJointSpyQueue;         // console +0x1F430  DWARF :745
        OutDriveSpyQueue        mDriveSpyQueue;         // console +0x20040  DWARF :746
    };
}
}
