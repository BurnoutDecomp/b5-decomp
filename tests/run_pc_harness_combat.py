from pathlib import Path
import argparse,os
from fxgs_common import compile_and_run, report, Tree
parser=argparse.ArgumentParser()
parser.add_argument('--old-aim',action='store_true')
args=parser.parse_args()
os.environ.pop("NoDefaultCurrentDirectoryInExePath",None)
path='src/GameSource/World/AI/BrnAIHarnessCombatPC.h'
shadow={path:Tree('a0426eaa').read(path)} if args.old_aim else {}
result = compile_and_run(Path(__file__).with_name("PCHarnessCombat.cpp"), "unused.inc", "", "PCHarnessCombat",shadow=shadow)
raise SystemExit(report("run_pc_harness_combat", [], result, 11))
