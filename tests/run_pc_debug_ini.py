"""Typed startup overrides using production Variable metadata, callbacks and menu-edit bodies."""
from pathlib import Path
import os
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
HERE = Path(__file__).resolve().parent
tree = Tree()
variable = tree.read('src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariable.cpp')
manager = tree.read('src/GameShared/GameClasses/Development/DebugSystem/Core/UI/Variables/CgsVariableManager.cpp')
code = 'namespace CgsDev { namespace DebugUI {\n'
for signature in (
    'bool VariableMetadata::Prepare(', 'bool Variable::Prepare(const Variant&',
    'Variant&    Variable::GetValue(', 'const char* Variable::GetName(',
    'void Variable::Release(', 'VariableMetadata* Variable::FindMetadata(',
    'bool Variable::IsReadOnly(', 'const StringList* Variable::GetStringList(',
    'void Variable::AddMetadata(', 'void Variable::RemoveMetadata(',
    'void Variable::Increment(', 'void Variable::OnChange(', 'void Variable::SetValueFromString('):
    code += definition(variable, signature) + '\n'
code += definition(manager, 'void VariableManager::ApplyIniOverridesPC(') + '\n'
code += definition(manager, 'void VariableManager::Destruct(') + '\n} }\n'
numeric = compile_and_run(HERE/'PCDebugIni.cpp', 'pc_debug_ini.inc', code, 'PCDebugIni',
    extra_sources=(HERE.parent/'src/GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.cpp',),
    extra_flags='user32.lib')
raise SystemExit(report('run_pc_debug_ini', [], numeric, 1))
