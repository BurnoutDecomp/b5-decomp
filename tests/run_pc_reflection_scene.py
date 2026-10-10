"""Observe production backdrop packets plus optional category distance/LOD policies."""
from pathlib import Path
import os
from fxgs_common import Tree, definition, compile_and_run, report
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
tree = Tree()
world = tree.read('src/GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.cpp')
capture = tree.read('src/pc/gcm/renderengine/reflections/SceneCapture.cpp')
code = 'namespace BrnWorld {\nvoid ' + definition(world, 'WorldEntityModule::GenerateDispatchListsForEnvironmentMap(') + '\n}\n'
backdrops = 'namespace CgsPC::Reflections {\n' + definition(capture, 'void WorldCapture::SubmitBackdrops(') + '\n}\n'
result = compile_and_run(Path(__file__).with_name('PCReflectionScene.cpp'), 'pc_world_reflection_distance.inc', code,
    'PCReflectionScene', extra_files={'pc_reflection_backdrops.inc': backdrops})
raise SystemExit(report('run_pc_reflection_scene', [], result, 1))
