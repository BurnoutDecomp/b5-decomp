#include "GameSource/Graphics/BrnEffectsArbitrator.h"
#include "SharedClasses/Graphics/BrnEffectsData.h"        // BrnEffectsFrame
#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT machinery
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "rw/rwcore_structs.h"                            // rw::IResourceAllocator / rw::Resource / rw::ResourceDescriptor
#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGraphics::EffectsArbitrator::Construct        @ 0x824008C0
//   BrnGraphics::EffectsArbitrator::EndOfFrame       @ 0x823F77C8
//   BrnGraphics::EffectsArbitrator::EvalTint         @ 0x823FD570
//   EvalEffectData<BrnEffects::BloomData>            @ 0x823F9AA8   (sub_)
//   EvalEffectData<BrnEffects::VignetteData>         @ 0x823F9DE0   (sub_)
//   EvalEffectData<BrnEffects::DepthOfFieldData>     @ 0x823FA060   (sub_)
//   EvalEffectData<BrnEffects::BlurData>             @ 0x823FA390   (sub_)
//   EvalEffectData<BrnEffects::TintData2d>           @ 0x823FA630   (sub_)
// Declaration shape: references/DecFIGS/dwarfdump/GameSource/Graphics/BrnEffectsArbitrator.{h,cpp}
// (that .cpp dump names EVERY helper below and gives its source line -- EvalUseEffect :52,
// EvalEffectData :59, Construct :87, Destruct :119, StartOfFrame :135, EndOfFrame :151,
// EvalBloom :178, EvalVignette :202, EvalDepthOfField :226, EvalBlur :250, EvalTint :275,
// EvalTint2d :350, and the EffectData/EffectWeight/UseEffect specialisations at :552..:671).
//
// ⭐ WHY THE FIVE `sub_` ARE **NOT** THE Eval*() MEMBERS.
// Their signature is (out /*r3*/, mapaEffectsFrames /*r4*/, mu8EffectsFrameInternal /*r5*/):
// r3 is the OUT-param, not `this` -- `mulli r27, r29, 0x1F0` then `lwz r11, 0(r28)` uses r4 as
// the frame-pointer ARRAY and r5 as the internal index (sub_823F9AA8 @0x823F9AC0..0x823F9AD0).
// Contrast EvalTint @0x823FD594, which IS a member: `lbz r11, 0xC(r30=r3)` / `lwz r10, 0(r30)`
// reads the arbitrator's own members off r3. That triple is exactly the DWARF's free function
//   template <class T> void EvalEffectData(T& lRes,
//        EffectsArbitrator::EffectsFramePair* const (&lapaEffectsFrames)[3], uint8_t lu8Internal)
// so the five subs are its five instantiations; the thin Eval*() members that wrap them were
// inlined into their single caller, BrnRendererModule::Render @0x8240BFA8, e.g.
//     if ( *(496 * *(v12 + 1164) + *(v12 + 1152)) )      // <- EvalUseEffect<BloomData>, frame +0
//     { sub_823F9AA8(&v311, v12 + 1152); v143 = 1; }     // <- EvalEffectData<BloomData>
//     else { v143 = 0; }                                 // (Render pseudocode lines 969-977)
// -- which is precisely `EvalBloom` as the DWARF spells it (EvalUseEffect then EvalEffectData).
//
// LAYOUT NOTE (guest vs host). Every frame address in the asm is `496 * (2*slot + internal) +
// mapaEffectsFrames[layer]`; that 496 is the CONSOLE sizeof(BrnEffectsFrame). Nothing below
// uses it: frames are reached as `mapaEffectsFrames[layer][slot][internal]` (host stride) and
// their fields BY NAME. KU_FRAME_SIZE_BYTES/KU_PAIR_SIZE_BYTES appear only in Construct's
// resource-descriptor size, which is a byte count the allocator wants, computed from the host
// sizeof so the carve is right on either target.

namespace BrnGraphics
{
namespace
{
    // A slot owns one internal + one external sub-frame; Construct carves KU_PAIR_SIZE_BYTES
    // per slot (X360: `mulli r11, r11, 0x3E0` @0x82400920, 0x3E0 == 992 == 2 * 496).
    const u32 KU_PAIR_SIZE_BYTES = 2 * sizeof(BrnEffectsFrame);

