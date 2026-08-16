#include "types.hpp"

#include <cstdio>   // std::snprintf (the one-shot diagnostics)

#include "GameShared/Jobs/TintBlend/TintBlend.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxtint.h"  // TintBlendParameters
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                    // the one-shot report

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   TintBlendEntry                          @ 0x82AD2CE8
//   rw::graphics::postfx::TintBlend         @ 0x82AD4860   (the variant dispatch)
//   rw::graphics::postfx::SetColour         @ 0x82AD4078   (1 source)
//   rw::graphics::postfx::Blend2Cubes       @ 0x82AD4170   (+ VMXBlend, the 2-source inner kernel)
//   rw::graphics::postfx::Blend3Cubes       @ 0x82AD4280   (+ sub_82AD2F38)
//   rw::graphics::postfx::Blend4Cubes       @ 0x82AD43D0   (+ sub_82AD3250)
//   rw::graphics::postfx::Blend5Cubes       @ 0x82AD4528   (+ sub_82AD3668)
//   rw::graphics::postfx::Blend6Cubes       @ 0x82AD46B0   (+ sub_82AD3AF8)
// (all seven bodies + the variant table dword_82F7238C are dumped, pseudocode AND asm, in
//  scratch/postfx_step10_finish/DATA_DUMP.md:93-1278 -- the BLOCKED note that used to stand in this
//  file named exactly those two things as the missing items, and both are now in hand.)
//
// WHAT THE JOB IS. BrnPostFx::BeginTintBlend locks the current Tint's colour-lookup VOLUME, fills a
// TintBlendParameters with the destination surface geometry plus up to five source colour cubes and
// their weights, and schedules this job; BrnPostFx::Render drains it (Job::WaitOn) and unlocks the
// surface through Tint::EndBlendJob before the composite samples the map at s3.

namespace rw
{
namespace graphics
{
namespace postfx
{
namespace
{
    // ============================================================================================
    // THE SHAPE ALL SEVEN KERNELS SHARE, read off the asm (DATA_DUMP.md). Taking SetColour
    // @0x82AD4078 as the reference and Blend2Cubes @0x82AD4170 as the corroboration:
    //
    //   size           = params[0]      `lwz r11, 0(r3)`      -- the DESTINATION edge
    //   dstStride      = params[2]      `lwz r11, 8(r3)`      added once per MIDDLE iteration
    //   dstSliceStride = params[3]      `lwz r8, 0xC(r3)`     added once per OUTER iteration
    //   dst            = params[4]      `lwz r29, 0x10(r3)`
    //   src[k]         = params[5 + k]  `lwz r9, 0x14(r3)` (+0x18, +0x1C, ... for the blends)
    //   factor[k]      = params[11 + k] `lfs f0, 0x2C(r27)` (+0x30, +0x34, ... )
    //
    //   for (z = 0; z < size; ++z)                       // r28/r23/r22/r21/r20/r19 <-> `cmplw r28, *r3`
    //       for (y = 0; y < size; ++y)                   // r30/r25/r24/r23/r22/r21
    //           for (block = 0; block < (size >> 4); ++block)   // `srwi r27, r11, 4`
    //               ... 16 texels ...                   // dst += 0x40, every src += 0x30
    //
    // THREE FACTS THAT FALL OUT OF THAT AND ARE LOAD-BEARING:
    //  1. THE DESTINATION IS 4 BYTES PER TEXEL AND EACH SOURCE IS 3 (`addi r11,r11,0x40` against
    //     `addi r9,r9,0x30` for the same 16 texels). The cubes are packed RGB; the lookup volume is
    //     a 32-bit surface.
    //  2. THE SOURCE CURSORS ARE NEVER RESET. They advance continuously across the whole cube while
    //     the destination walks rows/slices through its own strides -- i.e. the source is a LINEAR
    //     run in the destination's own scan order, and the kernel imposes no layout of its own.
    //  3. A SIZE THAT IS NOT A MULTIPLE OF 16 DROPS ITS REMAINDER (`size >> 4`, no tail loop). The
    //     shipped edge is 32, so two blocks per row. Reproduced by omission, not "fixed".
    //
    // THE ARITHMETIC, from the VMX in the inner kernels (sub_82AD2F38 is the 3-source one, dumped
    // in full): each byte is widened with `vcfux vD, vS, 8` (unsigned int -> float, /2^8), scaled by
    // the splatted weight (`vmulfp128 v, v, v1` for source 0, then `vmaddfp v, srcK, v, vK` for each
    // further source), and narrowed back with `vctuxs vD, vS, 8` (float -> unsigned fixed point,
    // *2^8, round toward zero). Net per channel:
    //       dst = trunc( SUM over k of  src_k * factor_k )
    //
    // ONE DELIBERATE DIVERGENCE, DECLARED: the console's `vctuxs` saturates at 32 bits and the byte
    // is then EXTRACTED by a vperm, so a channel sum above 1.0 WRAPS on the console (1.5 -> 0x180 ->
    // 0x80). This reconstruction CLAMPS to 255 instead. The weights are normalised blend weights
    // (EffectsArbitrator::EvalTint's layer weights) so no shipped path reaches the difference, and a
    // wrapped highlight would be a black hole in the middle of a bright grade -- reproducing that
    // faithfully would trade a correct picture for a bug nobody can observe. Recorded here rather
    // than smuggled.
    // ============================================================================================
    const u32 KU_SOURCE_BYTES_PER_TEXEL = 3u;   // `addi r9, r9, 0x30` per 16 texels
    const u32 KU_DEST_BYTES_PER_TEXEL   = 4u;   // `addi r11, r11, 0x40` per 16 texels
    const u32 KU_TEXELS_PER_BLOCK       = 16u;  // `srwi r27, r11, 4`

