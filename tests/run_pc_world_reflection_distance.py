"""Observe world reflection packets from the production body; --rev supplies a regression control."""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--rev')
args = parser.parse_args()
here = Path(__file__).resolve().parent
source = Tree(args.rev).read('src/GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.cpp')
code = 'namespace BrnWorld {\nvoid ' + definition(source, 'WorldEntityModule::GenerateDispatchListsForEnvironmentMap(') + '\n}\n'
numeric = compile_and_run(here/'PCWorldReflectionDistance.cpp', 'pc_world_reflection_distance.inc',
    code, 'PCWorldReflectionDistance')
raise SystemExit(report('run_pc_world_reflection_distance', [], numeric, 1))