    // flt_82001C98 == 1.0f and flt_82001CC0 == 0.0f -- both dumped byte-for-byte in
    // scratch/postfx_wave1b_dossiers/DATA_DUMP.md PART 2 (0x3F800000 / 0x00000000).
    // The 1.0f is the per-layer weight-sum bound AND the residual base `1 - w`
    // (`fsubs f0, f30, f31` @0x823F9D44); the 0.0f is the default-arm weight and
    // EvalTint's per-layer accumulator seed.
    const f32 KF_MAX_LAYER_WEIGHT_SUM = 1.0f;
    const f32 KF_ZERO_LAYER_WEIGHT    = 0.0f;

    // byte_8203E110 / byte_8203E114 (this TU's .rodata). Values attested twice -- see the
    // EffectLayerDefinition banner in BrnEffectsArbitrator.h. Read as `lbzx r11, <layer>, r18`
    // (bloom @0x823F9B4C), `lbzx r11, r29, r27` (Construct @0x82400918, EndOfFrame @0x823F77F8)
    // and `lbzx r11, r29, r27` off byte_8203E114 in EvalTint (@0x823FD5E0) -- i.e. the SLOT
    // table drives Construct/EndOfFrame/EvalEffectData and the COLOUR-CUBE table drives only
    // EvalTint. Sourced from the compile-time definitions so the two cannot drift.
    const u8 kau8SlotsPerEffectsLayer[E_EFLAYER_CNT] =
    {
        EffectLayerDefinition<E_EFLAYER_BASE>::KU_NUM_SLOTS,
        EffectLayerDefinition<E_EFLAYER_WORLD>::KU_NUM_SLOTS,
        EffectLayerDefinition<E_EFLAYER_FXEVENTS>::KU_NUM_SLOTS
    };
    const u8 kau8ColourCubesPerEffectsLayer[E_EFLAYER_CNT] =
    {
        EffectLayerDefinition<E_EFLAYER_BASE>::KU_NUM_COLOURCUBES,
        EffectLayerDefinition<E_EFLAYER_WORLD>::KU_NUM_COLOURCUBES,
        EffectLayerDefinition<E_EFLAYER_FXEVENTS>::KU_NUM_COLOURCUBES
    };

    // The two assert texts every EvalEffectData instantiation and EvalTint reference.
    //
    // aInvalidNumberO_3 is RECOVERED VERBATIM (Hex-Rays prints the whole literal at the
    // sub_823F9AA8 FireAssert call site) -- including the shipped typo "EffecsFrame".
    const char* const kacInvalidSlotCount = "Invalid number of slots per EffecsFrame source";

    // ⚠ aInvalidEffects is TEXT-PARTIAL. The export truncates it to
    //   "Invalid effects data weights summing up"... and the .rodata bytes are in no dossier,
    // so the TAIL is the committed tree's existing wording, carried over, NOT a fresh read.
    // What IS recovered from the asm is the ARGUMENT ORDER, and the committed EvalTint had it
    // backwards: every call site is
    //     fmr f1, f31 / stfd f1, var / ld r6, var   <- r6 = the WEIGHT (double bits)
    //     mr  r7, <layer>                           <- r7 = the LAYER index
    //     mr  r5, aInvalidEffects / li r4, 0x100 / addi r3, <buf> / bl CgsCore__SPrintf
    // (EvalTint @0x823FD658-0x823FD674; bloom @0x823F9D00-0x823F9D1C; vignette @0x823F9FE8;
    // dof @0x823FA2E4; blur @0x823FA5BC; tint2d @0x823FA8AC -- all identical). GPRs are 64-bit
    // on X360, so a double vararg takes ONE slot: vararg1 = weight (r6), vararg2 = layer (r7).
    // The %f therefore precedes the %d. See CONDUCTOR DUMP REQUEST in the wave report.
    const char* const kacInvalidLayerWeights =
        "Invalid effects data weights summing up to more than 1.0 (%f) on layer %d";

