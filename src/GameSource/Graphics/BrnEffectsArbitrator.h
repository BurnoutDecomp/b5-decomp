#pragma once

#include "SharedClasses/Graphics/BrnEffectsData.h"   // BrnEffectsFrame

// rw::IResourceAllocator (rwcore_structs.h) - Construct takes one; pointer-only here.
namespace rw { struct IResourceAllocator; }

// rw::graphics::postfx::ColourCube - EvalTint outputs an array of these (pointer-only here).
namespace rw { namespace graphics { namespace postfx { class ColourCube; } } }

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

        // Per-frame double-buffer flip: EndOfFrame promotes the internal frame to the
        // external slot and reconstructs the new internal frame ready for next frame.
        void EndOfFrame();

        // Resolve the tint colour-cubes and weights for the current external frame.
        // Returns false when the (layer-0, internal) frame's tint is disabled.
        bool EvalTint(rw::graphics::postfx::ColourCube** lppColourCubes, f32* lpfWeights) const;

    private:
        EffectsFramePairsPerLayerAndSlot mapaEffectsFrames;
        u8                               mu8EffectsFrameInternal;
        u8                               mu8EffectsFrameExternal;
    };
}
