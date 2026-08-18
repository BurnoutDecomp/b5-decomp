// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO_InputBuffer_PrePhysics.cpp
//
// Out-of-line body for BrnWorld::PropEntityIO::InputBuffer_PrePhysics::AppendPotentialContactQueue,
// the prop-entity module's pre-physics INPUT buffer append the physics/scene bridge fills.
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   AppendPotentialContactQueue @ 0x827AA170:
//     assert IsBufferLockedForWriting()   (status>>3 &1; "Not locked for writing\n", :548)
//     mPotentialContactQueue.Append(*lpQueue)   (addi r3,this,0x10 ; bl PotentialContact_::Append)
//
// The write-lock bit is `lbz r11,0(this); extrwi r11,r11,1,28` (bit 3 == IsBufferLockedForWriting()).
// The append target is the embedded EventQueue<PotentialContact,2048> at +0x10 (the asm's
// `addi r3, r28, 0x10`); Append is the BaseEventQueue<T> generic (the source EventQueue upcasts to
// the BaseEventQueue<T>& parameter). DWARF (BrnPropEntityModuleIO.h:541) declares this returning
// void; the asm's tail `b __restgprlr_27` after `bl Append` merely forwards the callee frame --
// the caller ignores r3. CGS_ASSERT stamps __FILE__/__LINE__, so the X360-baked path/line 548 is
// intentionally not reproduced. The rodata string carries a trailing newline (VERBATIM; the
// closest committed sibling BrnAIModuleIO_InputBuffer_Accessors keeps "Not locked for writing\n").
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

namespace BrnWorld
{
namespace PropEntityIO
{
    void InputBuffer_PrePhysics::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_PrePhysics, mPotentialContactQueue) == 0x10,
                      "mPotentialContactQueue @ +0x10");
    }

    // ========================================================================
    // InputBuffer_PrePhysics::Construct   @ 0x822EFD68   (DWARF :534)
    // ------------------------------------------------------------------------
    // ⭐ NEW 2026-08-12 (prop-BOOT wave, agent B8). Declared in the header since this buffer
    // landed, never bodied -- so WorldModule's `lpPropInput_PrePhysics->Construct()` resolved
    // to the inherited CgsModule::IOBuffer::Construct and both embedded queues kept
    // mpEvents == NULL (the "mpEvents != NULL" tripwire, then a null write, on the first
    // potential contact of the frame).
    //
    // The asm is five statements:
    //   stb 1, 0(this)                                         -> IOBuffer::Construct()
    //   bl  PotentialContact_2048_::Construct(this+16)          \ mPotentialContactQueue
    //   stw 0, 24(this)          == that queue's miLength       /  .Construct(); .Clear();
    //   bl  ResetOnTrackResult_128_::Construct(this+163872)     \ mResetOnTrackResultQueue
    //   stw 0, 163880(this)      == that queue's miLength       /  .Construct(); .Clear();
    // (+24 == +16+8 and +163880 == +163872+8 are each the BaseEventQueue miLength field, i.e.
    //  the inlined Clear; EventQueue::Construct already zeroes it, and the console clears
    //  again -- reproduced, not deduplicated.)
    // ========================================================================
    void InputBuffer_PrePhysics::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mPotentialContactQueue.Construct();
        mPotentialContactQueue.Clear();

        mResetOnTrackResultQueue.Construct();
        mResetOnTrackResultQueue.Clear();
    }

    // X360 0x827AA170 (:541) -- write-lock; append the source potential-contact queue onto the
    // embedded mPotentialContactQueue (this+0x10).
    void InputBuffer_PrePhysics::AppendPotentialContactQueue(const OutPotentialContactQueue* lpQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mPotentialContactQueue.Append(*lpQueue);
    }

    // ========================================================================
    // InputBuffer_PrePhysics::GetPotentialContactQueue() const   @ 0x822B92A0   (DWARF :540)
    // ------------------------------------------------------------------------
    // ⭐ NEW 2026-08-18 (wave Q round 2). Declared in the header since this buffer landed and
    // DEFINED NOWHERE, while two wave-Q partfiles already call it (PropEntityModule_wQ_05.cpp
    // :176 in ProcessPotentialContacts and PropEntityModule_wQ_07.cpp:628 in PrePhysicsUpdate's
    // paused-leg tripwire) -- a live unresolved external that `cl /c` cannot see.
    // The asm is the family's standard read handle: `lbz r11,0(this) ; extrwi r11,r11,1,27`
    // (READ-lock, bit 4), the "Not locked for reading\n" FireAssert baking
    // BrnPropEntityModuleIO.h line 0x223 == 547, then `addi r3, r28, 0x10` --
    // &mPotentialContactQueue, the same +0x10 seat AppendPotentialContactQueue writes.
    // ========================================================================
    const InputBuffer_PrePhysics::OutPotentialContactQueue* InputBuffer_PrePhysics::GetPotentialContactQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mPotentialContactQueue;
    }

    // ========================================================================
    // InputBuffer_PrePhysics::AppendResetOnTrackResultQueue   @ 0x827AA220   (DWARF :544)
    // ------------------------------------------------------------------------
    // ⭐ NEW 2026-08-18 (wave Q round 2). Declared-only until now.
    // ⚠️ There is NO per-address JSON export for 0x827AA220, so this body is grounded on a
    // headless-idat dump of the IDB rather than on .ida-exports. Two independent facts pin the
    // identity before a line of it was written: (a) the callee's own export,
    // .ida-exports/BURNOUT_X360_ARTIST.XEX/0x827A71B8.json (BaseEventQueue<ResetOnTrackResult>
    // ::Append), lists 0x827AA220 in its `xrefs_to` under exactly this name; (b) the function
    // sits immediately after AppendPotentialContactQueue @0x827AA170 and is the same 44
    // instructions long. Dumped body:
    //     lbz r11,0(r28) ; extrwi r11,r11,1,28        -- write-lock (bit 3)
    //     ... "Not locked for writing\n", baked line 0x227 == 551
    //     addis r3,r28,3 ; addi r3,r3,-0x7FE0         -- this + 0x28020
    //     mr r4,r27 ; bl BaseEventQueue<ResetOnTrackResult>::Append
    // this+0x28020 is mResetOnTrackResultQueue: the sibling read handle @0x822B9348 returns
    // the same +163872 == 0x28020 seat. Like AppendPotentialContactQueue, the DWARF declares
    // it void and the console's tail-call merely forwards the callee frame.
    // ========================================================================
    void InputBuffer_PrePhysics::AppendResetOnTrackResultQueue(const ResetOnTrackResultQueue* lpQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mResetOnTrackResultQueue.Append(*lpQueue);
    }

    // X360 0x822B9348 (R, :550) -- read-lock; return the reset-on-track result queue (this+163872).
    // mResetOnTrackResultQueue sits after mPotentialContactQueue (EventQueue<PotentialContact,2048>:
    // +0x10 base + 16 header + 2048*80 = +163872). Called by BrnWorld::PropEntityModule::PrePhysicsUpdate.
    // Rodata carries a trailing newline (VERBATIM, per Feb-2007 reference source).
    const InputBuffer_PrePhysics::ResetOnTrackResultQueue* InputBuffer_PrePhysics::GetResetOnTrackResultQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mResetOnTrackResultQueue;
    }
}
}