    // ---------------------------------------------------------------------------------
    // Per-effect frame accessors. DWARF BrnEffectsArbitrator.cpp declares one specialisation
    // of each per data type: UseEffect at :636 (Bloom) :643 (Vignette) :650 (DoF) :657 (Blur)
    // :664 (Tint) :671 (Tint2d); EffectData at :552/:559/:566/:573/(:580 Tint)/:587;
    // EffectWeight at :594/:601/:608/:615/(:622 Tint)/:629. They are one-line member reads --
    // the frame offsets each compiles to are pinned by the asm cited beside them.
    // ---------------------------------------------------------------------------------
    template <class T> bool     UseEffect(const BrnEffectsFrame& lEffectsFrame);
    template <class T> const T& EffectData(const BrnEffectsFrame& lEffectsFrame);
    template <class T> f32      EffectWeight(const BrnEffectsFrame& lEffectsFrame);

    // frame +0 / +0x20 / +8  (Render @0x8240BFA8 line 969; sub_823F9AA8 `addi r11, r11, 0x20`
    // @0x823F9AE0 and `lfs f31, 8(r10)` @0x823F9CD0)
    template <> bool UseEffect<BrnEffects::BloomData>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.GetUseBloom(); }
    template <> const BrnEffects::BloomData& EffectData<BrnEffects::BloomData>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.GetBloomData(); }
    template <> f32 EffectWeight<BrnEffects::BloomData>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.GetBloomWeight(); }

    // frame +1 / +0x40 / +0xC  (Render line 997; sub_823F9DE0 `addi r11, r11, 0x40`
    // @0x823F9E18 and `lfs f31, 0xC(r8)` @0x823F9FDC)
    template <> bool UseEffect<BrnEffects::VignetteData>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.GetUseVignette(); }
    template <> const BrnEffects::VignetteData& EffectData<BrnEffects::VignetteData>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.GetVignetteData(); }
    template <> f32 EffectWeight<BrnEffects::VignetteData>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.GetVignetteWeight(); }

    // frame +2 / +0x90 / +0x10  (Render line 1067; sub_823FA060 `addi r11, r11, 0x90`
    // @0x823FA094 and `lfs f31, 0x10(r8)` @0x823FA2D8)
    template <> bool UseEffect<BrnEffects::DepthOfFieldData>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.GetUseDepthOfField(); }
    template <> const BrnEffects::DepthOfFieldData& EffectData<BrnEffects::DepthOfFieldData>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.GetDepthOfFieldData(); }
    template <> f32 EffectWeight<BrnEffects::DepthOfFieldData>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.GetDepthOfFieldWeight(); }

    // frame +3 / +0xB0 / +0x14  (Render line 1097; sub_823FA390 `addi r11, r11, 0xB0`
    // @0x823FA3C8 and `lfs f31, 0x14(r8)` @0x823FA5B0)
    template <> bool UseEffect<BrnEffects::BlurData>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.GetUseBlur(); }
    template <> const BrnEffects::BlurData& EffectData<BrnEffects::BlurData>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.GetBlurData(); }
    template <> f32 EffectWeight<BrnEffects::BlurData>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.GetBlurWeight(); }

    // frame +4 / +0x110 / +0x18  (EvalTint @0x823FD5A4 `lbz r11, 4(r11)`,
    // @0x823FD62C `lwz r9, 0x110(r10)`, @0x823FD634 `lfs f0, 0x18(r10)`)
    template <> bool UseEffect<BrnEffects::TintData>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.GetUseTint(); }
    template <> const BrnEffects::TintData& EffectData<BrnEffects::TintData>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.GetTintData(); }
    template <> f32 EffectWeight<BrnEffects::TintData>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.GetTintWeight(); }

    // frame +5 / +0x120 / +0x1C  (Render line 1242; sub_823FA630 `addi r11, r11, 0x120`
    // @0x823FA668 and `lfs f31, 0x1C(r10)` @0x823FA88C)
    template <> bool UseEffect<BrnEffects::TintData2d>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.GetUseTint2d(); }
    template <> const BrnEffects::TintData2d& EffectData<BrnEffects::TintData2d>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.GetTintData2d(); }
    template <> f32 EffectWeight<BrnEffects::TintData2d>(const BrnEffectsFrame& lEffectsFrame)
    { return lEffectsFrame.Get2dTintWeight(); }

    // ---------------------------------------------------------------------------------
    // EvalUseEffect<T> (DWARF BrnEffectsArbitrator.cpp:52; its one local `lSlot` at :391).
    // The gate is the BASE layer's single slot only -- Render tests exactly one byte,
    // `*(496 * internal + mapaEffectsFrames[0] + <effect index>)`, for all six effects
    // (lines 969 / 997 / 1067 / 1097 / 1242 and EvalTint @0x823FD594).
    // ---------------------------------------------------------------------------------
    template <class T>
    bool EvalUseEffect(EffectsArbitrator::EffectsFramePair* const (&lapaEffectsFrames)[E_EFLAYER_CNT],
                       u8 lu8EffectsFrameInternal)
    {
        const BrnEffectsFrame& lSlot = lapaEffectsFrames[E_EFLAYER_BASE][0][lu8EffectsFrameInternal];
        return UseEffect<T>(lSlot);
    }

    // ---------------------------------------------------------------------------------
    // EvalEffectData<T> (DWARF BrnEffectsArbitrator.cpp:59; locals lu8Layer :404, the
    // single/two/four-slot lSlot* at :410 / :419,:421 / :434..:440, lAltData+lrAltWeight
    // :466,:467 with their own :472 / :482,:484 / :497..:503, lacError :534).
    //
    // Shape, store-for-store, from the five X360 bodies (they are byte-for-byte the same
    // algorithm; sub_823F9AA8 is quoted for line refs):
    //   1. seed lRes from the BASE layer's slot 0                        @0x823F9AF8-0x823F9B20
    //   2. for layer = WORLD..FXEVENTS:                                  @0x823F9B4C / 0x823F9DC8
    //        switch on kau8SlotsPerEffectsLayer[layer]                    @0x823F9B50-0x823F9B64
    //          1 -> lAltData = that slot's data, lrAltWeight = its weight @0x823F9CBC
    //          2 -> 2-way blend, lrAltWeight = w0+w1                      @0x823F9C0C
    //          4 -> 4-way blend, lrAltWeight = w0+w1+w2+w3                @0x823F9BA4
    //          default -> assert(522), lAltData.Construct(), weight 0     @0x823F9B68-0x823F9BA0
    //        assert if lrAltWeight > 1.0f (line 540)                      @0x823F9CF8-0x823F9D34
    //        lRes = lRes*(1-lrAltWeight) + lAltData*lrAltWeight           @0x823F9D38-0x823F9DC4
    //
    // DE-OPTIMISATIONS (each is the compiler folding something, not a behaviour change):
    //  * The DWARF shows a THREE-ARM switch for step 1 too (:410/:419/:434). The X360 emits
    //    only the single-slot arm and never loads byte_8203E110[0] -- because layer 0's count
    //    comes from EffectLayerDefinition<E_EFLAYER_BASE>::KU_NUM_SLOTS, a compile-time 1.
    //    Writing the dead arms here would be dead code, so step 1 is spelled as the arm the
    //    console kept, with this note. (Asm: 0x823F9AD0 `lwz r11, 0(r28)`, 0x823F9AE0
    //    `addi r11, r11, 0x20`, then four ld/std pairs -- no table load anywhere.)
    //  * Steps 2's blends are `T::SetToBlend(...)` in the source. The console OUTLINED the
    //    4-way for Bloom (BrnEffects::BloomData::SetToBlend @0x823F34B0, called @0x823F9C04),
    //    Vignette (@0x823F3758, called @0x823F9F6C) and Blur (@0x823F3C30, called @0x823FA540),
    //    outlined the 2-way for Vignette (@0x823F35B8) and Blur (@0x823F3A50), and INLINED
    //    every other one (all of DoF's and Tint2d's, and Bloom's 2-way). Restored as calls.
    //  * The final lerp is literally `SetToBlend(lRes, 1-w, lAltData, w)`: the vignette body
    //    calls it out of line -- @0x823FA020 `addi r6, r1, var_1F0` / `fmr f2, f31` /
    //    `mr r4, r17` / `fsubs f1, f29, f31` / `mr r3, r17` / `bl VignetteData::SetToBlend`
    //    (r3 == r4 == lRes, r6 == &lAltData) -- and the others inline the same arithmetic.
    //  * The default arm's stores are the T::Construct() defaults (bloom: 0.98 / 0.77 /
    //    unk_82FFAED0; vignette: flt_820A3A9C/AA0 + unk_82FFAE20/AEC0/B220/B1A0; blur:
    //    flt_820A3AB4..AC4 + unk_82FFAFC0/AE90/B030/B180; tint2d: unk_82FFB230), so it is
    //    written as Construct(). DoF's arm stores only 4 of its 5 floats -- mfDofAmount is
    //    never read downstream, so the compiler dropped that one store.
    //  * The `if (lrAltWeight > 1.0f)` test is emitted after the switch; the default arm
    //    branches straight past it because its weight is the literal 0.0f (`fmr f31, f29`
    //    @0x823F9B88 then `b loc_823F9D38`), i.e. constant-folded, not a different path.
    // ---------------------------------------------------------------------------------
    template <class T>
    void EvalEffectData(T& lRes,
                        EffectsArbitrator::EffectsFramePair* const (&lapaEffectsFrames)[E_EFLAYER_CNT],
                        u8 lu8EffectsFrameInternal)
    {
        u8 lu8Layer = E_EFLAYER_BASE;

        {
            const BrnEffectsFrame& lSlot = lapaEffectsFrames[lu8Layer][0][lu8EffectsFrameInternal];
            lRes = EffectData<T>(lSlot);
        }

        for (lu8Layer = E_EFLAYER_WORLD; lu8Layer < E_EFLAYER_CNT; ++lu8Layer)
        {
            T   lAltData;
            f32 lrAltWeight;

            switch (kau8SlotsPerEffectsLayer[lu8Layer])
            {
            case 1:
                {
                    const BrnEffectsFrame& lSlot =
                        lapaEffectsFrames[lu8Layer][0][lu8EffectsFrameInternal];

                    lAltData    = EffectData<T>(lSlot);
                    lrAltWeight = EffectWeight<T>(lSlot);
                }
                break;

            case 2:
                {
                    const BrnEffectsFrame& lSlot0 =
                        lapaEffectsFrames[lu8Layer][0][lu8EffectsFrameInternal];
                    const BrnEffectsFrame& lSlot1 =
                        lapaEffectsFrames[lu8Layer][1][lu8EffectsFrameInternal];

                    lrAltWeight = EffectWeight<T>(lSlot0) + EffectWeight<T>(lSlot1);
                    lAltData.SetToBlend(EffectData<T>(lSlot0), EffectWeight<T>(lSlot0),
                                        EffectData<T>(lSlot1), EffectWeight<T>(lSlot1));
                }
                break;

            case 4:
                {
                    const BrnEffectsFrame& lSlot0 =
                        lapaEffectsFrames[lu8Layer][0][lu8EffectsFrameInternal];
                    const BrnEffectsFrame& lSlot1 =
                        lapaEffectsFrames[lu8Layer][1][lu8EffectsFrameInternal];
                    const BrnEffectsFrame& lSlot2 =
                        lapaEffectsFrames[lu8Layer][2][lu8EffectsFrameInternal];
                    const BrnEffectsFrame& lSlot3 =
                        lapaEffectsFrames[lu8Layer][3][lu8EffectsFrameInternal];

                    lrAltWeight = EffectWeight<T>(lSlot0) + EffectWeight<T>(lSlot1)
                                + EffectWeight<T>(lSlot2) + EffectWeight<T>(lSlot3);
                    lAltData.SetToBlend(EffectData<T>(lSlot0), EffectWeight<T>(lSlot0),
                                        EffectData<T>(lSlot1), EffectWeight<T>(lSlot1),
                                        EffectData<T>(lSlot2), EffectWeight<T>(lSlot2),
                                        EffectData<T>(lSlot3), EffectWeight<T>(lSlot3));
                }
                break;

            default:
                // ../Graphics/BrnEffectsArbitrator.cpp:522 on the console (`li r5, 0x20A`).
                CGS_ASSERT(false, kacInvalidSlotCount);
                lAltData.Construct();
                lrAltWeight = KF_ZERO_LAYER_WEIGHT;
                break;
            }

            if (lrAltWeight > KF_MAX_LAYER_WEIGHT_SUM)
            {
                // ../Graphics/BrnEffectsArbitrator.cpp:540 on the console (`li r5, 0x21C`).
                char lacError[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsCore::SPrintf(lacError, CgsDev::Assert::KI_MESSAGEBUFFERSIZE,
                                 kacInvalidLayerWeights, lrAltWeight, lu8Layer);
                CGS_ASSERT(false, lacError);
            }

            // Fold this layer in: the accumulated result keeps (1 - layerWeight) of itself.
            // Self-blend is safe (and is what the console does -- r3 == r4 at 0x823FA034):
            // every output member is written only after its own inputs are read.
            lRes.SetToBlend(lRes, KF_MAX_LAYER_WEIGHT_SUM - lrAltWeight, lAltData, lrAltWeight);
        }
    }
}

