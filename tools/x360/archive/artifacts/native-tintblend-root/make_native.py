from pathlib import Path
import hashlib, json, subprocess, sys

ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
HERE = Path(__file__).resolve().parent
PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(PARENT / 'tools/re'))
import x360rd

assert x360rd.u32(0x82815570) == 0x7C8A2378
assert x360rd.u32(0x82F264A8) == 4
assert subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip() == '9acbf432092522daf57bfc087b42c9a0f9375f46'
assert not subprocess.check_output(['git', '-C', str(ROOT), 'diff', '--name-only']).strip()
source = ROOT / 'src/GameShared/Jobs/TintBlend/TintBlend.cpp'
old = source.read_bytes()
with (HERE / 'TintBlend-before.cpp').open('xb') as f:
    f.write(old)
constants = x360rd.rd(0x821431A0, 13 * 16)
lines = ['#include "GameShared/Jobs/TintBlend/TintBlend.h"',
         '#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxtint.h"',
         '#include <vectorintrinsics.h>', '#include <cstddef>', '',
         'namespace rw { namespace graphics { namespace postfx {', 'namespace {', '',
         '// Native zero/ones, four byte-gather masks, three word-pack masks,',
         '// and four RGB expansion masks from ARTIST 821431A0..8214326F.',
         'CGS_ALIGN(16) const u8 saTintMasks[13][16] =', '{']
for i in range(13):
    lines.append('    { ' + ', '.join('0x%02X' % b for b in constants[16*i:16*i+16]) + ' },')
lines += ['};', '',
'''CGS_STATIC_ASSERT(sizeof(TintBlendParameters) == 0x80, "ARTIST tint parameter block");
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
''']
names = {2: 'VMXBlend', 3: 'sub_82AD2F38', 4: 'sub_82AD3250', 5: 'sub_82AD3668', 6: 'sub_82AD3AF8'}
for n, name in names.items():
    args = ['u8* lpDestination'] + ['const u8* lpSource%d' % i for i in range(n)] + ['__vector4 lvWeight%d' % i for i in range(n)]
    lines += ['// ARTIST %s, genuine destination/source-pointer and VMX weight arguments.' % ('82AD2CF0' if n == 2 else name[4:]),
              'void %s(%s)' % (name, ', '.join(args)), '{',
              '    const u8* const lapSource[%d] = { %s };' % (n, ', '.join('lpSource%d' % i for i in range(n))),
              '    const __vector4 lavWeight[%d] = { %s };' % (n, ', '.join('lvWeight%d' % i for i in range(n))),
              '    BlendNativeBlock<%d>(lpDestination, lapSource, lavWeight);' % n, '}', '']
lines += ['''// ARTIST 82AD4078: the single-source variant ignores factor[0]. The four
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
''']
for n, name in names.items():
    lines += ['void Blend%dCubes(TintBlendParameters& lrParameters)' % n, '{',
              '    const u32 luBlocks = lrParameters.size >> 4;']
    for i in range(n):
        lines += ['    const u8* lpSource%d = lrParameters.src[%d];' % (i, i),
                  '    const __vector4 lvWeight%d = ReplicateWeight(lrParameters.factor[%d]);' % (i, i)]
    lines += ['    u8* lpSlice = lrParameters.dst;', '    for (u32 z = 0; z < lrParameters.size; ++z)', '    {',
              '        u8* lpRow = lpSlice;', '        for (u32 y = 0; y < lrParameters.size; ++y)', '        {',
              '            u8* lpDestination = lpRow;', '            for (u32 b = 0; b < luBlocks; ++b)', '            {',
              '                %s(%s);' % (name, ', '.join(['lpDestination'] + ['lpSource%d' % i for i in range(n)] + ['lvWeight%d' % i for i in range(n)]))]
    lines += ['                lpSource%d += 48;' % i for i in range(n)]
    lines += ['                lpDestination += 64;', '            }', '            lpRow += lrParameters.dstStride;',
              '        }', '        lpSlice += lrParameters.dstSliceStride;', '    }', '}', '']
lines += ['''namespace {
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
''']
new = ('\n'.join(lines) + '\n').replace('\n', '\r\n').encode('utf-8')
with (HERE / 'before-edit.json').open('x', encoding='utf-8') as f:
    json.dump({'base': '9acbf432092522daf57bfc087b42c9a0f9375f46',
               'source': source.relative_to(ROOT).as_posix(),
               'before_sha256': hashlib.sha256(old).hexdigest(),
               'after_sha256': hashlib.sha256(new).hexdigest(),
               'original_masks': {'address': '0x821431A0', 'bytes': constants.hex(),
                                  'sha256': hashlib.sha256(constants).hexdigest()}}, f, indent=2)
source.write_bytes(new)
print('Native TintBlend source written:', len(new), 'bytes; original208-byte mask table bound.')
