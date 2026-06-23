#ifndef CGS_PARTICLE_SYSTEM_2D_H
#define CGS_PARTICLE_SYSTEM_2D_H

#include "types.hpp"

// CgsGui::ParticleSystem2d - a 2D (screen-space) particle system the GUI custom
// renderers embed to drive HUD particle effects. The full type is its own translation
// unit (todo); this header declares the MINIMAL surface the in-scope users need: the
// default constructor, which is invoked in-place on each embedded instance.
//
// Reconstructed surface from BURNOUT_X360_ARTIST.XEX:
//   CgsGui::ParticleSystem2d::ParticleSystem2d @ 0x827DE758  (default ctor)
//
// The guest object is large (stride 0x21A0 == 8608 bytes when arrayed). Its internal
// layout is out of scope here, so it is modelled as an opaque fixed-size byte span of
// the attested guest size so that embedding owners (e.g. BrnGui::MainMapRenderer) can
// hold an array of instances by value and construct them. FLAG: opaque boundary type
// sized to the X360 stride; the real members are reconstructed in its own TU.

namespace CgsGui
{
    class ParticleSystem2d
    {
    public:
        // The X360 guest stride between arrayed instances (8608 bytes).
        static const u32 KU_GUEST_SIZE = 8608;

        // 0x827DE758 -- default constructor. Body links from the ParticleSystem2d TU.
        ParticleSystem2d();

    private:
        // Opaque storage sized to the attested guest object so owners can embed it by
        // value. FLAG: boundary placeholder -- the real layout is its own TU.
        u8 maOpaque[KU_GUEST_SIZE];
    };
}

#endif
