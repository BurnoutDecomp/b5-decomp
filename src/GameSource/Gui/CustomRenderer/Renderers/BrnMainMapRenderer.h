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

        // 0x82C290D8 -- `stb r4, 4(r3); blr` (returns this). DecFIGS attributes this leaf
        // to CgsCustomRenderer.h, so its BODY lives in the sibling
        // GameShared/.../CustomRenderer/CgsCustomRenderer.cpp; the DECLARATION belongs
        // here, on the one real MainMapRenderer, so there is a single class and a single
        // layout. (It was previously re-declared on two OTHER, differently-shaped
        // `BrnGui::MainMapRenderer` definitions -- see the ODR note in that .cpp.)
        MainMapRenderer* SetRenderEnabled(bool lbRenderEnabled);

    private:
        // Leading vtable slot (ctor writes &off_820CF868 to *this). FLAG: vtable pointer
        // modelled as an opaque slot set to a named data-segment constant. The real class
        // derives from CgsGui::CustomRenderComponentInterface (DWARF); this slice is not
        // reconstructed far enough to declare that base, so the vptr word stands in and
        // mbRenderEnabled below is the +0x04 flag the base owns on console.
        void* mpVtable;

        // [+0x04] the base CustomRenderComponentInterface::mbRenderEnabled flag
        // SetRenderEnabled writes. Declared here (not inherited) for the same
        // minimal-slice reason as mpVtable; the offset matches the console.
        bool mbRenderEnabled;

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
