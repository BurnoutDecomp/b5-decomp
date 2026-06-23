#include "GameSource/Graphics/BrnEffectsArbitrator.h"
#include "SharedClasses/Graphics/BrnEffectsData.h"        // BrnEffectsFrame
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT machinery
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "rw/rwcore_structs.h"                            // rw::IResourceAllocator / rw::Resource / rw::ResourceDescriptor
#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGraphics::EffectsArbitrator::Construct   @ 0x824008C0
//   BrnGraphics::EffectsArbitrator::EndOfFrame  @ 0x823F77C8
//   BrnGraphics::EffectsArbitrator::EvalTint    @ 0x823FD570
//
// The arbitrator owns a double-buffered set of post-FX effects frames, organised as
// three layers, each layer holding kau8SlotsPerEffectsLayer[layer] *pairs* of
// BrnEffectsFrame (one internal + one external sub-frame per slot). Each frame is
// 496 (0x1F0) bytes; a layer's frame array is carved from the renderer's resource
// allocator at Construct time. mu8EffectsFrameInternal / mu8EffectsFrameExternal
// select which sub-frame of each pair is being written vs read this frame; they
// flip in EndOfFrame.

namespace BrnGraphics
{
namespace
{
    const u32 KU_NUM_EFFECTS_LAYERS = 3;
    const u32 KU_FRAME_SIZE_BYTES   = 0x1F0;   // sizeof(BrnEffectsFrame)
    const u32 KU_PAIR_SIZE_BYTES    = 2 * KU_FRAME_SIZE_BYTES;  // 992 -- a slot's frame pair

    const f32 KF_MAX_TINT_WEIGHT_SUM = 1.0f;   // flt_82001C98 (EvalTint over-weight assert bound)
    const f32 KF_INITIAL_LAYER_SUM   = 0.0f;   // flt_82001CC0 (per-layer weight accumulator seed)
}

// FLOOR (documented placeholder): the per-layer slot-count tables are read-only data
// in the .rodata of this TU (byte_8203E110 / byte_8203E114). That data section is NOT
// present in the available X360 exports, so the exact three entries of each table are
// UNRECOVERED. The placeholder below preserves the table SHAPE (uint8_t[3]) and is the
// only fabricated surface in this TU; the function bodies are reconstructed
// store-for-store from the asm and are correct for whatever the real counts are. A
// single slot per layer keeps the bring-up path consistent (one internal + one
// external sub-frame per layer) without asserting a count the binary did not attest.
namespace
{
    // kau8SlotsPerEffectsLayer (byte_8203E110): slot count per layer (Construct/EndOfFrame).
    const u8 kau8SlotsPerEffectsLayer[KU_NUM_EFFECTS_LAYERS] = { 1, 1, 1 };
    // kau8ColourCubesPerEffectsLayer (byte_8203E114): colour-cube/slot count per layer (EvalTint).
    const u8 kau8ColourCubesPerEffectsLayer[KU_NUM_EFFECTS_LAYERS] = { 1, 1, 1 };
}

void EffectsArbitrator::Construct(rw::IResourceAllocator* lpAllocator)
{
    for (u32 luLayer = 0; luLayer < KU_NUM_EFFECTS_LAYERS; ++luLayer)
    {
        const u32 luSlots = kau8SlotsPerEffectsLayer[luLayer];

        // Carve this layer's frame array (one pair per slot) from the resource
        // allocator. The X360 built a five-entry descriptor whose first entry is
        // {992*slots, align 16} and whose remaining four were the inert {0, 1}; the
        // PC DoAllocate is modelled on the four-entry rw::ResourceDescriptor (the
        // trailing fifth X360 entry is inert), matching the CgsResource::Type::new path.
        rw::ResourceDescriptor lDescriptor;
        for (u32 luEntry = 0; luEntry < 4; ++luEntry)
        {
            lDescriptor.m_baseResourceDescriptors[luEntry].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[luEntry].m_alignment = 1;
        }
        lDescriptor.m_baseResourceDescriptors[0].m_size      = KU_PAIR_SIZE_BYTES * luSlots;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;

        rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, 0);
        mapaEffectsFrames[luLayer] = static_cast<EffectsFramePair*>(lResource.m_baseResources[0]);

        // Construct both sub-frames of every slot in the carved array.
        for (u32 luSlot = 0; luSlot < luSlots; ++luSlot)
        {
            mapaEffectsFrames[luLayer][luSlot][0].Construct();
            mapaEffectsFrames[luLayer][luSlot][1].Construct();
        }
    }

