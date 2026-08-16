#pragma once

#include "SharedClasses/Graphics/BrnEffectsData.h"   // BrnEffectsFrame, BrnEffects::*Data, MotionBlurData
#include "BrnCommonTypes.h"                          // Vector3 (rw::math::vpu::Vector3)

// rw::IResourceAllocator (rwcore_structs.h) - Construct takes one; pointer-only here.
namespace rw { struct IResourceAllocator; }

// rw::graphics::postfx::ColourCube - EvalTint outputs an array of these (pointer-only here).
namespace rw { namespace graphics { namespace postfx { class ColourCube; } } }

// BrnGraphics::EffectsArbitrator - owns the renderer's double-buffered post-FX effects
// frames (per layer/slot) and arbitrates which effects are active each frame.
//
// Declaration shape from the DecFIGS DWARF (references/DecFIGS/dwarfdump/GameSource/
// Graphics/BrnEffectsArbitrator.h:67..125 -- every method below is listed there with its
// :line); the layer tables and the inline bodies are corroborated verbatim by the
// Feb-2007 header (references/Feb-2007/BrnEntityModuleUnity/GameSource/Graphics/
// BrnEffectsArbitrator.h). The X360 ARTIST build attests:
//   Construct   @ 0x824008C0     EndOfFrame @ 0x823F77C8     EvalTint @ 0x823FD570
//   EvalEffectData<BloomData>        @ 0x823F9AA8 (sub_)
//   EvalEffectData<VignetteData>     @ 0x823F9DE0 (sub_)
//   EvalEffectData<DepthOfFieldData> @ 0x823FA060 (sub_)
//   EvalEffectData<BlurData>         @ 0x823FA390 (sub_)
//   EvalEffectData<TintData2d>       @ 0x823FA630 (sub_)
// (the five Eval*() members themselves are INLINED into BrnRendererModule::Render
// @0x8240BFA8 -- see BrnEffectsArbitrator.cpp for the pasted call sites.)
//
// The arbitrator owns a double-buffered set of effects frames organised as three layers,
// each layer holding EffectLayerDefinition<layer>::KU_NUM_SLOTS *pairs* of BrnEffectsFrame
// (one internal + one external sub-frame per slot). Each frame is 496 (0x1F0) bytes; a
// layer's frame array is carved from the renderer's resource allocator at Construct time.
// mu8EffectsFrameInternal / mu8EffectsFrameExternal select which sub-frame of each pair is
// read vs written this frame; they flip in EndOfFrame.
namespace BrnGraphics
{
    // The three producer layers. Names + order verbatim from the Feb-2007 header; the
    // count (3) is attested by every X360 loop over the tables (`cmplwi cr6, r29, 3`
    // in Construct @0x82400998, EndOfFrame @0x823F7844, EvalTint @0x823FD6CC and all
    // five EvalEffectData instantiations).
    enum EEffectsLayers
    {
        E_EFLAYER_BASE = 0,     // BrnEffects::EffectsModule::GenerateRenderRequests @0x8227FF10
        E_EFLAYER_WORLD,        // BrnWorld ...::EnvironmentManager::GenerateEffects  @0x827BE698
        E_EFLAYER_FXEVENTS,     // BrnGui::EffectsArbitrator::GenerateEffectFrameEvents @0x82503060

        E_EFLAYER_CNT
    };

    // Per-layer compile-time slot / colour-cube counts. Feb-2007 header, verbatim.
    //
    // ⭐ THESE ARE THE VALUES THE .rodata TABLES HOLD, AND THEY ARE NOW ATTESTED TWICE.
    // The committed floor said "{1,1,1}, UNRECOVERED". It is neither:
    //   (a) conductor idat dump of ARTIST_copy.i64 (scratch/postfx_step4_bloom/DATA_NOTE.md
    //       sections "What exists"/2): byte_8203E110 = 01 04 02 00, byte_8203E114 = 00 04 01 00;
    //   (b) this Feb-2007 header, independently:
    //         EffectLayerDefinition<E_EFLAYER_BASE>     { KU_NUM_SLOTS 1, KU_NUM_COLOURCUBES 0 }
    //         EffectLayerDefinition<E_EFLAYER_WORLD>    { KU_NUM_SLOTS 4, KU_NUM_COLOURCUBES 4 }
    //         EffectLayerDefinition<E_EFLAYER_FXEVENTS> { KU_NUM_SLOTS 2, KU_NUM_COLOURCUBES 1 }
    // The two agree exactly, and they explain the one thing a runtime table cannot: the X360
    // EvalEffectData bodies fold the LAYER-0 arm at compile time (no load of byte_8203E110[0];
    // see the asm pasted in BrnEffectsArbitrator.cpp) precisely because layer 0's count comes
    // through this template, while layers 1..2 are read from the runtime table by index.
    template <u32 LU_LAYER> struct EffectLayerDefinition;
    template <> struct EffectLayerDefinition<E_EFLAYER_BASE>     { enum { KU_NUM_SLOTS = 1, KU_NUM_COLOURCUBES = 0 }; };
    template <> struct EffectLayerDefinition<E_EFLAYER_WORLD>    { enum { KU_NUM_SLOTS = 4, KU_NUM_COLOURCUBES = 4 }; };
    template <> struct EffectLayerDefinition<E_EFLAYER_FXEVENTS> { enum { KU_NUM_SLOTS = 2, KU_NUM_COLOURCUBES = 1 }; };

