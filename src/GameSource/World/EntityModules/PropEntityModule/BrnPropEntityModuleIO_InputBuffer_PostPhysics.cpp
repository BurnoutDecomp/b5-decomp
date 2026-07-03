// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO_InputBuffer_PostPhysics.cpp
//
// Out-of-line body for BrnWorld::PropEntityIO::InputBuffer_PostPhysics::Construct, the
// prop-entity module's post-physics input buffer initialiser. Reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
//   InputBuffer_PostPhysics::Construct @ 0x822EFDC8:
//       *this = 1;                        // li r11,1 ; stb r11,0(r31)  -> IOBuffer status byte
//       mUpdatedPropQueue.Construct();    // addi r3,r31,0x10 ; bl EventQueue<UpdatePropEvent,200>::Construct
//       mContactSpyInterface.Construct(); // addi r3,r31,4    ; bl ContactSpyInterface::Construct
//
// The leading `*this = 1` is the inlined CgsModule::IOBuffer::Construct() (FlagSet<s8> is a
// single byte; Clear() + SetBit(eStatusConstructed) collapses to a `stb 1`). It is restored
// here as the named base call for semantic parity rather than a raw status-byte poke.
//
// The two member Construct()s build the embedded sub-objects BY NAME at their real offsets
// (mUpdatedPropQueue @ +0x10, mContactSpyInterface @ +0x04 -- see the header layout note); the
// asm's construct-queue-before-spy ordering is preserved (this is a hand-written method, so the
// body order is the call order, independent of declaration order).
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

namespace BrnWorld
{
namespace PropEntityIO
{
    void InputBuffer_PostPhysics::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_PostPhysics, mContactSpyInterface) == 0x04,
                      "mContactSpyInterface @ +0x04");
        static_assert(offsetof(InputBuffer_PostPhysics, mUpdatedPropQueue) == 0x10,
                      "mUpdatedPropQueue @ +0x10");
    }

    // X360 0x822EFDC8 -- mark the IOBuffer base constructed, then construct the two embedded
    // sub-objects (update-prop queue, then contact-spy interface, matching the asm order).
    void InputBuffer_PostPhysics::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mUpdatedPropQueue.Construct();
        mContactSpyInterface.Construct();
    }

    // X360 0x822B93F0 (R, :569) -- read-lock; return the embedded contact-spy interface (this+4).
    // Called by BrnWorld::PropEntityModule::ProcessContacts. (class:BrnWorld::PropEntityIO TU; no \n.)
    const InputBuffer_PostPhysics::ContactSpyInterface* InputBuffer_PostPhysics::GetContactSpyInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mContactSpyInterface;
    }

    // X360 0x827A1628 (:572) -- write-lock; return the mutable contact-spy interface handle
    // (this+0x04). This is the NON-const overload: it tests the WRITE-lock bit
    // (`extrwi r11,r11,1,28` == bit 3 == IsBufferLockedForWriting()) -- reproduced as attested.
    // The rodata assert string carries a trailing newline (VERBATIM).
    InputBuffer_PostPhysics::ContactSpyInterface* InputBuffer_PostPhysics::GetContactSpyInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mContactSpyInterface;
    }

    // X360 0x827AA2D8 (:573) -- write-lock; append the source update-prop queue onto the embedded
    // mUpdatedPropQueue (this+0x10). Append is the BaseEventQueue<UpdatePropEvent> generic.
    // The rodata assert string carries a trailing newline (VERBATIM).
    void InputBuffer_PostPhysics::AppendUpdatedPropQueue(const UpdatePropEventQueue* lpQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        mUpdatedPropQueue.Append(*lpQueue);
    }
}
}
