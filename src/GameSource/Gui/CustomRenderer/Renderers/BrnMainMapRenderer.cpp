#include "BrnMainMapRenderer.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGui::MainMapRenderer::MainMapRenderer @ 0x827DF3E8  (EXECUTED in the boot trace)
//
// The constructor installs the renderer's vtable (&off_820CF868), zero-initialises six
// small state groups (guest +0x48..+0xD0, each written as five dwords with the
// compiler's duplicate-first-write artifact on a 24-byte stride), then constructs the
// four embedded particle systems in a count-down loop (guest r30 = 3 .. 0 inclusive ->
// four iterations, base +0xF0, stride 0x21A0). Returns `this` (r3).

namespace BrnGui
{
namespace
{
    // FLAG: vtable pointer recovered from the data segment (ctor writes *this = &off_820CF868).
    // 64-bit literal so the guest 32-bit address widens cleanly to the host void* (no C4312).
    void* const KP_Vtable = reinterpret_cast<void*>(0x820CF868ull);
}

MainMapRenderer::MainMapRenderer()
{
    // Vtable install: stw &off_820CF868, 0(this).
    mpVtable = KP_Vtable;

    // Zero the six state groups (guest +0x48..+0xD0). Each group is five dwords; the X360
    // emits a duplicate-first-write per group (a clear-loop unroll artifact) and never
    // touches the 6th dword of each 24-byte stride.
    for (int g = 0; g < 6; ++g)
    {
        for (int i = 0; i < 5; ++i)
        {
            maZeroGroups[g][i] = 0;
        }
    }

    // Construct the four embedded particle systems (guest do-while, base +0xF0, stride
    // 0x21A0, r30 = 3 down through 0). Each array element is default-constructed in place;
    // the explicit loop mirrors the guest's per-element ctor call.
    for (int p = 0; p < 4; ++p)
    {
        // Placement re-construction over the already-default-constructed members would be
        // a no-op; the four CgsGui::ParticleSystem2d members are already constructed by the
        // implicit member-init above. The guest's explicit loop is the same construction.
        (void)maParticleSystems[p];
    }
}
}
