"""Native shader/declaration state, pixels and fixed-function/modal transitions."""
from pathlib import Path
import argparse
import os
import re
from fxgs_common import Tree, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
os.environ.pop('BRN_SHADER_BIND_CACHE', None)
parser = argparse.ArgumentParser()
parser.add_argument('--skip-fvf-invalidation', action='store_true')
parser.add_argument('--skip-assert-invalidation', action='store_true')
args = parser.parse_args()
tree = Tree()
binding = 'src/pc/gcm/renderengine/ShaderBindingsPCLeaf.h'
modal = 'src/pc/gcm/renderengine/AssertFramePCLeaf.h'
headers = {binding: tree.read(binding), modal: tree.read(modal)}
if args.skip_fvf_invalidation:
    needle = '            mDeclaration = {};\n            return lpDevice->SetFVF(luFvf);'
    assert headers[binding].count(needle) == 1
    headers[binding] = headers[binding].replace(needle, '            return lpDevice->SetFVF(luFvf);')
if args.skip_assert_invalidation:
    needle = '                gPCShaderBindingCache.Invalidate();'
    assert headers[modal].count(needle) == 1
    headers[modal] = headers[modal].replace(needle, '')
writers = [
    'src/pc/gcm/renderengine/XenonD3D9Shims.cpp',
    'src/pc/gcm/renderengine/InstancingPCLeaf.h',
    'src/GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.cpp',
    'src/GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.cpp',
    'src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.cpp',
]
raw = re.compile(r'->\s*(SetVertexShader|SetPixelShader|SetVertexDeclaration|SetFVF)\s*\(')
wiring = [(name, not raw.search(tree.read(name))) for name in writers]
result = compile_and_run(Path(__file__).with_name('PCShaderBindings.cpp'), 'unused.inc', '',
                         'PCShaderBindings', shadow=headers, extra_flags='d3d9.lib d3dcompiler.lib user32.lib')
raise SystemExit(report('run_pc_shader_bindings', wiring, result, 21))