    // ============================================================================================
    // [FLAG PC-platform leaf] THE DESTINATION CHANNEL ORDER.
    //
    // The console writes its four destination bytes through vperm control vectors held at
    // unk_82143260-0xB0.. (SetColour) and unk_82143220-0x80.. (the blend kernels); those tables
    // encode the XENOS surface's byte order for GPU format 0x18280186, and they are meaningless on
    // D3D9. The PC lookup volume is created D3DFMT_A8R8G8B8 (rwgpfxtint.cpp), whose in-memory byte
    // order on this little-endian target is B, G, R, A -- so the mapping below is fixed by the
    // FORMAT THIS TREE CHOSE, not guessed from the console.
    //
    // WHICH SOURCE BYTE IS WHICH CHANNEL is not a guess either. It is measured off the shipped
    // IDENTITY cube, "rgb_colourcube.tga" (POSTFX/COLOURCUBEDICTIONARY.BIN resource 0x1331EC9D):
    // decoding every one of its 32,768 cells as (cell[0], cell[1], cell[2]) * 31/255 yields a
    // PERFECT BIJECTION onto the 32^3 coordinate grid, and the composite's own tint fetch is
    // `tex3D(Sampler3dTint, lComposite).rgb` (tools/assets/shaders/brn_postfx_composite.fx:334) with
    // lComposite.x == red -> u == the volume's WIDTH axis. Cell byte 0 is therefore RED, byte 1
    // GREEN, byte 2 BLUE. Independently corroborated by the shipped "Black_White.psd" cube
    // (0x4873FB1D), which after the untile below is pure grey and responds IDENTICALLY to all three
    // axes (0 -> 0, 8 -> 27, 16 -> 92, 31 -> 126, (31,31,31) -> 255) -- a symmetry that any wrong
    // axis or channel assignment destroys.
    //
    // The alpha byte: on the console it comes from a constant vector at unk_821431B0 that is NOT in
    // the dump set (see the CONDUCTOR DUMP REQUEST in this wave's report). It is unobservable --
    // the composite samples `.rgb` only -- so it is written opaque here.
    // ============================================================================================
    const u32 KU_DEST_BYTE_B = 0u;
    const u32 KU_DEST_BYTE_G = 1u;
    const u32 KU_DEST_BYTE_R = 2u;
    const u32 KU_DEST_BYTE_A = 3u;
    const u8  KU_DEST_ALPHA  = 0xFFu;

