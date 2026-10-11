"""Exercise the original reset-transform store before the car-state branches."""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report, code_only

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rev')
args = parser.parse_args()
tree = Tree(args.rev)
base = 'src/GameSource/World/EntityModules/RaceCarEntityModule/'
body = definition(tree.read(base+'BrnRaceCarEntityModule.cpp'), 'void RaceCarEntityModule::ResetActiveRaceCar(')
prefix = body[body.index('{')+1:body.index('    ActiveRaceCar* lpActiveRaceCar')]
methods = 'void RaceCarEntityModule::RecordResetTransform(const Matrix44Affine& lrTransform) {\n'+prefix+'\n}\n'
header = code_only(tree.read(base+'BrnRaceCarEntityModule.h'))
wiring = [('the module owns the diagnostic component by value',
           'RaceCarEntityModuleDebugComponent mRCEntityModuleDebugComponent;' in header)]
result = compile_and_run(Path(__file__).with_name('RaceCarResetHistory.cpp'),
    'race_car_reset_history.inc', methods, 'RaceCarResetHistory')
raise SystemExit(report('run_race_car_reset_history', wiring, result, 2))
