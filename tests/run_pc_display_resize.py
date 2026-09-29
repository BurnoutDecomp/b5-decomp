"""Real D3D9 checks of the production display/pool resize transaction."""
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

HERE = Path(__file__).resolve().parent
tree = Tree()
leaf = tree.read('src/pc/gcm/renderengine/PostFxRenderTargetPCLeaf.cpp')
device = tree.read('src/pc/gcm/renderengine/device.cpp')
code = 'namespace renderengine {\n' + definition(leaf, 'class RenderTargetState\n') + ';\n}\n'
code += definition(leaf, 'struct DepthTargetRecord\n') + ';\nDepthTargetRecord gaDepthTargetRecords[8] = {};\n'
code += '''static IDirect3DDevice9* Dev() { return renderengine::gDevice; }
namespace renderengine { const RenderTargetState* gpLastRenderTargetState = nullptr; }
namespace rw { namespace graphics { namespace postfx { renderengine::RenderTargetState* gpDefaultRenderTargetState = nullptr; } } }
'''
code += 'namespace renderengine {\n' + definition(leaf, 'void PCInstallDefaultRenderTargetState(') + '\n'
code += definition(leaf, 'bool PCResizeDisplayTargets(') + '\n}\n'
code += definition(device, 'HRESULT renderengine::PCGetBackBuffer(') + '\n'
code += definition(device, 'bool renderengine::Device::ResizeDisplay(') + '\n'
code += 'namespace rw { namespace graphics { namespace postfx {\n'
code += definition(leaf, 'renderengine::RenderTargetState* RenderTarget::GetSectionRenderTargetState(')
code += '\n} } }\n'
code += definition(tree.read('src/GameSource/Graphics/BrnRendererMemory.cpp'),
                   'void BrnRendererMemory::PCResizeDisplay(') + '\n'
numeric = compile_and_run(HERE / 'PCDisplayResize.cpp', 'pc_display_resize.inc', code,
                          'PCDisplayResize', extra_flags='d3d9.lib user32.lib')
raise SystemExit(report('run_pc_display_resize', [], numeric, 37))
