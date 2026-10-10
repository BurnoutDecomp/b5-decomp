"""Exercise production activation paths and error-dialog colours, including login-style HUDs."""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--rev')
args = parser.parse_args()
tree = Tree(args.rev)
root = 'src/GameShared/GameClasses/Development/DebugSystem/Core/'
manager = tree.read(root + 'CgsDebugManager.cpp')
component = tree.read(root + 'CgsDebugComponent.cpp')
script = tree.read(root + 'UI/ScriptInterface/CgsScriptInterface.cpp')
error = tree.read(root + 'UI/Windows/CgsErrorWindow.cpp')
methods = 'namespace CgsDev {\n'
methods += definition(manager, 'void DebugManager::ActivateComponent(') + '\n'
methods += definition(component, 'void DebugComponent::DebugUISectionCallback(') + '\n'
methods += 'namespace DebugUI {\n'
methods += definition(script, 'bool ScriptInterface::ExecuteScriptComponent(') + '\n'
methods += definition(script, 'void ScriptInterface::ScriptCommand_ActivateComponent(') + '\n'
methods += 'namespace {\n' + definition(error, 'RGBA ScalePulseColour(') + '\n}\n'
methods += definition(error, 'void ErrorWindow::Render(') + '\n'
methods += definition(error, 'void ErrorWindow::Update(') + '\n}}\n'
result = compile_and_run(Path(__file__).with_name('PCDebugComponentActivation.cpp'),
    'pc_debug_component_activation.inc', methods, 'PCDebugComponentActivation',
    shadow={'src/GameShared/GameClasses/Network/CgsNetworkVersionDisplay.h':
        '#pragma once\nnamespace CgsNetwork { struct VersionDisplay : CgsDev::DebugComponent {}; }\n'})
raise SystemExit(report('run_pc_debug_component_activation', [], result, 1))
