"""Execute the production opcode12 layout and state transition against native bytes."""
import argparse
from pathlib import Path
import sys
sys.dont_write_bytecode = True
from fxgs_common import Tree, definition, compile_and_run, report
parser = argparse.ArgumentParser()
parser.add_argument('--rev')
parser.add_argument('--source', type=Path)
args = parser.parse_args()
path = 'src/SDKs/EATech/include/snd/sndaems.cpp'
source = args.source.read_text(encoding='utf-8') if args.source else Tree(args.rev).read(path)
code = definition(source, 'struct StateGeneratorBlock') + ';\n'
code += definition(source, 's32 UpdateStateGenerator(')
numeric = compile_and_run(Path(__file__).with_name('FxAemsStateGenerator.cpp'),
                          'fx_aems_state_generator.inc', code, 'FxAemsStateGenerator')
raise SystemExit(report('run_fx_aems_state_generator', [], numeric, 10))
