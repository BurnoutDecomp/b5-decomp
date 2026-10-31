#include "GameShared/Jobs/TintBlend/TintBlend.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxtint.h"
#include <vectorintrinsics.h>
#include <cstddef>

namespace rw { namespace graphics { namespace postfx {
namespace {

// Native zero/ones, four byte-gather masks, three word-pack masks,
// and four RGB expansion masks from ARTIST 821431A0..8214326F.
CGS_ALIGN(16) const u8 saTintMasks[13][16] =
{
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
    { 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x13 },
    { 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x17 },
    { 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x1A, 0x00, 0x00, 0x00, 0x1B },
    { 0x00, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0x1D, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0x1F },
    { 0x00, 0x13, 0x17, 0x1B, 0x04, 0x1F, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F },
    { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x13, 0x17, 0x08, 0x1B, 0x1F, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F },
    { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x13, 0x0C, 0x17, 0x1B, 0x1F },
    { 0x00, 0x10, 0x11, 0x12, 0x07, 0x13, 0x14, 0x15, 0x0A, 0x16, 0x17, 0x18, 0x0F, 0x19, 0x1A, 0x1B },
    { 0x00, 0x0C, 0x0D, 0x0E, 0x07, 0x0F, 0x10, 0x11, 0x0A, 0x12, 0x13, 0x14, 0x0F, 0x15, 0x16, 0x17 },
    { 0x00, 0x08, 0x09, 0x0A, 0x07, 0x0B, 0x0C, 0x0D, 0x0A, 0x0E, 0x0F, 0x10, 0x0F, 0x11, 0x12, 0x13 },
    { 0x00, 0x14, 0x15, 0x16, 0x07, 0x17, 0x18, 0x19, 0x0A, 0x1A, 0x1B, 0x1C, 0x0F, 0x1D, 0x1E, 0x1F },
};

CGS_STATIC_ASSERT(sizeof(TintBlendParameters) == 0x80, "ARTIST tint parameter block");
CGS_STATIC_ASSERT(offsetof(TintBlendParameters, numSources) == 4, "ARTIST tint variant");
CGS_STATIC_ASSERT(offsetof(TintBlendParameters, dst) == 0x10, "ARTIST tint destination");
CGS_STATIC_ASSERT(offsetof(TintBlendParameters, src) == 0x14, "ARTIST tint sources");
CGS_STATIC_ASSERT(offsetof(TintBlendParameters, factor) == 0x2C, "ARTIST tint factors");

inline __vector4 ReplicateWeight(f32 lfWeight)
{
    // The native wrappers load f32 and store the four replicated weight words.
    CGS_ALIGN(16) const f32 lafWeight[4] = { lfWeight, lfWeight, lfWeight, lfWeight };
    return __lvx(lafWeight, 0);
}

// Each native leaf reads sixteen packed RGB cells and writes sixteen XRGB cells.
// The twelve vectors are four consecutive channels each. Preserve the native
// fixed-point conversion, first multiply, successive fused adds, and low-byte
// extraction after unsigned WORD saturation; there is no byte clamp.
template <u32 luSources>
inline void BlendNativeBlock(u8* lpDestination, const u8* const* lapSource,
                             const __vector4* lavWeight)
{
    __vector4 lavSource[luSources][3];
    for (u32 s = 0; s < luSources; ++s)
        for (u32 v = 0; v < 3; ++v)
            lavSource[s][v] = __lvx(lapSource[s], v * 16);

    const __vector4 lvZero = __lvx(saTintMasks[0], 0);
    __vector4 lavResult[12];
    for (u32 g = 0; g < 12; ++g)
    {
        const __vector4 lvGather = __lvx(saTintMasks[2 + (g & 3)], 0);
        __vector4 lvSample = __vcfux(__vperm(lvZero, lavSource[0][g >> 2], lvGather), 8);
        __vector4 lvSum = __vmulfp(lvSample, lavWeight[0]);
        for (u32 s = 1; s < luSources; ++s)
        {
            lvSample = __vcfux(__vperm(lvZero, lavSource[s][g >> 2], lvGather), 8);
            lvSum = __vmaddfp(lvSample, lavWeight[s], lvSum);
        }
        lavResult[g] = __vctuxs(lvSum, 8);
    }

    const __vector4 lvOnes = __lvx(saTintMasks[1], 0);
    const __vector4 lvPack0 = __lvx(saTintMasks[6], 0);
    const __vector4 lvPack1 = __lvx(saTintMasks[7], 0);
    const __vector4 lvPack2 = __lvx(saTintMasks[8], 0);
    // The native leaf stores the last three groups, then the first group.
    for (u32 n = 1; n <= 4; ++n)
    {
        const u32 group = n & 3;
        __vector4 lvOutput = __vperm(lvOnes, lavResult[group * 3], lvPack0);
        lvOutput = __vperm(lvOutput, lavResult[group * 3 + 1], lvPack1);
        lvOutput = __vperm(lvOutput, lavResult[group * 3 + 2], lvPack2);
        __stvx(lvOutput, lpDestination, group * 16);
    }
}
} // anonymous namespace

// ARTIST 82AD2CF0, genuine destination/source-pointer and VMX weight arguments.
void VMXBlend(u8* lpDestination, const u8* lpSource0, const u8* lpSource1, __vector4 lvWeight0, __vector4 lvWeight1)
{
    const u8* const lapSource[2] = { lpSource0, lpSource1 };
    const __vector4 lavWeight[2] = { lvWeight0, lvWeight1 };
    BlendNativeBlock<2>(lpDestination, lapSource, lavWeight);
}

// ARTIST 82AD2F38, genuine destination/source-pointer and VMX weight arguments.
void sub_82AD2F38(u8* lpDestination, const u8* lpSource0, const u8* lpSource1, const u8* lpSource2, __vector4 lvWeight0, __vector4 lvWeight1, __vector4 lvWeight2)
{
    const u8* const lapSource[3] = { lpSource0, lpSource1, lpSource2 };
    const __vector4 lavWeight[3] = { lvWeight0, lvWeight1, lvWeight2 };
    BlendNativeBlock<3>(lpDestination, lapSource, lavWeight);
}

// ARTIST 82AD3250, genuine destination/source-pointer and VMX weight arguments.
void sub_82AD3250(u8* lpDestination, const u8* lpSource0, const u8* lpSource1, const u8* lpSource2, const u8* lpSource3, __vector4 lvWeight0, __vector4 lvWeight1, __vector4 lvWeight2, __vector4 lvWeight3)
{
    const u8* const lapSource[4] = { lpSource0, lpSource1, lpSource2, lpSource3 };
    const __vector4 lavWeight[4] = { lvWeight0, lvWeight1, lvWeight2, lvWeight3 };
    BlendNativeBlock<4>(lpDestination, lapSource, lavWeight);
}

// ARTIST 82AD3668, genuine destination/source-pointer and VMX weight arguments.
void sub_82AD3668(u8* lpDestination, const u8* lpSource0, const u8* lpSource1, const u8* lpSource2, const u8* lpSource3, const u8* lpSource4, __vector4 lvWeight0, __vector4 lvWeight1, __vector4 lvWeight2, __vector4 lvWeight3, __vector4 lvWeight4)
{
    const u8* const lapSource[5] = { lpSource0, lpSource1, lpSource2, lpSource3, lpSource4 };
    const __vector4 lavWeight[5] = { lvWeight0, lvWeight1, lvWeight2, lvWeight3, lvWeight4 };
    BlendNativeBlock<5>(lpDestination, lapSource, lavWeight);
}

// ARTIST 82AD3AF8, genuine destination/source-pointer and VMX weight arguments.
void sub_82AD3AF8(u8* lpDestination, const u8* lpSource0, const u8* lpSource1, const u8* lpSource2, const u8* lpSource3, const u8* lpSource4, const u8* lpSource5, __vector4 lvWeight0, __vector4 lvWeight1, __vector4 lvWeight2, __vector4 lvWeight3, __vector4 lvWeight4, __vector4 lvWeight5)
{
    const u8* const lapSource[6] = { lpSource0, lpSource1, lpSource2, lpSource3, lpSource4, lpSource5 };
    const __vector4 lavWeight[6] = { lvWeight0, lvWeight1, lvWeight2, lvWeight3, lvWeight4, lvWeight5 };
    BlendNativeBlock<6>(lpDestination, lapSource, lavWeight);
}

// ARTIST 82AD4078: the single-source variant ignores factor[0]. The four
// original permutations also preserve the unused X-byte contents of the middle
// eight cells; do not replace them with a synthetic opaque-alpha fill.
void SetColour(TintBlendParameters& lrParameters)
{
    const u32 luBlocks = lrParameters.size >> 4;
    const u8* lpSource = lrParameters.src[0];
    u8* lpSlice = lrParameters.dst;
    for (u32 z = 0; z < lrParameters.size; ++z)
    {
        u8* lpRow = lpSlice;
        for (u32 y = 0; y < lrParameters.size; ++y)
        {
            u8* lpDestination = lpRow;
            for (u32 b = 0; b < luBlocks; ++b)
            {
                const __vector4 lv0 = __lvx(lpSource, 0);
                const __vector4 lv1 = __lvx(lpSource, 16);
                const __vector4 lv2 = __lvx(lpSource, 32);
                const __vector4 lvOnes = __lvx(saTintMasks[1], 0);
                __stvx(__vperm(lvOnes, lv0, __lvx(saTintMasks[9], 0)), lpDestination, 0);
                __stvx(__vperm(lv0, lv1, __lvx(saTintMasks[10], 0)), lpDestination, 16);
                __stvx(__vperm(lv1, lv2, __lvx(saTintMasks[11], 0)), lpDestination, 32);
                __stvx(__vperm(lvOnes, lv2, __lvx(saTintMasks[12], 0)), lpDestination, 48);
                lpSource += 48;
                lpDestination += 64;
            }
            lpRow += lrParameters.dstStride;
        }
        lpSlice += lrParameters.dstSliceStride;
    }
}

void Blend2Cubes(TintBlendParameters& lrParameters)
{
    const u32 luBlocks = lrParameters.size >> 4;
    const u8* lpSource0 = lrParameters.src[0];
    const __vector4 lvWeight0 = ReplicateWeight(lrParameters.factor[0]);
    const u8* lpSource1 = lrParameters.src[1];
    const __vector4 lvWeight1 = ReplicateWeight(lrParameters.factor[1]);
    u8* lpSlice = lrParameters.dst;
    for (u32 z = 0; z < lrParameters.size; ++z)
    {
        u8* lpRow = lpSlice;
        for (u32 y = 0; y < lrParameters.size; ++y)
        {
            u8* lpDestination = lpRow;
            for (u32 b = 0; b < luBlocks; ++b)
            {
                VMXBlend(lpDestination, lpSource0, lpSource1, lvWeight0, lvWeight1);
                lpSource0 += 48;
                lpSource1 += 48;
                lpDestination += 64;
            }
            lpRow += lrParameters.dstStride;
        }
        lpSlice += lrParameters.dstSliceStride;
    }
}

void Blend3Cubes(TintBlendParameters& lrParameters)
{
    const u32 luBlocks = lrParameters.size >> 4;
    const u8* lpSource0 = lrParameters.src[0];
    const __vector4 lvWeight0 = ReplicateWeight(lrParameters.factor[0]);
    const u8* lpSource1 = lrParameters.src[1];
    const __vector4 lvWeight1 = ReplicateWeight(lrParameters.factor[1]);
    const u8* lpSource2 = lrParameters.src[2];
    const __vector4 lvWeight2 = ReplicateWeight(lrParameters.factor[2]);
    u8* lpSlice = lrParameters.dst;
    for (u32 z = 0; z < lrParameters.size; ++z)
    {
        u8* lpRow = lpSlice;
        for (u32 y = 0; y < lrParameters.size; ++y)
        {
            u8* lpDestination = lpRow;
            for (u32 b = 0; b < luBlocks; ++b)
            {
                sub_82AD2F38(lpDestination, lpSource0, lpSource1, lpSource2, lvWeight0, lvWeight1, lvWeight2);
                lpSource0 += 48;
                lpSource1 += 48;
                lpSource2 += 48;
                lpDestination += 64;
            }
            lpRow += lrParameters.dstStride;
        }
        lpSlice += lrParameters.dstSliceStride;
    }
}

void Blend4Cubes(TintBlendParameters& lrParameters)
{
    const u32 luBlocks = lrParameters.size >> 4;
    const u8* lpSource0 = lrParameters.src[0];
    const __vector4 lvWeight0 = ReplicateWeight(lrParameters.factor[0]);
    const u8* lpSource1 = lrParameters.src[1];
    const __vector4 lvWeight1 = ReplicateWeight(lrParameters.factor[1]);
    const u8* lpSource2 = lrParameters.src[2];
    const __vector4 lvWeight2 = ReplicateWeight(lrParameters.factor[2]);
    const u8* lpSource3 = lrParameters.src[3];
    const __vector4 lvWeight3 = ReplicateWeight(lrParameters.factor[3]);
    u8* lpSlice = lrParameters.dst;
    for (u32 z = 0; z < lrParameters.size; ++z)
    {
        u8* lpRow = lpSlice;
        for (u32 y = 0; y < lrParameters.size; ++y)
        {
            u8* lpDestination = lpRow;
            for (u32 b = 0; b < luBlocks; ++b)
            {
                sub_82AD3250(lpDestination, lpSource0, lpSource1, lpSource2, lpSource3, lvWeight0, lvWeight1, lvWeight2, lvWeight3);
                lpSource0 += 48;
                lpSource1 += 48;
                lpSource2 += 48;
                lpSource3 += 48;
                lpDestination += 64;
            }
            lpRow += lrParameters.dstStride;
        }
        lpSlice += lrParameters.dstSliceStride;
    }
}

void Blend5Cubes(TintBlendParameters& lrParameters)
{
    const u32 luBlocks = lrParameters.size >> 4;
    const u8* lpSource0 = lrParameters.src[0];
    const __vector4 lvWeight0 = ReplicateWeight(lrParameters.factor[0]);
    const u8* lpSource1 = lrParameters.src[1];
    const __vector4 lvWeight1 = ReplicateWeight(lrParameters.factor[1]);
    const u8* lpSource2 = lrParameters.src[2];
    const __vector4 lvWeight2 = ReplicateWeight(lrParameters.factor[2]);
    const u8* lpSource3 = lrParameters.src[3];
    const __vector4 lvWeight3 = ReplicateWeight(lrParameters.factor[3]);
    const u8* lpSource4 = lrParameters.src[4];
    const __vector4 lvWeight4 = ReplicateWeight(lrParameters.factor[4]);
    u8* lpSlice = lrParameters.dst;
    for (u32 z = 0; z < lrParameters.size; ++z)
    {
        u8* lpRow = lpSlice;
        for (u32 y = 0; y < lrParameters.size; ++y)
        {
            u8* lpDestination = lpRow;
            for (u32 b = 0; b < luBlocks; ++b)
            {
                sub_82AD3668(lpDestination, lpSource0, lpSource1, lpSource2, lpSource3, lpSource4, lvWeight0, lvWeight1, lvWeight2, lvWeight3, lvWeight4);
                lpSource0 += 48;
                lpSource1 += 48;
                lpSource2 += 48;
                lpSource3 += 48;
                lpSource4 += 48;
                lpDestination += 64;
            }
            lpRow += lrParameters.dstStride;
        }
        lpSlice += lrParameters.dstSliceStride;
    }
}

void Blend6Cubes(TintBlendParameters& lrParameters)
{
    const u32 luBlocks = lrParameters.size >> 4;
    const u8* lpSource0 = lrParameters.src[0];
    const __vector4 lvWeight0 = ReplicateWeight(lrParameters.factor[0]);
    const u8* lpSource1 = lrParameters.src[1];
    const __vector4 lvWeight1 = ReplicateWeight(lrParameters.factor[1]);
    const u8* lpSource2 = lrParameters.src[2];
    const __vector4 lvWeight2 = ReplicateWeight(lrParameters.factor[2]);
    const u8* lpSource3 = lrParameters.src[3];
    const __vector4 lvWeight3 = ReplicateWeight(lrParameters.factor[3]);
    const u8* lpSource4 = lrParameters.src[4];
    const __vector4 lvWeight4 = ReplicateWeight(lrParameters.factor[4]);
    const u8* lpSource5 = lrParameters.src[5];
    const __vector4 lvWeight5 = ReplicateWeight(lrParameters.factor[5]);
    u8* lpSlice = lrParameters.dst;
    for (u32 z = 0; z < lrParameters.size; ++z)
    {
        u8* lpRow = lpSlice;
        for (u32 y = 0; y < lrParameters.size; ++y)
        {
            u8* lpDestination = lpRow;
            for (u32 b = 0; b < luBlocks; ++b)
            {
                sub_82AD3AF8(lpDestination, lpSource0, lpSource1, lpSource2, lpSource3, lpSource4, lpSource5, lvWeight0, lvWeight1, lvWeight2, lvWeight3, lvWeight4, lvWeight5);
                lpSource0 += 48;
                lpSource1 += 48;
                lpSource2 += 48;
                lpSource3 += 48;
                lpSource4 += 48;
                lpSource5 += 48;
                lpDestination += 64;
            }
            lpRow += lrParameters.dstStride;
        }
        lpSlice += lrParameters.dstSliceStride;
    }
}

namespace {
typedef void (*TintBlendVariant)(TintBlendParameters&);
TintBlendVariant const gapTintBlendVariants[7] =
{
    0, &SetColour, &Blend2Cubes, &Blend3Cubes, &Blend4Cubes, &Blend5Cubes, &Blend6Cubes
};
}

// ARTIST 82AD4860 has no range/null gate or reporting side effect.
void TintBlend(TintBlendParameters& lrParameters)
{
    gapTintBlendVariants[lrParameters.numSources](lrParameters);
}
}}} // rw::graphics::postfx

// ARTIST 82AD2CE8: use the second EA::Jobs word as the native descriptor.
void TintBlendEntry(EA::Jobs::Param, EA::Jobs::Param lParam1,
                    EA::Jobs::Param, EA::Jobs::Param)
{
    rw::graphics::postfx::TintBlend(
        *static_cast<rw::graphics::postfx::TintBlendParameters*>(lParam1.mpValue));
}
