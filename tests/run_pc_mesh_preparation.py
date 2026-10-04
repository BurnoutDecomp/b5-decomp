"""Production mesh preparation/publication with concurrent producer and consumer.

Conversion is an observable boundary here; run_pc_object_mesh_jobs --write-bank
separately executes the real conversion, partitioning, bins and native workers.
"""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, definition, code_only, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
p = argparse.ArgumentParser()
p.add_argument('--stale-resources', action='store_true')
p.add_argument('--wrong-bank', action='store_true')
p.add_argument('--skip-sort-join', action='store_true')
a = p.parse_args()
t = Tree()
renderer = t.read('src/GameSource/Graphics/BrnRendererModule.cpp')
header = t.read('src/GameSource/Graphics/BrnRendererModule.h')
code = ''
for sig in ['void BrnRendererModule::InitializeDispatchContextPC(',
            'bool BrnRendererModule::BuildDispatchLists(',
            'void BrnRendererModule::PrepareMeshFramePC(',
            'void BrnRendererModule::BeginMeshFramePC(',
            'void BrnRendererModule::PrepareMeshFrameForWritePC(',
            'void BrnRendererModule::PublishMeshFramePC(']:
    code += definition(renderer, sig) + '\n'
if a.stale_resources:
    code = code.replace('&& lrPrepared.muResourceEpoch == luEpoch', '')
if a.wrong_bank:
    code = code.replace('PrepareMeshFramePC(1u - muMeshReadFramePC,',
                        'PrepareMeshFramePC(muMeshReadFramePC,')
if a.skip_sort_join:
    code = code.replace('lrPrepared.mSortJobs.WaitAll();', '')
state = definition(header, 'struct PreparedMeshFramePC') + ';\n'
state += definition(header, 'CgsGraphics::DispatchFrame& GetMeshFrameForReadPC(')

start = code_only(definition(renderer, 'void BrnRendererModule::StartOfFrame('))
swap = code_only(definition(renderer, 'void BrnRendererModule::SwapBuffers('))
game = t.read('src/GameSource/Game/BrnGameModule.cpp')
update = code_only(definition(game, 'bool BrnGameModule::UpdateThread('))
wiring = [
    ('prepare completed GDL after all update stages',
     update.rfind('PrepareMeshFrameForWritePC();') > update.rfind('GameMain();')
     and update.rfind('PrepareMeshFrameForWritePC();') > update.rfind('GameRelease()')
     and update.count('return ') == 1),
    ('read preparation starts before releasing render worker', 'BeginMeshFramePC();' in start),
    ('mesh publication precedes the matching GDL swap',
     0 <= swap.find('PublishMeshFramePC();') < swap.find('mDoubleBufferedDispatchFrame.Swap();')),
]
for sig in ['void BrnRendererModule::RenderShadowMapPasses(',
            'void BrnRendererModule::RenderWorldPasses(', 'void BrnRendererModule::Render(']:
    body = code_only(definition(renderer, sig))
    wiring.append((sig + ' consumes published mesh bank',
                   'mSingleBufferedDispatchFrame' not in body and 'GetMeshFrameForReadPC()' in body))

result = compile_and_run(Path(__file__).with_name('PCMeshPreparation.cpp'),
    'pc_mesh_preparation.inc', code, 'PCMeshPreparation',
    extra_files={'pc_mesh_preparation_state.inc': state})
raise SystemExit(report('run_pc_mesh_preparation', wiring, result, 19))