// ---- Construct @0x824008C0 (DWARF BrnEffectsArbitrator.cpp:87) -----------------------
void EffectsArbitrator::Construct(rw::IResourceAllocator* lpAllocator)
{
    for (u32 luLayer = 0; luLayer < E_EFLAYER_CNT; ++luLayer)
    {
        const u32 luSlots = kau8SlotsPerEffectsLayer[luLayer];

        // Carve this layer's frame array (one pair per slot) from the resource
        // allocator. The X360 built a five-entry descriptor whose first entry is
        // {992*slots, align 16} (`mulli r11, r11, 0x3E0` @0x82400920, `li r11, 0x10`
        // @0x824008CC) and whose remaining four were the inert {0, 1} (the eight
        // `stw r25/r24` pairs @0x824008F8..0x8240092C); the PC DoAllocate is modelled on
        // the four-entry rw::ResourceDescriptor (the trailing fifth X360 entry is inert),
        // matching the CgsResource::Type::new path.
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

        // Construct both sub-frames of every slot in the carved array
        // (@0x82400960-0x8240098C: BrnEffectsFrame::Construct on the pair base, then on
        // base + 0x1F0).
        for (u32 luSlot = 0; luSlot < luSlots; ++luSlot)
        {
            mapaEffectsFrames[luLayer][luSlot][0].Construct();
            mapaEffectsFrames[luLayer][luSlot][1].Construct();
        }
    }

    mu8EffectsFrameInternal = 0;   // stb r25, 0xC(r26) @0x824009A0
    mu8EffectsFrameExternal = 1;   // stb r24, 0xD(r26) @0x824009A4
}

