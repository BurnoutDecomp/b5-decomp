"""Native reflection pixels through the production viewport, clears and sky state.

Mesh contents and the D3D9 device are fixture boundaries. --rev exercises the
same checks against a previous b5 revision, including the missing-prop control.
"""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--rev', help='b5 source revision for the regression control')
args = parser.parse_args()
tree = Tree(args.rev)
shims = tree.read('src/pc/gcm/renderengine/XenonD3D9Shims.cpp')
immediate = tree.read('src/pc/gcm/renderengine/ImmediateModePCLeaf.cpp')
target = tree.read('src/pc/gcm/renderengine/PostFxRenderTargetPCLeaf.cpp')
sky = re.search(r'ImDepthStencilState sSkyDomeEnvMapDepthStencilState\s*=\s*\{[^}]+\};', immediate).group()
methods = '\n'.join(definition(shims, sig) for sig in (
    'inline DWORD XenonCompareToD3D9(', 'void D3DDevice_SetViewportF(',
    'void D3DDevice_SetRenderState_ZFunc('))
methods += '\n' + definition(immediate, 'struct ImDepthStencilState\n') + ';\n' + sky
methods += '\n' + '\n'.join(definition(immediate, sig) for sig in (
    'void ImDeviceSetDepthStencilState(void*', 'void DeviceClearDepthStencil(',
    'void renderengine::Device::Clear(const renderengine::ClearColorParameters& lrClearColour,\n'
    '                                 const renderengine::ClearDepthStencilParameters&'))
# The normal target's actual viewport install, including its mode transition.
begin = definition(target, 'void RenderTarget::Begin(')
start = begin.index('        D3DVIEWPORT9 lViewport;')
end = begin.index(';', begin.index('SetViewport(', start)) + 1
methods += '\nvoid BeginNormalTargetViewport(u32 muWidth, u32 muHeight)\n{\n'
methods += 'IDirect3DDevice9* const lpDevice = Dev();\n' + begin[start:end] + '\n}\n'
methods += '\nnamespace renderengine {\n' + definition(target, 'class RenderTargetState\n') + ';\n}\n'
methods += definition(target, 'void Device::SetState(const RenderTargetState*').replace(
    'void Device::SetState(', 'void renderengine::Device::SetState(')
depth_header = 'src/pc/gcm/renderengine/DepthRangePCLeaf.h'
result = compile_and_run(Path(__file__).with_name('PCReflectionDepth.cpp'),
    'reflection_depth.inc', methods, 'PCReflectionDepth',
    shadow={depth_header: tree.read(depth_header) or '#pragma once\n'},
    extra_flags='d3d9.lib user32.lib')
raise SystemExit(report('run_pc_reflection_depth', [], result, 1))
