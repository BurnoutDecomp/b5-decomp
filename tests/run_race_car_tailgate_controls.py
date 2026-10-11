"""Exercise the production tailgate predicate and its boost-credit threshold.

--rev HEAD runs the same stimuli against the pre-repair consumers. The constants,
predicate and SetTailgating statement are extracted from that revision, so a local
constant cannot accidentally pass as a live debug variable.
"""
import argparse
import os
from pathlib import Path
import re
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rev')
args = parser.parse_args()
tree = Tree(args.rev)
base = 'src/GameSource/World/EntityModules/RaceCarEntityModule/'
source = tree.read(base + 'BrnRaceCarEntityModule_NearMissTailgate.cpp')
predicate = definition(source, 'bool RaceCarEntityModule::IsPlayerCarTailgatingOtherRaceCars(')
# Carry the old file-local constants into the RED fixture exactly as written.
constants = re.findall(r'const f32 KF_TAILGATE_\w+\s*=\s*[^;]+;', source)
boost = definition(tree.read(base + 'BrnRaceCarEntityModule.cpp'),
                   'void RaceCarEntityModule::UpdateBoost(')
threshold = re.findall(r'const f32 KF_MIN_TAILGATE_DURATION\s*=\s*[^;]+;', boost)
credit = re.search(r'mBoostManager\.SetTailgating\(\s*mfCurrentTailgateDuration[\s\S]*?\);', boost)
if credit is None:
    raise SystemExit('missing production tailgate credit call')
tunables = tree.read(base + 'BrnRaceCarEntityModule_wBT_01.cpp')
tunables = re.sub(r'^#include[^\n]*', '', tunables, flags=re.M)
methods = tunables + '\nnamespace BrnWorld {\nnamespace {\n' + '\n'.join(constants) + '\n}\n'
methods += predicate + '\nvoid RaceCarEntityModule::UpdateTailgateBoost() {\n'
methods += '\n'.join(threshold) + '\n' + credit[0] + '\n}\n}\n'
result = compile_and_run(Path(__file__).with_name('RaceCarTailgateControls.cpp'),
    'race_car_tailgate_controls.inc', methods, 'RaceCarTailgateControls')
raise SystemExit(report('run_race_car_tailgate_controls', [], result, 1))