// ---- Destruct (DWARF BrnEffectsArbitrator.cpp:119) -----------------------------------
// EMPTY, and that is a read, not a guess:
//  (a) the DWARF body carries no locals and no calls at all -- contrast EndOfFrame :151,
//      whose dump lists lu8Layer/lu8Slot, and Construct :87, which lists its allocator calls;
//  (b) the X360 export has no BrnGraphics::EffectsArbitrator::Destruct symbol:
//        $ python - <<'PY'  (over all 30,095 .ida-exports/BURNOUT_X360_ARTIST.XEX/*.json)
//          ... prints every d['name'] containing 'EffectsArbitrator' ...
//        BrnGraphics::EffectsArbitrator::EndOfFrame / ::EvalTint / ::Construct   <- only three
//        (the other 19 hits are all BrnGui::EffectsArbitrator::*)
//      -- so nothing was outlined; an empty body is the only shape that leaves no trace.
// The frames themselves are owned by the resource allocator, which is torn down with the
// renderer's memory, so there is nothing to release here.
void EffectsArbitrator::Destruct()
{
}

// ---- StartOfFrame (DWARF BrnEffectsArbitrator.cpp:135) ------------------------------
// EMPTY, same two witnesses as Destruct, plus a third: its console caller does not touch a
// single arbitrator member. BrnRendererModule::StartOfFrame @0x823FC160 is 117 instructions
// and every `this`-relative offset it writes is 0x2A8 / 0xB0C / 0xB20 / 0xB28 / 0xB2C /
// 0x1094 / 0x1274 / 0x1554 / 0x159C / 0x1730 / 0x1798 / 0x17AC / 0x17B4 / 0x17B8 / 0x3800 --
// none of 0x480..0x48D (the arbitrator). So the call, if it is there, expands to nothing.
void EffectsArbitrator::StartOfFrame()
{
}

