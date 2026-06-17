#include "types.hpp"
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"  // BrnPhysics::ContactSpy::ContactSpyInterface (canonical home)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   (BrnPhysics::ContactSpy::ContactSpyInterface)
//
//   Construct @ 0x82A61A18:
//       *this = 0;
//       return this;
//
// Trivial in-place constructor shared by every IO input-buffer (16 call sites):
// it clears the leading 32-bit field of the contact-spy interface and returns
// the object pointer. The struct definition now lives in the canonical header
// (above) so the RaceCarEntityModuleIO IO-buffer unlock can embed it by value.

namespace BrnPhysics
{
namespace ContactSpy
{
    ContactSpyInterface* ContactSpyInterface::Construct()
    {
        muField0 = 0;
        return this;
    }
}
}
