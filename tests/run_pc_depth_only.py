"""Native depth/cutout oracle through the production technique and program binders.

The real blend factory and serializer are linked. Only allocator, diagnostics,
unchanged shader metadata and constant/sampler upload boundaries are fixtures.
"""
import argparse
import os
from pathlib import Path
from fxgs_common import REPO, Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--material-depth-shader', action='store_true', help='negative: opaque depth executes material clip')
parser.add_argument('--material-depth-state', action='store_true', help='negative: depth copies the material blend')
parser.add_argument('--alpha-uses-depth-shader', action='store_true', help='negative: alpha depth loses texture coverage')
parser.add_argument('--mix-shader-models', action='store_true', help='negative: pair an older vertex program with the SM3 depth program')
args = parser.parse_args()
tree = Tree()
device = tree.read('src/GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.cpp')
shims = tree.read('src/pc/gcm/renderengine/XenonD3D9Shims.cpp')
technique = definition(tree.read('src/GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h'),
                       'struct MaterialTechniqueView') + ';'
bind = definition(shims, 'bool WorldPrograms_Bind(')
states = definition(device, 'void Device::SetMaterialRenderStatesPC(')
mesh = definition(device, 'void Device::SetMeshTechniquePC(')
if args.mix_shader_models:
    old = 'lbDepthOnly && *static_cast<const DWORD*>(lpVertexPayload) == D3DVS_VERSION(3, 0)'
    assert bind.count(old) == 1
    bind = bind.replace(old, 'lbDepthOnly')
if args.material_depth_shader:
    old = ('if (lbDepthOnly && *static_cast<const DWORD*>(lpVertexPayload) == D3DVS_VERSION(3, 0))\n'
           '            lpPixelPayload = DepthOnlyPC::KAU_PIXEL_CODE;')
    assert bind.count(old) == 1
    bind = bind.replace(old, '(void)lbDepthOnly;')
if args.material_depth_state:
    first = states.index('        if (lbZOnly)\n')
    last = states.index('        if (lpWantedBlend != mpBlendState)', first)
    states = states[:first] + states[last:]
if args.alpha_uses_depth_shader:
    old = 'lbZOnly, lpTechnique ? lpTechnique->mu16Flags : 0u)'
    assert mesh.count(old) == 1
    mesh = mesh.replace(old, 'lbZOnly, 0u)')
native = '\n'.join(definition(shims, signature) for signature in (
    'bool ProgramDeclaresFloat4Constant(',
    'inline bool LooksLikeD3D9Bytecode(', 'inline DWORD XenonCompareToD3D9(', 'void D3DDevice_SetPixelShader(',
    'void D3DDevice_SetRenderState_ColorWriteEnable(',
    'void D3DDevice_SetRenderState_AlphaTestEnable(',
    'void D3DDevice_SetRenderState_AlphaRef(', 'void D3DDevice_SetRenderState_AlphaFunc('))
bodies = (native + '\nnamespace renderengine {\n' + bind + '\n}\nnamespace shadow {\n'
          + '\n'.join(definition(device, signature) for signature in (
              'bool Device::SetVertexProgram(', 'bool Device::SetPixelProgram('))
          + '\n' + states + '\n' + mesh + '\n}\n')
shadow = {
    'src/GameShared/GameClasses/Core/CgsAssert.h':
        '#pragma once\n#include <cstdlib>\n#define CGS_ASSERT(ok, ...) do { if (!(ok)) std::abort(); } while (0)\n',
    'src/GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h':
        '#pragma once\nvoid* AllocateTestBlend(void*, const void*);\nnamespace CgsGraphics {\n'
        'inline void* ResourceAllocatorCreate(void*, void* out, const void* descriptor) '
        '{ return AllocateTestBlend(out, descriptor); }\n}\n'
}
result = compile_and_run(Path(__file__).with_name('PCDepthOnly.cpp'), 'depth_only.inc', bodies,
    'PCDepthOnly', shadow=shadow,
    extra_files={'depth_technique.inc': 'namespace CgsGraphics {\n' + technique + '\n}\n'},
    extra_sources=[REPO / 'src/GameShared/GameClasses/Graphics/CgsBlendStateFactory.cpp',
                   REPO / 'src/SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.cpp'],
    extra_flags='d3d9.lib d3dcompiler.lib user32.lib')
raise SystemExit(report('run_pc_depth_only', [], result, 27))
