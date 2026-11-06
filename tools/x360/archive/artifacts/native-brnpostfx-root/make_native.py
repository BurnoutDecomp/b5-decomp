from pathlib import Path
import hashlib, json, re

HERE = Path(__file__).resolve().parent
ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
meta = json.loads((HERE / 'before-edit.json').read_text())
for path, digest in meta['sources'].items():
    assert hashlib.sha256((ROOT / path).read_bytes()).hexdigest() == digest, path

header = '''#ifndef RW_GPFX_COLOUR_CUBE_H
#define RW_GPFX_COLOUR_CUBE_H

#include "types.hpp"
#include "rw/rwcore_structs.h"
#include <cstddef>

namespace rw { namespace graphics { namespace postfx {
    // ARTIST 823F83F0..83FC reads the fixed-up pixel pointer at +0x04.
    // DecFIGS rwgpfxcolourcube.h supplies the two-member declaration.
    struct ColourCube
    {
        struct Parameters
        {
            u32 size;
        };

        // ARTIST 82402C48 reserves a 16-byte resource header before the RGB
        // cells. That allocation extent is distinct from sizeof this type.
        static const u32 KU_HEADER_BYTES = 16u;
        static const u32 KU_BYTES_PER_CELL = 3u;

        static rw::BaseResourceDescriptors<5>* GetResourceDescriptor(
            rw::BaseResourceDescriptors<5>* lpResult, const Parameters* lpParameters);

        u32 GetSize() const { return m_size; }
        u8* GetPixels() const { return m_pixels; }

        u32 m_size;
        u8* m_pixels;
    };

    CGS_STATIC_ASSERT(sizeof(ColourCube) == 8, "ARTIST colour cube declaration");
    CGS_STATIC_ASSERT(offsetof(ColourCube, m_pixels) == 4, "ARTIST fixed-up cube pixels");
} } }

#endif // RW_GPFX_COLOUR_CUBE_H
'''
cpp = (HERE / 'BrnPostFx.cpp.before').read_text()
# Retire the historical PC bring-up commentary along with its unused seam.
cpp = '\n'.join(line for line in cpp.splitlines() if not line.lstrip().startswith('//')) + '\n'
cpp = re.sub(r'\n{3,}', '\n\n', cpp)
cpp = re.sub(r'#include <cstdio>[^\n]*\n', '', cpp)
cpp = re.sub(r'namespace renderengine \{ extern u32 guDiagComposites;[^\n]*\n', '', cpp)
cpp = re.sub(r'#include "GameShared/GameClasses/Development/Log/CgsLog.h"[^\n]*\n', '', cpp)
cpp = cpp.replace('namespace\n{\n    bool sbPostFxConstructed = false;\n}\n', '')
cpp = cpp.replace('#include <new>', '#include <cstddef>\n#include <new>', 1)
cpp = cpp.replace('#include "GameShared/Jobs/TintBlend/TintBlend.h"',
    '#include "GameShared/GameClasses/System/CgsHardwareInit.h"\n'
    '#include "SDKs/EATech/eajobs/job_scheduler.h"\n'
    '#include "GameShared/Jobs/TintBlend/TintBlend.h"', 1)
assert cpp.count('->mpColourTextureState') == 1
cpp = cpp.replace('->mpColourTextureState', '->mDepthTarget.mpTextureState')
assert '(lpColourCube != 0) ? lpColourCube->GetPixels() : 0' in cpp
cpp = cpp.replace('(lpColourCube != 0) ? lpColourCube->GetPixels() : 0',
                  'm_colourCubes[luSource]->GetPixels()')
assert 'lHalfTexelOffset.z = KF_POST_FX_ZERO;' in cpp
cpp = cpp.replace('lHalfTexelOffset.z = KF_POST_FX_ZERO;', 'lHalfTexelOffset.z = lHalfTexelOffset.x;')
cpp = cpp.replace('lHalfTexelOffset.w = KF_POST_FX_ZERO;', 'lHalfTexelOffset.w = lHalfTexelOffset.x;')
old_sync = '''    {
        EA::Jobs::Param laJobParams[4];
        laJobParams[1] = EA::Jobs::Param(static_cast<void*>(&lrBlendParameters));
        TintBlendEntry(laJobParams[0], laJobParams[1], laJobParams[2], laJobParams[3]);
    }'''
assert old_sync in cpp
cpp = cpp.replace(old_sync, '    CgsSystem::JobManager()->AddJobs(&m_blendJob, 1);')
cpp = cpp.replace('void BrnPostFx::BeginTintBlend()\n{', '''// ARTIST 823F8380: fill the fixed-up source pointers, then submit the native job.
void BrnPostFx::BeginTintBlend()
{
    CGS_STATIC_ASSERT(offsetof(BrnPostFx, mnActiveLayerCount) == 0x380, "ARTIST postfx active count");
    CGS_STATIC_ASSERT(offsetof(BrnPostFx, m_pfxTint) == 0x5C0, "ARTIST postfx tints");
    CGS_STATIC_ASSERT(offsetof(BrnPostFx, m_colourCubes) == 0x5CC, "ARTIST postfx cubes");
    CGS_STATIC_ASSERT(offsetof(BrnPostFx, m_tintFactors) == 0x5E0, "ARTIST postfx factors");
    CGS_STATIC_ASSERT(offsetof(BrnPostFx, m_processTint) == 0x5F4, "ARTIST postfx pending byte");
    CGS_STATIC_ASSERT(offsetof(BrnPostFx, m_blendJob) == 0x600, "ARTIST postfx blend job");
    CGS_STATIC_ASSERT(offsetof(rw::graphics::postfx::RenderTarget, mDepthTarget) +
        offsetof(rw::graphics::postfx::Target, mpTextureState) == 0x8C, "ARTIST postfx depth state");''')
anchors = {'BrnPostFx::BrnPostFx()': '82407FC8', 'void BrnPostFx::Construct(': '82409F80',
           'void BrnPostFx::Destruct()': '82408138', 'void BrnPostFx::PrepareDownSampleBuffers(': '82408E88',
           'void BrnPostFx::Render(': '8240A468'}
for token, address in anchors.items():
    assert cpp.count(token) == 1
    cpp = cpp.replace(token, '// ARTIST ' + address + '.\n' + token, 1)
cpp = re.sub(r'\n{3,}', '\n\n', cpp)
(ROOT / 'src/SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxcolourcube.h').write_bytes(header.replace('\n', '\r\n').encode())
(ROOT / 'src/GameSource/Graphics/PostFx/BrnPostFx.cpp').write_bytes(cpp.rstrip().replace('\n', '\r\n').encode() + b'\r\n')
print('Native caller/header written')
