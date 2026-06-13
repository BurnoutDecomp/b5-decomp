#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (BrnPhysics::ContactSpy::ContactSpyInterface)
//
//   Construct @ 0x82A61A18:
//       *this = 0;
//       return this;
//
// Trivial in-place constructor shared by every IO input-buffer (16 call sites):
// it clears the leading 32-bit field of the contact-spy interface and returns
// the object pointer.

namespace BrnPhysics
{
namespace ContactSpy
{
    struct ContactSpyInterface
    {
        u32 muField0;       // [0x00] cleared on construct

        ContactSpyInterface* Construct();
    };

    ContactSpyInterface* ContactSpyInterface::Construct()
    {
        muField0 = 0;
        return this;
    }
}
}