    mu8EffectsFrameInternal = 0;
    mu8EffectsFrameExternal = 1;
}

void EffectsArbitrator::EndOfFrame()
{
    // Flip the double buffer: the frame just written (the old EXTERNAL sub-frame index)
    // becomes the new internal (this frame's read source), and the other sub-frame
    // becomes the new external (next frame's write target).
    const u8 lu8OldExternal = mu8EffectsFrameExternal;
    mu8EffectsFrameInternal = lu8OldExternal;
    mu8EffectsFrameExternal = static_cast<u8>(1 - lu8OldExternal);

    // Reset every slot's new-internal sub-frame ready for the next frame's writes.
    for (u32 luLayer = 0; luLayer < KU_NUM_EFFECTS_LAYERS; ++luLayer)
    {
        const u32 luSlots = kau8SlotsPerEffectsLayer[luLayer];
        for (u32 luSlot = 0; luSlot < luSlots; ++luSlot)
        {
            mapaEffectsFrames[luLayer][luSlot][mu8EffectsFrameExternal].Construct();
        }
    }
}

bool EffectsArbitrator::EvalTint(rw::graphics::postfx::ColourCube** lppColourCubes, f32* lpfWeights) const
{
    // Gate on the layer-0 internal frame's tint flag. (Layer 0 holds a single
    // internal/external pair selected by mu8EffectsFrameInternal.)
    const BrnEffectsFrame& lInternalLayer0Frame =
        mapaEffectsFrames[0][0][mu8EffectsFrameInternal];
    if (!lInternalLayer0Frame.GetUseTint())
    {
        return false;
    }

    u8 lu8ColourCube = 0;   // running output index across layers 1..2
    for (u32 luLayer = 1; luLayer < KU_NUM_EFFECTS_LAYERS; ++luLayer)
    {
        f32 lfLayerWeight = KF_INITIAL_LAYER_SUM;
        const u8 lu8LayerBase = lu8ColourCube;    // first output index this layer touches

        const u32 luSlots = kau8ColourCubesPerEffectsLayer[luLayer];
        for (u32 luSlot = 0; luSlot < luSlots; ++luSlot)
        {
            const BrnEffectsFrame& lFrame =
                mapaEffectsFrames[luLayer][luSlot][mu8EffectsFrameInternal];

            lppColourCubes[lu8ColourCube] =
                reinterpret_cast<rw::graphics::postfx::ColourCube*>(
                    static_cast<uintptr_t>(lFrame.GetTintData().muColourCube));
            lpfWeights[lu8ColourCube] = lFrame.GetTintWeight();
            lfLayerWeight += lFrame.GetTintWeight();
            ++lu8ColourCube;
        }

        if (lfLayerWeight > KF_MAX_TINT_WEIGHT_SUM)
        {
            char lacError[256];
            CgsCore::SPrintf(lacError, 256,
                             "Invalid effects data weights summing up to more than 1.0 on layer %d (%f)",
                             luLayer, lfLayerWeight);
            CGS_ASSERT(false, lacError);
        }

        // Re-weight the colour-cubes accumulated by the EARLIER layers by the residual
        // (1.0 - this layer's weight) so the running total stays normalised.
        const f32 lfResidual = KF_MAX_TINT_WEIGHT_SUM - lfLayerWeight;
        // The X360 forms the start index as a SIGN-EXTENDED byte: (s8)(base - 1).
        for (s32 liIndex = static_cast<s8>(lu8LayerBase - 1); liIndex >= 0; --liIndex)
        {
            lpfWeights[liIndex] *= lfResidual;
        }
    }

    return true;
}
}
