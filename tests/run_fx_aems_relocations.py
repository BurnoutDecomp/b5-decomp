"""Execute the production AEMS relocation dispatch against typed resolver spies."""
import argparse
from pathlib import Path
import sys
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
from fxgs_common import Tree, definition, compile_and_run, report

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path)
parser.add_argument('--rev')
args = parser.parse_args()
path = 'src/SDKs/EATech/include/snd/sndaems.cpp'
source = args.source.read_text() if args.source else Tree(args.rev).read(path)
code = '\n'.join(definition(source, start) for start in (
    'bool ValidSpan(', 'bool ValidResidentSpan(', 'bool ApplyCsisRelocations('))
numeric = compile_and_run(HERE / 'FxAemsRelocations.cpp', 'fx_aems_relocations.inc',
                          code, 'FxAemsRelocations')
raise SystemExit(report('run_fx_aems_relocations', [], numeric, 12))