    // Narrow one accumulated channel back to a destination byte (the `vctuxs ..., 8` step).
    inline u8 NarrowChannel(f32 lfValue)
    {
        if (lfValue <= 0.0f)   { return 0u; }
        if (lfValue >= 255.0f) { return 255u; }
        return static_cast<u8>(lfValue);   // C++ float->integral truncates, as vctuxs does
    }

    // Write one destination texel from an already-accumulated RGB triple.
    inline void StoreTexel(u8* lpDest, f32 lfR, f32 lfG, f32 lfB)
    {
        lpDest[KU_DEST_BYTE_R] = NarrowChannel(lfR);
        lpDest[KU_DEST_BYTE_G] = NarrowChannel(lfG);
        lpDest[KU_DEST_BYTE_B] = NarrowChannel(lfB);
        lpDest[KU_DEST_BYTE_A] = KU_DEST_ALPHA;
    }

    // The shared walk. `luSourceCount` sources are mixed by their weights; the six console kernels
    // are the compiler's/author's per-count specialisations of exactly this loop nest (AGENTS.md:
    // re-roll the unrolled form, keep the dispatch that selects it).
    void BlendCubes(TintBlendParameters& lrParameters, u32 luSourceCount)
    {
        const u32 luSize   = lrParameters.size;
        const u32 luBlocks = luSize >> 4;   // `srwi r27, r11, 4` -- the tail is dropped
        if (luSize == 0u || luBlocks == 0u || lrParameters.dst == 0)
        {
            return;
        }

        const u8* lapSource[6];
        f32       lafFactor[6];
        for (u32 luSource = 0u; luSource < luSourceCount; ++luSource)
        {
            lapSource[luSource] = lrParameters.src[luSource];
            lafFactor[luSource] = lrParameters.factor[luSource];
            if (lapSource[luSource] == 0)
            {
                return;   // [FLAG PC bring-up] a cube that never resolved; see TintBlend below
            }
        }

        u8* lpDestSlice = lrParameters.dst;
        for (u32 luZ = 0u; luZ < luSize; ++luZ)
        {
            u8* lpDestRow = lpDestSlice;
            for (u32 luY = 0u; luY < luSize; ++luY)
            {
                u8* lpDestTexel = lpDestRow;
                for (u32 luTexel = 0u; luTexel < luBlocks * KU_TEXELS_PER_BLOCK; ++luTexel)
                {
                    f32 lafChannel[3] = { 0.0f, 0.0f, 0.0f };
                    for (u32 luSource = 0u; luSource < luSourceCount; ++luSource)
                    {
                        const u8* const lpCell = lapSource[luSource];
                        const f32 lfWeight = lafFactor[luSource];
                        lafChannel[0] += static_cast<f32>(lpCell[0]) * lfWeight;
                        lafChannel[1] += static_cast<f32>(lpCell[1]) * lfWeight;
                        lafChannel[2] += static_cast<f32>(lpCell[2]) * lfWeight;
                        lapSource[luSource] = lpCell + KU_SOURCE_BYTES_PER_TEXEL;
                    }
                    StoreTexel(lpDestTexel, lafChannel[0], lafChannel[1], lafChannel[2]);
                    lpDestTexel += KU_DEST_BYTES_PER_TEXEL;
                }
                lpDestRow += lrParameters.dstStride;
            }
            lpDestSlice += lrParameters.dstSliceStride;
        }
    }
}

