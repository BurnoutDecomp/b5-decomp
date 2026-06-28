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
}
}
