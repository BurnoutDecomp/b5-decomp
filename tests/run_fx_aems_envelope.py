"""Execute production AEMS envelopes through start, pause, resume, release and finish."""
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
code = definition(source, 'struct EnvelopeBlock') + ';\n'
code += definition(source, 'inline s32 RoundAems(') + '\n'
code += definition(source, 's32 UpdateEnvelope(')
numeric = compile_and_run(Path(__file__).with_name('FxAemsEnvelope.cpp'),
                          'fx_aems_envelope.inc', code, 'FxAemsEnvelope')
raise SystemExit(report('run_fx_aems_envelope', [], numeric, 24))
