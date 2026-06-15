#pragma once

#include "SharedClasses/Graphics/BrnEffectsData.h"   // BrnEffectsFrame

// rw::IResourceAllocator (rwcore_structs.h) - Construct takes one; pointer-only here.
namespace rw { struct IResourceAllocator; }

// BrnGraphics::EffectsArbitrator - owns the renderer's double-buffered post-FX effects
// frames (per layer/slot) and arbitrates which effects are active each frame. Layout
// reconstructed from the DecFIGS DWARF (GameSource/Graphics/BrnEffectsArbitrator.h). The
// methods (Construct/StartOfFrame/EndOfFrame/Eval*/Get*) are reconstructed with their
// bodies when the renderer's effects path is restored; the layout here is what
// BrnRendererModule embeds.
namespace BrnGraphics
{
    class EffectsArbitrator
    {
    public:
        typedef BrnEffectsFrame   EffectsFramePair[2];
        typedef EffectsFramePair* EffectsFramePairsPerLayerAndSlot[3];

        void Construct(rw::IResourceAllocator* lpAllocator);

    private:
        EffectsFramePairsPerLayerAndSlot mapaEffectsFrames;
        u8                               mu8EffectsFrameInternal;
        u8                               mu8EffectsFrameExternal;
    };
}