// ---- EndOfFrame @0x823F77C8 (DWARF BrnEffectsArbitrator.cpp:151) --------------------
void EffectsArbitrator::EndOfFrame()
{
    // Flip the double buffer: the frame just written (the old EXTERNAL sub-frame index)
    // becomes the new internal (this frame's read source), and the other sub-frame
    // becomes the new external (next frame's write target).
    // `lbz r11, 0xD(r30)` / `subfic r10, r10, 1` / `stb r11, 0xC(r30)` / `stb r10, 0xD(r30)`
    // @0x823F77DC-0x823F77F4.
    const u8 lu8OldExternal = mu8EffectsFrameExternal;
    mu8EffectsFrameInternal = lu8OldExternal;
    mu8EffectsFrameExternal = static_cast<u8>(1 - lu8OldExternal);

    // Reset every slot's new-external sub-frame ready for next frame's writes. The console
    // re-reads mu8EffectsFrameExternal inside the loop (`lbz r9, 0xD(r30)` @0x823F780C).
    for (u32 luLayer = 0; luLayer < E_EFLAYER_CNT; ++luLayer)
    {
        const u32 luSlots = kau8SlotsPerEffectsLayer[luLayer];
        for (u32 luSlot = 0; luSlot < luSlots; ++luSlot)
        {
            mapaEffectsFrames[luLayer][luSlot][mu8EffectsFrameExternal].Construct();
        }
    }
}

