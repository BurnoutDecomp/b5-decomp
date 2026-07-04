// ============================================================================
// b5-decomp/src/GameSource/World/AI/SharedIO/BrnAIModuleIO_InputBuffer_PostPhysics.cpp
//
// Out-of-line bodies for BrnAI::AIModuleIO::InputBuffer_PostPhysics::Construct / ::Destruct,
// the AI module's post-physics INPUT buffer (filled by the physics side with contact-spy
// results, drained by the AI post-physics step). Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   Construct @ 0x8277BCD0:
//       *this = 1;                       // li r10,1 ; stb r10,0(r11)  -> IOBuffer status byte
//       mContactInterface.Construct();   // addi r3,r11,4 ; b ContactSpyInterface::Construct (tail)
//   Destruct  @ 0x8277BCE8:
//       mContactInterface.Construct();   // addi r3,r31,4 ; bl ContactSpyInterface::Construct (RE-init)
//       IOBuffer::Destruct();            // mr r3,r31    ; bl CgsModule::IOBuffer::Destruct
//
// The leading `*this = 1` in Construct is the inlined CgsModule::IOBuffer::Construct() (FlagSet<s8>
// is a single byte; Clear() + SetBit(eStatusConstructed) collapses to a `stb 1`); restored here as
// the named base call for semantic parity, mirroring the committed BrnPropEntityModuleIO sibling.
//
// Destruct's leading mContactInterface.Construct() is an asm-attested IN-PLACE re-init of the
// contact-spy member (the asm literally `bl ContactSpyInterface::Construct`, NOT a destructor);
// reproduced verbatim -- NOT "fixed" to a member destruct.
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"  // BrnPhysics::ContactSpy::ContactSpyInterface

#include <cstddef>   // offsetof

namespace BrnAI
{
namespace AIModuleIO
{
    void InputBuffer_PostPhysics::_AssertLayout()
    {
        static_assert(offsetof(InputBuffer_PostPhysics, mContactInterface) == 0x04,
                      "mContactInterface @ +0x04");
    }

    // X360 0x8277BCD0 -- mark the IOBuffer base constructed, then construct the embedded
    // contact-spy interface (this+0x04).
    void InputBuffer_PostPhysics::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mContactInterface.Construct();
    }

    // X360 0x8277BCE8 -- re-init the embedded contact-spy interface (this+0x04) via its in-place
    // Construct (asm-attested), then tear down the IOBuffer base. Order preserved from the asm.
    void InputBuffer_PostPhysics::Destruct()
    {
        mContactInterface.Construct();

        CgsModule::IOBuffer::Destruct();
    }
}
}
