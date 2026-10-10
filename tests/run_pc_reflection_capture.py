"""Native cube feedback isolation, capture depth state, and vehicle mesh filtering."""
from pathlib import Path
import os
from fxgs_common import Tree, definition, compile_and_run, report
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
tree = Tree()
scene = tree.read('src/pc/gcm/renderengine/reflections/SceneRender.cpp')
wiring = []
if scene.find('DecalCapture::Render(') > scene.find('ParticleCapture::Render('):
    wiring.append('road decals must precede translucent particle draws')
body = definition(tree.read('src/GameShared/GameClasses/Graphics/CgsMaterialAssembly.cpp'),
    'MaterialTechnique* MaterialAssembly::GetMaterial(')
body += '\n' + definition(Tree().read('src/GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.cpp'), 'void* DispatchBin::AllocateMemoryFast(')
result = compile_and_run(Path(__file__).with_name('PCReflectionCapture.cpp'),
    'pc_reflection_capture.inc', 'namespace CgsGraphics {\n'+body+'\n}',
    'PCReflectionCapture', extra_flags='d3d9.lib user32.lib')
raise SystemExit(report('run_pc_reflection_capture', wiring, result, 1))