// ---- EvalBloom (DWARF BrnEffectsArbitrator.cpp:178) ---------------------------------
// Inlined into BrnRendererModule::Render @0x8240BFA8; the call site (pseudocode 969-977) is
//   if ( *(496 * *(v12 + 1164) + *(v12 + 1152)) ) { sub_823F9AA8(&v311, v12 + 1152); v143 = 1; }
//   else                                          { v143 = 0; }
// i.e. exactly EvalUseEffect<BloomData> -> EvalEffectData<BloomData> -> true, and the
// returned bool is the caller's "bloom is active" flag (it goes on to set BrnPostFx's
// bloom bit and write mColour/mfThreshold/mfLuminance).
bool EffectsArbitrator::EvalBloom(BrnEffects::BloomData& lData) const
{
    if (!EvalUseEffect<BrnEffects::BloomData>(mapaEffectsFrames, mu8EffectsFrameInternal))
    {
        return false;
    }

    EvalEffectData<BrnEffects::BloomData>(lData, mapaEffectsFrames, mu8EffectsFrameInternal);
    return true;
}

// ---- EvalVignette (DWARF BrnEffectsArbitrator.cpp:202) ------------------------------
// Render pseudocode 996-1001: `v151 = *(v12 + 1164); if (*(496*v151 + *(v12+1152) + 1))
// { sub_823F9DE0(v330, (v12 + 1152), v151); v152 = 1; } else v152 = 0;`
bool EffectsArbitrator::EvalVignette(BrnEffects::VignetteData& lData) const
{
    if (!EvalUseEffect<BrnEffects::VignetteData>(mapaEffectsFrames, mu8EffectsFrameInternal))
    {
        return false;
    }

    EvalEffectData<BrnEffects::VignetteData>(lData, mapaEffectsFrames, mu8EffectsFrameInternal);
    return true;
}

// ---- EvalDepthOfField (DWARF BrnEffectsArbitrator.cpp:226) --------------------------
// Render pseudocode 1067-1071 (frame byte +2 -> sub_823FA060).
bool EffectsArbitrator::EvalDepthOfField(BrnEffects::DepthOfFieldData& lData) const
{
    if (!EvalUseEffect<BrnEffects::DepthOfFieldData>(mapaEffectsFrames, mu8EffectsFrameInternal))
    {
        return false;
    }

    EvalEffectData<BrnEffects::DepthOfFieldData>(lData, mapaEffectsFrames, mu8EffectsFrameInternal);
    return true;
}

// ---- EvalBlur (DWARF BrnEffectsArbitrator.cpp:250) ----------------------------------
// Render pseudocode 1097-1101 (frame byte +3 -> sub_823FA390).
bool EffectsArbitrator::EvalBlur(BrnEffects::BlurData& lData) const
{
    if (!EvalUseEffect<BrnEffects::BlurData>(mapaEffectsFrames, mu8EffectsFrameInternal))
    {
        return false;
    }

    EvalEffectData<BrnEffects::BlurData>(lData, mapaEffectsFrames, mu8EffectsFrameInternal);
    return true;
}

