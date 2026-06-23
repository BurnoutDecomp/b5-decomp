#ifndef BRN_MAIN_MAP_RENDERER_H
#define BRN_MAIN_MAP_RENDERER_H

#include "types.hpp"
#include "GameShared/GameClasses/Gui/View/CgsParticleSystem2d.h" // CgsGui::ParticleSystem2d

// BrnGui::MainMapRenderer - the custom HUD renderer that draws the main (full-screen)
// map screen. Owns a small bank of screen-space particle systems for the map's
// animated dressing.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::MainMapRenderer::MainMapRenderer @ 0x827DF3E8  (constructor; EXECUTED in the boot trace)
//
// MINIMAL-SLICE class: only the constructor is in scope. It installs the renderer's
// vtable, zero-initialises a band of small fixed-size state groups, then in-place
// constructs four embedded particle systems. The full renderer (its
// CustomRenderComponentInterface base, the map geometry / icon state, the draw
// machinery) is uncommitted and OMITTED beyond what the ctor touches. FLAG:
// minimal-slice class. Byte offsets are not load-bearing on the 64-bit host, so members
// are declared by name (no raw-offset casts).

namespace BrnGui
{
    class MainMapRenderer
    {
    public:
        // 0x827DF3E8 -- zero the state groups and construct the four particle systems.
        MainMapRenderer();

    private:
        // Leading vtable slot (ctor writes &off_820CF868 to *this). FLAG: vtable pointer
        // modelled as an opaque slot set to a named data-segment constant.
        void* mpVtable;

        // Six zero-initialised state groups (guest +0x48..+0xD0). The ctor clears each as
        // five dwords with the compiler's duplicate-first-write unroll; the 6th dword of
        // each 24-byte stride (+0x5C/+0x74/+0x8C/+0xA4/+0xBC/+0xD4) is a GAP the guest never
        // writes, so only the five real slots per group are modelled (gap-aware).
        u32 maZeroGroups[6][5];

        // Four embedded screen-space particle systems (guest array at +0xF0, stride 0x21A0;
        // the ctor's do-while runs r30 = 3 down through 0 -> four in-place constructions).
        CgsGui::ParticleSystem2d maParticleSystems[4];
    };
}

#endif
