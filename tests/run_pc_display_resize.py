"""Real D3D9 checks of creation dimensions and atomic native pool resize.

--rev runs the previous native consumer; its original three-target transaction
remains intact while the new assertions observe the omitted quarter targets.
"""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--rev')
args = parser.parse_args()
HERE = Path(__file__).resolve().parent
tree = Tree(args.rev)
leaf = tree.read('src/pc/gcm/renderengine/PostFxRenderTargetPCLeaf.cpp')
resize = definition(leaf, 'bool PCResizeDisplayTargets(')
memory = tree.read('src/GameSource/Graphics/BrnRendererMemory.cpp')
device = tree.read('src/pc/gcm/renderengine/device.cpp')
code = 'namespace renderengine {\n' + definition(leaf, 'class RenderTargetState\n') + ';\n}\n'
code += definition(leaf, 'struct DepthTargetRecord\n') + ';\nDepthTargetRecord gaDepthTargetRecords[8] = {};\n'
code += '''static IDirect3DDevice9* Dev() { return renderengine::gDevice; }
namespace renderengine { const RenderTargetState* gpLastRenderTargetState = nullptr; }
namespace rw { namespace graphics { namespace postfx { renderengine::RenderTargetState* gpDefaultRenderTargetState = nullptr; } } }
'''
code += 'namespace renderengine {\n' + definition(leaf, 'void PCInstallDefaultRenderTargetState(') + '\n'
code += resize + '\n}\n'
code += definition(device, 'HRESULT renderengine::PCGetBackBuffer(') + '\n'
code += definition(device, 'bool renderengine::Device::ResizeDisplay(') + '\n'
code += 'namespace rw { namespace graphics { namespace postfx {\n'
code += definition(leaf, 'renderengine::RenderTargetState* RenderTarget::GetSectionRenderTargetState(')
code += '\n} } }\n'
code += definition(memory, 'void BrnRendererMemory::PCResizeDisplay(') + '\n'
for name in ('CreateBloomBuffer', 'CreateDepthOfFieldBuffer', 'CreateWorkBuffer'):
    code += definition(memory, f'void BrnRendererMemory::{name}(') + '\n'
code = '#define PC_QUARTER_TARGETS ' + str(int('lpBloom' in resize.split('{',1)[0])) + '\n' + code
header = 'src/pc/gcm/renderengine/ShadowPassPCLeaf.h'
numeric = compile_and_run(HERE / 'PCDisplayResize.cpp', 'pc_display_resize.inc', code,
                          'PCDisplayResize', extra_flags='d3d9.lib user32.lib',
                          shadow={header: tree.read(header)})
raise SystemExit(report('run_pc_display_resize', [], numeric, 120))
