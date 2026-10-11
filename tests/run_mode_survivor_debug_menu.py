"""Exercise the Mode Manager menu and its four Survivor tuning consumers."""
from pathlib import Path
import argparse
import os
import re

from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rev')
args = parser.parse_args()
tree = Tree(args.rev)

debug_path = 'src/GameSource/GameState/ModeManager/Debug/BrnModeManagerDebugComponent.cpp'
survivor_path = 'src/GameSource/GameState/ModeManager/GameModes/BrnSurvivor.cpp'
debug_source = tree.read(debug_path)
survivor_source = tree.read(survivor_path)

global_names = (
    'giMarkedManMinOpponentCount',
    'giMarkedManMaxOpponentCount',
    'gfMarkedManMinRampTime',
    'gfMarkedManMaxRampTime',
)
global_lines = []
for name in global_names:
    match = re.search(r'^\s*(?:s32|f32)\s+' + re.escape(name) + r'\s*=\s*[^;]+;',
                      debug_source, re.MULTILINE)
    if match:
        global_lines.append(match.group(0).strip())

debug_signatures = (
    'void ModeManagerDebugComponent::Construct(ModeManager* lpModeManager)',
    'void ModeManagerDebugComponent::OnActivate()',
    'void ModeManagerDebugComponent::FinshMode(void* lpContext)',
)
survivor_signature = 'void SurvivorMode::Start('
missing = [name for name in global_names if not any(name in line for line in global_lines)]
methods = []
for signature in debug_signatures:
    try:
        methods.append(definition(debug_source, signature))
    except (ValueError, IndexError):
        missing.append(signature.strip())
try:
    survivor_body = definition(survivor_source, survivor_signature)
    methods.append(survivor_body)
except (ValueError, IndexError):
    missing.append(survivor_signature)

generated = 'namespace BrnGameState {\n' + '\n'.join(global_lines + methods) + '\n}\n'
result = None if missing else compile_and_run(
    Path(__file__).with_name('ModeSurvivorDebugMenu.cpp'),
    'mode_survivor_debug_menu.inc', generated, 'ModeSurvivorDebugMenu')

wiring = [(f'production body present: {signature}', signature not in missing)
          for signature in debug_signatures + (survivor_signature,)]
wiring += [(f'production backing value present: {name}', name not in missing)
           for name in global_names]
raise SystemExit(report('run_mode_survivor_debug_menu', wiring, result, 41))
