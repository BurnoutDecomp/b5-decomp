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

    // X360 0x827AA170 (:541) -- write-lock; append the source potential-contact queue onto the
    // embedded mPotentialContactQueue (this+0x10).
    void InputBuffer_PrePhysics::AppendPotentialContactQueue(const OutPotentialContactQueue* lpQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mPotentialContactQueue.Append(*lpQueue);
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
