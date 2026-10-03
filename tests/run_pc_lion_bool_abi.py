"""Link two real Lion interface consumers with opposite EABase include order."""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--rev', help='source revision for the include-order regression control')
args = parser.parse_args()
header = 'src/SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRender/ParticleRender.h'
here = Path(__file__).parent
result = compile_and_run(here/'PCLionBoolABI.cpp', 'unused.inc', '', 'PCLionBoolABI',
    shadow={header:Tree(args.rev).read(header)}, extra_sources=[here/'PCLionBoolABIProvider.cpp'],
    extra_flags='/GL')
raise SystemExit(report('run_pc_lion_bool_abi', [], result, 3))