    // ============================================================================================
    // SetColour @0x82AD4078 -- the ONE-SOURCE variant, and it is NOT "blend with weight 1".
    //
    // The body has no `lfs` at all: it never touches params+0x2C, so factor[0] IS IGNORED. All it
    // does is expand the 3-byte cells into 4-byte destination texels through the vperm table at
    // unk_82143260 (`lvx128 v0/v12/v11` = 48 source bytes -> four `stvx128` = 64 destination bytes,
    // per 16 texels). Reproduced exactly: a straight copy with no scaling.
    // ============================================================================================
    void SetColour(TintBlendParameters& lrParameters)
    {
        const u32 luSize   = lrParameters.size;
        const u32 luBlocks = luSize >> 4;
        const u8* lpSource = lrParameters.src[0];
        if (luSize == 0u || luBlocks == 0u || lrParameters.dst == 0 || lpSource == 0)
        {
            return;
        }

        u8* lpDestSlice = lrParameters.dst;
        for (u32 luZ = 0u; luZ < luSize; ++luZ)
        {
            u8* lpDestRow = lpDestSlice;
            for (u32 luY = 0u; luY < luSize; ++luY)
            {
                u8* lpDestTexel = lpDestRow;
                for (u32 luTexel = 0u; luTexel < luBlocks * KU_TEXELS_PER_BLOCK; ++luTexel)
                {
                    lpDestTexel[KU_DEST_BYTE_R] = lpSource[0];
                    lpDestTexel[KU_DEST_BYTE_G] = lpSource[1];
                    lpDestTexel[KU_DEST_BYTE_B] = lpSource[2];
                    lpDestTexel[KU_DEST_BYTE_A] = KU_DEST_ALPHA;
                    lpSource    += KU_SOURCE_BYTES_PER_TEXEL;
                    lpDestTexel += KU_DEST_BYTES_PER_TEXEL;
                }
                lpDestRow += lrParameters.dstStride;
            }
            lpDestSlice += lrParameters.dstSliceStride;
        }
    }

    // Blend2Cubes @0x82AD4170 / Blend3Cubes @0x82AD4280 / Blend4Cubes @0x82AD43D0 /
    // Blend5Cubes @0x82AD4528 / Blend6Cubes @0x82AD46B0 -- the same nest with one more source and
    // one more splatted weight each (`lfs f0,0x2C` .. `lfs f9,0x40`; `lwz r29,0x14` .. `lwz r24,0x28`).
    void Blend2Cubes(TintBlendParameters& lrParameters) { BlendCubes(lrParameters, 2u); }
    void Blend3Cubes(TintBlendParameters& lrParameters) { BlendCubes(lrParameters, 3u); }
    void Blend4Cubes(TintBlendParameters& lrParameters) { BlendCubes(lrParameters, 4u); }
    void Blend5Cubes(TintBlendParameters& lrParameters) { BlendCubes(lrParameters, 5u); }
    void Blend6Cubes(TintBlendParameters& lrParameters) { BlendCubes(lrParameters, 6u); }

    // ============================================================================================
    // TintBlend @0x82AD4860 -- eight instructions, and all eight are one indirect jump:
    //
    //     lwz   r10, 4(r3)                 ; numSources  (TintBlendParameters +0x04)
    //     lis   r11, dword_82F7238C@ha
    //     addi  r11, r11, dword_82F7238C@l
    //     slwi  r10, r10, 2                ; * sizeof(void*)
    //     lwzx  r11, r10, r11
    //     mtctr r11
    //     bctr                             ; TAIL call, r3 still the parameter block
    //
    // i.e. `gapTintBlendVariants[lrParameters.numSources](lrParameters)`. The table is DUMPED
    // (DATA_DUMP.md:93-96, 245, 407, 601, 803, 1029, 1279):
    //     [0] 0x00000000  (NULL)
    //     [1] 0x82AD4078  SetColour
    //     [2] 0x82AD4170  Blend2Cubes
    //     [3] 0x82AD4280  Blend3Cubes
    //     [4] 0x82AD43D0  Blend4Cubes
    //     [5] 0x82AD4528  Blend5Cubes
    //     [6] 0x82AD46B0  Blend6Cubes
    //     [7] 0x82143274  aBadAllocation_10 -- a STRING, i.e. past the end of the table.
    // Seven entries, and slot 0 really is a null: the console would `bctr` to address 0 on a
    // zero-source blend.
    //
    // [FLAG PC bring-up] THE NULL SLOT IS TESTED HERE, and only that. BrnPostFx::BeginTintBlend
    // schedules this job whenever the tint bit is set, without testing m_numCubesToBlend first, so a
    // frame in which the arbitrator produced no cube reaches the table with index 0. Jumping to
    // address 0 is not behaviour worth reproducing; the report names the condition instead. The
    // upper bound is tested for the same reason -- the console's own assert
    // ("m_numCubesToBlend <= 5", BrnPostFx.cpp:266) fires one call earlier and then indexes anyway.
    // ============================================================================================
    typedef void (*TintBlendVariant)(TintBlendParameters&);

