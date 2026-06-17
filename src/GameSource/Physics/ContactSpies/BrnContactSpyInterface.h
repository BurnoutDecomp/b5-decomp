#pragma once

// Canonical home for BrnPhysics::ContactSpy::ContactSpyInterface (DWARF home
// GameSource/Physics/ContactSpies/BrnContactSpyInterface.h:48).
//
// This is the COMMITTED type already reconstructed in-tree by the contact-spy
// Construct TU (BrnContactSpyInterface.cpp, X360 0x82A61A18): the trivial in-place
// constructor clears the leading 32-bit field and returns the object pointer. That
// .cpp previously defined the struct inline; the definition now lives here so the
// RaceCarEntityModuleIO IO-buffer unlock can #include it and reference the real
// fully-qualified type (it is embedded BY VALUE as InputBuffer_PostPhysics
// mContactSpyInterface, IO header :541, and accessed only by-name via
// Get/SetContactSpyInterface returning &member).
//
// Size 4 (X360-VERIFIED: single 32-bit field). The PS3 DecFIGS DWARF spells the
// member as `ContactSpyData* mpData` -- the same leading 32-bit slot on the 32-bit
// console; on this PC build the full ContactSpyData* layout (RaceCarContactQueue,
// TrafficContactQueue, ... run lists) is grown by ContactSpyData's own TU. The
// leading word is named muField0 to match the committed Construct body verbatim;
// rename to the typed pointer when ContactSpyData lands. Natural alignment: no
// Vector*/Matrix*/SIMD or EventQueue member.

#include "types.hpp"   // u32

namespace BrnPhysics
{
namespace ContactSpy
{
    // DWARF: BrnContactSpyInterface.h:48 (struct BrnPhysics::ContactSpy::ContactSpyInterface).
    struct ContactSpyInterface
    {
        u32 muField0;       // [0x00] cleared on construct (X360 0x82A61A18)

        ContactSpyInterface* Construct();
    };
}
}
