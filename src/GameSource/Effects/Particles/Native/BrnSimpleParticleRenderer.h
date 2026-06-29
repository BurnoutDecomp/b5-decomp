#pragma once

#include "types.hpp"

// BrnParticle::Native::BrnSimpleParticleRenderer
// A small POD struct that caches three pre-built renderengine blend states
// (one per global template: dword_83010F20/F24/F28).  Construct() is
// the only method; it is attributed to the stateparams.h TU by the X360 DWARF.
// Layout: 4 × u32-wide slots (no vtable).

namespace BrnParticle { namespace Native {

struct BrnSimpleParticleRenderer
{
    int   mInitParam;       // slot 0: receives the a3 argument from Construct()
    void* mpBlendState0;    // slot 1: blend state from dword_83010F20 template
    void* mpBlendState1;    // slot 2: blend state from dword_83010F24 template
    void* mpBlendState2;    // slot 3: blend state from dword_83010F28 template

    // X360 @ 0x82284518. Builds the three cached blend states from their global
    // templates; a2 is unused (present in the X360 call signature but never read).
    // Called from BrnParticle::ParticleModule::Prepare.
    int Construct(int a2, int a3);
};

} } // namespace BrnParticle::Native
