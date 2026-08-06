#include "types.hpp"
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"  // BrnPhysics::ContactSpy::ContactSpyInterface (canonical home)
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT

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
        // The console clears the (32-bit) pointer slot; on this host the slot IS the pointer.
        // (PROMOTED 2026-08-06: was `muField0 = 0;` over a u32 stand-in slot -- see the header.)
        mpData = nullptr;
        return this;
    }

    // BrnPhysics::ContactSpy::ContactSpyInterface::GetRaceCarContactRunList @ 0x82355BF0
    //   Asserts mpData != NULL, then returns &mpData->mRaceCarContactRunList at byte offset
    //   0x198D0 (104656) into ContactSpyData
    //   (asm: addis r3,r11,2 ; addi r3,r3,-0x6730  ==>  mpData + 0x20000 - 0x6730 = mpData + 0x198D0).
    //   Identity PROVEN by computing ContactSpyData's DWARF member layout (queues then run lists,
    //   batch-attested strides) -- mRaceCarContactRunList lands at exactly 0x198D0.
    //   RETYPED 2026-08-06 (bridge de-facade wave): ContactSpyData is now in-tree, so the raw
    //   `muField0 + 0x198D0` byte math (which TRUNCATED the pointer through a u32 on this host)
    //   becomes the named-member access the console inlined.
    const ContactSpyData::RaceCarContactRunList* ContactSpyInterface::GetRaceCarContactRunList() const
    {
        CGS_ASSERT(mpData != nullptr, "mpData != NULL");
        return mpData->GetRaceCarContactRunList();
    }
}
}
