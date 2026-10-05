"""Run production save task submission/update callbacks against the real native writer."""
from pathlib import Path
import os
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
os.environ.pop('BRN_HARNESS_SLOT', None)
tree = Tree()
source = tree.read('src/GameShared/GameClasses/Gui/CgsSaveLoadPS3.cpp')
helpers = definition(source, 'struct PendingSavePC') + ';\n'
helpers += 'std::unordered_map<CgsGui::SaveLoadSystem*, PendingSavePC> sPendingSavesPC;\n'
helpers += definition(source, 'void FinishPendingSavePC(') + '\n' + definition(source, 'void ReportSavePC(')
methods = '\n'.join(definition(source, name) for name in (
    'void SaveLoadSystem::Update()', 'void SaveLoadSystem::Save()',
    'void SaveLoadSystem::Save(SaveLoadTaskResultHandler*'))
body = helpers + '\nnamespace CgsGui {\n' + methods + '\n}\n'
backend = tree.read('src/GameShared/GameClasses/Gui/PC/CgsSaveLoadPC.cpp')
result = compile_and_run(Path(__file__).with_name('PCAsyncSaveTask.cpp'),
                         'async_save_task.inc', body, 'PCAsyncSaveTask',
                         extra_files={'async_save_backend.inc': backend})
raise SystemExit(report('run_pc_async_save_task', [], result, 14))