    class EffectsArbitrator
    {
    public:
        // DWARF BrnEffectsArbitrator.h:63 / :64 (both nested in the class).
        typedef BrnEffectsFrame   EffectsFramePair[2];
        typedef EffectsFramePair* EffectsFramePairsPerLayerAndSlot[E_EFLAYER_CNT];

        // Seam spelling used by the wave-4 renderer/world call sites. These are ALIASES of
        // the attested EEffectsLayers enumerators -- no second source of truth. Prefer
        // E_EFLAYER_* in new code.
        static const u32 KU_EFFECTS_LAYER_BASE      = E_EFLAYER_BASE;
        static const u32 KU_EFFECTS_LAYER_WORLD     = E_EFLAYER_WORLD;
        static const u32 KU_EFFECTS_LAYER_FX_EVENTS = E_EFLAYER_FXEVENTS;

        void Construct(rw::IResourceAllocator* lpAllocator);   // DWARF :72, X360 @0x824008C0
        void Destruct();                                       // DWARF :76
        void StartOfFrame();                                   // DWARF :79
        void EndOfFrame();                                     // DWARF :82, X360 @0x823F77C8

        // The Update-side write cursor: the sub-frame the producers fill this frame.
        inline BrnEffectsFrame* GetExternalEffectsFrame(u8 luLayer, u8 luSlot);   // DWARF :85

        // The Render-side read cursor: the sub-frame the consumers read this frame. NOT a DWARF method:
        // the console INLINES this indexing at every read site in BrnRendererModule::Render @0x8240BFA8
        // (`496 * mu8EffectsFrameInternal + mapaEffectsFrames[0]` at 0x8240D714 / 0x8240D7C4 / 0x8240D8E4 /
        // 0x8240D9B0 / 0x8240DCAC and the motion-blur reads at 0x8240C2C0). Named here, as the mirror of the
        // DWARF's GetExternalEffectsFrame, so no caller reaches into mapaEffectsFrames with a stride.
        inline const BrnEffectsFrame& GetInternalEffectsFrame(u8 luLayer, u8 luSlot) const;

        bool EvalBloom(BrnEffects::BloomData& lData) const;                       // DWARF :88
        bool EvalVignette(BrnEffects::VignetteData& lData) const;                 // DWARF :91
        bool EvalDepthOfField(BrnEffects::DepthOfFieldData& lData) const;         // DWARF :94
        bool EvalBlur(BrnEffects::BlurData& lData) const;                         // DWARF :97
        // The length of the two arrays EvalTint fills, and therefore the length every caller
        // must supply. It is the sum of the per-layer COLOUR-CUBE counts over the layers EvalTint
        // walks (WORLD..FXEVENTS -- it starts at 1, `li r29, 1` @0x823FD5D4, and stops at 3,
        // `cmplwi cr6, r29, 3` @0x823FD6CC), i.e. 4 + 1 == 5. Sourced from the same compile-time
        // definitions as kau8ColourCubesPerEffectsLayer so the two cannot drift, and it is the
        // same 5 BrnPostFx's m_colourCubes[5] / m_tintFactors[5] hold and the same 5
        // BrnPostFx::BeginTintBlend @0x823F8380 asserts against ("m_numCubesToBlend <= 5",
        // BrnPostFx.cpp:266).
        static const u32 KU_TINT_COLOUR_CUBE_CNT =
            EffectLayerDefinition<E_EFLAYER_WORLD>::KU_NUM_COLOURCUBES
          + EffectLayerDefinition<E_EFLAYER_FXEVENTS>::KU_NUM_COLOURCUBES;

