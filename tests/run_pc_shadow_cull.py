"""Native pixels and state suppression through the production shadow cascade loop.

The rasterizer and blend factories/serializers are linked unmodified. Target
binding, allocator, debug probes, and mesh contents are fixture boundaries.
"""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import REPO, Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--omit-brackets', action='store_true')
parser.add_argument('--invert-group', action='store_true')
parser.add_argument('--rev', help='b5 source revision, including the missing-traffic regression control')
args = parser.parse_args()
tree = Tree(args.rev)
manager = tree.read('src/GameSource/Graphics/BrnShadowMapRenderManager.cpp')
renderer = tree.read('src/GameSource/Graphics/BrnRendererModule.cpp')
device = tree.read('src/GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.cpp')
shims = tree.read('src/pc/gcm/renderengine/XenonD3D9Shims.cpp')
native = '\n'.join(definition(shims, signature) for signature in (
    ['inline DWORD XenonCullToD3D9(', 'inline DWORD XenonFillToD3D9('] +
    ['void D3DDevice_SetRenderState_' + name + '(' for name in (
        'CullMode', 'FillMode', 'ScissorTestEnable', 'SlopeScaleDepthBias', 'DepthBias',
        'MultiSampleAntiAlias', 'MultiSampleMask', 'ViewportEnable', 'HalfPixelOffset',
        'PrimitiveResetEnable', 'PrimitiveResetIndex')]))
methods = '\n'.join(definition(manager, 'void BrnGraphics::ShadowMapRenderManager::' + name + '(')
                    for name in ('Construct', 'BeginFrontFaceCullRender', 'EndFrontFaceCullRender',
                                 'BeginBackFaceCullRender', 'EndBackFaceCullRender'))
bodies = native + '\nnamespace shadow {\n' + '\n'.join(definition(device, signature) for signature in (
    'void* Device::LockRasteriserState(', 'void* Device::UnlockRasteriserState(',
    'void* Device::Xbox2SetRasterizerStateLowLevelShadowed(',
    'void Device::SetState(const renderengine::RasterizerState*',
    'void Device::SetMaterialRenderStatesPC(')) + '\n}\n' + methods
render = definition(renderer, 'void BrnRendererModule::RenderShadowMapPasses(')
mapping = re.search(r'static const s32 KAI_CASCADE_LISTS.*?;', render, re.S).group()
# Exact production loop, including the bracket calls and the empty second slot.
first = render.index('    const u64 luDrawCallsBeforePass')
last = render.index('    renderengine::PCSurfaceBracket_Restore();', first)
admission_first = render.index('    u32 lauCascadeCounts')
admission_last = render.index('#if !BRN_SHADOW_MAP_TARGET_AVAILABLE', admission_first)
loop = mapping + '\n' + render[admission_first:admission_last] + render[first:last]
if args.omit_brackets:
    for name in ('BeginFrontFaceCullRender', 'EndFrontFaceCullRender', 'BeginBackFaceCullRender', 'EndBackFaceCullRender'):
        loop = loop.replace('mShadowMapRenderManager.' + name + '();', '(void)0;')
if args.invert_group:
    assert loop.count('if (liSlot == 0)') == 2
    loop = loop.replace('if (liSlot == 0)', 'if (liSlot != 0)')
construct = definition(renderer, 'void BrnRendererModule::Construct(')
construct_call = re.search(r'mShadowMapRenderManager\.Construct\([^;]+;', construct).group()
world = tree.read('src/GameSource/World/BrnWorldModule.cpp')
if 'WorldModule::GenerateDispatchListsBringUp(' in world:
    producer = definition(world, 'WorldModule::GenerateDispatchListsBringUp(')
    traffic_gate = re.search(r'const bool lbTraffic = mShadowMap\.GetRenderTrafficIntoShadowMap\(\).*?;', producer, re.S)
    traffic = (traffic_gate.group() + '\n' + definition(producer, 'if (lbTraffic)\n')) if traffic_gate else '// No traffic caster leg in this source revision.\n'
else:
    producer = definition(world, 'WorldModule::GenerateShadowMapDispatchLists(')
    traffic_gate = re.search(r'const bool lbTraffic\s*= mShadowMap\.GetRenderTrafficIntoShadowMap\(\).*?;', producer, re.S)
    start = producer.rfind('        if ( lbTraffic )')
    traffic = traffic_gate.group() + '\n' + definition(producer[start:], 'if ( lbTraffic )')
traffic = '#define TEST_TRAFFIC_RENDER_SWITCH %d\n' % ('lpSwitches->mbRenderTraffic' in traffic) + traffic
technique = definition(tree.read('src/GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h'),
                       'struct MaterialTechniqueView') + ';'
shadow = {
    'src/GameShared/GameClasses/Core/CgsAssert.h':
        '#pragma once\n#include <cstdlib>\n#define CGS_ASSERT(ok, ...) do { if (!(ok)) std::abort(); } while (0)\n',
    'src/GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h':
        '#pragma once\nvoid* AllocateTestState(void*, const void*);\nnamespace CgsGraphics {\n'
        'inline void* ResourceAllocatorCreate(void*, void* out, const void* descriptor) '
        '{ return AllocateTestState(out, descriptor); }\n}\n'
}
result = compile_and_run(Path(__file__).with_name('PCShadowCull.cpp'), 'shadow_cull.inc', bodies,
    'PCShadowCull', shadow=shadow, extra_files={
        'shadow_cull_technique.inc': 'namespace CgsGraphics {\n' + technique + '\n}\n',
        'shadow_cull_loop.inc': loop, 'shadow_cull_construct.inc': construct_call,
        'shadow_traffic_caster.inc': traffic},
    extra_sources=[REPO / 'src/GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.cpp',
                   REPO / 'src/pc/gcm/renderengine/RasterizerState.cpp',
                   REPO / 'src/GameShared/GameClasses/Graphics/CgsBlendStateFactory.cpp',
                   REPO / 'src/SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.cpp'],
    extra_flags='d3d9.lib user32.lib')
raise SystemExit(report('run_pc_shadow_cull', [], result, 1))