// ---- EvalTint2d (DWARF BrnEffectsArbitrator.cpp:350) --------------------------------
// Render pseudocode 1242-1243: `if (v296 && *(496 * *(v12+1164) + *(v12+1152) + 5))
// sub_823FA630(&v302, (v12 + 1152), *(v12 + 1164));` -- the `v296 &&` is the CALLER's
// "effects allowed" gate (it guards all six effects), not part of this function.
bool EffectsArbitrator::EvalTint2d(BrnEffects::TintData2d& lData) const
{
    if (!EvalUseEffect<BrnEffects::TintData2d>(mapaEffectsFrames, mu8EffectsFrameInternal))
    {
        return false;
    }

    EvalEffectData<BrnEffects::TintData2d>(lData, mapaEffectsFrames, mu8EffectsFrameInternal);
    return true;
}

// ---- EvalTint @0x823FD570 (DWARF BrnEffectsArbitrator.cpp:275) ----------------------
// The odd one out: colour cubes cannot be blended, so instead of folding the layers into one
// value it EMITS one (cube, weight) pair per contributing slot and renormalises the earlier
// entries as each layer is added. Note it walks kau8ColourCubesPerEffectsLayer (byte_8203E114,
// `lbzx r11, r29, r27` @0x823FD5E0 / 0x823FD63C), NOT the slot table -- layer 2 has 2 slots
// but only 1 colour cube.
//
// ⚠ OUTPUT SIZE. With the attested tables the caller must supply arrays of at least
//   kau8ColourCubesPerEffectsLayer[WORLD] + [FXEVENTS] == 4 + 1 == 5 entries.
bool EffectsArbitrator::EvalTint(rw::graphics::postfx::ColourCube** lppColourCubes, f32* lpfWeights) const
{
    // Gate on the layer-0 internal frame's tint flag (@0x823FD594-0x823FD5AC).
    if (!EvalUseEffect<BrnEffects::TintData>(mapaEffectsFrames, mu8EffectsFrameInternal))
    {
        return false;
    }

    u8 luColourCube = 0;   // running output index across layers WORLD..FXEVENTS
    for (u32 lu8Layer = E_EFLAYER_WORLD; lu8Layer < E_EFLAYER_CNT; ++lu8Layer)
    {
        f32      lfLayerWeight = KF_ZERO_LAYER_WEIGHT;
        const s8 li8LayerBase  = static_cast<s8>(luColourCube);   // first output index this layer touches

        const u32 luColourCubes = kau8ColourCubesPerEffectsLayer[lu8Layer];
        for (u32 luSlot = 0; luSlot < luColourCubes; ++luSlot)
        {
            const BrnEffectsFrame& lSlot =
                mapaEffectsFrames[lu8Layer][luSlot][mu8EffectsFrameInternal];

            // X360 stores the cube as a 32-bit pointer in the frame; the tree's TintData
            // still models it as the raw word (see the SUSPECT note in BrnEffectsData.h).
            lppColourCubes[luColourCube] =
                reinterpret_cast<rw::graphics::postfx::ColourCube*>(
                    static_cast<uintptr_t>(EffectData<BrnEffects::TintData>(lSlot).muColourCube));
            lpfWeights[luColourCube] = EffectWeight<BrnEffects::TintData>(lSlot);
            lfLayerWeight += EffectWeight<BrnEffects::TintData>(lSlot);
            ++luColourCube;
        }

        if (lfLayerWeight > KF_MAX_LAYER_WEIGHT_SUM)
        {
            // ../Graphics/BrnEffectsArbitrator.cpp:320 on the console (`li r5, 0x140`).
            char lacError[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsCore::SPrintf(lacError, CgsDev::Assert::KI_MESSAGEBUFFERSIZE,
                             kacInvalidLayerWeights, lfLayerWeight, lu8Layer);
            CGS_ASSERT(false, lacError);
        }

        // Re-weight the colour cubes accumulated by the EARLIER layers by the residual
        // (1.0 - this layer's weight) so the running total stays normalised. The X360 forms
        // the start index as a SIGN-EXTENDED byte (`addi r11, r25, -1` / `extsb r11, r11`
        // @0x823FD690-0x823FD694), so an empty first layer leaves the loop immediately.
        const f32 lfResidual = KF_MAX_LAYER_WEIGHT_SUM - lfLayerWeight;
        for (s32 liIndex = li8LayerBase - 1; liIndex >= 0; --liIndex)
        {
            lpfWeights[liIndex] *= lfResidual;
        }
    }

    return true;
}
}
