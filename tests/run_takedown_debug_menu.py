"""Exercise every Takedown Manager debug-menu control against the production bodies."""
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

debug_component = tree.read('src/GameSource/GameState/TakedownManager/BrnTakedownManagerDebugComponent.cpp')
debug_header = tree.read('src/GameSource/GameState/TakedownManager/BrnTakedownManagerDebugComponent.h')
race_car_data = tree.read('src/GameSource/GameState/TakedownManager/BrnTakedownManagerRaceCarData.cpp')
shunt_effect = tree.read('src/GameSource/Physics/VehicleManager/VehiclePhysics/ShuntEffect.cpp')
rgba = tree.read('src/GameShared/GameClasses/RenderWare/RwRGBA.cpp')

debug_signatures = (
    'const char* TakedownManagerDebugComponent::GetName() const',
    'void TakedownManagerDebugComponent::Construct(',
    'void TakedownManagerDebugComponent::OnActivate()',
    'void TakedownManagerDebugComponent::ForceTakedownCallback(',
    'void TakedownManagerDebugComponent::RecordTakedown(',
    'void TakedownManagerDebugComponent::RenderHUD(',
    'void TakedownManagerDebugComponent::RenderWorld(',
)
shared_signatures = (
    (race_car_data, 'void TakedownManager::RaceCarData::Clear()'),
    (shunt_effect, 'bool VehiclePhysics::ShuntEffect::IsActive() const'),
)

missing = []
debug_methods = []
for signature in debug_signatures:
    try:
        debug_methods.append(definition(debug_component, signature))
    except ValueError:
        missing.append(signature.strip())

shared_methods = []
for source, signature in shared_signatures:
    try:
        shared_methods.append((signature, definition(source, signature)))
    except ValueError:
        missing.append(signature)

gameplay_page = re.search(
    r'GetPath\(\)\s*const\s*override\s*\{\s*return\s*"Gameplay";\s*\}',
    debug_header) is not None

generated = 'namespace rw {\n' + definition(rgba, 'RGBA::RGBA(uint8_t') + '\n}\n'
generated += 'namespace BrnGameState {\n' + '\n'.join(debug_methods) + '\n'
for signature, body in shared_methods:
    if signature.startswith('void TakedownManager::'):
        generated += body + '\n'
generated += '}\nnamespace BrnPhysics { namespace Vehicle {\n'
for signature, body in shared_methods:
    if signature.startswith('bool VehiclePhysics::'):
        generated += body + '\n'
generated += '} }\n'

result = None if missing else compile_and_run(
    Path(__file__).with_name('TakedownDebugMenu.cpp'),
    'takedown_debug_menu.inc', generated, 'TakedownDebugMenu')
raise SystemExit(report(
    'run_takedown_debug_menu',
    [(f'production body present: {signature}', signature not in missing)
     for signature in debug_signatures] +
    [(f'production helper present: {signature}', signature not in missing)
     for _, signature in shared_signatures] +
    [('component remains on the Gameplay debug page', gameplay_page)],
    result,
    24))