        bool EvalTint(rw::graphics::postfx::ColourCube** lppColourCubes,
                      f32* lpfWeights) const;                                     // DWARF :102
        bool EvalTint2d(BrnEffects::TintData2d& lData) const;                     // DWARF :105

        // Camera/vehicle state carried on the layer-0 INTERNAL frame. Bodies verbatim from
        // the Feb-2007 header (which spells them `inline` in the class); DWARF :110..:125
        // confirms the shape for the X360 build and adds GetMotionBlurData (post-Feb-2007).
        inline Vector3 GetLinearVelocity() const;                                 // DWARF :110
        inline Vector3 GetAngularVelocity() const;                                // DWARF :113
        inline f32     GetSpeedMPH() const;                                       // DWARF :116
        inline f32     GetSteering() const;                                       // DWARF :119
        inline bool    GetIsRacingGameplayCamera() const;                         // DWARF :122
        inline const BrnDirector::Camera::MotionBlurData& GetMotionBlurData() const;  // DWARF :125

    private:
        EffectsFramePairsPerLayerAndSlot mapaEffectsFrames;    // DWARF :130 (arbitrator +0x00)
        u8                               mu8EffectsFrameInternal;  // DWARF :131 (+0x0C)
        u8                               mu8EffectsFrameExternal;  // DWARF :131 (+0x0D)
    };

    // ---- inline bodies ---------------------------------------------------------------
    // All seven are the Feb-2007 bodies unchanged. The member offsets they compile to are
    // the ones the X360 uses: EvalTint @0x823FD594 reads `lbz r11, 0xC(r30)` (internal) and
    // `lwz r10, 0(r30)` (mapaEffectsFrames[0]); Construct @0x824009A0/A4 writes
    // `stb r25, 0xC(r26)` / `stb r24, 0xD(r26)`.

    inline BrnEffectsFrame*
    EffectsArbitrator::GetExternalEffectsFrame(u8 luLayer, u8 luSlot)
    {
        // Pair stride: EvalTint @0x823FD600..0x823FD618 forms the frame address as
        // `496 * (2 * slot + internal) + mapaEffectsFrames[layer]`, i.e. [slot] indexes
        // PAIRS and [internal/external] selects inside the pair -- exactly this expression.
        return &mapaEffectsFrames[luLayer][luSlot][mu8EffectsFrameExternal];
    }

    inline const BrnEffectsFrame&
    EffectsArbitrator::GetInternalEffectsFrame(u8 luLayer, u8 luSlot) const
    {
        return mapaEffectsFrames[luLayer][luSlot][mu8EffectsFrameInternal];
    }

    inline Vector3
    EffectsArbitrator::GetLinearVelocity() const
    {
        return mapaEffectsFrames[E_EFLAYER_BASE][0][mu8EffectsFrameInternal].GetLinearVelocity();
    }

    inline Vector3
    EffectsArbitrator::GetAngularVelocity() const
    {
        return mapaEffectsFrames[E_EFLAYER_BASE][0][mu8EffectsFrameInternal].GetAngularVelocity();
    }

    inline f32
    EffectsArbitrator::GetSpeedMPH() const
    {
        return mapaEffectsFrames[E_EFLAYER_BASE][0][mu8EffectsFrameInternal].GetSpeedMPH();
    }

    inline f32
    EffectsArbitrator::GetSteering() const
    {
        return mapaEffectsFrames[E_EFLAYER_BASE][0][mu8EffectsFrameInternal].GetSteering();
    }

    inline bool
    EffectsArbitrator::GetIsRacingGameplayCamera() const
    {
        return mapaEffectsFrames[E_EFLAYER_BASE][0][mu8EffectsFrameInternal].GetIsRacingGameplayCamera();
    }

    // Post-Feb-2007 addition (DWARF :125). The three fields it exposes are read by
    // BrnRendererModule::Render @0x8240BFA8 (pseudocode lines 398-401) off the layer-0
    // INTERNAL frame at +0x1D8 / +0x1DC / +0x1E0 -- mfCarsBlurAmount, mfWorldBlurAmount and
    // mbIsActive of the frame's MotionBlurData. Same [BASE][0][internal] cursor as the
    // Feb-2007 readers above.
    inline const BrnDirector::Camera::MotionBlurData&
    EffectsArbitrator::GetMotionBlurData() const
    {
        return mapaEffectsFrames[E_EFLAYER_BASE][0][mu8EffectsFrameInternal].GetMotionBlurData();
    }
}