    TintBlendVariant const gapTintBlendVariants[7] =
    {
        0,              // [0] X360 0x00000000
        &SetColour,     // [1] X360 0x82AD4078
        &Blend2Cubes,   // [2] X360 0x82AD4170
        &Blend3Cubes,   // [3] X360 0x82AD4280
        &Blend4Cubes,   // [4] X360 0x82AD43D0
        &Blend5Cubes,   // [5] X360 0x82AD4528
        &Blend6Cubes,   // [6] X360 0x82AD46B0
    };

    void TintBlend(TintBlendParameters& lrParameters)
    {
        const u32 luSources = lrParameters.numSources;
        if (luSources == 0u || luSources >= 7u || gapTintBlendVariants[luSources] == 0)
        {
            static bool sbReportedBadSourceCount = false;
            if (!sbReportedBadSourceCount)
            {
                sbReportedBadSourceCount = true;
                char lacMsg[160];
                std::snprintf(lacMsg, sizeof(lacMsg),
                              "[postfx-tint] blend skipped: numSources=%u has no variant"
                              " (X360 dword_82F7238C holds 1..6) [FLAG PC bring-up]\n",
                              static_cast<unsigned>(luSources));
                CgsDev::Log::WriteToLog(lacMsg);
            }
            return;
        }

        gapTintBlendVariants[luSources](lrParameters);

        // [DIAG one-shot] what the blend actually saw. The boot proof for this wave.
        {
            static bool sbReported = false;
            if (!sbReported)
            {
                sbReported = true;
                char lacMsg[224];
                std::snprintf(lacMsg, sizeof(lacMsg),
                              "[postfx-tint] blend sources=%u size=%u dstStride=%u dstSlice=%u"
                              " w=[%.3f %.3f %.3f %.3f %.3f]\n",
                              static_cast<unsigned>(luSources),
                              static_cast<unsigned>(lrParameters.size),
                              static_cast<unsigned>(lrParameters.dstStride),
                              static_cast<unsigned>(lrParameters.dstSliceStride),
                              (double)lrParameters.factor[0], (double)lrParameters.factor[1],
                              (double)lrParameters.factor[2], (double)lrParameters.factor[3],
                              (double)lrParameters.factor[4]);
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }
    }
}
}
}

// ================================================================================================
// TintBlendEntry @0x82AD2CE8
//
//     mr r3, r4
//     b  rw__graphics__postfx__TintBlend
//
// A two-instruction thunk: take the job's SECOND parameter word and tail-call the blend with it.
// Param 1 is where EA::Jobs::Job::SetData parks the data pointer (job.cpp:137), and
// BrnPostFx::BeginTintBlend @0x823F8464 calls SetData with the TintBlendParameters block
// Tint::BeginBlendJob just returned -- so the cast below is naming what the console already relies
// on, not reinterpreting an unknown word. The other three Params are untouched (r5/r6/r7 are never
// read), which is reproduced by leaving them unnamed.
// ================================================================================================
void TintBlendEntry(EA::Jobs::Param /*lParam0*/, EA::Jobs::Param lParam1,
                    EA::Jobs::Param /*lParam2*/, EA::Jobs::Param /*lParam3*/)
{
    rw::graphics::postfx::TintBlend(
        *static_cast<rw::graphics::postfx::TintBlendParameters*>(lParam1.mpValue));
}
